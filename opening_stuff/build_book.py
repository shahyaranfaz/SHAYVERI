import os
import threading
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor, FIRST_COMPLETED, wait

import chess
import chess.engine

try:
    import orjson
    _json_dumps_bytes = lambda obj: orjson.dumps(obj)
    _json_loads = orjson.loads
except ImportError:
    import json as _json_fallback
    _json_dumps_bytes = lambda obj: _json_fallback.dumps(obj).encode("utf-8")
    _json_loads = lambda b: _json_fallback.loads(b.decode("utf-8"))

try:
    import pandas as pd
    _has_pandas = True
except ImportError:
    import csv
    _has_pandas = False

# --- CONFIGURATION ---
STOCKFISH_PATH = os.getenv("STOCKFISH_PATH", "stockfish")
BOOK_DEPTH = int(os.getenv("BOOK_DEPTH", "18"))

CACHE_FILE = os.getenv("CACHE_FILE", "eval_cache.json")
SAVE_INTERVAL = int(os.getenv("SAVE_INTERVAL", "500"))

# 13700KS: start with 16; try 12..24 depending on thermals/OS responsiveness.
MAX_WORKERS = int(os.getenv("BOOK_WORKERS", "16"))

# Total RAM used by SF hash ~= MAX_WORKERS * STOCKFISH_HASH_MB
STOCKFISH_HASH_MB = int(os.getenv("SF_HASH_MB", "256"))

# Bound number of in-flight futures to keep RAM flat.
MAX_LIVE_FUTURES = int(os.getenv("MAX_LIVE_FUTURES", str(MAX_WORKERS * 4)))

ZOBRIST_KEYS_FILE = os.getenv("ZOBRIST_KEYS_FILE", "zobrist_keys.json")

CSV_FILE = os.getenv("CSV_FILE", "table.csv")
MAX_PLIES_PER_ROW = int(os.getenv("MAX_PLIES_PER_ROW", "30"))

# Book filtering
MIN_TOTAL_PLAYS = int(os.getenv("MIN_TOTAL_PLAYS", "5"))


# ---------------- ZOBRIST (EXACT MATCH VIA C++ DUMP) ----------------

def _u64(x) -> int:
    return int(x) & 0xFFFFFFFFFFFFFFFF


def load_zobrist_keys(path: str):
    if not os.path.exists(path):
        raise FileNotFoundError(
            f"{path} not found.\n"
            f"Generate it with your C++ dump_keys tool first."
        )

    with open(path, "rb") as f:
        data = _json_loads(f.read())

    pieces = [[_u64(v) for v in row] for row in data["pieces"]]
    sides = _u64(data["sides"])
    castlings = [_u64(v) for v in data["castlings"]]
    en_passants = [_u64(v) for v in data["en_passants"]]

    if len(pieces) != 13:
        raise ValueError(f"Expected 13 piece rows (PIECE_COUNT), got {len(pieces)}")
    if any(len(row) != 64 for row in pieces):
        raise ValueError("Each pieces[p] row must have 64 entries")
    if len(castlings) != 16:
        raise ValueError(f"Expected 16 castling keys, got {len(castlings)}")
    if len(en_passants) != 8:
        raise ValueError(f"Expected 8 en-passant file keys, got {len(en_passants)}")

    return pieces, sides, castlings, en_passants


def compute_zobrist_from_cpp_dump(board: chess.Board, pieces, sides, castlings, en_passants) -> int:
    """Mirrors src/zobrist.cpp exactly."""
    h = 0

    for sq, p in board.piece_map().items():
        # C++ Piece enum indexing:
        # WP..WK = 1..6, BP..BK = 7..12
        p_idx = p.piece_type + (0 if p.color == chess.WHITE else 6)
        h ^= pieces[p_idx][sq]

    if board.turn == chess.BLACK:
        h ^= sides

    c = 0
    if board.has_kingside_castling_rights(chess.WHITE):  c |= 1
    if board.has_queenside_castling_rights(chess.WHITE): c |= 2
    if board.has_kingside_castling_rights(chess.BLACK):  c |= 4
    if board.has_queenside_castling_rights(chess.BLACK): c |= 8
    h ^= castlings[c & 15]

    if board.ep_square is not None:
        h ^= en_passants[chess.square_file(board.ep_square)]

    return _u64(h)


# --- STOCKFISH WORKER (persistent engine per process) ---
worker_engine = None

def init_worker():
    global worker_engine
    worker_engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
    worker_engine.configure({"Hash": STOCKFISH_HASH_MB, "Threads": 1})

def analyze_position(args):
    """Returns (zobrist_key_str, eval_float)."""
    global worker_engine
    key_str, fen = args
    board = chess.Board(fen)
    info = worker_engine.analyse(board, chess.engine.Limit(depth=BOOK_DEPTH))
    score = info["score"].white()
    if score.is_mate():
        val = 100.0 if score.mate() > 0 else -100.0
    else:
        val = (score.score() or 0) / 100.0
    return key_str, float(val)


# --- BACKGROUND CACHE SAVER ---
_save_lock = threading.Lock()

def _atomic_write_bytes(path: str, data: bytes):
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)

def _save_cache_thread(cache_snapshot, path):
    with _save_lock:
        _atomic_write_bytes(path, _json_dumps_bytes(cache_snapshot))

def save_cache_async(cache: dict, path: str):
    snapshot = dict(cache)
    threading.Thread(target=_save_cache_thread, args=(snapshot, path), daemon=True).start()


def read_rows():
    if _has_pandas:
        df = pd.read_csv(CSV_FILE, encoding="utf-8-sig")
        return df.to_dict("records")

    with open(CSV_FILE, newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def build_book_uci(rows, pieces, sides, castlings, en_passants):
    """
    Build book keyed by engine Zobrist key directly.
    Moves are UCI like 'e2e4' (fast).
    """
    book = defaultdict(lambda: {"fen": "", "moves": defaultdict(int)})

    for row in rows:
        board = chess.Board()
        moves = str(row["Moves"]).split()[:MAX_PLIES_PER_ROW]
        played = int(row["Played"])

        for uci in moves:
            # record current position before applying this ply
            key_str = str(compute_zobrist_from_cpp_dump(board, pieces, sides, castlings, en_passants))

            if not book[key_str]["fen"]:
                book[key_str]["fen"] = board.fen()

            try:
                board.push_uci(uci)
            except Exception:
                break

            book[key_str]["moves"][uci] += played

    return book


if __name__ == "__main__":
    pieces, sides, castlings, en_passants = load_zobrist_keys(ZOBRIST_KEYS_FILE)

    eval_cache = {}
    if os.path.exists(CACHE_FILE):
        print("Loading existing cache...")
        with open(CACHE_FILE, "rb") as f:
            eval_cache = _json_loads(f.read())

    print("Reading CSV...")
    rows = read_rows()
    print(f"Rows: {len(rows)}")

    print("Building book (UCI moves)...")
    book = build_book_uci(rows, pieces, sides, castlings, en_passants)

    to_analyze = [(key, data["fen"]) for key, data in book.items() if data["fen"] and key not in eval_cache]
    total_to_do = len(to_analyze)
    print(f"Unique positions: {len(book)} | Cached: {len(eval_cache)} | To analyze: {total_to_do}")

    if to_analyze:
        print(f"Starting parallel analysis with {MAX_WORKERS} workers "
              f"(depth {BOOK_DEPTH}, SF Hash {STOCKFISH_HASH_MB} MB each, futures cap {MAX_LIVE_FUTURES})...")

        with ProcessPoolExecutor(max_workers=MAX_WORKERS, initializer=init_worker) as executor:
            pending = set()
            it = iter(to_analyze)
            submitted = 0
            completed = 0

            def _fill_queue():
                nonlocal submitted
                while len(pending) < MAX_LIVE_FUTURES and submitted < total_to_do:
                    args = next(it, None)
                    if args is None:
                        break
                    pending.add(executor.submit(analyze_position, args))
                    submitted += 1

            _fill_queue()

            while pending:
                done, pending = wait(pending, return_when=FIRST_COMPLETED)
                _fill_queue()

                for fut in done:
                    key_str, val = fut.result()
                    eval_cache[key_str] = val
                    completed += 1

                    if completed % SAVE_INTERVAL == 0:
                        pct = completed / total_to_do * 100
                        print(f"  {completed}/{total_to_do} ({pct:.1f}%) — saving cache async...")
                        save_cache_async(eval_cache, CACHE_FILE)

        _atomic_write_bytes(CACHE_FILE, _json_dumps_bytes(eval_cache))
        print("Cache saved.")

    print("Generating header...")
    entries = []
    for key_str, data in book.items():
        if key_str not in eval_cache:
            continue
        if not data["moves"]:
            continue
        if sum(data["moves"].values()) < MIN_TOTAL_PLAYS:
            continue

        best_move = max(data["moves"], key=data["moves"].get)
        entries.append((int(key_str), best_move, float(eval_cache[key_str])))

    # REQUIRED for uci.cpp binary search
    entries.sort(key=lambda x: x[0])

    os.makedirs("include", exist_ok=True)
    with open("include/opening_book.h", "w", encoding="utf-8") as out:
        out.write("#ifndef OPENING_BOOK_H\n#define OPENING_BOOK_H\n#include <cstdint>\n\n")
        out.write("struct BookEntry { U64 key; char move[6]; float evaluation; };\n\n")
        out.write("static constexpr BookEntry OPENING_BOOK[] = {\n")
        for k, mv, ev in entries:
            out.write(f'    {{ 0x{k:016x}ULL, "{mv}", {ev}f }},\n')
        out.write("};\n\n")
        out.write(f"static constexpr int OPENING_BOOK_SIZE = {len(entries)};\n")
        out.write("#endif\n")

    print(f"Done — {len(entries)} entries written to include/opening_book.h")
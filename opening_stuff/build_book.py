import chess
import chess.engine
import os
import threading
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor, FIRST_COMPLETED, wait

try:
    import orjson
    _json_dumps = lambda obj: orjson.dumps(obj).decode()
    _json_loads = orjson.loads
except ImportError:
    import json as _json_fallback
    _json_dumps = lambda obj: _json_fallback.dumps(obj)
    _json_loads = _json_fallback.loads

try:
    import pandas as pd
    _has_pandas = True
except ImportError:
    import csv
    _has_pandas = False

# --- CONFIGURATION ---
STOCKFISH_PATH = "stockfish"
BOOK_DEPTH = 18
CACHE_FILE = "eval_cache.json"
SAVE_INTERVAL = 500
MAX_WORKERS = os.cpu_count()
STOCKFISH_HASH_MB = 256
MAX_LIVE_FUTURES = MAX_WORKERS * 4

# IMPORTANT:
# This file MUST be produced by your C++ engine using the same Zobrist::init()
# (same platform/libc++/libstdc++ build you care about).
ZOBRIST_KEYS_FILE = "zobrist_keys.json"


# ---------------- ZOBRIST (EXACT MATCH VIA C++ DUMP) ----------------

def _u64(x) -> int:
    return int(x) & 0xFFFFFFFFFFFFFFFF


def load_zobrist_keys(path: str):
    if not os.path.exists(path):
        raise FileNotFoundError(
            f"{path} not found.\n\n"
            "To make Python match C++ exactly, dump the Zobrist arrays from your C++ build "
            "after Zobrist::init() into this JSON file."
        )

    with open(path, "rb") as f:
        data = _json_loads(f.read())

    # Expected schema:
    # {
    #   "pieces": [[...64...], ... PIECE_COUNT ...],
    #   "sides": <u64>,
    #   "castlings": [...16...],
    #   "en_passants": [...8...]
    # }
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
    """
    Mirrors zobrist.cpp exactly:

      - pieces indexed by C++ Piece enum:
        0 NONE, 1..6 WP..WK, 7..12 BP..BK
      - side XOR if black to move
      - castlings[castling_bits & 15]
      - en_passants[file] if ep exists
    """
    h = 0

    # python-chess piece_map gives squares 0..63, Piece(piece_type 1..6, color bool)
    for sq, p in board.piece_map().items():
        # Map to C++ Piece enum index:
        # WP=1..WK=6, BP=7..BK=12
        # python-chess: piece_type 1..6, color: True=WHITE, False=BLACK
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


# --- PERSISTENT WORKER (Stockfish per process) ---
worker_engine = None

def init_worker():
    global worker_engine
    worker_engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
    worker_engine.configure({"Hash": STOCKFISH_HASH_MB, "Threads": 1})

def analyze_position(args):
    """Returns (zobrist_key_str, eval_float)"""
    global worker_engine
    fen, key_str = args
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

def _save_cache_thread(cache_snapshot, path):
    with _save_lock:
        with open(path, "wb") as f:
            f.write(_json_dumps(cache_snapshot).encode())

def save_cache_async(cache: dict, path: str):
    snapshot = dict(cache)
    t = threading.Thread(target=_save_cache_thread, args=(snapshot, path), daemon=True)
    t.start()


# --- MAIN ---
if __name__ == "__main__":
    # Load Zobrist keys dumped from C++ so our keys match b.hash exactly.
    pieces, sides, castlings, en_passants = load_zobrist_keys(ZOBRIST_KEYS_FILE)

    book = defaultdict(lambda: {"moves": defaultdict(int), "fen": ""})
    eval_cache = {}

    if os.path.exists(CACHE_FILE):
        print("Loading existing cache...")
        with open(CACHE_FILE, "rb") as f:
            eval_cache = _json_loads(f.read())

    print("Reading CSV...")
    if _has_pandas:
        df = pd.read_csv("table.csv", encoding="utf-8-sig")
        rows = df.to_dict("records")
    else:
        with open("table.csv", newline="", encoding="utf-8-sig") as f:
            rows = list(csv.DictReader(f))

    # Build book frequencies
    for row in rows:
        board = chess.Board()
        moves = str(row["Moves"]).split()[:30]
        played = int(row["Played"])

        for m_str in moves:
            key = str(compute_zobrist_from_cpp_dump(board, pieces, sides, castlings, en_passants))
            book[key]["fen"] = board.fen()
            try:
                move = board.push_san(m_str)
                book[key]["moves"][move.uci()] += played
            except Exception:
                break

    to_analyze = [(data["fen"], key) for key, data in book.items() if key not in eval_cache]
    total_to_do = len(to_analyze)
    print(f"Unique positions: {len(book)} | Cached: {len(eval_cache)} | To analyze: {total_to_do}")

    if to_analyze:
        print(f"Starting parallel analysis with {MAX_WORKERS} workers...")
        with ProcessPoolExecutor(max_workers=MAX_WORKERS, initializer=init_worker) as executor:
            pending = set()
            it = iter(to_analyze)
            count = 0
            submitted = 0

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

                for future in done:
                    key_str, val = future.result()
                    eval_cache[key_str] = val
                    count += 1
                    if count % SAVE_INTERVAL == 0:
                        pct = count / total_to_do * 100
                        print(f"  {count}/{total_to_do} ({pct:.1f}%) — saving cache async...")
                        save_cache_async(eval_cache, CACHE_FILE)

        with open(CACHE_FILE, "wb") as f:
            f.write(_json_dumps(eval_cache).encode())
        print("Cache saved.")

    print("Generating header...")
    entries = []
    for key, data in book.items():
        if not data["moves"] or key not in eval_cache:
            continue
        if sum(data["moves"].values()) < 5:
            continue
        best_move = max(data["moves"], key=data["moves"].get)
        entries.append((int(key), best_move, eval_cache[key]))

    entries.sort(key=lambda x: x[0])

    os.makedirs("include", exist_ok=True)
    with open("include/opening_book.h", "w") as out:
        out.write("#ifndef OPENING_BOOK_H\n#define OPENING_BOOK_H\n#include <cstdint>\n\n")
        out.write("struct BookEntry { U64 key; char move[6]; float evaluation; };\n\n")
        out.write("static constexpr BookEntry OPENING_BOOK[] = {\n")
        for k, mv, ev in entries:
            out.write(f'    {{ 0x{k:016x}ULL, "{mv}", {ev}f }},\n')
        out.write("};\n\n")
        out.write(f"static constexpr int OPENING_BOOK_SIZE = {len(entries)};\n")
        out.write("#endif\n")

    print(f"Done — {len(entries)} entries written to include/opening_book.h")
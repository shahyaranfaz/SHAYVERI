import csv
import chess
import chess.engine
import json
import os
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed

# --- CONFIGURATION ---
STOCKFISH_PATH = "stockfish" 
BOOK_DEPTH = 18
CACHE_FILE = "eval_cache.json"
SAVE_INTERVAL = 500  # Save progress every 500 new positions
MAX_WORKERS = os.cpu_count() - 1

# --- ZOBRIST LOGIC (Matched to C++) ---
class MT19937_64:
    def __init__(self, seed):
        self.mt = [0] * 312
        self.index = 312
        self.mt[0] = seed
        for i in range(1, 312):
            self.mt[i] = (6364136223846793005 * (self.mt[i - 1] ^ (self.mt[i - 1] >> 62)) + i) & 0xFFFFFFFFFFFFFFFF
    def extract_number(self):
        if self.index >= 312: self.twist()
        y = self.mt[self.index]; y ^= (y >> 29) & 0x5555555555555555
        y ^= (y << 17) & 0x71D67FFFEDA60000; y ^= (y << 37) & 0xFFF7EEE000000000
        y ^= (y >> 43); self.index += 1
        return y & 0xFFFFFFFFFFFFFFFF
    def twist(self):
        for i in range(312):
            x = (self.mt[i] & 0xFFFFFFFF80000000) + (self.mt[(i + 1) % 312] & 0x7FFFFFFF)
            xA = x >> 1
            if x % 2 != 0: xA ^= 0xB5026F5AA96619E9
            self.mt[i] = self.mt[(i + 156) % 312] ^ xA
        self.index = 0

rng = MT19937_64(0xdeadbeefcafe1234)
pieces = [[rng.extract_number() for _ in range(64)] for _ in range(13)]
side_key = rng.extract_number()
castlings = [rng.extract_number() for _ in range(16)]
en_passants = [rng.extract_number() for _ in range(8)]

def compute_zobrist(board):
    h = 0
    for sq, p in board.piece_map().items():
        p_idx = p.piece_type + (0 if p.color == chess.WHITE else 6)
        h ^= pieces[p_idx][sq]
    if board.turn == chess.BLACK: h ^= side_key
    c_idx = 0
    if board.has_kingside_castling_rights(chess.WHITE): c_idx |= 1
    if board.has_queenside_castling_rights(chess.WHITE): c_idx |= 2
    if board.has_kingside_castling_rights(chess.BLACK): c_idx |= 4
    if board.has_queenside_castling_rights(chess.BLACK): c_idx |= 8
    h ^= castlings[c_idx]
    if board.ep_square: h ^= en_passants[chess.square_file(board.ep_square)]
    return h & 0xFFFFFFFFFFFFFFFF

# --- PERSISTENT WORKER ---
worker_engine = None
def init_worker():
    global worker_engine
    worker_engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)

def analyze_position(fen):
    global worker_engine
    board = chess.Board(fen)
    info = worker_engine.analyse(board, chess.engine.Limit(depth=BOOK_DEPTH))
    score = info["score"].white()
    val = (100.0 if score.mate() > 0 else -100.0) if score.is_mate() else (score.score() or 0) / 100.0
    return fen, val

# --- MAIN ---
if __name__ == "__main__":
    book = defaultdict(lambda: {"moves": defaultdict(int), "fen": ""})
    eval_cache = {}

    if os.path.exists(CACHE_FILE):
        print(f"Loading existing cache...")
        with open(CACHE_FILE, 'r') as f:
            eval_cache = json.load(f)

    print(f"Reading CSV (96k rows)...")
    with open("table.csv", newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        for row in reader:
            board = chess.Board()
            moves = row['Moves'].split()[:30]
            played = int(row['Played'])
            for m_str in moves:
                key = str(compute_zobrist(board))
                book[key]["fen"] = board.fen()
                try:
                    move = board.push_san(m_str)
                    book[key]["moves"][move.uci()] += played
                except: break

    to_analyze = [data["fen"] for k, data in book.items() if k not in eval_cache]
    total_to_do = len(to_analyze)
    print(f"Unique positions: {len(book)} | Already cached: {len(eval_cache)} | To analyze: {total_to_do}")

    if to_analyze:
        print(f"Starting parallel analysis...")
        with ProcessPoolExecutor(max_workers=MAX_WORKERS, initializer=init_worker) as executor:
            # We use as_completed to handle results as they come in
            future_to_fen = {executor.submit(analyze_position, fen): fen for fen in to_analyze}
            
            count = 0
            for future in as_completed(future_to_fen):
                fen, val = future.result()
                # Re-hash to update cache
                k = str(compute_zobrist(chess.Board(fen)))
                eval_cache[k] = val
                
                count += 1
                if count % SAVE_INTERVAL == 0:
                    print(f"Progress: {count}/{total_to_do} ({(count/total_to_do)*100:.1f}%) - Saving cache...")
                    with open(CACHE_FILE, 'w') as f:
                        json.dump(eval_cache, f)

        # Final save
        with open(CACHE_FILE, 'w') as f:
            json.dump(eval_cache, f)

    print("Generating header...")
    entries = []
    for key, data in book.items():
        if not data["moves"] or key not in eval_cache: continue
        if sum(data["moves"].values()) < 5: continue
        best_move = max(data["moves"], key=data["moves"].get)
        entries.append((int(key), best_move, eval_cache[key]))

    entries.sort(key=lambda x: x[0])
    
    os.makedirs("include", exist_ok=True)
    with open("include/opening_book.h", "w") as out:
        out.write("#ifndef OPENING_BOOK_H\n#define OPENING_BOOK_H\n#include <cstdint>\n\n")
        out.write("struct BookEntry { U64 key; char move[6]; float evaluation; };\n\n")
        out.write(f"static constexpr BookEntry OPENING_BOOK[] = {{\n")
        for k, mv, ev in entries:
            out.write(f'    {{ 0x{k:016x}ULL, "{mv}", {ev}f }},\n')
        out.write(f"}};\n\nstatic constexpr int OPENING_BOOK_SIZE = {len(entries)};\n#endif\n")

    print(f"Successfully generated book with {len(entries)} entries.")
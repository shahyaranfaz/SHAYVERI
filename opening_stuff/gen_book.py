import csv
import chess
import chess.engine
from collections import defaultdict

STOCKFISH_PATH = "stockfish.exe"
BOOK_DEPTH = 18

# Cache to avoid re-analyzing transpositions
eval_cache = {}

class MT19937_64:
    def __init__(self, seed):
        self.mt = [0] * 312
        self.index = 312
        self.mt[0] = seed
        for i in range(1, 312):
            self.mt[i] = (6364136223846793005 * (self.mt[i - 1] ^ (self.mt[i - 1] >> 62)) + i) & 0xFFFFFFFFFFFFFFFF

    def extract_number(self):
        if self.index >= 312: self.twist()
        y = self.mt[self.index]
        y ^= (y >> 29) & 0x5555555555555555
        y ^= (y << 17) & 0x71D67FFFEDA60000
        y ^= (y << 37) & 0xFFF7EEE000000000
        y ^= (y >> 43)
        self.index += 1
        return y & 0xFFFFFFFFFFFFFFFF

    def twist(self):
        for i in range(312):
            x = (self.mt[i] & 0xFFFFFFFF80000000) + (self.mt[(i + 1) % 312] & 0x7FFFFFFF)
            xA = x >> 1
            if x % 2 != 0: xA ^= 0xB5026F5AA96619E9
            self.mt[i] = self.mt[(i + 156) % 312] ^ xA
        self.index = 0

# Initialize Zobrist constants to match C++ Zobrist::init()
rng = MT19937_64(0xdeadbeefcafe1234)
pieces = [[rng.extract_number() for _ in range(64)] for _ in range(13)]
side_key = rng.extract_number()
castlings = [rng.extract_number() for _ in range(16)]
en_passants = [rng.extract_number() for _ in range(8)]

def compute_zobrist(board):
    h = 0
    for sq in range(64):
        p = board.piece_at(sq)
        if p:
            p_idx = p.piece_type + (0 if p.color == chess.WHITE else 6)
            h ^= pieces[p_idx][sq]
    if board.turn == chess.BLACK:
        h ^= side_key
    c_idx = 0
    if board.has_kingside_castling_rights(chess.WHITE): c_idx |= 1
    if board.has_queenside_castling_rights(chess.WHITE): c_idx |= 2
    if board.has_kingside_castling_rights(chess.BLACK): c_idx |= 4
    if board.has_queenside_castling_rights(chess.BLACK): c_idx |= 8
    h ^= castlings[c_idx]
    if board.ep_square:
        h ^= en_passants[chess.square_file(board.ep_square)]
    return h & 0xFFFFFFFFFFFFFFFF

book = defaultdict(lambda: {"moves": defaultdict(int), "eval": 0.0})

print("Starting Stockfish and processing CSV...")

with chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH) as engine:
    with open("table.csv", newline='', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)

        for row_idx, row in enumerate(reader):
            moves_list = row['Moves'].strip().split()
            played = int(row['Played'])
            board = chess.Board()

            for i, move_str in enumerate(moves_list):
                if i >= 30: break

                current_key = compute_zobrist(board)

                # 1. Get Eval for the CURRENT position (before the move)
                if current_key not in eval_cache:
                    info = engine.analyse(board, chess.engine.Limit(depth=BOOK_DEPTH))
                    score = info["score"].white()
                    if score.is_mate():
                        val = 100.0 if score.mate() > 0 else -100.0
                    else:
                        val = (score.score() or 0) / 100.0
                    eval_cache[current_key] = val

                book[current_key]["eval"] = eval_cache[current_key]

                # 2. Record the move and update board
                try:
                    move = board.push_san(move_str)
                    book[current_key]["moves"][move.uci()] += played
                except:
                    break

            if row_idx % 5 == 0:
                print(f"Row {row_idx} | Cache Size: {len(eval_cache)} positions", end='\r')

# Build and sort entries
entries = []
for key, data in book.items():
    if not data["moves"]: continue
    # Only include positions with enough data
    if sum(data["moves"].values()) < 5: continue

    best_move = max(data["moves"], key=data["moves"].get)
    entries.append((key, best_move, data["eval"]))

entries.sort(key=lambda x: x[0])

# --- HEADER GENERATION ---
with open("include/opening_book.h", "w") as out:
    out.write("#ifndef OPENING_BOOK_H\n#define OPENING_BOOK_H\n\n")
    out.write("#include \"types.h\"\n#include <cstdint>\n\n")
    out.write("struct BookEntry {\n    U64 key;\n    char move[6];\n    float evaluation;\n};\n\n")
    out.write(f"static constexpr BookEntry OPENING_BOOK[] = {{\n")
    for k, mv, ev in entries:
        out.write(f'    {{ 0x{k:016x}ULL, "{mv}", {ev}f }},\n')
    out.write("};\n\n")
    out.write(f"static constexpr int OPENING_BOOK_SIZE = {len(entries)};\n")
    out.write("#endif\n")

print(f"\nSuccess. Generated {len(entries)} book entries.")
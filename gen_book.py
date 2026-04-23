import csv
from collections import defaultdict

book = defaultdict(lambda: defaultdict(int))

with open("table.csv", newline='', encoding='utf-8-sig') as f:
    reader = csv.DictReader(f)
    for row in reader:
        moves_str = row['Moves'].strip()
        played = int(row['Played'])
        moves = moves_str.split()
        for i in range(min(len(moves) - 1, 30)):  # cap at 30 plies
            prefix = ' '.join(moves[:i])
            next_move = moves[i]
            book[prefix][next_move] += played

def fnv64(s: str) -> int:
    h = 14695981039346656037
    for c in s.encode():
        h ^= c
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

MIN_PLAYED = 5
entries = []
for prefix, moves in book.items():
    total = sum(moves.values())
    if total < MIN_PLAYED:
        continue
    best_move = max(moves, key=moves.get)
    h = fnv64(prefix)
    entries.append((h, best_move))

print(f"Total book entries: {len(entries)}")

with open("include/opening_book.h", "w") as out:
    out.write("#ifndef OPENING_BOOK_H\n")
    out.write("#define OPENING_BOOK_H\n\n")
    out.write("#include \"types.h\"\n\n")
    out.write("#include <cstdint>\n\n")
    out.write("struct BookEntry {\n")
    out.write("    U64 key;\n")
    out.write("    const char move[6];\n")
    out.write("};\n\n")
    out.write(f"static constexpr BookEntry OPENING_BOOK[] = {{\n")
    for h, mv in entries:
        out.write(f'    {{ 0x{h:016x}ULL, "{mv}" }},\n')
    out.write("};\n\n")
    out.write(f"static constexpr int OPENING_BOOK_SIZE = {len(entries)};\n")
    out.write("#endif\n")
print("Done")

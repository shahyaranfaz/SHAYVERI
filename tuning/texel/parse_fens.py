import random
from pathlib import Path

lines = []

for path in Path("fens").glob("*"):
    if not path.is_file():
        continue

    with open(path) as f:
        for line in f:
            parts = line.strip().split(";")
            if len(parts) < 2:
                continue

            fen = parts[0]

            try:
                outcome = parts[-1].split(":")[1]
            except IndexError:
                continue

            lines.append(f"{fen} [{outcome}]")

random.shuffle(lines)

with open("self_games.txt", "w") as f:
    f.write("\n".join(lines))
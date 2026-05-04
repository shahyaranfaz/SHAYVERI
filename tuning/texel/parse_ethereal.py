import random

lines = []

with open("E12.52-1M-D12-Resolved.book") as f:
    for line in f:
        parts = line.strip().split(" [")

        if len(parts) < 2:
            continue

        fen = parts[0]

        rest = parts[1].split("]")
        result_float = rest[0]

        if result_float == "1.0":
            result = "1-0"
        elif result_float == "0.0":
            result = "0-1"
        elif result_float == "0.5":
            result = "1/2-1/2"
        else:
            continue

        lines.append(f"{fen} [{result}]")

random.shuffle(lines)

with open("ethereal_converted.txt", "w") as f:
    f.write("\n".join(lines))
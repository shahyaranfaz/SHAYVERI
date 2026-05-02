import re
from pathlib import Path

ROOT = Path("tuning")
pattern = re.compile(
    r'"#meta-sigma":\s*[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?,\s*(.+)'
    r'|'
    r'"#optimizer#algorithm":\s*"[^"]+",\s*(.+)'
)


def process_batch(batch_path: Path):
    unique_suffixes = set()

    for file in batch_path.iterdir():
        if not file.is_file():
            continue

        if file.suffix not in {".log", ".txt"}:
            continue

        if file.name.endswith("_overall.log") or file.name.endswith("_overall.txt"):
            continue

        with file.open("r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                m = pattern.search(line)
                if not m:
                    continue

                suffix = m.group(1) or m.group(2)
                if suffix:
                    unique_suffixes.add(suffix.rstrip("\n"))

    return len(unique_suffixes)


def main():
    for pass_dir in sorted(ROOT.iterdir()):
        if not pass_dir.is_dir():
            continue

        for batch_dir in sorted(pass_dir.iterdir()):
            if not batch_dir.is_dir():
                continue

            count = process_batch(batch_dir)

            print(f"{batch_dir}: {count} unique suffixes")


if __name__ == "__main__":
    main()

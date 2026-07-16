#!/usr/bin/env python3
"""End-to-end datagen checks, with an optional sanitizer workload matrix."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import traceback


ENGINE_BINARY_NAME = "SHAYVERI.exe" if os.name == "nt" else "SHAYVERI"
DEFAULT_ENGINE_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ENGINE_BINARY_NAME)
)
ENGINE_PATH = os.environ.get("SHAYVERI_ENGINE", DEFAULT_ENGINE_PATH)
TIMEOUT_SEC = float(os.environ.get("SHAYVERI_DATAGEN_TIMEOUT_SEC", "20"))
GAMES = int(os.environ.get("SHAYVERI_DATAGEN_GAMES", "1"))
THREADS = int(os.environ.get("SHAYVERI_DATAGEN_THREADS", "1"))
RUN_MATRIX = os.environ.get("SHAYVERI_DATAGEN_MATRIX", "0") == "1"


def summary_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        parts = line.split(maxsplit=1)
        if len(parts) == 2 and parts[0] not in values:
            values[parts[0]] = parts[1]
    return values


def run_format(
    root: Path,
    name: str,
    output_format: str,
    *,
    games: int = GAMES,
    threads: int = THREADS,
    eval_file: str = "<hce>",
    nodes: int = 1,
    include_duplicates: bool = True,
    adjudication: bool = True,
    start_file: Path | None = None,
) -> None:
    prefix = root / name
    command = [
        ENGINE_PATH,
        "datagen",
        "--threads", str(threads),
        "--games", str(games),
        "--output-prefix", str(prefix),
        "--output-format", output_format,
        "--eval-file", eval_file,
        "--nodes", str(nodes),
        "--opening-min-plies", "0",
        "--opening-max-plies", "0",
        "--book-prob", "0",
        "--seed", "1",
        "--max-abs-cp", "100000",
        "--include-checks", "true",
        "--include-captures", "true",
        "--include-mate-scores", "true",
        "--include-duplicates", str(include_duplicates).lower(),
        "--enable-adjudication", str(adjudication).lower(),
        "--adjudication-cp", "1",
        "--adjudication-plies", "4",
        "--print-interval", "0",
    ]
    if start_file is not None:
        command.extend(["--start-file", str(start_file), "--start-file-prob", "1"])
    print(f"[datagen] {name}: games={games} threads={threads} format={output_format}")
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=TIMEOUT_SEC,
        check=False,
    )
    if completed.returncode != 0:
        raise AssertionError(
            f"{name} datagen failed with {completed.returncode}:\n"
            f"{completed.stdout[-2000:]}"
        )

    summary_path = Path(str(prefix) + ".summary.txt")
    done_path = Path(str(prefix) + ".DONE")
    if not summary_path.is_file() or not done_path.is_file():
        raise AssertionError(f"{name} did not write summary and DONE files")

    summary = summary_values(summary_path)
    done = summary_values(done_path)
    positions = int(summary.get("positions", "0"))
    if positions <= 0 or done.get("positions") != str(positions):
        raise AssertionError(f"{name} wrote inconsistent position counts")
    if summary.get("games") != str(games) or done.get("games") != str(games):
        raise AssertionError(f"{name} did not complete exactly {games} games")
    if summary.get("format") != output_format:
        raise AssertionError(f"{name} summary reported the wrong format")

    if output_format == "shayveri-plain-v1":
        shards = sorted(root.glob(prefix.name + "_*.plain"))
        lines = [
            line
            for shard in shards
            for line in shard.read_text().splitlines()
            if line.strip()
        ]
        if len(lines) != positions or any(line.count(" | ") != 2 for line in lines):
            raise AssertionError("plain datagen records do not match the declared count/shape")
    else:
        shards = sorted(root.glob(prefix.name + "_*.bullet.bin"))
        if sum(shard.stat().st_size for shard in shards) != positions * 32:
            raise AssertionError("Bullet datagen output is not composed of 32-byte records")


def run_matrix(root: Path) -> None:
    # Pressure: 100 games per format, two workers, and all output validation.
    run_format(root, "pressure_plain", "shayveri-plain-v1")
    run_format(root, "pressure_bullet", "bullet-v1")

    # Option-specific paths that random self-play cannot guarantee.
    run_format(
        root,
        "duplicate_filter",
        "shayveri-plain-v1",
        games=4,
        threads=1,
        nodes=8,
        include_duplicates=False,
    )

    starts = root / "sanitizer_starts.fen"
    starts.write_text(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n"
        "rnbqkbnr/pppppp2/7p/6pP/8/8/PPPPPPP1/RNBQKBNR w KQkq g6 7 14\n"
        "r3k2r/8/8/8/8/8/8/R3K2R b Kq - 10 42\n"
    )
    run_format(
        root,
        "external_starts_nnue",
        "bullet-v1",
        games=4,
        threads=1,
        eval_file="<embedded>",
        nodes=8,
        start_file=starts,
    )

    # Disabling adjudication takes the natural game-end path.  This near-mate
    # start keeps the run short while still exercising it deterministically.
    no_adjudication_starts = root / "no_adjudication.fen"
    no_adjudication_starts.write_text("5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1\n")
    run_format(
        root,
        "natural_end_no_adjudication",
        "shayveri-plain-v1",
        games=1,
        threads=1,
        nodes=8,
        adjudication=False,
        start_file=no_adjudication_starts,
    )


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(
            f"engine binary not found: {ENGINE_PATH} "
            "(run `make` in repo root or set SHAYVERI_ENGINE)",
            file=sys.stderr,
        )
        return 1

    try:
        with tempfile.TemporaryDirectory(prefix="shayveri_datagen_check_") as tmp:
            root = Path(tmp)
            if RUN_MATRIX:
                run_matrix(root)
            else:
                run_format(root, "plain", "shayveri-plain-v1")
                run_format(root, "bullet", "bullet-v1")
        if RUN_MATRIX:
            print(f"Datagen sanitizer matrix passed ({GAMES} games per format, {THREADS} threads).")
        else:
            print(f"Datagen checks passed (plain + Bullet, {GAMES} games, {THREADS} threads).")
        return 0
    except Exception as exc:
        print(f"Datagen checks failed: {exc}", file=sys.stderr)
        traceback.print_exc()
        return 2


if __name__ == "__main__":
    sys.exit(main())

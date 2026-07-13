#!/usr/bin/env python3
"""Fast end-to-end checks for both supported datagen formats."""

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


def summary_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        parts = line.split(maxsplit=1)
        if len(parts) == 2 and parts[0] not in values:
            values[parts[0]] = parts[1]
    return values


def run_format(root: Path, output_format: str) -> None:
    prefix = root / output_format.replace("-", "_")
    command = [
        ENGINE_PATH,
        "datagen",
        "--threads", "1",
        "--games", "1",
        "--output-prefix", str(prefix),
        "--output-format", output_format,
        "--eval-file", "<hce>",
        "--nodes", "1",
        "--opening-min-plies", "0",
        "--opening-max-plies", "0",
        "--book-prob", "0",
        "--seed", "1",
        "--max-abs-cp", "100000",
        "--include-checks", "true",
        "--include-captures", "true",
        "--include-mate-scores", "true",
        "--enable-adjudication", "true",
        "--adjudication-cp", "1",
        "--adjudication-plies", "4",
        "--print-interval", "0",
    ]
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
            f"{output_format} datagen failed with {completed.returncode}:\n"
            f"{completed.stdout[-2000:]}"
        )

    summary_path = Path(str(prefix) + ".summary.txt")
    done_path = Path(str(prefix) + ".DONE")
    if not summary_path.is_file() or not done_path.is_file():
        raise AssertionError(f"{output_format} did not write summary and DONE files")

    summary = summary_values(summary_path)
    done = summary_values(done_path)
    positions = int(summary.get("positions", "0"))
    if positions <= 0 or done.get("positions") != str(positions):
        raise AssertionError(f"{output_format} wrote inconsistent position counts")
    if summary.get("games") != "1" or done.get("games") != "1":
        raise AssertionError(f"{output_format} did not complete exactly one game")
    if summary.get("format") != output_format:
        raise AssertionError(f"{output_format} summary reported the wrong format")

    if output_format == "shayveri-plain-v1":
        shard = Path(str(prefix) + "_0.plain")
        lines = [line for line in shard.read_text().splitlines() if line.strip()]
        if len(lines) != positions or any(line.count(" | ") != 2 for line in lines):
            raise AssertionError("plain datagen records do not match the declared count/shape")
    else:
        shard = Path(str(prefix) + "_0.bullet.bin")
        if shard.stat().st_size != positions * 32:
            raise AssertionError("Bullet datagen output is not composed of 32-byte records")


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
            run_format(root, "shayveri-plain-v1")
            run_format(root, "bullet-v1")
        print("Datagen checks passed (plain + Bullet).")
        return 0
    except Exception as exc:
        print(f"Datagen checks failed: {exc}", file=sys.stderr)
        traceback.print_exc()
        return 2


if __name__ == "__main__":
    sys.exit(main())

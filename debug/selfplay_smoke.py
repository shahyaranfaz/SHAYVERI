#!/usr/bin/env python3
"""
Self-play smoke test wrapper for cutechess-cli.
Runs only when cutechess-cli is installed.
"""

import os
import shutil
import subprocess
import sys

ENGINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SHAYVERI"))
GAMES = int(os.environ.get("SHAYVERI_SELFPLAY_GAMES", "20"))
TC = os.environ.get("SHAYVERI_SELFPLAY_TC", "10+0.1")


def default_timeout_seconds(tc: str, games: int) -> int:
    try:
        base, inc = tc.split("+", 1)
        base_s = float(base)
        inc_s = float(inc)
        expected_per_game = 2.0 * (base_s + 40.0 * inc_s)
        return int(expected_per_game * games * 2.5 + 120.0)  # safety margin
    except Exception:
        return 3600


TIMEOUT_SEC = int(os.environ.get("SHAYVERI_SELFPLAY_TIMEOUT_SEC", str(default_timeout_seconds(TC, GAMES))))


def main() -> int:
    if not os.path.exists(ENGINE):
        print(f"engine binary not found: {ENGINE}", file=sys.stderr)
        return 1

    cutechess = shutil.which("cutechess-cli")
    if not cutechess:
        print("Skipping self-play smoke: cutechess-cli not installed.")
        return 0

    cmd = [
        cutechess,
        "-engine", f"cmd={ENGINE}", "name=SHAYVERI_A", "proto=uci",
        "-engine", f"cmd={ENGINE}", "name=SHAYVERI_B", "proto=uci",
        "-each", f"tc={TC}",
        "-games", str(GAMES),
        "-repeat",
        "-recover",
    ]

    proc = subprocess.run(
        cmd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TIMEOUT_SEC,
    )
    out = proc.stdout
    print(out)
    if proc.returncode != 0:
        print("Self-play smoke failed: cutechess returned non-zero exit code.", file=sys.stderr)
        return 2

    lower = out.lower()
    bad_tokens = ["illegal", "crash", "timeout", "forfeit on time", "disconnect"]
    for token in bad_tokens:
        if token in lower:
            print(f"Self-play smoke failed: found '{token}' in cutechess output.", file=sys.stderr)
            return 3

    print(f"Self-play smoke passed ({GAMES} games at tc={TC}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())

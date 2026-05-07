#!/usr/bin/env python3
"""
UCI smoke checks borrowed from the style of:
- official-stockfish/Stockfish/tests/instrumented.py
"""

import os
import re
import subprocess
import sys


ENGINE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SHAYVERI"))
TIMEOUT_SEC = 20


def run_engine(commands: list[str]) -> str:
    proc = subprocess.run(
        [ENGINE_PATH],
        input="\n".join(commands) + "\n",
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TIMEOUT_SEC,
        check=False,
    )
    return proc.stdout


def require(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing '{needle}' in output: {text[-500:]}")


def require_bestmove(text: str) -> None:
    if not re.search(r"^bestmove\s+\S+", text, flags=re.MULTILINE):
        raise AssertionError(f"missing bestmove in output: {text[-500:]}")


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(f"engine binary not found: {ENGINE_PATH}", file=sys.stderr)
        return 1

    try:
        handshake = run_engine(["uci", "isready", "quit"])
        require(handshake, "uciok")
        require(handshake, "readyok")

        startpos = run_engine(["ucinewgame", "position startpos", "go nodes 1000", "quit"])
        require_bestmove(startpos)

        # Position borrowed from official Stockfish UCI smoke tests.
        fen_case = run_engine([
            "ucinewgame",
            "position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1",
            "go nodes 1000",
            "quit",
        ])
        require_bestmove(fen_case)

        print("UCI smoke tests passed (Stockfish-inspired command flow).")
        return 0
    except Exception as exc:
        print(f"UCI smoke tests failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""
UCI smoke checks borrowed from the style of:
- official-stockfish/Stockfish/tests/instrumented.py
"""

import os
import re
import subprocess
import sys

DEFAULT_ENGINE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SHAYVERI"))
ENGINE_PATH = os.environ.get("SHAYVERI_ENGINE", DEFAULT_ENGINE_PATH)
TIMEOUT_SEC = 20
TAIL_CHARS = 500


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
        raise AssertionError(f"missing '{needle}' in output: {text[-TAIL_CHARS:]}")


def require_bestmove(text: str) -> None:
    if not re.search(r"^bestmove\s+\S+", text, flags=re.MULTILINE):
        raise AssertionError(f"missing bestmove in output: {text[-TAIL_CHARS:]}")

def extract_bestmove(text: str) -> str:
    m = re.search(r"^bestmove\s+(\S+)", text, flags=re.MULTILINE)
    if not m:
        raise AssertionError(f"missing bestmove in output: {text[-TAIL_CHARS:]}")
    return m.group(1)

def extract_last_pv(text: str) -> str:
    pvs = re.findall(r"^info .*?\spv\s+(.+)$", text, flags=re.MULTILINE)
    return pvs[-1].strip() if pvs else ""


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(
            f"engine binary not found: {ENGINE_PATH} "
            "(run `make` in repo root or set SHAYVERI_ENGINE)",
            file=sys.stderr,
        )
        return 1

    try:
        handshake = run_engine(["uci", "isready", "quit"])
        require(handshake, "uciok")
        require(handshake, "readyok")

        startpos = run_engine(["ucinewgame", "position startpos", "go nodes 1000", "quit"])
        require_bestmove(startpos)

        depth1 = run_engine(["ucinewgame", "position startpos", "go depth 1", "quit"])
        require_bestmove(depth1)

        bench = run_engine(["bench 16 1 3 default depth", "quit"])
        if not re.search(r"^Nodes:\s+\d+", bench, flags=re.MULTILINE):
            raise AssertionError(f"missing bench node summary in output: {bench[-TAIL_CHARS:]}")

        # Position borrowed from official Stockfish UCI smoke tests.
        fen_case = run_engine([
            "ucinewgame",
            "position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1",
            "go nodes 1000",
            "quit",
        ])
        require_bestmove(fen_case)

        determinism_1 = run_engine([
            "setoption name Threads value 1",
            "setoption name OwnBook value false",
            "ucinewgame",
            "position fen 5K2/8/2qk4/2nPp3/3r4/6B1/B7/3R4 w - e6 0 1",
            "go depth 4",
            "quit",
        ])
        determinism_2 = run_engine([
            "setoption name Threads value 1",
            "setoption name OwnBook value false",
            "ucinewgame",
            "position fen 5K2/8/2qk4/2nPp3/3r4/6B1/B7/3R4 w - e6 0 1",
            "go depth 4",
            "quit",
        ])
        bm1, bm2 = extract_bestmove(determinism_1), extract_bestmove(determinism_2)
        pv1, pv2 = extract_last_pv(determinism_1), extract_last_pv(determinism_2)
        if bm1 != bm2 or pv1 != pv2:
            raise AssertionError(
                f"determinism failure: run1 bestmove={bm1} pv={pv1!r}, run2 bestmove={bm2} pv={pv2!r}"
            )

        print("UCI smoke tests passed (Stockfish-inspired flow + depth1 + bench + determinism).")
        return 0
    except Exception as exc:
        print(f"UCI smoke tests failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())

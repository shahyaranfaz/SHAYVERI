#!/usr/bin/env python3
"""Focused lifecycle checks for asynchronous fixed-depth/node UCI searches."""

import os
import sys
import time
import traceback

from time_control_integration import EngineSession


ENGINE_BINARY_NAME = "SHAYVERI.exe" if os.name == "nt" else "SHAYVERI"
DEFAULT_ENGINE_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ENGINE_BINARY_NAME)
)
ENGINE_PATH = os.environ.get("SHAYVERI_ENGINE", DEFAULT_ENGINE_PATH)
ITERATIONS = int(os.environ.get("SHAYVERI_FIXED_SEARCH_ITERATIONS", "25"))


def initialize(session: EngineSession) -> None:
    session.send("uci")
    session.wait_for_prefix("uciok")
    session.send("setoption name Threads value 1")
    session.send("setoption name OwnBook value false")
    session.send("isready")
    session.wait_for_prefix("readyok")


def run_completed_searches(session: EngineSession, command: str) -> None:
    for _ in range(ITERATIONS):
        session.send("position startpos")
        session.send(command)
        session.wait_for_prefix("bestmove ")
        # Leave a short window for faults that occur as the worker exits.
        time.sleep(0.02)


def test_stop(session: EngineSession) -> None:
    session.send("position startpos")
    session.send("go depth 64")
    time.sleep(0.1)
    session.send("stop")
    session.wait_for_prefix("bestmove ")


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(f"engine binary not found: {ENGINE_PATH}", file=sys.stderr)
        return 1

    session = EngineSession()
    try:
        initialize(session)
        run_completed_searches(session, "go depth 4")
        run_completed_searches(session, "go nodes 1000")
        test_stop(session)
        print(
            "Fixed-search integration checks passed "
            f"({ITERATIONS} depth + {ITERATIONS} node searches + stop)."
        )
        return 0
    except Exception as exc:
        print(f"Fixed-search integration checks failed: {exc}", file=sys.stderr)
        traceback.print_exc()
        return 2
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main())

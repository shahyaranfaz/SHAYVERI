#!/usr/bin/env python3
"""Check GUI-supplied period resets and low-clock survival at 1/4 threads."""
import sys
import time

from fixed_search_integration import initialize
from time_control_integration import EngineSession


def main():
    session = EngineSession()
    try:
        initialize(session)
        session.send("setoption name MoveOverhead value 20")
        samples = 0
        for threads in (1, 4):
            session.send(f"setoption name Threads value {threads}")
            for side in ("w", "b"):
                for move, bank, moves, hard in ((39, 800, 2, 636),
                                                (40, 600, 1, 472),
                                                (41, 5000, 40, 500)):
                    session.send(f"position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR {side} KQkq - 0 {move}")
                    white, black = (bank, 30000) if side == "w" else (30000, bank)
                    start = time.monotonic()
                    session.send(f"go wtime {white} btime {black} movestogo {moves}")
                    result = session.wait_for_prefix("bestmove ", timeout=hard / 1000 + 0.5)
                    assert result.split()[1] != "0000", result
                    assert time.monotonic() - start < hard / 1000 + 0.5
                    samples += 1
                for remaining in (0, -3):
                    session.send(f"position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR {side} KQkq - 0 1")
                    white, black = (remaining, 30000) if side == "w" else (30000, remaining)
                    session.send(f"go wtime {white} btime {black}")
                    result = session.wait_for_prefix("bestmove ", timeout=0.08)
                    assert result.split()[1] != "0000", result
                    samples += 1
                for increment in (0, 10):
                    session.send("position startpos")
                    start = time.monotonic()
                    session.send(f"go wtime 100 btime 100 winc {increment} binc {increment}")
                    result = session.wait_for_prefix("bestmove ", timeout=0.35)
                    assert result.split()[1] != "0000", result
                    assert time.monotonic() - start < 0.35
                    samples += 1
        print(f"[PASS] Period reset and low-clock integration: {samples} searches, both colors, 1/4 threads")
        return 0
    except Exception as error:
        print(f"[FAIL] Time-boundary integration: {error}", file=sys.stderr)
        return 1
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main())

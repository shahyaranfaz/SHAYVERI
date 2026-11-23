#!/usr/bin/env python3
"""Check go option ordering, restricted fallback moves, and immediate stop."""
import re
import sys

from fixed_search_integration import initialize
from time_control_integration import EngineSession


def check_ordering(session):
    session.send("position startpos")
    start = len(session.output)
    session.send("go searchmoves h2h4 depth 1")
    move = session.wait_for_prefix("bestmove ")
    depths = [int(m.group(1)) for line in session.output[start:]
              if (m := re.match(r"info depth (\d+)", line))]
    assert move.split()[1] == "h2h4", move
    assert depths and max(depths) == 1, f"depth limit ignored: {depths}"


def check_fallback(session):
    for command in ("go nodes 1 searchmoves h2h4", "go searchmoves h2h4 nodes 1"):
        session.send("position startpos")
        start = len(session.output)
        session.send(command)
        move = session.wait_for_prefix("bestmove ")
        assert move.split()[1] == "h2h4", f"{command}: {move.strip()}"
        counts = [int(m.group(1)) for line in session.output[start:]
                  if (m := re.search(r"\bnodes (\d+)", line))]
        assert counts and counts[-1] == 1, f"node limit ignored: {counts}"


def check_immediate_stop(session):
    for command in ("go depth 64 searchmoves h2h4", "go infinite searchmoves h2h4"):
        for _ in range(20):
            session.send("position startpos")
            # One pipe write avoids giving the worker a deliberate startup delay.
            session.send(command + "\nstop")
            move = session.wait_for_prefix("bestmove ")
            assert move.split()[1] == "h2h4", move
            session.send("isready")
            session.wait_for_prefix("readyok")


def main():
    failures = 0
    for check in (check_ordering, check_fallback, check_immediate_stop):
        session = EngineSession()
        try:
            initialize(session)
            check(session)
            print(f"[PASS] {check.__name__}")
        except Exception as error:
            failures += 1
            print(f"[FAIL] {check.__name__}: {error}", file=sys.stderr)
        finally:
            session.close()
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())

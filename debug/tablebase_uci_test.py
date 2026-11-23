#!/usr/bin/env python3
"""Verify configured Syzygy tables actually affect UCI search."""
from pathlib import Path
import re
import sys

from fixed_search_integration import initialize
from time_control_integration import EngineSession


def main():
    session = EngineSession()
    fixtures = Path(__file__).resolve().parent / "fixtures" / "syzygy"
    try:
        initialize(session)
        session.send(f"setoption name SyzygyPath value {fixtures}")
        session.send("isready")
        session.wait_for_prefix("readyok")

        def search(command, *, hold=False):
            session.send("position fen 7k/8/8/8/8/8/6Q1/6K1 w - - 0 1")
            start = len(session.output)
            session.send(command)
            if hold:
                session.assert_no_prefix_for("bestmove ", 0.1)
                session.send("ponderhit" if "ponder" in command else "stop")
            move = session.wait_for_prefix("bestmove ")
            lines = session.output[start:]
            return move, lines

        for threads in (1, 4):
            session.send(f"setoption name Threads value {threads}")
            move, lines = search("go depth 2" if threads == 1
                                 else "go wtime 1000 btime 1000")
            assert move.split()[1] != "0000", move
            assert any(re.search(r"\btbhits [1-9]\d*", x) for x in lines), lines
            assert any("score cp 30000" in x for x in lines), lines
            assert not any("score mate" in x for x in lines), lines
        move, _ = search("go searchmoves g2f2 depth 1")
        assert move.split()[1] == "g2f2", move
        search("go infinite", hold=True)
        search("go ponder wtime 1000 btime 1000", hold=True)

        session.send(f"setoption name SyzygyPath value {fixtures / 'missing'}")
        _, lines = search("go depth 1")
        assert any("score cp 30000" in x for x in lines), "invalid path lost the active tables"
        session.send("setoption name SyzygyPath value <empty>")
        _, lines = search("go depth 1")
        assert not any(re.search(r"\btbhits [1-9]\d*", x) for x in lines), "disabled tables were still used"
        print("[PASS] UCI Syzygy use/reporting, restrictions, SMP, infinite/ponder, and path recovery")
        return 0
    except Exception as error:
        print(f"[FAIL] UCI Syzygy: {error}", file=sys.stderr)
        return 1
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main())

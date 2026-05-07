#!/usr/bin/env python3
"""
Tactical regression harness using mate-in-1 positions adapted from:
- StuartRiffle/JaglavakTestData (Suites/mate-in-1.epd.json)
"""

import os
import re
import subprocess
import sys

ENGINE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SHAYVERI"))
DEPTH = int(os.environ.get("SHAYVERI_TACTICAL_DEPTH", "4"))
MIN_SOLVED = int(os.environ.get("SHAYVERI_TACTICAL_MIN_SOLVED", "0"))
TIMEOUT_SEC = int(os.environ.get("SHAYVERI_TACTICAL_TIMEOUT_SEC", "15"))

TACTICAL_CASES = [
    ("mate-in-1-1", "5rk1/ppp3pp/8/3pQ3/3P2b1/6P1/PP1P2K1/R1BB1r2 b - - 51 53", "f8f2"),
    ("mate-in-1-2", "4rk2/5p1B/2p4p/1p1pR3/1q1P2Q1/6PP/5P1K/8 w - - 49 53", "g4g8"),
    ("mate-in-1-3", "8/6r1/8/3Rb1Np/1p2p3/pPk5/P1P3PP/1K6 w - - 49 53", "g5e4"),
    ("mate-in-1-4", "r1b1kb2/5q2/p1p3Qp/1p6/8/1B6/PP3PPP/3R2K1 w - - 49 53", "g6f7"),
    ("mate-in-1-5", "r1b1k2r/pppp1p1p/1b2nP2/8/2B1R3/Q1Pp1N2/P4PPP/R5K1 w - - 49 53", "a3e7"),
    ("mate-in-1-6", "7k/p5p1/1p5p/1Pp5/2RPp1P1/P3P1P1/2Q3K1/2N1q2r b - - 50 53", "e1f1"),
    ("mate-in-1-7", "r2rk3/1q2b1p1/ppb1Q1N1/4p3/P7/1N5R/1P4BP/n6K w - - 51 53", "h3h8"),
    ("mate-in-1-8", "2r1r3/pp1n1N1Q/4p2b/q6k/P1pP2R1/2P2P2/1P6/R5K1 w - - 51 53", "h7h6"),
    ("mate-in-1-9", "r3r3/pb1n3k/1p1q1ppB/4p3/2BP4/2P4R/P4PPP/4R1K1 w - - 52 53", "h6f8"),
    ("mate-in-1-10", "r1b1Rqk1/p4p1p/6pB/2pp4/3b4/3B4/PPP2PPP/4R1K1 w - - 52 53", "e8f8"),
    ("mate-in-1-11", "r1b2k2/2p1q1pQ/2p2rP1/p2p1p2/4pP2/1PN1P3/PKPP4/3R2N1 w - - 52 53", "h7h8"),
    ("mate-in-1-12", "5b1k/2q4r/1np1Bp2/1p1p3P/p2P2R1/P4PP1/1PR2PK1/8 w - - 48 53", "g4g8"),
    ("mate-in-1-13", "5Q2/6R1/p3p2r/qb2k2P/3p4/1p1Pr3/PP6/1K5R w - - 50 53", "g7g5"),
    ("mate-in-1-14", "4r1nr/3R3p/p1N1kp2/1p6/2p1P3/5K2/PP6/3R4 w - - 51 53", "d1d6"),
    ("mate-in-1-15", "2bQ4/2p2r1k/p2p2R1/1p3P2/3q4/1P6/2P3PP/5K2 w - - 49 53", "d8g8"),
    ("mate-in-1-16", "8/6pk/3pp2p/4p1nP/1P2P3/3P3q/5P2/2QN2KR b - - 52 53", "g5f3"),
    ("mate-in-1-17", "2Q2b1r/4kp1p/3p1p2/3p4/p2NPP2/8/q1P3PP/4K2R w K - 51 53", "d4f5"),
    ("mate-in-1-18", "3r2k1/6p1/1b3p1p/4p3/p1R1P2N/P7/1P6/5qQK b - - 49 53", "f1g1"),
    ("mate-in-1-19", "3r2k1/pp6/6p1/2Ppq2p/4N3/4BK2/PP2P2r/R1Q1R2B b - - 49 53", "d5e4"),
    ("mate-in-1-20", "1q1N1k2/3Q3p/4B2r/5p2/3P3P/8/5PPb/3n3K w - - 48 53", "d7f7"),
]


def run_engine(commands: list[str]) -> str:
    out = subprocess.run(
        [ENGINE_PATH],
        input="\n".join(commands) + "\n",
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TIMEOUT_SEC,
        check=False,
    )
    return out.stdout


def extract_bestmove(text: str) -> str:
    m = re.search(r"^bestmove\s+(\S+)", text, flags=re.MULTILINE)
    return m.group(1) if m else ""


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(f"engine binary not found: {ENGINE_PATH}", file=sys.stderr)
        return 1

    solved = 0
    missing = 0
    for case_id, fen, expected in TACTICAL_CASES:
        output = run_engine([
            "setoption name Threads value 1",
            "setoption name OwnBook value false",
            "ucinewgame",
            f"position fen {fen}",
            f"go depth {DEPTH}",
            "quit",
        ])
        best = extract_bestmove(output)
        if not best:
            print(f"[FAIL] {case_id}: missing bestmove")
            missing += 1
            continue
        if best == expected:
            solved += 1
            print(f"[PASS] {case_id}: {best}")
        else:
            print(f"[MISS] {case_id}: expected {expected}, got {best}")

    total = len(TACTICAL_CASES)
    print(f"Tactical solved: {solved}/{total} (depth={DEPTH})")
    if missing > 0:
        print(f"Tactical suite failed: {missing} positions missing bestmove", file=sys.stderr)
        return 2
    if solved < MIN_SOLVED:
        print(f"Tactical suite failed: solved {solved} < required {MIN_SOLVED}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())

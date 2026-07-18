"""Representative, stable workloads for SHAYVERI performance baselines."""

POSITION_CASES = [
    (
        "startpos",
        "startpos",
    ),
    (
        "kiwipete",
        "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    ),
    (
        "endgame",
        "fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    ),
    (
        "tactical",
        "fen 4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 5 20",
    ),
]

TIMED_CASES = POSITION_CASES[:2]

TACTICAL_CASES = [
    ("mate-01", "fen 5rk1/ppp3pp/8/3pQ3/3P2b1/6P1/PP1P2K1/R1BB1r2 b - - 51 53", "f8f2"),
    ("mate-02", "fen 4rk2/5p1B/2p4p/1p1pR3/1q1P2Q1/6PP/5P1K/8 w - - 49 53", "g4g8"),
    ("mate-03", "fen 8/6r1/8/3Rb1Np/1p2p3/pPk5/P1P3PP/1K6 w - - 49 53", "g5e4"),
    ("mate-04", "fen r1b1kb2/5q2/p1p3Qp/1p6/8/1B6/PP3PPP/3R2K1 w - - 49 53", "g6f7"),
    ("mate-05", "fen r1b1k2r/pppp1p1p/1b2nP2/8/2B1R3/Q1Pp1N2/P4PPP/R5K1 w - - 49 53", "a3e7"),
    ("mate-06", "fen 7k/p5p1/1p5p/1Pp5/2RPp1P1/P3P1P1/2Q3K1/2N1q2r b - - 50 53", "e1f1"),
    ("mate-07", "fen r2rk3/1q2b1p1/ppb1Q1N1/4p3/P7/1N5R/1P4BP/n6K w - - 51 53", "h3h8"),
    ("mate-08", "fen 2r1r3/pp1n1N1Q/4p2b/q6k/P1pP2R1/2P2P2/1P6/R5K1 w - - 51 53", "h7h6"),
]

#!/usr/bin/env python3
"""Audit SHAYVERI's 32-byte Bullet records without trainer dependencies."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys


RECORD = struct.Struct("<Q16shBBB3s")


def fail(path: Path, index: int, message: str) -> None:
    raise ValueError(f"{path}: record {index}: {message}")


def audit(paths: list[Path]) -> dict[str, object]:
    results: Counter[int] = Counter()
    score_buckets: Counter[str] = Counter()
    piece_counts: Counter[int] = Counter()
    exact_hashes: set[bytes] = set()
    position_hashes: set[bytes] = set()
    exact_duplicates = 0
    position_duplicates = 0
    records = 0

    for path in paths:
        size = path.stat().st_size
        if size % RECORD.size:
            raise ValueError(f"{path}: size {size} is not divisible by {RECORD.size}")
        with path.open("rb") as source:
            for index in range(size // RECORD.size):
                raw = source.read(RECORD.size)
                occupancy, packed, score, result, king_square, opp_king_square, padding = RECORD.unpack(raw)
                count = occupancy.bit_count()
                if count < 2 or count > 32:
                    fail(path, index, f"invalid piece count {count}")
                if result > 2:
                    fail(path, index, f"invalid result {result}")
                if padding != b"\0\0\0":
                    fail(path, index, "nonzero padding")

                squares = [square for square in range(64) if occupancy & (1 << square)]
                codes = [
                    (packed[piece // 2] >> (4 * (piece % 2))) & 0xF
                    for piece in range(count)
                ]
                if any((code & 7) > 5 for code in codes):
                    fail(path, index, "invalid piece code")
                white_kings = [square for square, code in zip(squares, codes) if code == 5]
                black_kings = [square for square, code in zip(squares, codes) if code == 13]
                if len(white_kings) != 1 or len(black_kings) != 1:
                    fail(path, index, "record must contain exactly one king per side")
                if king_square != white_kings[0]:
                    fail(path, index, "white king-square field mismatch")
                if opp_king_square != (black_kings[0] ^ 56):
                    fail(path, index, "opponent king-square field mismatch")
                if max(abs(white_kings[0] % 8 - black_kings[0] % 8),
                       abs(white_kings[0] // 8 - black_kings[0] // 8)) <= 1:
                    fail(path, index, "adjacent kings")
                for square, code in zip(squares, codes):
                    if (code & 7) == 0 and square // 8 in (0, 7):
                        fail(path, index, "pawn on back rank")
                for colour in (0, 8):
                    side_codes = [code for code in codes if code & 8 == colour]
                    if len(side_codes) > 16 or sum((code & 7) == 0 for code in side_codes) > 8:
                        fail(path, index, "impossible side material count")

                exact = hashlib.blake2b(raw, digest_size=16).digest()
                position = hashlib.blake2b(raw[:24] + raw[27:29], digest_size=16).digest()
                exact_duplicates += exact in exact_hashes
                position_duplicates += position in position_hashes
                exact_hashes.add(exact)
                position_hashes.add(position)
                results[result] += 1
                piece_counts[count] += 1
                absolute_score = abs(score)
                bucket = next(
                    label for limit, label in (
                        (50, "000-050"), (100, "051-100"), (200, "101-200"),
                        (400, "201-400"), (800, "401-800"), (1200, "801-1200"),
                        (2000, "1201-2000"), (32768, "2000+"),
                    ) if absolute_score <= limit
                )
                score_buckets[bucket] += 1
                records += 1

    return {
        "files": len(paths),
        "records": records,
        "exact_duplicates": exact_duplicates,
        "exact_duplicate_pct": 100 * exact_duplicates / max(records, 1),
        "position_duplicates": position_duplicates,
        "position_duplicate_pct": 100 * position_duplicates / max(records, 1),
        "results_stm_pov": {str(key): results[key] for key in range(3)},
        "score_abs_buckets": dict(score_buckets),
        "piece_counts": dict(sorted(piece_counts.items())),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(sorted(args.files)), indent=2))
        return 0
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

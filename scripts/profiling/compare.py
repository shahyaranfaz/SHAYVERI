#!/usr/bin/env python3
"""Compare two baseline.json files produced by baseline.py."""

import argparse
import json
from pathlib import Path


def load(path: Path) -> dict[tuple[str, str, int], dict[str, object]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    return {
        (row["category"], row["case"], int(row["threads"])): row
        for row in document["summary"]
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()
    before = load(args.before)
    after = load(args.after)
    print("category,case,threads,metric,before,after,delta_percent")
    for key in sorted(before.keys() & after.keys()):
        for metric in ("median_nps", "median_nodes", "median_wall_ms"):
            if metric not in before[key] or metric not in after[key]:
                continue
            old = float(before[key][metric])
            new = float(after[key][metric])
            delta = (new / old - 1.0) * 100.0 if old else 0.0
            print(f"{key[0]},{key[1]},{key[2]},{metric},{old:g},{new:g},{delta:+.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

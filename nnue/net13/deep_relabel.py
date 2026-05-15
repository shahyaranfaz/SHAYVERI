#!/usr/bin/env python3
"""Relabel SHAYVERI self-play positions with deeper SHAYVERI search.

Input and output use the project plain format:

    FEN | white-pov cp | WDL

The filter implements the net13 deep-hard lane:
- keep if shallow/deep score differs by at least --cp-diff;
- keep if shallow/deep best move differs;
- keep if deep score is near equality;
- keep if the original game result contradicts the shallow score, a local
  proxy for "loss/draw collapse" positions from self-play.

Labels are still self-referential: only the supplied SHAYVERI engine is queried.
"""

from __future__ import annotations

import argparse
import glob
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


MATE_CP = 30000


@dataclass
class SearchInfo:
    score_cp_white: int
    bestmove: str


class UciEngine:
    def __init__(self, command: list[str], eval_file: str | None, hash_mb: int, threads: int) -> None:
        self.proc = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        assert self.proc.stdin is not None
        assert self.proc.stdout is not None
        self.stdin = self.proc.stdin
        self.stdout = self.proc.stdout
        self._send("uci")
        self._wait_for("uciok")
        self.setoption("OwnBook", "false")
        self.setoption("Hash", str(hash_mb))
        self.setoption("Threads", str(threads))
        if eval_file:
            self.setoption("EvalFile", eval_file)
        self._send("isready")
        self._wait_for("readyok")

    def setoption(self, name: str, value: str) -> None:
        self._send(f"setoption name {name} value {value}")

    def _send(self, line: str) -> None:
        self.stdin.write(line + "\n")
        self.stdin.flush()

    def _wait_for(self, token: str) -> None:
        for line in self.stdout:
            if token in line:
                return
        raise RuntimeError(f"engine exited before {token}")

    def search_nodes(self, fen: str, nodes: int) -> SearchInfo:
        self._send("ucinewgame")
        self._send(f"position fen {fen}")
        self._send(f"go nodes {nodes}")
        side_to_move = fen.split()[1]
        last_score: int | None = None
        bestmove = ""
        for line in self.stdout:
            line = line.strip()
            if line.startswith("info "):
                score = parse_score_cp(line)
                if score is not None:
                    last_score = score
            elif line.startswith("bestmove "):
                parts = line.split()
                bestmove = parts[1] if len(parts) > 1 else ""
                break
        if last_score is None:
            last_score = 0
        score_white = last_score if side_to_move == "w" else -last_score
        return SearchInfo(score_white, bestmove)

    def close(self) -> None:
        try:
            self._send("quit")
        except BrokenPipeError:
            pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()


def parse_score_cp(line: str) -> int | None:
    parts = line.split()
    for i, tok in enumerate(parts):
        if tok != "score" or i + 2 >= len(parts):
            continue
        kind = parts[i + 1]
        value = parts[i + 2]
        try:
            raw = int(value)
        except ValueError:
            continue
        if kind == "cp":
            return raw
        if kind == "mate":
            sign = 1 if raw > 0 else -1
            return sign * max(0, MATE_CP - abs(raw))
    return None


def parse_plain(line: str) -> tuple[str, int, float] | None:
    parts = [part.strip() for part in line.rstrip("\n").split("|")]
    if len(parts) != 3:
        return None
    try:
        cp = int(float(parts[1]))
        wdl = float(parts[2])
    except ValueError:
        return None
    return parts[0], cp, wdl


def contradicts_result(score_white: int, wdl: float, draw_margin: int) -> bool:
    if math.isclose(wdl, 1.0):
        return score_white < -draw_margin
    if math.isclose(wdl, 0.0):
        return score_white > draw_margin
    return abs(score_white) > draw_margin


def should_keep(
    original_cp: int,
    original_wdl: float,
    shallow: SearchInfo,
    deep: SearchInfo,
    cp_diff: int,
    near_eq: int,
    draw_margin: int,
    bestmove_change: bool,
) -> bool:
    if abs(deep.score_cp_white - shallow.score_cp_white) >= cp_diff:
        return True
    if abs(deep.score_cp_white - original_cp) >= cp_diff:
        return True
    if bestmove_change and shallow.bestmove and deep.bestmove and shallow.bestmove != deep.bestmove:
        return True
    if abs(deep.score_cp_white) <= near_eq:
        return True
    if contradicts_result(shallow.score_cp_white, original_wdl, draw_margin):
        return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", nargs="+", required=True)
    parser.add_argument("--eval-file", default="")
    parser.add_argument("--input-glob", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--shallow-nodes", type=int, default=10_000)
    parser.add_argument("--deep-nodes", type=int, default=100_000)
    parser.add_argument("--cp-diff", type=int, default=80)
    parser.add_argument("--near-eq", type=int, default=150)
    parser.add_argument("--draw-margin", type=int, default=150)
    parser.add_argument("--hash", type=int, default=64)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--target-positions", type=int, default=0)
    parser.add_argument("--bestmove-change", action=argparse.BooleanOptionalAction, default=True)
    args = parser.parse_args()

    inputs = sorted(glob.glob(args.input_glob))
    if not inputs:
        print(f"no input files matched {args.input_glob}", file=sys.stderr)
        return 1

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    engine = UciEngine(args.engine, args.eval_file or None, args.hash, args.threads)
    seen = kept = invalid = 0
    try:
        with open(args.output, "w", encoding="utf-8") as out:
            for input_path in inputs:
                with open(input_path, "r", encoding="utf-8") as f:
                    for line in f:
                        parsed = parse_plain(line)
                        if parsed is None:
                            invalid += 1
                            continue
                        fen, original_cp, original_wdl = parsed
                        seen += 1
                        shallow = engine.search_nodes(fen, args.shallow_nodes)
                        deep = engine.search_nodes(fen, args.deep_nodes)
                        if should_keep(
                            original_cp,
                            original_wdl,
                            shallow,
                            deep,
                            args.cp_diff,
                            args.near_eq,
                            args.draw_margin,
                            args.bestmove_change,
                        ):
                            out.write(f"{fen} | {deep.score_cp_white} | {original_wdl:.1f}\n")
                            kept += 1
                            if args.target_positions and kept >= args.target_positions:
                                print(f"deep relabel kept target={kept} seen={seen} invalid={invalid}")
                                return 0
                        if seen % 1000 == 0:
                            print(f"deep relabel seen={seen} kept={kept} invalid={invalid}", file=sys.stderr)
    finally:
        engine.close()

    print(f"deep relabel complete kept={kept} seen={seen} invalid={invalid}")
    if args.target_positions and kept < args.target_positions:
        print(
            f"kept only {kept} positions, below target {args.target_positions}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

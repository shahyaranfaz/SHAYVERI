#!/usr/bin/env python3
"""Validate a completed Elo-pin PGN and its optional Ordo outputs."""

from __future__ import annotations

import argparse
import csv
import itertools
import re
import sys
from collections import Counter
from pathlib import Path


TAG_RE = re.compile(r'^\[([A-Za-z0-9_]+) "(.*)"\]$')
VALID_RESULTS = {"1-0", "0-1", "1/2-1/2"}
ORDO_FILES = (
    "results.txt",
    "h2h.txt",
    "results.csv",
    "cfs.csv",
    "err.csv",
    "ordo.stdout.txt",
)


def fail(message: str) -> None:
    raise ValueError(message)


def read_games(path: Path) -> list[dict[str, str]]:
    games: list[dict[str, str]] = []
    tags: dict[str, str] = {}

    with path.open("r", encoding="utf-8", errors="strict") as handle:
        for line_number, raw_line in enumerate(handle, 1):
            line = raw_line.rstrip("\r\n")
            match = TAG_RE.match(line)
            if not match:
                continue
            key, value = match.groups()
            if key == "Event" and tags:
                games.append(tags)
                tags = {}
            if key in tags:
                fail(f"{path}:{line_number}: duplicate {key} tag")
            tags[key] = value

    if tags:
        games.append(tags)
    return games


def validate_games(path: Path, players: list[str], games_per_pair: int) -> None:
    if len(players) != len(set(players)):
        fail("expected player list contains duplicates")
    if len(players) < 2:
        fail("at least two expected players are required")
    if games_per_pair <= 0 or games_per_pair % 2 != 0:
        fail("games per pairing must be a positive even number")

    games = read_games(path)
    expected_players = set(players)
    expected_pairs = {
        tuple(sorted(pair)) for pair in itertools.combinations(players, 2)
    }
    expected_total = len(expected_pairs) * games_per_pair
    if len(games) != expected_total:
        fail(f"found {len(games)} games, expected {expected_total}")

    pair_counts: Counter[tuple[str, str]] = Counter()
    colour_counts: Counter[tuple[str, str]] = Counter()
    seen_players: set[str] = set()

    for number, tags in enumerate(games, 1):
        missing = [name for name in ("White", "Black", "Result") if name not in tags]
        if missing:
            fail(f"game {number} is missing tags: {', '.join(missing)}")

        white = tags["White"]
        black = tags["Black"]
        result = tags["Result"]
        if white not in expected_players:
            fail(f"game {number} has unexpected White player: {white}")
        if black not in expected_players:
            fail(f"game {number} has unexpected Black player: {black}")
        if white == black:
            fail(f"game {number} is a self-pairing: {white}")
        if result not in VALID_RESULTS:
            fail(f"game {number} has invalid or unfinished result: {result}")

        pair = tuple(sorted((white, black)))
        pair_counts[pair] += 1
        colour_counts[(white, black)] += 1
        seen_players.update((white, black))

    if seen_players != expected_players:
        missing = sorted(expected_players - seen_players)
        fail(f"players without games: {', '.join(missing)}")

    for pair in sorted(expected_pairs):
        count = pair_counts[pair]
        if count != games_per_pair:
            fail(f"pairing {pair[0]} vs {pair[1]} has {count} games, expected {games_per_pair}")
        first_white = colour_counts[(pair[0], pair[1])]
        second_white = colour_counts[(pair[1], pair[0])]
        expected_colours = games_per_pair // 2
        if first_white != expected_colours or second_white != expected_colours:
            fail(
                f"pairing {pair[0]} vs {pair[1]} has unbalanced colors "
                f"({first_white}/{second_white}, expected {expected_colours}/{expected_colours})"
            )

    unexpected_pairs = set(pair_counts) - expected_pairs
    if unexpected_pairs:
        fail(f"unexpected pairings: {sorted(unexpected_pairs)}")

    print(
        f"PGN valid: games={len(games)} players={len(players)} "
        f"pairings={len(expected_pairs)} games_per_pair={games_per_pair}"
    )


def validate_ordo(directory: Path, players: list[str]) -> None:
    for name in ORDO_FILES:
        path = directory / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty Ordo output: {path}")

    results_text = (directory / "results.txt").read_text(encoding="utf-8")
    h2h_text = (directory / "h2h.txt").read_text(encoding="utf-8")
    for player in players:
        if player not in results_text:
            fail(f"results.txt does not contain player: {player}")
        if player not in h2h_text:
            fail(f"h2h.txt does not contain player: {player}")

    with (directory / "results.csv").open(newline="", encoding="utf-8") as handle:
        csv_text = "\n".join(",".join(row) for row in csv.reader(handle))
    for player in players:
        if player not in csv_text:
            fail(f"results.csv does not contain player: {player}")

    print(f"Ordo outputs valid: directory={directory} players={len(players)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pgn", type=Path, required=True)
    parser.add_argument("--games-per-pair", type=int, required=True)
    parser.add_argument("--player", action="append", dest="players", required=True)
    parser.add_argument("--ordo-dir", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        validate_games(args.pgn, args.players, args.games_per_pair)
        if args.ordo_dir is not None:
            validate_ordo(args.ordo_dir, args.players)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

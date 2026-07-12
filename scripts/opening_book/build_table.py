#!/usr/bin/env python3
"""Build a full-line opening table from the local TWIC corpus."""

from __future__ import annotations

import csv
from collections import defaultdict
from itertools import islice
from pathlib import Path

import chess.pgn


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUTS_DIR = SCRIPT_DIR / "outputs"
PGN_FILE = OUTPUTS_DIR / "twic.pgn"

BOOK_PLIES = 24
# Retain every qualifying full line. Prefix aggregation applies the effective
# MIN_TOTAL_PLAYS threshold when the compiled book is built.
MIN_GAMES = 1
FIELDNAMES = [
    "Moves",
    "Played",
    "WhiteWins",
    "Draws",
    "BlackWins",
    "WhiteScore",
    "BlackScore",
    "AvgElo",
]


if not PGN_FILE.exists():
    raise FileNotFoundError(
        f"Missing {PGN_FILE}. Run 'make -C scripts/opening_book fetch' first."
    )

print(f"Using corpus: {PGN_FILE}")

with PGN_FILE.open(encoding="utf-8", errors="ignore") as corpus:
    total_games = sum(1 for line in corpus if line.startswith("[Event "))
print(f"Processing {total_games} games...")

openings = defaultdict(
    lambda: {
        "games_played": 0,
        "white_win": 0,
        "draw": 0,
        "black_win": 0,
        "total_elo": 0,
    }
)

skipped_variant = 0
skipped_setup = 0
skipped_rating = 0
accepted_games = 0
seen_games = 0

with PGN_FILE.open(encoding="utf-8", errors="ignore") as game_file:
    game = chess.pgn.read_game(game_file)
    while game is not None:
        seen_games += 1
        if seen_games % 1000 == 0 or seen_games == total_games:
            print(
                f"Processed {seen_games} / {total_games} games...",
                end="\r",
                flush=True,
            )

        variant = str(game.headers.get("Variant", "")).strip().casefold()
        if variant and variant not in {"standard", "normal"}:
            skipped_variant += 1
            game = chess.pgn.read_game(game_file)
            continue

        if game.headers.get("SetUp") == "1" or game.headers.get("FEN"):
            skipped_setup += 1
            game = chess.pgn.read_game(game_file)
            continue

        try:
            white_elo = int(game.headers.get("WhiteElo"))
            black_elo = int(game.headers.get("BlackElo"))
        except (TypeError, ValueError):
            skipped_rating += 1
            game = chess.pgn.read_game(game_file)
            continue

        if white_elo < 2600 or black_elo < 2600:
            skipped_rating += 1
            game = chess.pgn.read_game(game_file)
            continue

        board = game.board()
        sequence_moves = []
        for move in islice(game.mainline_moves(), BOOK_PLIES):
            if not board.is_legal(move):
                raise RuntimeError(
                    f"illegal parsed move {move.uci()} in "
                    f"{game.headers.get('Event', '<unknown event>')}"
                )
            sequence_moves.append(move.uci())
            board.push(move)

        sequence = " ".join(sequence_moves)
        if not sequence:
            game = chess.pgn.read_game(game_file)
            continue

        accepted_games += 1
        stats = openings[sequence]
        stats["games_played"] += 1
        stats["total_elo"] += (white_elo + black_elo) / 2

        result = game.headers.get("Result")
        if result == "1-0":
            stats["white_win"] += 1
        elif result == "0-1":
            stats["black_win"] += 1
        elif result == "1/2-1/2":
            stats["draw"] += 1

        game = chess.pgn.read_game(game_file)

print(f"Processed {seen_games} / {total_games} games.")
rows = []
for moves, stats in openings.items():
    if stats["games_played"] < MIN_GAMES:
        continue

    elo_weight = (stats["total_elo"] / stats["games_played"]) / 2800
    white_expectancy = (
        stats["white_win"] + 0.5 * stats["draw"]
    ) / stats["games_played"]
    black_expectancy = (
        stats["black_win"] + 0.5 * stats["draw"]
    ) / stats["games_played"]

    rows.append(
        {
            "Moves": moves,
            "Played": stats["games_played"],
            "WhiteWins": stats["white_win"],
            "Draws": stats["draw"],
            "BlackWins": stats["black_win"],
            "WhiteScore": white_expectancy * elo_weight,
            "BlackScore": black_expectancy * elo_weight,
            "AvgElo": stats["total_elo"] / stats["games_played"],
        }
    )

rows.sort(key=lambda row: row["Moves"])
OUTPUTS_DIR.mkdir(exist_ok=True)
table_path = OUTPUTS_DIR / "table.csv"
with table_path.open("w", newline="", encoding="utf-8") as output:
    writer = csv.DictWriter(output, fieldnames=FIELDNAMES)
    writer.writeheader()
    writer.writerows(rows)

print(
    f"Opening table built: {len(rows)} distinct full lines "
    f"written to {table_path}"
)
print(
    f"Games accepted: {accepted_games}; skipped non-standard: "
    f"{skipped_variant}; skipped setup/FEN: {skipped_setup}; "
    f"skipped rating: {skipped_rating}"
)

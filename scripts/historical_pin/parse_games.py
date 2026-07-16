#!/usr/bin/env python3
"""Build balanced reference PGNs from the v2.6.0 Elo-pin games."""

from __future__ import annotations

import argparse
import random
from collections import defaultdict
from pathlib import Path

import chess.pgn

DEFAULT_GAMES_PER_PAIRING = {"stc": 200, "ltc": 100}
DEFAULT_SEED = 2600
EXPECTED_PLAYERS = {
    "Alexandria9",
    "Berserk13",
    "Ethereal14",
    "PlentyChess7",
    "SF2850",
    "SF3000",
    "Weiss2",
}
EXPECTED_TIME_CONTROLS = {"stc": "10+0.1", "ltc": "90+0.5"}


def find_repo_root(start: Path | None = None) -> Path:
    path = (start or Path(__file__)).resolve()

    if path.is_file():
        path = path.parent

    for directory in (path, *path.parents):
        if (directory / ".git").exists():
            return directory

    raise FileNotFoundError("Could not find repository root")


ROOT = find_repo_root()
DEFAULT_INPUTS = {
    "stc": (
        ROOT / "scripts" / "elo_pin" / "outputs" / "v2.6.0" / "stc" / "rating_pool.pgn"
    ),
    "ltc": (
        ROOT / "scripts" / "elo_pin" / "outputs" / "v2.6.0" / "ltc" / "rating_pool.pgn"
    ),
}
DEFAULT_OUTPUT_DIR = ROOT / "scripts" / "historical_pin" / "outputs"


def pairing_key(game: chess.pgn.Game) -> tuple[str, str]:
    return tuple(sorted((game.headers["White"], game.headers["Black"])))


def read_reference_games(path: Path) -> list[chess.pgn.Game]:
    games = []

    with path.open(encoding="utf-8") as pgn:
        while game := chess.pgn.read_game(pgn):
            if game.errors:
                raise ValueError(f"Failed to parse a game in {path}: {game.errors}")
            players = (game.headers.get("White", ""), game.headers.get("Black", ""))
            if not all(players):
                raise ValueError(f"Game in {path} is missing a player header")
            if any("shayveri" in player.casefold() for player in players):
                continue
            games.append(game)

    return games


def make_opening_pairs(
        games: list[chess.pgn.Game],
) -> dict[tuple[str, str], list[tuple[chess.pgn.Game, chess.pgn.Game]]]:
    by_pairing_and_fen = defaultdict(lambda: defaultdict(lambda: [[], []]))

    for game in games:
        pairing = pairing_key(game)
        fen = game.headers.get("FEN", chess.STARTING_FEN)
        orientation = 0 if game.headers["White"] == pairing[0] else 1
        by_pairing_and_fen[pairing][fen][orientation].append(game)

    opening_pairs = {}
    for pairing, by_fen in by_pairing_and_fen.items():
        pairs = []
        for orientations in by_fen.values():
            if len(orientations[0]) != len(orientations[1]):
                raise ValueError(
                    f"Unbalanced colours for {pairing}: "
                    f"{len(orientations[0])} versus {len(orientations[1])}"
                )
            pairs.extend(zip(orientations[0], orientations[1], strict=True))
        opening_pairs[pairing] = pairs

    return opening_pairs


def select_games(
        opening_pairs: dict[
            tuple[str, str], list[tuple[chess.pgn.Game, chess.pgn.Game]]
        ],
        games_per_pairing: int,
        seed: int,
) -> list[chess.pgn.Game]:
    if games_per_pairing <= 0 or games_per_pairing % 2:
        raise ValueError("Games per pairing must be a positive even number")

    required_pairs = games_per_pairing // 2
    selected = []

    for index, pairing in enumerate(sorted(opening_pairs)):
        candidates = opening_pairs[pairing].copy()
        random.Random(seed + index).shuffle(candidates)

        if len(candidates) < required_pairs:
            raise ValueError(
                f"Only {len(candidates) * 2} paired games are available for "
                f"{pairing}, but {games_per_pairing} were requested"
            )

        for game_pair in candidates[:required_pairs]:
            selected.extend(game_pair)

    return selected


def write_games(path: Path, games: list[chess.pgn.Game]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("w", encoding="utf-8", newline="\n") as pgn:
        exporter = chess.pgn.FileExporter(pgn)
        for game in games:
            game.accept(exporter)


def process_time_control(
        label: str,
        input_path: Path,
        output_path: Path,
        games_per_pairing: int,
        seed: int,
) -> None:
    if not input_path.is_file():
        raise FileNotFoundError(f"Missing {label.upper()} PGN: {input_path}")

    games = read_reference_games(input_path)
    players = {
        game.headers[colour]
        for game in games
        for colour in ("White", "Black")
    }
    if players != EXPECTED_PLAYERS:
        raise ValueError(
            f"Unexpected {label.upper()} reference players: {sorted(players)}"
        )

    time_controls = {game.headers.get("TimeControl") for game in games}
    if time_controls != {EXPECTED_TIME_CONTROLS[label]}:
        raise ValueError(
            f"Unexpected {label.upper()} time controls: {time_controls!r}"
        )

    opening_pairs = make_opening_pairs(games)
    selected = select_games(opening_pairs, games_per_pairing, seed)
    write_games(output_path, selected)

    print(
        f"{label.upper()}: wrote {len(selected)} games across "
        f"{len(opening_pairs)} pairings to {output_path}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Remove all SHAYVERI games from the v2.6.0 pin and retain a "
            "balanced sample of paired games for every external pairing."
        )
    )
    parser.add_argument(
        "--stc-games-per-pairing",
        type=int,
        default=DEFAULT_GAMES_PER_PAIRING["stc"],
    )
    parser.add_argument(
        "--ltc-games-per-pairing",
        type=int,
        default=DEFAULT_GAMES_PER_PAIRING["ltc"],
    )
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--stc-input", type=Path, default=DEFAULT_INPUTS["stc"])
    parser.add_argument("--ltc-input", type=Path, default=DEFAULT_INPUTS["ltc"])
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    for label, input_path in (("stc", args.stc_input), ("ltc", args.ltc_input)):
        process_time_control(
            label,
            input_path,
            args.output_dir / f"reference_{label}.pgn",
            getattr(args, f"{label}_games_per_pairing"),
            args.seed,
        )


if __name__ == "__main__":
    main()

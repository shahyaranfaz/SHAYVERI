import subprocess
from itertools import islice
import pandas as pd
import chess.pgn
from collections import defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ARTIFACTS_DIR = SCRIPT_DIR / "artifacts"

DEPTH = 24
MIN_GAMES = 25

ARTIFACTS_DIR.mkdir(exist_ok=True)
subprocess.run(["bash", "fetch_twic.sh", "920", "2500", str(ARTIFACTS_DIR / "twic.pgn")],
               cwd=SCRIPT_DIR, check=True)

openings = defaultdict(lambda: {"games_played": 0, "white_win": 0, "draw": 0, "black_win": 0, "total_elo": 0})

with open(ARTIFACTS_DIR / "twic.pgn", encoding="utf-8", errors="ignore") as game_file:
    game = chess.pgn.read_game(game_file)
    while game is not None:
        try:
            w_elo = int(game.headers.get("WhiteElo"))
            b_elo = int(game.headers.get("BlackElo"))
            if w_elo < 2600 or b_elo < 2600:
                game = chess.pgn.read_game(game_file)
                continue
        except ValueError:
            game = chess.pgn.read_game(game_file)
            continue

        sequence = " ".join([move.uci() for move in islice(game.mainline_moves(), DEPTH)])
        openings[sequence]["games_played"] += 1

        elo = w_elo + b_elo
        openings[sequence]["total_elo"] += elo / 2


        result = game.headers.get("Result")
        if result == "1-0":
            openings[sequence]["white_win"] += 1
        elif result == "0-1":
            openings[sequence]["black_win"] += 1
        elif result == "1/2-1/2":
            openings[sequence]["draw"] += 1

        game = chess.pgn.read_game(game_file)

data = []
for moves, stats in openings.items():
    if moves.strip() == "" or stats["white_win"] + stats["black_win"] + stats["draw"] == 0 or stats["games_played"] < MIN_GAMES:
        continue

    elo_weight = (stats["total_elo"] / stats["games_played"]) / 2800
    white_expectancy = (stats["white_win"] + 0.5 * stats["draw"]) / stats["games_played"]
    black_expectancy = (stats["black_win"] + 0.5 * stats["draw"]) / stats["games_played"]

    data.append([
        moves,
        stats["games_played"],
        stats["white_win"],
        stats["draw"],
        stats["black_win"],
        white_expectancy * elo_weight,
        black_expectancy * elo_weight,
        stats["total_elo"] / stats["games_played"]
    ])

opening_book = pd.DataFrame(data, columns=[
    "Moves",
    "Played",
    "WhiteWins",
    "Draws",
    "BlackWins",
    "WhiteScore",
    "BlackScore",
    "AvgElo",
])
opening_book = opening_book.sort_values(by="Moves", ascending=True)
opening_book.to_csv(ARTIFACTS_DIR / "table.csv", index=False)

print("Opening base has been built! (written to artifacts/table.csv)")

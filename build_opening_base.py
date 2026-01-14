import subprocess
from itertools import islice
import pandas as pd
import chess.pgn
from collections import defaultdict

DEPTH = 24

subprocess.run(["bash", "filter.sh", "920", "2500"])

openings = defaultdict(lambda: {"games_played": 0, "white_win": 0, "draw": 0, "black_win": 0, "total_elo": 0})

i = 0
with open("db/twic.pgn", encoding="utf-8", errors="ignore") as game_file:
    game = chess.pgn.read_game(game_file)
    while game is not None:
        i += 1
        if i % 1000 == 0:
            print(f"Processed {i} games...")
        sequence = " ".join([move.uci() for move in islice(game.mainline_moves(), DEPTH)])
        openings[sequence]["games_played"] += 1

        elo = int(game.headers.get("WhiteElo")) + int(game.headers.get("BlackElo"))
        openings[sequence]["total_elo"] += elo / 2

        result = game.headers.get("Result")
        if result == "1-0":
            openings[sequence]["white_win"] += 1
        elif result == "0-1":
            openings[sequence]["black_win"] += 1
        else:
            openings[sequence]["draw"] += 1

        game = chess.pgn.read_game(game_file)

data = []
for moves, stats in openings.items():
    data.append([
        moves,
        stats["games_played"],
        stats["white_win"],
        stats["draw"],
        stats["black_win"],
        stats["total_elo"] / stats["games_played"]
    ])

data_frame = pd.DataFrame(data, columns=["Moves", "Played", "WhiteWins", "Draws", "BlackWins", "AvgElo"])
data_frame = data_frame.sort_values(by="Moves", ascending=True)
data_frame.to_csv("opening_table.csv", index=False)
print("Opening table saved to opening_table.csv")
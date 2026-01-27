import pandas as pd

opening_book = pd.read_csv("table.csv")

# We will read from the opening book and choose the best move. we can decide later
# how to build the best book. Once the book lookup fails, we transition to midgame.py

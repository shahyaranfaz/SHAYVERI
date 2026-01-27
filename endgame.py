import chess
import chess.syzygy

TABLEBASE_PATH = None
SYZYGY_MAX_PIECES = None

# When there are fewer than X pieces on the board, we lookup the best move from a syzygy
# tablebase. X is likely 5, maybe 6. 7 would be a gigabyte.

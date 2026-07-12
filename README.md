# SHAYVERI

**Super Heuristic Adaptive Yield Variation Engine for Rook Intelligence**

SHAYVERI is a UCI-compliant C++20 chess engine with a classical search core, a handcrafted evaluation path, and an embedded NNUE evaluation path. The current public strength numbers come from controlled multi-engine round-robin gauntlets analyzed with Ordo. They are pool-relative, anchored to fixed-strength `SF2850` and `SF3000`, and should not be read as universal CCRL ratings.

Release labels separate the SHAYVERI source version, evaluator, and network. The current release is `SHAYVERI v2.6 / NNUE SHAYVERI2_5_0`.

## Elo results

| Engine        | Evaluation    | Network       | Time control | Rating |   Error | Gap to SF2850 | Gap to SF3000 |
|---------------|---------------|---------------|--------------|-------:|--------:|--------------:|--------------:|
| SHAYVERI v2.6 | NNUE          | SHAYVERI2_5_0 | STC 10+0.1   | 3045.7 | +/-14.8 |        +195.7 |         +45.7 |
| SHAYVERI v2.6 | NNUE          | SHAYVERI2_5_0 | LTC 90+0.5   | 3136.5 | +/-28.9 |        +286.5 |        +136.5 |
| SHAYVERI v2.6 | HCE-classical | none          | STC 10+0.1   | 2594.5 | +/-19.8 |        -255.5 |        -405.5 |
| SHAYVERI v2.6 | HCE-classical | none          | LTC 90+0.5   | 2718.8 | +/-33.2 |        -131.2 |        -281.2 |

The v2.6 NNUE configuration is about +451.2 Elo over HCE-classical at STC and +417.7 Elo at LTC in this anchored pool.

## How to build

Requires a C++ compiler with C++20 support and a modern x86-64 CPU with BMI/BMI2, LZCNT, and POPCNT support. 
SHAYVERI can then be loaded into any UCI-compatible GUI such as Arena, Cutechess, or Lichess via the [Lichess bot API](https://lichess.org/api#tag/Bot).

```bash
make
make windows
make macos
make clean
```

The default net, `SHAYVERI2_5_0.nnue`, is automatically embedded into the binary.
Hence, the `.nnue` file must be at root-level only while building.
An external network can be loaded through the `EvalFile` UCI option.

## How to use

SHAYVERI implements the UCI protocol. Load it as an engine in any UCI-compatible GUI, or run it directly and communicate via stdin/stdout:

```
uci
isready
position startpos moves e2e4 e7e5
go movetime 1000
```

## Search

- Negamax with alpha-beta pruning and Principal Variation Search
- Iterative deepening with aspiration windows
- Quiescence search with legal check evasions at the depth floor
- Null move pruning
- Internal iterative reductions (IIR)
- Late move reductions (LMR), including improving and cut-node-aware reductions
- Late move pruning (LMP)
- Singular extensions
- Futility pruning and reverse futility pruning
- Delta pruning
- SEE (Static Exchange Evaluation) for move ordering
- Conservative quiescence SEE pruning for non-promotion captures
- Killer moves, gravity-based main, continuation, and capture histories
- Static-evaluation correction history
- Atomic 4-entry bucketed transposition table with Zobrist hashing
- Repetition detection (2-fold during search, 3-fold draw claim)
- Lazy SMP with shared TT for timed multi-threaded search
- Dynamic time management

## Evaluation paths

There are two available evaluation paths: `NNUE` and `HCE-classical`, which can be chosen between
with the `UseNNUE` UCI option.

## Handcrafted evaluation

- Piece-square tables with tapered middlegame/endgame interpolation
- Pawn structure: passed pawns, isolated pawns, doubled/backward pawns, pawn chains, pawn storms, pawn islands
- King safety: pawn shield integrity, open files toward king, enemy pressure in king zone, escape squares
- Piece mobility weighted by piece type with openness multipliers
- Coordination: mutual protection graph, batteries, support chains, shared attack targets
- Tactical pressure: pins, skewers, x-ray pressure, overloaded defenders, undefended pieces
- Outpost squares for knights, bishops, rooks, and queens
- Tempo and initiative bonuses
- All evaluation parameters tuned via SPSA and Texel tuning

## NNUE evaluation

The embedded default, `SHAYVERI2_5_0.nnue`, is a `KB16x512` network: Chess768
features split across 16 mirrored king buckets, a 512-wide hidden layer, and
SCReLU activation. Search keeps both perspectives in incrementally updated
accumulators as moves are made and unmade.

The runtime also accepts compatible external classic Chess768 and KB8/KB16
networks with 256 or 512 hidden units. Select one through `EvalFile`. Changing
the network clears the transposition table and persistent search histories.

The current public NNUE line was trained using external Stockfish/RobotMoon-style
position corpora, which are not included in this repository.

## Opening book

Built from 2600+ rated games sourced from The Week in Chess (TWIC). The pipeline keeps 24-ply
lines from standard-chess games where both players are rated at least 2600, aggregates observed
move counts, and compiles the majority move for positions reaching at least five weighted plays.
The compiled book stores only the selected move and does not embed evaluator-specific metadata.
`Book_Info_Depth` controls optional search output for book hits. Its default value of `8` searches
only the selected book move and reports normal depth-by-depth score and PV information. Set it to
`0` for the immediate book-only fast path.

Opening-book helper tools live in `scripts/opening_book/`:

```bash
make -C scripts/opening_book run_all
```

## UCI options

- `Hash`: transposition table size in MB.
- `Clear Hash`: clear the transposition table.
- `Threads`: requested search threads, bounded by detected hardware capacity.
- `Ponder`: enable ponder output/search support.
- `OwnBook`: enable the embedded opening book.
- `Book_Info_Depth`: depth for optional book-move evaluation output. Set to `0` for the fast path.
- `UseNNUE`: enable or disable NNUE evaluation without changing the configured network path.
- `EvalFile`: NNUE network path. Set an explicit path to load an external network.
- `Minimum Thinking Time`: minimum move time, in milliseconds.
- `Move Overhead`: GUI/network delay reserve, in milliseconds.

## Verification

The debug harness lives in `debug/`, and the root Makefile forwards to it.

```bash
make test
```

`make test` runs:

- Perft regression positions from Chessprogramming Wiki, including startpos, Kiwipete, and Position 3.
- FEN state validation for side to move, castling rights, en-passant square, counters, and key piece placement.
- Make/unmake roundtrip checks for board state, hash, castling, en-passant, side, and counters.
- En-passant legality edge cases adapted from `niklasf/python-chess`.
- SEE regression checks.
- Evaluation symmetry checks on vertically mirrored, color-swapped positions.
- Transposition-table safety checks.
- UCI checks covering handshake, fixed-depth search, bench node signature, opening-book on/off behavior, and deterministic `Threads=1` search.
- Tactical mate-in-1 regression positions adapted from `StuartRiffle/JaglavakTestData`.

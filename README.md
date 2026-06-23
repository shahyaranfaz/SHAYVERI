# SHAYVERI

**Super Heuristic Adaptive Yield Variation Engine for Rook Intelligence**

SHAYVERI is a UCI-compliant C++20 chess engine with a classical search core, a v1.0 handcrafted evaluation fallback, and a v2.2.0 NNUE evaluation path. The current public strength numbers come from controlled multi-engine round-robin gauntlets analyzed with Ordo. They are pool-relative, anchored to fixed-strength `SF2850` and `SF3000`, and should not be read as universal CCRL ratings.

## Elo results

| Configuration   | Evaluation    | Time control | Rating |   Error | Gap to SF2850 | Gap to SF3000 |
|-----------------|---------------|--------------|-------:|--------:|--------------:|--------------:|
| SHAYVERI v1.0   | Classical HCE | STC 10+0.1   | 2578.6 | +/-19.9 |        -271.4 |        -421.4 |
| SHAYVERI v1.0   | Classical HCE | LTC 90+0.5   | 2699.7 | +/-33.2 |        -150.3 |        -300.3 |
| SHAYVERI v2.2.0 | NNUE          | STC 10+0.1   | 2938.3 | +/-14.9 |         +88.3 |         -61.7 |
| SHAYVERI v2.2.0 | NNUE          | LTC 90+0.5   | 3046.2 | +/-28.6 |        +196.2 |         +46.2 |

SHAYVERI v2.2.0 improved over v1.0 by about +359.7 Elo at STC and +346.5 Elo at LTC in this anchored pool.

## How to build

The default target builds a native Linux binary named `SHAYVERI`:

```bash
make
```
Other targets:

```bash
make windows
make macos
make dump_keys
make clean
```
Requires a C++ compiler with C++20 support and a modern x86-64 CPU with BMI/BMI2, LZCNT, and POPCNT support. The engine binary can then be loaded into any UCI-compatible GUI such as Arena, Cutechess, or Lichess via the [Lichess bot API](https://lichess.org/api#tag/Bot).

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
- Quiescence search
- Null move pruning
- Late move reductions (LMR)
- Late move pruning (LMP)
- Singular extensions
- Futility pruning and reverse futility pruning
- Delta pruning
- Check extensions
- SEE (Static Exchange Evaluation) for move ordering
- Killer moves, gravity-based history tables, and two-ply continuation histories
- Transposition table with Zobrist hashing
- Repetition detection (2-fold during search, 3-fold draw claim)
- Lazy SMP for multi-threaded search
- Dynamic time management

## NNUE and v1.0

Current builds support `EvalFile SHAYVERI2_2_0.nnue`, making NNUE the stronger tested evaluation route when the NNUE file is available. Otherwise, SHAYVERI still has the original handcrafted evaluator.

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

## Opening book

Built from 2600+ rated games sourced from The Week in Chess (TWIC). Book evaluations are precomputed and cached.

## UCI options

- `Hash`: transposition table size in MB.
- `Threads`: number of search threads.
- `Ponder`: enable ponder output/search support.
- `OwnBook`: enable the embedded opening book.
- `UseNNUE`: enable or disable NNUE evaluation without changing the configured network path.
- `EvalFile`: NNUE network path. Defaults to `SHAYVERI2_2_0.nnue`.
- `Move Overhead`: GUI/network delay reserve, in milliseconds.

## Verification

The debug harness lives in `debug/`.

```bash
cd debug
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
- UCI smoke tests covering handshake, fixed-depth search, bench node signature, opening-book on/off behavior, and deterministic `Threads=1` search.
- Tactical mate-in-1 regression positions adapted from `StuartRiffle/JaglavakTestData`.

Optional release smoke:

```bash
cd debug
make release_test
```

This runs `make test` and then a short Cute Chess self-play smoke if `cutechess-cli` is installed. The self-play smoke is meant to catch crashes, illegal moves, disconnects, time forfeits, and protocol regressions; it is not an Elo test.

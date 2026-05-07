# SHAYVERI

**Super Heuristic Adaptive Yield Variation Engine for Rook Intelligence**

SHAYVERI is a UCI-compliant chess engine written in C++ that achieved 2750 Elo on Lichess. It implements a full classical search and evaluation stack, with all parameters tuned via SPSA and Texel tuning on self-play datasets.

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

## Evaluation

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
- `Clear Hash`: clears the transposition table.
- `Threads`: number of search threads.
- `Ponder`: enable ponder output/search support.
- `OwnBook`: enable the embedded opening book.
- `Minimum Thinking Time`: lower bound for allocated move time, in milliseconds.
- `Move Overhead`: GUI/network delay reserve, in milliseconds.

The engine supports normal chess only. Chess960 is not advertised.

If tuning support is enabled in `include/tune.h`, additional SPSA tuning options may also be advertised by UCI. The release values are frozen in the source.

## Verification

## Tests

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

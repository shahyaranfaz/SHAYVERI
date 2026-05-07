# SHAYVERI

**Super Heuristic Adaptive Yield Variation Engine for Rook Intelligence**

SHAYVERI is a UCI-compliant chess engine written in C++ that achieved 2750 Elo on Lichess. It implements a full classical search and evaluation stack, with all parameters tuned via SPSA and Texel tuning on self-play datasets.

## How to build

```
bash
make
```

Requires a C++ compiler with C++17 support. The engine binary can then be loaded into any UCI-compatible GUI such as Arena, Cutechess, or Lichess via the [Lichess bot API](https://lichess.org/api#tag/Bot).

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

`Hash` — transposition table size in MB  
`Threads` — number of search threads  
`MultiPV` — number of principal variations  
`Move Overhead` — compensation for GUI latency  
`UCI_ShowWDL` — display win/draw/loss percentages  
`UCI_AnalyseMode` — disable draw contempt for analysis  
`UCI_chess960` — Fischer Random Chess support  
`Ponder` — think on opponent's time  

## Verification

Move generation is verified via perft testing with correct node counts at every depth. Search correctness is validated against a testing suite.

Run the debug perft regression suite:

```bash
cd debug
make
./perft suite
```

The suite includes standard public perft regression positions and expected node counts from Chessprogramming Wiki (including Kiwipete) with attribution printed in failure output.

Run all debug regressions (perft + UCI smoke):

```bash
cd debug
make test
```

The UCI smoke flow is adapted from the public Stockfish test style (`official-stockfish/Stockfish/tests/instrumented.py`) and checks protocol handshake plus `bestmove` responses on standard positions.

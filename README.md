# SHAYVERI

**Super Heuristic Adaptive Yield Variation Engine for Rook Intelligence**

SHAYVERI is a UCI-compliant C++20 chess engine with a classical search core, a
handcrafted evaluation path, and an embedded NNUE evaluation path. The current
public strength numbers come from controlled multi-engine round-robin gauntlets
analyzed with Ordo. They are pool-relative, anchored to fixed-strength `SF2850`
and `SF3000`, and should not be read as universal CCRL ratings.

The current release is `SHAYVERI v2.10.0`, which embeds `SHAYVERI2_10_4.nnue` as
its default network. The versioning scheme is explained in
[`docs/VERSIONING.md`](docs/VERSIONING.md).

## Elo results

|           Engine | Evaluation | Time control | Rating |   Error |
|-----------------:|-----------:|-------------:|-------:|--------:|
| SHAYVERI v2.10.0 |       NNUE |   STC 10+0.1 | 3261.4 | +/-16.0 |
| SHAYVERI v2.10.0 |       NNUE |   LTC 90+0.5 | 3320.6 | +/-30.3 |
| SHAYVERI v2.10.0 |        HCE |   STC 10+0.1 | 2674.8 | +/-18.1 |
| SHAYVERI v2.10.0 |        HCE |   LTC 90+0.5 | 2794.1 | +/-30.9 |

The v2.10.0 NNUE configuration is about +586.6 Elo over HCE at STC and +526.5
Elo at LTC in this anchored pool. Against the identically anchored v2.9.0
results, it gained +67.0 Elo at STC and +50.3 Elo at LTC.

## How to build

Requires a C++ compiler with C++20 support and a modern x86-64 CPU with
BMI/BMI2, LZCNT, and POPCNT support. SHAYVERI can then be loaded into any
UCI-compatible GUI such as Arena, Cutechess, or Lichess via the
[Lichess bot API](https://lichess.org/api#tag/Bot). Build targets are provided
for Linux, Windows, and Intel macOS.

```bash
make
make windows
make macos
make pgo
make clean
```

On Linux, the `pgo` target builds the profile-guided `SHAYVERI_pgo` binary. It
trains the engine on representative fixed and timed single-thread and Lazy SMP
searches, then rebuilds it using the collected profile. This machine-native
build takes longer but provides the best local performance. Release binaries
remain portable non-PGO builds.

The default net, `SHAYVERI2_10_4.nnue`, is automatically embedded into the
binary while building. Hence, the `.nnue` file must be at root-level only
while building. At runtime, the UCI option `EvalFile` can be used to load
a compatible external network instead.

## How to use

SHAYVERI implements the UCI protocol. Load it as an engine in any UCI-compatible
GUI, or run it directly and communicate via stdin/stdout:

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
- Mate-distance pruning and transposition-table cutoffs with reusable static
  evaluations
- Null move pruning
- Capture-only ProbCut searches
- Internal iterative reductions (IIR)
- Late move reductions (LMR), including improving and cut-node-aware reductions
- Late move pruning (LMP)
- Singular, double, and triple extensions, multi-cut pruning, and reduced TT
  moves from excluded-move verification
- Futility pruning and reverse futility pruning
- History pruning
- Delta pruning
- SEE (Static Exchange Evaluation) for move ordering
- Staged TT, capture, promotion, killer, countermove, quiet, and losing-capture
  move ordering
- Gravity-based main, continuation, follow-up, and capture histories
- Pawn-keyed static-evaluation correction history persisted between searches
- Race-safe four-way cache-line transposition table with Zobrist hashing
- Repetition detection (2-fold during search, 3-fold draw claim)
- Terminal, fifty-move, and insufficient-material handling inside search
- Lazy SMP with shared TT for timed multi-threaded search
- Soft and hard time limits based on clock, increment, moves to go, minimum
  thinking time, and move overhead
- Soft-limit scaling from best-move stability, score drops, root node share,
  and multi-depth evaluation stability
- Hard timer enforcement, low-clock sudden-death protection, fixed move time,
  and ponder integration

## Evaluation paths

To select between the `HCE` and `NNUE` evaluation paths, use the UCI option 
`UseNNUE`.

## Handcrafted evaluation

- Piece-square tables with tapered middlegame/endgame interpolation
- Pawn-hash evaluation of passed, isolated, doubled, backward, chained,
  storming, weak, and islanded pawns
- King safety: pawn shield integrity, open files toward king, enemy pressure in
  king zone, coordinated attacker types, and escape squares
- Piece mobility weighted by piece type with openness multipliers
- Coordination: mutual protection graph, batteries, support chains, shared
  attack targets
- Tactical pressure: pins, skewers, x-ray pressure, overloaded defenders,
  undefended pieces
- Pawn and minor-piece threats against loose or more valuable pieces
- Outpost squares for knights, bishops, rooks, and queens
- Bishop pair, development, tempo, and initiative bonuses
- Evaluation and search parameters refined through Texel tuning and SPSA

## NNUE evaluation

The supported NNUE architectures, binary formats, embedding behavior, and
compatibility policy are documented in [`docs/NNUE.md`](docs/NNUE.md).

The embedded default, `SHAYVERI2_10_4.nnue`, is a `KB16x512` network: Chess768
features split across 16 mirrored king buckets, a 512-wide hidden layer, and
SCReLU activation. Search keeps both perspectives in incrementally updated
accumulators as moves are made and unmade.

Scalar and AVX2 inference paths evaluate the same network format.

The runtime also accepts compatible external classic Chess768 and KB8/KB16
networks with 256 or 512 hidden units. Select one through the UCI option
`EvalFile`. Changing the network clears the transposition table and persistent
search histories.

The current public NNUE line was produced through two continuation stages. Net1
was warm-started from `SHAYVERI2_5_0.nnue` on external RobotMoon/Stockfish data.
Net4 was then warm-started from Net1 using a deterministic batch mix of 45%
corrected SHAYVERI self-play data and 55% frozen Net1 external data.
The training data are not included in this repository.

## Opening book

The self-built embedded book uses standard-chess games from The Week in Chess
(TWIC) where both players are rated at least 2600. Its reproducible pipeline
keeps the first 30 plies, aggregates observed moves for positions with at least
five weighted plays, and compiles the majority move without evaluator-specific
metadata.

The UCI option `BookInfoDepth` controls optional search output for book hits.
Its default value of `8` searches only the selected book move and reports
SHAYVERI's own depth-by-depth score and PV information. Set it to `0` for the
immediate book-only fast path.

The generation method and helper tools are documented in
[`scripts/opening_book/README.md`](scripts/opening_book/README.md):

```bash
make -C scripts/opening_book run_all
```

## UCI options

### Resources

- `Hash`: set the transposition-table allocation, in MB.
- `ClearHash`: clear the transposition table and all persistent search
  histories.
- `Threads`: set the requested search-thread count, bounded by detected
  hardware capacity.

### Evaluation

- `UseNNUE`: toggle between the NNUE and HCE evaluation paths without changing
  the configured network path.
- `EvalFile`: specify the path to a compatible external NNUE network. The
  embedded default is used when left unchanged.

### Opening book

- `OwnBook`: toggle use of SHAYVERI's built-in opening book.
- `BookInfoDepth`: set the search depth used to report SHAYVERI's own book-move
  information. A value of `0` disables the search.

### Search timing

- `Ponder`: toggle ponder-move output so a chess GUI can start a `go ponder`
  search during the opponent's turn.
- `MinimumThinkingTime`: set the minimum time, in milliseconds, allocated to
  dynamically timed moves.
- `MoveOverhead`: reserve time for chess GUI and network delay, in
  milliseconds, before calculating the search limits.

## Verification

The debug harness lives in `debug/`, and the root Makefile forwards to it.

```bash
make test
```

`make test` runs:

- Warning-clean strict compilation across the production, embed-tool, and debug
  sources.
- High-signal `clang-tidy` checks, with every diagnostic treated as an error.
- Perft regression positions from Chessprogramming Wiki, including startpos,
  Kiwipete, and Position 3.
- FEN state validation for side to move, castling rights, en-passant square,
  counters, and key piece placement.
- Make/unmake roundtrip checks for board state, hash, castling, en-passant,
  side, and counters.
- En-passant legality edge cases adapted from `niklasf/python-chess`.
- SEE regression checks.
- Evaluation symmetry checks on vertically mirrored, colour-swapped positions.
- Transposition-table safety checks.
- Time-manager policy checks covering adaptive scaling, safe clock ceilings, and
  no-increment survival.
- Search regression checks covering terminal positions, draw rules, checked
  qsearch, `searchmoves`, fixed-node limits, and root node-share callbacks.
- Datagen checks covering CLI execution, summaries, completion markers, and
  plain/Bullet record integrity.
- UCI checks covering handshake, fixed-depth search, the mandatory bench node
  signature, opening-book on/off behavior, and deterministic `Threads=1`
  search.
- Timed Lazy SMP and ponder integration checks covering `ponderhit` and `stop`.
- Tactical mate-in-1 regression positions adapted from
  `StuartRiffle/JaglavakTestData`.

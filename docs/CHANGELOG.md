# Changelog

This changelog records SHAYVERI engine releases. Each entry describes changes
since the previous version. NNUE artifact names are recorded separately from
engine versions. See [VERSIONING.md](VERSIONING.md).

### Notes

The current versioning scheme was created with v2.5.0 and strengthened through
v2.6.0 and v2.7.0. Earlier version numbers were assigned to historical engine
snapshots for consistency, though those versions were never actually released.
This distinction concerns versioning and release organization and does not
necessarily reflect the rigour of testing performed on the engine.

Since v2.3.0, all releases were pinned against a broad collection of reputable
engines. These pins include 800 games per pairing at `10+0.1` (STC) and 200
games per pairing at `90+0.5` (LTC). The framework is described in
[`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md).

Prior releases were retroactively pinned at the same time controls and against
the same pool, though with 200 games per pairing at STC and 100 games per
pairing at LTC. The framework is described in
[`scripts/historical_pin/README.md`](../scripts/historical_pin/README.md).

## NNUE Era

### v2.8.0 - DEVELOPMENT BRANCH

**Default network:** embedded `SHAYVERI2_5_0.nnue`.

- Added reproducible profiling baselines covering trusted bench, fixed-depth,
  fixed-node, tactical, timed, and real 1/2/4/8-thread Lazy SMP workloads, with
  machine/build metadata and before-and-after comparison output.
- Combined ordinary quiet-move NNUE parent copying, subtraction, and addition
  into one pass, preserving exact refreshed values while removing redundant
  hot-path traversals.
- Rebuilt only the moved king's NNUE perspective after ordinary quiet king
  moves, updating the unchanged perspective incrementally while retaining full
  refreshes for king captures and castling.
- Fused ordinary-capture NNUE parent copying and all three feature deltas into
  one accumulator pass.
- Reduced AVX2 evaluation widening by accumulating bounded products in safe
  32-bit blocks before converting them to the 64-bit output sum.
- Reduced full NNUE refresh traffic by retaining accumulator blocks in AVX2
  registers across all active features and storing each block once.
- Replaced division-based piece type and colour decoding with equivalent
  comparisons and subtraction in hot board, search, SEE, and NNUE paths.
- Centralized single-move legality probes and first-legal-move lookup, avoiding
  full legal-list construction in UCI parsing, book, ponder, and fallback paths.
- Simplified UCI option, position, ponder, and asynchronous-output plumbing,
  and removed an unreachable legacy Texel parameter-printing implementation.
- Centralized the generated opening-book lookup shared by UCI play and datagen.
- Centralized castling rook/right handling across make and unmake operations.
- Made FEN parsing transactional and rejected malformed rank widths, invalid
  move counters, trailing fields, impossible piece totals, and missing or
  duplicate kings instead of changing the board.
- Consolidated colour-specialized pawn, piece, castling, promotion, and attack
  initialization logic while preserving move order and generated move sets.
- Simplified transposition-table replacement selection and removed unused time
  manager and NNUE state-query APIs.
- Added a profile-guided optimization target trained on the representative
  bench, fixed-depth, fixed-node, tactical, timed, and Lazy SMP workloads.
- Hardened PGO construction with strict missing/mismatched-profile failures,
  separate generation, training, and use stages, a portable `x86-64-v3`
  target, retained build/profile diagnostics, bounded single-thread
  node-normalized training, and timed 1/2/4/8-thread Lazy SMP coverage that
  avoids relying solely on distorted instrumented timing.
- Qualified optional native Linux PGO with repeatable `+2.00%` and `+2.20%`
  geometric timed NPS across bracketed 1/2/4/8-thread comparisons. Normal
  portable release builds remain non-PGO.
- Fixed fixed-node searches counting an unreported preliminary search outside
  the requested node budget.
- Fixed fixed-depth and fixed-node searches blocking UCI command processing,
  and made each asynchronous search worker own its completion output instead
  of relaying results through a second thread.
- Fixed an optimized MinGW access violation before `bestmove` by avoiding an
  unconditional by-value board copy during ponder-move lookup.
- Serialized asynchronous UCI output so search information, command responses,
  and completion messages cannot corrupt one another through byte interleaving.
- Fixed `go movetime` searches being shortened by adaptive clock-management
  scalers instead of using the requested duration minus safety overhead.
- Made NNUE file changes transactional: invalid or unreadable `EvalFile`
  values now report an error and preserve the active evaluator instead of
  terminating the engine or leaving partial network state.
- Hardened UCI boolean and tuning-value parsing so malformed values leave the
  current setting unchanged, and tuning changes now invalidate stale search
  state.
- Rejected negative unsigned and non-finite floating-point datagen arguments
  instead of accepting wrapped counters or `nan`/`inf` configuration values.
- Prevented malformed overpopulated FEN input from overflowing NNUE refresh
  scratch storage.
- Scored `+55.71 +/- 15.95`, `+37.67 +/- 13.56`, and `+32.05 +/- 12.50` Elo
  over v2.7.0 in 1,000-game colour-paired matches at `1+0.01`, `5+0.05`, and
  `10+0.1`, respectively, at one thread and 64 MB hash.

**Validation note:** ThreadSanitizer validation of the asynchronous search and
Lazy SMP paths is still required on a compatible native Linux environment. The
available machines abort inside both GCC and Clang TSan runtimes before
`main()` with an `unexpected memory mapping` error; this is therefore an
outstanding validation requirement rather than a completed engine test.

### v2.7.0 - Search and Time Improvements

**Default network:** embedded `SHAYVERI2_5_0.nnue`.

- Reworked LMR with improving and cut-node awareness, PV and non-PV offsets,
  move-count and history adjustments, and consistent cut-node propagation.
- Added capture-only ProbCut, quiet PVS SEE pruning, expanded qsearch pruning,
  and singular-search multi-cut. Check, negative, double, and triple extensions
  and regressing pruning policies remain disabled after direct testing.
- Retuned aspiration, futility, LMR, qsearch, reverse futility, SEE, null-move,
  and singular-search parameters.
- Added root-node-share and multi-depth evaluation-stability time scaling,
  safe clock ceilings, emergency no-increment behavior, and deadline-aware
  hard-timer sleeps.
- Fixed correctness and lifecycle issues across search, qsearch, repetition
  history, rule handling, transposition-table concurrency, pondering, and UCI
  state changes.
- Delayed NNUE accumulator updates until moves pass legality and pruning,
  reused SEE results from move ordering, precomputed LMR logarithms, and
  removed redundant datagen move generation.
- Bounded active Lazy SMP workers by detected hardware capacity.
- Rebuilt the opening book from qualifying TWIC games using one deterministic
  weighted-majority move per position and no stored evaluator scores.
- Added the UCI option `BookInfoDepth` for SHAYVERI-generated scores on book
  moves, which now respect `searchmoves` and legality.
- Standardized public UCI option names to PascalCase.
- Made `bench` independent of prior search state and report microsecond timing.
- Recorded `3094.7 +/-14.9` at STC and `3235.7 +/-29.8` at LTC, improvements
  of +49.0 STC and +99.2 LTC over v2.6.0 in the same pool.

### v2.6.0 - Atomic Lazy SMP, Embedded NNUE, and Search Refinements

**Default network:** embedded `SHAYVERI2_5_0.nnue`.

- Reworked the shared transposition table into an atomic four-entry bucketed
  table and fixed timed Lazy SMP sharing. Fixed-depth, fixed-node, and datagen
  searches remained deterministic.
- Embedded the default NNUE in the executable while retaining the ability to
  load `.nnue` files passed through the UCI option `EvalFile`.
- Added pawn-structure correction history to adjust static evaluations from
  prior search results. Search histories now persist between moves and reset
  with the hash or evaluation state.
- Reworked null-move pruning with evaluation- and depth-based reductions,
  non-pawn-material gating, and verification at high depths.
- Replaced the fixed LMR table with a live formula and added PV, move-count,
  and quiet-history adjustments.
- Revised sudden-death time allocation with separate increment and no-increment
  horizons, a larger hard bound, and extra time after a root best-move change.
- Moved oversized search histories and stack state off the call stack, and
  stopped PV reconstruction at repetitions and the fifty-move boundary.
- Same-net comparisons against the previous search scored `199-52-149` at one
  thread and `102-3-45` at four threads.
- Recorded `3045.7 +/-14.8` at STC and `3136.5 +/-28.9` at LTC, improvements
  of +93.3 STC and +54.6 LTC over v2.5.0 in the same pool.

### v2.5.0 - KB16x512 Network and Expanded Training

**Default network:** external `SHAYVERI2_5_0.nnue`.

- Added KB16x512 inference and conversion support.
- Promoted `SHAYVERI2_5_0.nnue`, trained on larger and more diverse
  RobotMoon/Stockfish corpora.
- Implemented `go nodes N` limits through UCI and search.
- Fixed Lazy SMP issues discovered after v2.4.0.
- Recorded `2952.4 +/-14.7` at STC and `3081.9 +/-28.4` at LTC, improvements
  of +14.1 STC and +35.7 LTC over v2.4.0 in the same pool.

### v2.4.0 - Datagen Repair and External-Corpus Network

**Default network:** external `SHAYVERI2_2_0.nnue`.

- Reworked datagen with Stockfish-like quiet-position filters and added direct
  32-byte `bulletformat` output.
- Fixed KB8 SCReLU inference and Bullet conversion output.
- The repaired datagen and SHAYVERI-generated corpus failed to produce a
  strong network.
- Experiments with an external RobotMoon corpus, labeled with Stockfish
  evaluations, produced the strong `SHAYVERI2_2_0.nnue` network and improved
  with scale.
- Added the UCI option `UseNNUE` for easy selection between the NNUE and HCE
  evaluation paths.
- Recorded `2938.3 +/-14.9` at STC and `3046.2 +/-28.6` at LTC, improvements
  of +222.1 STC and +253.9 LTC over v2.3.0 in the same pool.

### v2.3.0 - Scaling King Buckets

**Default network:** external `net5_final.nnue`.

- Completed the net9-net11 KB8 continuation experiments and established that
  later continuation checkpoints drifted rather than producing a reliable
  promoted net.
- Fixed classic-versus-bucketed feature indexing and added KB16 inference,
  conversion, and trainer support.
- Ran the net12 KB16/256 experiment. It failed the HCE and net5 anchors and was
  stopped.
- Retained the known-good net5 engine path after the experimental net13 line
  also failed to produce a promotable replacement.
- Recorded `2716.2 +/-16.4` at STC and `2792.3 +/-41.5` at LTC, approximately
  +140.9 STC and +76.1 LTC higher than v2.2.0, though the results are not
  directly comparable due to different Elo-pinning strategies.

### v2.2.0 - King Buckets and Bullet

**Default network:** external `net8_final.nnue`.

- Added KB8 feature indexing and inference support.
- Trained and integrated the first working KB8/256 network, using Bullet
  for training.
- Established that the KB8 pipeline produced legal usable networks, although
  later comparisons showed that this line had not surpassed the HCE anchor.
- Fixed illegal-move behavior occurring with the NNUE evaluation.
- Fixed worker threads retaining references to moves from an earlier search.
- Recorded `2575.3 +/-43.7` at STC and `2716.2 +/-50.8` at LTC in historical
  pins, regressions of -164.7 STC and -123.0 LTC from v2.1.0 in the same pool.

### v2.1.0 - NNUE Self-Data-Loop

**Default network:** external `net5_final.nnue`.

- Trained the classic Chess768/256 net2-net7 sequence from SHAYVERI-generated
  data and kept net5 as the strongest network.
- Changed datagen to use NNUE evaluation where configured and fixed the
  parallel-search thread crash found during generation.
- Net5 used 1B positions, WDL 0.1, 10k-node labels, and mixed HCE/NNUE data.
- Identified arrival-order chunk consumption as the source of noisy,
  non-monotonic checkpoint strength. Net6 and net7 did not replace net5.
- Recorded `2740.0 +/-35.3` at STC and `2839.2 +/-45.8` at LTC in historical
  pins, improvements of +113.4 STC and +111.2 LTC over v2.0.0 in the same pool.

### v2.0.0 - First NNUE Engine

**Default network:** external `first_net.nnue`.

- Added the UCI option `EvalFile`, NNUE loading, scalar and AVX2 inference,
  accumulator refresh and incremental updates, and HCE fallback behavior.
- Added engine datagen for producing NNUE training positions.
- Integrated the first trained Chess768/256 network, trained using SHAYVERI
  data and Marlinflow.
- Recorded `2626.6 +/-41.1` at STC and `2728.0 +/-50.1` at LTC in historical
  pins, a regression of -30.6 STC and an improvement of +25.0 LTC relative to
  v1.3.0 in the same pool.

## HCE Era

### v1.3.0 - Final HCE Refinements

- Completed the final SPSA cycle and promoted the winning parameter set.
- Fixed previously unwired LMR, aspiration, and SEE-pruning parameters and
  retuned the affected search behavior.
- Improved search speed, quiet and capture move ordering, TT static-evaluation
  storage, qsearch TT use, capture history, countermoves, and continuation
  histories.
- Added Lichess-oriented engine support.
- Implemented fixed-depth `go depth N` searches.
- Hardened UCI option parsing with range checks and clamping, handled allocation
  failures, and rejected invalid search inputs safely.
- Recorded `2657.2 +/-39.5` at STC and `2703.0 +/-51.2` at LTC in historical
  pins, improvements of +94.7 STC and +33.7 LTC over v1.2.0 in the same pool.

### v1.2.0 - Tuned Search and Evaluation

- Completed four SPSA passes over the centralized HCE and search parameters.
- Replaced the initial hand-selected defaults with the final SPSA result.
- Adopted Texel-tuned material and PSTs, pawn/evaluation terms, king PSTs,
  king-safety and tactical terms, and a final refinement phase.
- Normalized piece values and PSTs.
- Recorded `2562.5 +/-44.8` at STC and `2669.3 +/-53.5` at LTC in historical
  pins, improvements of +115.0 STC and +92.8 LTC over v1.1.0 in the same pool.

### v1.1.0 - Improved HCE

- Added aspiration windows, PVS, LMP, singular extensions, internal iterative
  reduction, mate-distance pruning, and the completed handcrafted evaluation.
- Added BMI2/PEXT support, Lazy SMP, dynamic time management, `movestogo`,
  ponder, UCI options, PV/mate/nodes/NPS output, and GUI compatibility fixes.
- Rebuilt the compiled opening book with precomputed Stockfish evaluations for
  its positions, reported those stored scores on book moves, and corrected the
  engine-side book behavior.
- Centralized search and evaluation constants in `tune.h` and prepared the
  engine for SPSA.
- Recorded `2447.5 +/-55.0` at STC and `2576.5 +/-60.2` at LTC in historical
  pins, improvements of +131.9 STC and +154.5 LTC over v1.0.0 in the same pool.

### v1.0.0 - First Engine

- Added the C++20 board representation, FEN parser, attack generation, legal
  move generation, make/unmake, perft foundation, search, and the UCI loop.
- Added Zobrist hashing, a transposition table, repetition detection, SEE, and
  check extension support.
- Added the compiled opening book, piece-square tables, bishop-pair evaluation,
  square and move ordering, clock-based time allocation, and UCI compliance.
- Added a tapered HCE covering pawn structure, king safety, mobility,
  coordination, tactical pressure, threats, and outposts.
- Added futility pruning, delta pruning, reverse futility pruning, null-move
  pruning, and LMR.
- Recorded `2315.6 +/-74.1` at STC and `2422.0 +/-79.6` at LTC in historical
  pins.

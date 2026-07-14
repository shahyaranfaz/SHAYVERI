# Changelog

This changelog records SHAYVERI engine releases. Each entry describes changes
since the previous version. NNUE artifact names are recorded separately from
engine versions. See [VERSIONING.md](VERSIONING.md).

### Note

The current versioning scheme was created with v2.5.0 and strengthened through
v2.6.0 and the development of v2.7.0. Earlier version numbers were assigned to
historical engine snapshots for consistency. Those releases remain usable
milestones. Published releases since v2.3.0 include recorded STC and LTC
strength pins. This distinction concerns versioning and release organization
and does not necessarily reflect the rigor of testing or Elo pinning performed
on the engine.

## NNUE Era

### v2.7.0 - Search and Engine Refinement (Unreleased)

**Default network:** embedded `SHAYVERI2_5_0.nnue`.

**Candidate range:** `1013ebb1a5c7f312e418f944fb737b44fc85810b` through
the final validated `search_overhaul` commit. Tag only after SPSA, regression,
and Elo gates.

- Expanded the v2.6.0 tuning surface with configurable improving, cut-node,
  PV/non-PV LMR, singular-search, extension, pruning, and time-management
  controls, and removed the remaining hard-coded search constants.
- Added improving and cut-node-aware LMR and propagated cut-node state through
  PVS, null-move, and singular-search calls. The initial pair scored
  `466-417-717` against the preceding configuration at `3+0.03`.
- Stopped active searches before changing TT, NNUE, thread, or tuning state,
  and bounded active SMP by detected hardware capacity.
- Fixed qsearch at the checked-position depth floor so legal evasions are
  searched instead of returning a normal static evaluation.
- Removed repeated LMR logarithms, reused SEE results, delayed NNUE accumulator
  work until legality was known, and removed redundant datagen move generation.
- Verified the initial audit fixes in a 1,600-game Fastchess regression:
  `454-419-727`, or `+7.6 +/-12.6` Elo versus the preceding binary.
- Added and ablated qsearch SEE/futility pruning, late-history pruning,
  quiet-move PVS SEE pruning, and capture-only ProbCut. Regressing qsearch
  futility and late-history pruning remain disabled.
- Corrected repetition, fifty-move, insufficient-material, mate-at-the-rule
  boundary, maximum-ply, excluded-move, and null-move stack handling.
- Serialized whole TT-bucket writes so concurrent probes cannot accept a
  mixed snapshot, and preserved the newest available repetition history.
- Disabled check extensions after direct testing.
- Reworked time management with root best-move node-share scaling,
  multi-iteration evaluation-stability scaling, enforced soft/hard/minimum
  limits, low-clock survival, and corrected ponder-hit initialization.
- Extended singular-search handling and promoted multi-cut pruning. Negative,
  double, and triple extensions remain disabled after testing.
- Replaced the legacy book, whose compiled entries carried precomputed
  Stockfish evaluations, with a book rebuilt from qualifying TWIC games. The
  generator now selects one deterministic weighted-majority move per position
  and stores moves without evaluator-specific scores.
- Added `Book_Info_Depth`. A positive value searches the forced book move to
  emit normal UCI information using SHAYVERI's own evaluation instead of the
  old stored Stockfish score. A value of `0` keeps the immediate book response.
- Made `bench` start from clean TT, history, stop, and node-count state and
  report short-run timing at microsecond resolution.
- Promoted the first SPSA batch's search-parameter values. Final validation and
  the release tag remain pending.

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
- Recorded `3045.7 +/-14.8` STC and `3136.5 +/-28.9` LTC, improvements of
  +93.3 STC and +54.6 LTC over v2.5.0 in the same pool.

### v2.5.0 - KB16x512 Network and Expanded Training

**Default network:** external `SHAYVERI2_5_0.nnue`.

- Added KB16x512 inference and conversion support.
- Promoted `SHAYVERI2_5_0.nnue`, trained on larger and more diverse
  RobotMoon/Stockfish corpora.
- Implemented `go nodes N` limits through UCI and search.
- Fixed Lazy SMP issues discovered after v2.4.0.
- Recorded `2952.4 +/-14.7` STC and `3081.9 +/-28.4` LTC, improvements of
  +14.1 STC and +35.7 LTC over the previous promoted NNUE in the same pool.

### v2.4.0 - Datagen Repair and External-Corpus Network

**Default network:** external `SHAYVERI2_2_0.nnue`.

- Reworked datagen with Stockfish-like quiet-position filters and added direct
  32-byte `bulletformat` output.
- Fixed KB8 SCReLU inference and Bullet conversion output.
- The repaired datagen and SHAYVERI-generated corpus failed to produce a
  strong network.
- Experiments with an external RobotMoon corpus labeled with Stockfish
  evaluations produced the strong `SHAYVERI2_2_0.nnue` network, which improved
  as the corpus scaled from 250M to 500M to 1B positions.
- Added the UCI option `UseNNUE` for easy selection between the NNUE and HCE
  evaluation paths.
- Recorded `2938.3 +/-14.9` STC and `3046.2 +/-28.6` LTC, improvements of
  +222.1 STC and +253.9 LTC over the previous promoted NNUE in the same pool.

### v2.3.0 - KB8 Continuation and KB16 Attempt

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
- Recorded net5 at `2716.2 +/-16.4` STC and `2792.3 +/-41.5` LTC, improvements
  of +63.5 STC and +57.8 LTC over HCE in the same pool.

### v2.2.0 - King Buckets and Bullet

**Default network:** external `net8_final.nnue`.

- Added KB8 feature indexing and inference support.
- Trained and integrated the first working KB8/256 network, using Bullet
  for training.
- Established that the KB8 pipeline produced legal usable networks, although
  later comparisons showed that this line had not surpassed the HCE anchor.
- Fixed illegal-move behavior occurring with the NNUE evaluation.
- Fixed worker threads retaining references to moves from an earlier search.

### v2.1.0 - Classic NNUE Iteration

**Default network:** external `net5_final.nnue`.

- Trained the classic Chess768/256 net2-net7 sequence from SHAYVERI-generated
  data and kept net5 as the strongest network.
- Changed datagen to use NNUE evaluation where configured and fixed the
  parallel-search thread crash found during generation.
- Net5 used 1B positions, WDL 0.1, 10k-node labels, and mixed HCE/NNUE data.
- Identified arrival-order chunk consumption as the source of noisy,
  non-monotonic checkpoint strength. Net6 and net7 did not replace net5.
- Direct matches measured `+236.8 +/-24.5` STC and `+220.1 +/-42.5` LTC over
  HCE.

### v2.0.0 - First NNUE Engine

**Default network:** external `first_net.nnue`.

- Added the UCI option `EvalFile`, NNUE loading, scalar and AVX2 inference,
  accumulator refresh and incremental updates, and HCE fallback behavior.
- Integrated the first trained network after correcting conversion scaling and
  NNUE runtime issues.
- Added engine datagen for producing NNUE training positions.

## HCE Era

### v1.3.0 - Final HCE Search

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

### v1.2.0 - Texel-Tuned HCE

- Adopted Texel-tuned material and PSTs, pawn/evaluation terms, king PSTs,
  king-safety and tactical terms, and a final refinement phase.
- Normalized piece values and PSTs.

### v1.1.0 - SPSA-Tuned HCE

- Completed four SPSA passes over the centralized HCE and search parameters.
- Replaced the initial hand-selected defaults with the final SPSA result.

### v1.0.0 - Complete Handcrafted Engine

- Added aspiration windows, PVS, LMP, singular extensions, internal iterative
  reduction, mate-distance pruning, and the completed handcrafted evaluation.
- Added BMI2/PEXT support, Lazy SMP, dynamic time management, `movestogo`,
  ponder, UCI options, PV/mate/nodes/NPS output, and GUI compatibility fixes.
- Rebuilt the compiled opening book with precomputed Stockfish evaluations for
  its positions, reported those stored scores on book moves, and corrected the
  engine-side book behavior.
- Centralized search and evaluation constants in `tune.h` and prepared the
  engine for SPSA.

## HCE Development Era

### v0.3.0 - Evaluation and Pruning

- Replaced the early evaluation with a tapered HCE covering pawn structure,
  king safety, mobility, coordination, tactical pressure, threats, and
  outposts.
- Added futility pruning, delta pruning, reverse futility pruning, null-move
  pruning, and LMR.
- Fixed evaluation parity and early search/GUI correctness issues.

### v0.2.0 - Search and Evaluation Infrastructure

- Added Zobrist hashing, a transposition table, repetition detection, SEE, and
  check extension support.
- Added the compiled opening book, piece-square tables, bishop-pair evaluation,
  square and move ordering, and improved UCI compliance.

### v0.1.0 - First Usable Engine

- Added the C++20 board representation, FEN parser, attack generation, legal
  move generation, make/unmake, perft foundation, basic HCE, search, UCI loop,
  and a runnable UCI engine.
- This was the first working but low-strength SHAYVERI snapshot.

## Future Era

### v3.0.0 - Undetermined

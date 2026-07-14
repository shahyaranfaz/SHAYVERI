# Changelog

This changelog records SHAYVERI engine releases. Each entry describes changes
since the previous version. NNUE artifact names are recorded separately from
engine versions. See [VERSIONING.md](VERSIONING.md).

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

### v2.6.0 - Atomic Lazy SMP, Embedded NNUE, and Search Tuning

**Default network:** embedded `SHAYVERI2_5_0.nnue`.

- Reworked the shared transposition table into an atomic four-entry bucketed
  table and fixed timed Lazy SMP sharing. Fixed-depth, fixed-node, and datagen
  searches remained deterministic.
- Embedded the default NNUE in the executable while retaining the ability to
  load `.nnue` files passed through the UCI option `EvalFile`.
- Added the NNUE search-tuning foundation: live LMR, null-move, qsearch, and
  static-evaluation correction-history controls.
- Reduced oversized per-call search-stack allocations.
- Same-net comparisons against the previous search scored `199-52-149` at one
  thread and `102-3-45` at four threads. The recorded pins were
  `3045.7 +/-14.8` STC and `3136.5 +/-28.9` LTC.

### v2.5.0 - KB16x512 External-Corpus Network

**Default network:** external `SHAYVERI2_5_0.nnue`.

- Added KB16x512 inference and conversion support.
- Promoted `SHAYVERI2_5_0.nnue`, trained on larger and more diverse
  RobotMoon/Stockfish corpora.
- Recorded `2952.4 +/-14.7` STC and `3081.9 +/-28.4` LTC, improvements of
  +14.1 STC and +35.7 LTC over the previous promoted NNUE in the same pool.

### v2.4.1 - Node Limits and Lazy SMP Fixes

**Default network:** external `SHAYVERI2_2_0.nnue`.

- Correctly wired `go nodes N` through UCI and search.
- Fixed Lazy SMP behavior discovered after v2.4.0.

### v2.4.0 - Datagen Repair and Promoted NNUE

**Default network:** external `SHAYVERI2_2_0.nnue`.

- Reworked datagen with Stockfish-like quiet-position filters and added direct
  32-byte `bulletformat` output.
- Fixed KB8 SCReLU inference and the generated Bullet-network artifacts.
- The repaired datagen and SHAYVERI-generated corpus failed to produce a
  strong network.
- Experiments with an external RobotMoon corpus labeled with Stockfish
  evaluations produced a strong network.
- Scaling this corpus from 250M to 500M to 1B positions continued to improve
  strength.
- Added the UCI option `UseNNUE` for easy selection between the NNUE and HCE
  evaluation paths.
- Promoted `SHAYVERI2_2_0.nnue` and recorded `2938.3 +/-14.9` STC and
  `3046.2 +/-28.6` LTC.

### v2.3.0 - Architecture and Training Reset

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

### v2.2.1 - NNUE Legality and Threading Fixes

**Default network:** external `net8_final.nnue`.

- Fixed illegal-move behavior in the NNUE-era engine.
- Fixed worker threads retaining references to moves from an earlier search.

### v2.2.0 - KB8 and Bullet

**Default network:** external `net8_final.nnue`.

- Added KB8 feature indexing and inference support.
- Trained and integrated the first working KB8/256 network.
- Established that the KB8 pipeline produced legal usable networks, although
  later external gates showed that this line had not surpassed the HCE anchor.

### v2.1.0 - Classic NNUE Iteration

**Default network:** external `net5_final.nnue`.

- Trained the classic Chess768/256 net2-net7 sequence from SHAYVERI-generated
  data and kept net5 as the strongest network.
- Changed datagen to use NNUE evaluation where configured and fixed the
  parallel-search thread crash found during generation.
- Net5 used 1B positions, WDL 0.1, 10k-node labels, and mixed HCE/NNUE data.
  It beat HCE by `+236.8 +/-24.5` in the recorded STC direct match and
  `+220.1 +/-42.5` at LTC.
- Identified arrival-order chunk consumption as the source of noisy,
  non-monotonic checkpoint strength. Net6 and net7 did not replace net5.

### v2.0.0 - First NNUE Engine

**Default network:** external `first_net.nnue`.

- Added engine datagen for producing NNUE training positions.
- Added the UCI option `EvalFile`, NNUE loading, scalar and AVX2 inference,
  accumulator refresh and incremental updates, and HCE fallback behavior.
- Integrated the first trained network after correcting conversion scaling and
  NNUE runtime issues.

## HCE Refinement Era

### v1.3.1 - UCI and Input-Safety Fixes

- Correctly implemented fixed-depth `go depth N` searches.
- Hardened UCI option parsing with range checks and clamping, handled allocation
  failures, and rejected invalid search inputs safely.
- Cleaned the exposed tuning registry without changing the promoted HCE.

### v1.3.0 - Final HCE Search

- Completed the final SPSA cycle and promoted the winning parameter set.
- Fixed previously unwired LMR, aspiration, and SEE-pruning parameters and
  retuned the affected search behavior.
- Improved search speed, quiet and capture move ordering, TT static-evaluation
  storage, qsearch TT use, capture history, countermoves, and continuation
  histories.
- Added Lichess-oriented engine support.

### v1.2.0 - Texel-Tuned HCE

- Adopted Texel-tuned material and PSTs, pawn/evaluation terms, king PSTs,
  king-safety and tactical terms, and a final refinement phase.
- Normalized piece values and PSTs and prepared the resulting HCE for its final
  SPSA search pass.

### v1.1.0 - Multi-Pass SPSA

- Completed the second through fourth SPSA passes after discarding and
  restarting a broken second pass.
- Froze the SPSA result as the input to the subsequent Texel evaluation work.

### v1.0.0 - First HCE Tuning Pass

- Completed the first full SPSA pass over the centralized HCE and search
  parameter surface.
- Replaced the initial hand-selected defaults with the first measured tuning
  result.
- Renamed the engine and project from ShayBot to SHAYVERI during this tuning
  line.

## Engine Development Era

### v0.4.0 - Complete Handcrafted Engine

- Added aspiration windows, PVS, LMP, singular extensions, internal iterative
  reduction, mate-distance pruning, and the completed handcrafted evaluation.
- Added BMI2/PEXT support, Lazy SMP, dynamic time management, `movestogo`,
  ponder, UCI options, PV/mate/nodes/NPS output, and GUI compatibility fixes.
- Rebuilt the compiled opening book with precomputed Stockfish evaluations for
  its positions, reported those stored scores on book moves, and corrected the
  engine-side book behavior.
- Centralized search and evaluation constants in `tune.h` and prepared the
  engine for SPSA.

### v0.3.0 - Evaluation and Pruning

- Replaced the early evaluation with a tapered HCE covering pawn structure,
  king safety, mobility, coordination, tactical pressure, threats, and
  outposts.
- Added futility pruning, delta pruning, reverse futility pruning, null-move
  pruning, and LMR.
- Fixed evaluation parity and early search/GUI correctness issues.

### v0.2.0 - Search Infrastructure

- Added the compiled opening book, piece-square tables, bishop-pair evaluation,
  square and move ordering, and improved UCI compliance.
- Added Zobrist hashing, a transposition table, repetition detection, SEE, and
  check extension support.

### v0.1.0 - First Usable Engine

- Added the C++20 board representation, FEN parser, attack generation, legal
  move generation, make/unmake, perft foundation, basic HCE, search, UCI loop,
  and a runnable UCI engine.
- This was the first working but deliberately low-strength SHAYVERI snapshot.

## Future Era

### v3.0.0 - Undetermined

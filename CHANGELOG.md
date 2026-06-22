# Changelog

## v1.0 Classical Baseline

Version 1.0 is the final handcrafted-evaluation release before NNUE development.

### Engine Strength

- Completed the final classical tuning cycle.
- Froze the `spsa_f3` parameter set as the strongest v1.0 line.
- Validated the final engine through large self-play and Stockfish reference matches.
- Repaired the final search tuning after discovering some tuned search parameters were not actually wired into search behavior.
- Retuned search after activating the missing LMR, aspiration, and SEE-pruning parameters.

### Search

- Added and tuned aspiration windows, PVS, LMR, LMP, null move pruning, reverse futility pruning, futility pruning, delta pruning, SEE pruning, singular extensions, internal iterative reduction, and mate-distance pruning.
- Added qsearch TT probing.
- Added capture history for capture ordering.
- Added countermove ordering.
- Added continuation-style history through counter-move and follow-up history tables.
- Improved move ordering with TT moves, SEE/MVV-LVA, killers, quiet history, capture history, countermoves, and continuation histories.
- Added a 4-entry bucketed transposition table with generation aging.
- Added static-eval caching in TT entries.
- Added Lazy SMP and dynamic time management.
- Added support for `movestogo` in time management.

### Evaluation

- Completed a large handcrafted evaluation build-out.
- Added tapered midgame/endgame evaluation.
- Added Texel-tuned material values and piece-square tables.
- Added bishop pair, tempo, and development/initiative terms.
- Added detailed pawn structure evaluation:
  - passed pawns;
  - candidate passers;
  - connected and outside passers;
  - isolated, doubled, backward, weak, and supported pawns;
  - pawn islands;
  - pawn storms.
- Added king safety:
  - pawn shield;
  - open/semi-open files near the king;
  - king-zone attack pressure;
  - escape squares;
  - nonlinear danger scaling.
- Added piece activity terms:
  - mobility;
  - seventh-rank bonuses;
  - file/diagonal openness scaling.
- Added coordination terms:
  - defended pieces;
  - shared targets;
  - rook/queen and bishop/queen batteries;
  - support chains.
- Added tactical pressure terms:
  - undefended-piece pressure;
  - pins and x-ray pressure;
  - overloaded defenders;
  - unreciprocated pressure.
- Added explicit threat evaluation for pawn, minor-piece, rook, and hanging-piece threats.
- Added knight, bishop, rook, and queen outpost terms.
- Added a pawn hash.
- Optimized pawn attack generation and pawn-structure helpers.

### Tuning

- Added SPSA/CMA-ES tuning infrastructure.
- Centralized engine parameters in `tune.h`.
- Ran multiple SPSA passes over search and evaluation parameters.
- Added Texel tuning support and scripts.
- Generated and used a large quiet-filtered position set for Texel tuning.
- Ran phased Texel tuning:
  - PST/material phase;
  - pawn/eval phase;
  - king PST phase;
  - king safety/tactical phase;
  - final refinement phase.
- Normalized piece values and PSTs after Texel.
- Archived tuning logs and result notes under `tuning/`.
- Disabled the tuning registry for release UCI output.

### Opening Book

- Added compiled opening book support.
- Added opening book generation/writer tooling.
- Added precomputed book evaluations.
- Added `OwnBook` UCI option.

### UCI And Compatibility

- Implemented core UCI commands:
  - `uci`;
  - `isready`;
  - `ucinewgame`;
  - `position`;
  - `go`;
  - `stop`;
  - `ponderhit`;
  - `quit`;
  - `bench`.
- Added UCI options:
  - `Hash`;
  - `Clear Hash`;
  - `Threads`;
  - `Ponder`;
  - `OwnBook`;
  - `Minimum Thinking Time`;
  - `Move Overhead`.
- Added `searchmoves` support.
- Added PV output, mate score output, nodes, NPS, and time reporting.
- Added ponder move output.
- Improved GUI/cutechess compatibility.
- Added Lichess-oriented support work.
- Removed Chess960 from the v1.0 target after testing showed it needed a deeper castling/legal-move pass.

### Infrastructure

- Added the C++ engine core:
  - board representation;
  - FEN parsing;
  - move generation;
  - make/unmake;
  - perft-oriented foundations.
- Added Zobrist hashing.
- Added transposition tables.
- Added repetition detection.
- Added SEE.
- Added BMI2/PEXT bitboard support.
- Added Linux and Windows build targets.
- Added macOS Makefile target.
- Fixed Makefile header dependencies so header-only changes trigger rebuilds.
- Added bench/smoke validation flow.

## v2.0 NNUE Development Line

Version 2.0 covers the first SHAYVERI NNUE development line before the later
external-corpus bootstrap work: classic Chess768/256 Marlinflow nets, KB8/KB16
Bullet experiments, data scheduling fixes, and the final diagnosis that the
self-generated corpus/labels were the blocker.

### Engine And Tooling

- Added NNUE evaluation support.
- Added `EvalFile` loading through UCI.
- Kept handcrafted evaluation available as the no-net/debug path.
- Added SHAYVERI `.nnue` conversion support for Marlinflow and Bullet artifacts.
- Built multi-worker generation/training scripts, checkpoint conversion, and
  cutechess gate scripts.
- Added the Bullet king-bucket trainer/conversion path for architecture
  experiments.

### v2.1: Classic Chess768 / 256 Progress

- Built the first working Chess768 NNUE bootstrap from SHAYVERI-generated data.
- Net5 was the best classic result: Chess768 / 256, Marlinflow, 1B generated
  positions, WDL=0.1, 10k-node labels, and mixed HCE/NNUE generation.
- Lesson: mixed HCE/NNUE data worked, but arrival-order chunk consumption made
  checkpoint strength noisy and non-monotonic.

### v2.2: King Buckets And Bullet

- Switched from continuing Chess768 to KB8/Bullet architecture tests.
- Proved the KB8/Bullet pipeline was legal and viable, but later runs were
  hurt by disk/queue/truncated chunk issues.
- Found that KB16 and bucket-only scaling did not produce more strength by
  themselves.

### v2.3: Deep-Hard Reset

- Tried to repair the datagen signal with deeper relabeling instead of just
  adding more positions.
- Designed a Chess768 / 256 recovery path around the v2.1 line with deeper
  SHAYVERI self-distillation and hard-position relabeling.
- Added a branch matrix over net5/HCE baseline and deep-hard lanes.
- Reinforced that HCE/net5/SF2850 gates need to happen early, before internal
  pool optimism wastes training time.

### v2.4: Persistent SHAYVERI Corpus

- Built and preserved about 1.7B SHAYVERI-generated positions, mostly 5k-node
  data plus about 130M earlier 10k-node positions.
- Trained persistent 500M, 1B, and fullmove-8-plus filtered Board768 / 256
  views through the patched Marlinflow path.
- Best observed HCE transfer from this corpus was only around score 0.25 vs HCE,
  roughly -190 Elo; the fullmove-8-plus slice was worse.
- Concluded failure was not tiny data volume, streaming churn, or checkpoint
  selection; the blocker was generated data distribution and/or labels.

## v3.0 Datagen And KB16 Self-Loop Foundation

Version 3.0 starts the serious self-looping NNUE line: train a KB16 network
from stronger SHAYVERI-generated data and fixes the datagen problems that capped
the v2.x self-generated corpora.

### Datagen

- Reworked datagen toward training-ready NNUE data instead of loosely filtered
  legacy text output.
- Added TWIC-book rooted starts as the baseline opening distribution.
- Added qsearch/static-eval labeling so training targets are quieter and not
  polluted by raw search noise.
- Added cp3000-compatible filtering before data is written.
- Added explicit handling for mate-like and tablebase-like scores so sentinel
  values do not leak into training.
- Added filtering for illegal, malformed, duplicate-terminal, impossible, in-check,
  and noisy tactical states when the recipe expects quiet positions.
- Added deterministic seeds, resumable shard output, `.incomplete` / `DONE`
  markers, and per-shard summaries.
- Added per-shard stats for positions written, filtered counts by reason, cp
  buckets, WDL buckets, side-to-move, phase, source concentration, positions per
  game, max positions from one game, TWIC-start percentage, and duplicate rate
  when duplicate tracking is enabled.
- Added generation controls such as `--positions`, `--games`, and
  `--print-interval`.

### Training

- Moved the main NNUE target to KB16.
- Uses stronger datagen output as the basis for the next promoted net.
- Keeps HCE as a flavor/debugging reference without letting it dominate
  training labels.
- Uses fixed external anchors for promotion instead of trusting internal-family
  pools alone.

## Future Work

- Direct Bullet-format datagen output or a fast lossless Bullet conversion path.
- Scale architecture after the self-loop is stable.
- Syzygy tablebases and tablebase policy for NNUE.

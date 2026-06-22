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

Version 2.1 covers the first SHAYVERI NNUE development line before the later
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
- Net5 iter064 beat iter037 by +30.9 +/- 17.0 Elo at 1+0.1.
- Direct HCE gates: +236.8 +/- 24.5 at 10+0.1 and +220.1 +/- 42.5 at 90+0.5.
- HCE-anchored estimates: 2889.5 +/- 27.1 STC and 2954.6 +/- 47.1 LTC.
- Lesson: mixed HCE/NNUE data worked, but arrival-order chunk consumption made
  checkpoint strength noisy and non-monotonic.

### v2.2: King Buckets And Bullet

- Switched from continuing Chess768 to KB8/Bullet architecture tests.
- v1.2 net1 used KB8 / 256, 80/20 HCE/NNUE data, WDL=0.1, and 10k-node labels.
- `net1_019` roughly matched net5: 418 - 389 - 193, score 0.514 over 1000
  games, +10.1 +/- 19.3 Elo.
- Proved the KB8/Bullet pipeline was legal and viable, but later runs were
  hurt by disk/queue/truncated chunk issues.
- v1.2 net2 fixed chunk sizing and validation but did not improve over v1.2
  net1.
- KB16 and bucket-only scaling did not produce anchored strength by themselves.

### v2.3: Deep-Hard Reset

- Diagnosed that 10k-node labels were too weak, continuation training overwrote
  good checkpoints, internal family pools misled, and data distribution was too
  narrow.
- Designed a Chess768 / 256 recovery path around `net5_final.nnue` with deeper
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

### Takeaways

- SHAYVERI NNUE runtime, conversion, and training became operational.
- Net5 was the best pre-v2.0 self-generated-data net.
- Architecture changes alone did not fix the label/data problem.
- More generated volume alone did not fix transfer.
- The release points directly to the next datagen overhaul: better filters,
  qsearch labels, TWIC-rooted starts, summary stats, DONE markers, and faster
  direct training dataflow.

## v3.0 Datagen And KB16 Self-Loop Foundation

Version 3.0 is the first serious self-looping NNUE line. The goal is to train a
KB16 network from stronger SHAYVERI-generated data, while fixing the datagen
issues that limited the v2.x self-generated corpora.

### Datagen

- Reworked datagen around training-ready NNUE output instead of loosely filtered
  legacy text.
- Added TWIC-book rooted starts as the baseline opening distribution.
- Added qsearch/static-eval labeling so training targets are quieter and less
  polluted by raw search noise.
- Added cp3000-compatible filtering before data is written.
- Added explicit handling for mate-like and tablebase-like scores so sentinel
  values do not leak into training.
- Added filtering for illegal, malformed, duplicate-terminal, impossible, and
  check/noisy tactical states when the target recipe expects quiet positions.
- Added deterministic seeds, resumable shard output, `.incomplete` / `DONE`
  markers, and per-shard summaries.
- Added per-shard stats for positions written, filtered counts by reason, cp
  buckets, WDL buckets, side-to-move, phase, source concentration, positions per
  game, max positions from one game, TWIC-start percentage, and duplicate rate
  when duplicate tracking is enabled.
- Added generation controls such as `--positions`, `--games`, and
  `--print-interval`.

### Training

- Moves the main NNUE target to KB16.
- Uses the stronger datagen output as the basis for the next promoted net.
- Keeps HCE as a flavor/debugging reference without letting it dominate the
  training labels.
- Uses fixed external anchors for promotion instead of trusting internal family
  pools alone.

### Takeaways

- v3.0 is a datagen repair plus the KB16 training line.
- The main question is whether cleaner SHAYVERI-generated data can finally make
  self-looping stronger.

## Future Work

- Direct Bullet-format datagen output or a fast lossless Bullet conversion path.
- Scale architecture after the self-loop is stable.
- Syzygy tablebases and tablebase policy for NNUE.

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

## Pre-v1 Development

- Started with early Python/prototype work around opening books and ML evaluation ideas.
- Restarted the project around a C++ engine core.
- Iteratively built the engine from a weak but functional UCI engine into the tuned v1.0 classical baseline.

## Planned v2.0

- Add NNUE evaluation.
- Keep handcrafted evaluation as fallback/debug eval.
- Add `EvalFile`/NNUE loading support.
- Retune search after NNUE is stable.
- Consider Syzygy tablebase support after NNUE.
- Add a practical regression test suite:
  - perft;
  - make/unmake/hash consistency;
  - UCI smoke tests;
  - fixed-position bench checks.

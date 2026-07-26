# Opening-book generator

## Purpose

Build SHAYVERI's embedded opening book from strong human games instead of
maintaining moves by hand.

## Motivation

The book should provide deterministic, broadly established openings without
embedding evaluator scores or hand-picked engine preferences. Regenerating it
from a documented corpus keeps the process repeatable.

## Method

1. Download The Week in Chess PGNs.
2. Keep standard games where both players are rated at least 2600.
3. Record up to 30 plies from each accepted game.
4. Aggregate observed continuations for every reached position.
5. Keep positions with at least five cumulative plays and select the most
   frequent legal move.
6. Generate SHAYVERI-compatible Zobrist keys and write the sorted table to
   `src/opening_book.cpp`.

The generated book stores one move per position. Ties retain the pipeline's
deterministic lexicographic ordering.

## Usage

From the repository root:

```bash
make -C scripts/opening_book run_all
```

The default run reuses `outputs/twic.pgn` when present. To fetch newly
available TWIC issues before rebuilding:

```bash
make -C scripts/opening_book update
make -C scripts/opening_book table keys book
```

Python dependencies include `python-chess`. Fetching also requires `wget` and
`unzip`. Generated corpus, table, key, and helper files stay under `outputs/`.

`include/opening_book.h` is the small, handwritten API declaration used by the
engine. Do not edit the generated `src/opening_book.cpp` manually.

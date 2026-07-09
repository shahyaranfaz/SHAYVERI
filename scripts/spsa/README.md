SHAYVERI SPSA/CMAES scripts
===========================

Purpose
-------

Distributed search-parameter tuning helpers for SHAYVERI. The scripts wrap the
local Lakas/nevergrad CMAES driver and fastchess so one master can queue tuning
jobs while one or more workers consume them.

Layout
------

- `common.sh`: shared defaults, pass selection, batch parameter sets.
- `master.sh`: queues CMAES eval jobs for one pass/batch.
- `worker.sh`: claims eval jobs and runs fastchess locally.
- `watcher.sh`: prints queue and artifact status.
- `build_book.sh`: helper for constructing a mixed tuning book.
- `lakas/`: Python tuning and inspection helpers.
- `output/`: ignored generated output, organized by pass and batch.

Output shape
------------

Generated files live under:

```text
scripts/spsa/output/<PASS_NAME>/
  results.txt
  batch0/
  batch1/
  batch2/
  ...
  <RUN_NAME>/shared/.lakas_dist/
```

`results.txt` is the pass-level tracker. Batch directories hold CMAES artifacts
such as `batch4.dat`, `batch4.log`, and `batch4.html`.

Defaults
--------

Defaults are intentionally centralized in `common.sh`.

- `PASS_NAME=pass_v2_6`
- `RUN_NAME=pass2_pruning_stc`
- `BATCH_ID=4`
- `BATCH_NAME=batch${BATCH_ID}`
- `BUDGET=400`
- `MASTER_JOBS=6`
- `WORKER_JOBS=23`
- `JOB_CLAIMS=1`
- `BASE_TIME_SEC=3`
- `INC_TIME_SEC=0.03`
- `GAMES_PER_BUDGET=300`
- `OPENING_FILE=../books/final_search_mix_shuf.epd`
- `FASTCHESS=$HOME/chess_arena/fastchess/fastchess`

Usage
-----

From repo root or from a trainer-box directory containing the copied scripts:

```bash
bash scripts/spsa/master.sh
bash scripts/spsa/worker.sh
bash scripts/spsa/watcher.sh
```

Typical current continuation:

```bash
BATCH_ID=4 BUDGET=800 bash scripts/spsa/master.sh
```

Worker defaults are usually enough:

```bash
bash scripts/spsa/worker.sh
```

Override examples:

```bash
BATCH_ID=2 BUDGET=500 bash scripts/spsa/master.sh
WORKER_JOBS=16 JOB_CLAIMS=1 bash scripts/spsa/worker.sh
OPENING_FILE=../books/other.epd bash scripts/spsa/master.sh
```

Worker semantics
----------------

- `MASTER_JOBS` controls outstanding CMAES eval jobs.
- `WORKER_JOBS` controls fastchess concurrency on that machine.
- `JOB_CLAIMS` controls how many CMAES eval jobs one worker process claims at
  once.
- The default is one claimed eval per worker process, with that eval using local
  cores through fastchess.

Runtime copy layout
-------------------

In the repository, Python helpers live in `scripts/spsa/lakas/`. On a trainer
box, `lakas_nnue.py` may instead be copied side-by-side with `master.sh`,
`worker.sh`, and `common.sh`; the scripts check for that flat layout first.
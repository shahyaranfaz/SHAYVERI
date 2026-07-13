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
- `outputs/`: ignored generated output, organized by pass and batch.

Output shape
------------

Generated files live under:

```text
scripts/spsa/outputs/<PASS_NAME>/
  results.txt
  batch1/
  batch2/
  ...
  batch5/
  shared/.lakas_dist/
```

`results.txt` is the pass-level tracker. Batch directories hold CMAES artifacts
such as `batch4.dat`, `batch4.log`, and `batch4.html`.

Defaults
--------

Defaults are intentionally centralized in `common.sh`.

- `PASS_NAME=pass_v2_7`
- `BATCH_ID=1`
- `BATCH_NAME=batch${BATCH_ID}`
- `BUDGET=400` total evaluated CMAES candidates for the batch
- `MASTER_JOBS=6`
- `WORKER_JOBS=23`
- `JOB_CLAIMS=1`
- `GAMES_PER_BUDGET=300`
- `OPENING_FILE=../books/final_search_mix_shuf.epd`
- `FASTCHESS=$HOME/chess_arena/fastchess/fastchess`
- `SPSA_ENGINE_OPTIONS=option.OwnBook=false option.Book_Info_Depth=0`

SPSA matches disable the embedded book and book-info search so tuning measures
the configured search parameters rather than opening-book behavior or book-info
overhead.

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
PASS_NAME=pass_v2_7 BATCH_ID=1 BUDGET=400 bash scripts/spsa/master.sh
```

`BUDGET` is a total target, not an additive per-invocation count. If a resumed
`batch1.dat` already contains 400 evaluated candidates, `BUDGET=800` schedules
roughly 400 more candidates and then stops.

Batch controls
--------------

The batch time controls match the comments in `include/tune.h`:

| Batch | Time control | Games per candidate |
| --- | --- | ---: |
| 1 | `5+0.05` | 300 |
| 2 | `10+0.1` | 300 |
| 3 | `5+0.05` | 300 |
| 4 | `5+0.05` | 300 |
| 5 | `10+0.1` | 300 |

`common.sh` selects the time control from `BATCH_ID`. `BASE_TIME_SEC` and
`INC_TIME_SEC` can still be overridden explicitly for diagnostics.

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

Inspecting a run
----------------

Use the JSONL viewer for measured match outcomes and distribution summaries:

```bash
python3 scripts/spsa/lakas/view_cmaes_log.py \
  scripts/spsa/outputs/pass_v2_7/batch1/batch1.log
```

It excludes the synthetic initialization row and failed-job penalties by
default. Its lowest-loss and top-percent sections are selection-biased. The
reported aggregates are unevaluated candidates for direct A/B validation, not
automatic freeze values.

Use the checkpoint viewer for the live optimizer state and recommendation:

```bash
python3 scripts/spsa/lakas/view_cmaes_checkpoint.py \
  scripts/spsa/outputs/pass_v2_7/batch1/batch1.dat --show-best
```

The checkpoint viewer reports optimizer counters, normalized CMA step size,
the covariance condition number, actual CMA stop conditions, and the decoded
CMA distribution center. Validate that center first. Nevergrad's generic CMA
recommendation is the lowest observed single-match result, so `--show-best`
prints it only as a winner's-curse diagnostic.

Runtime copy layout
-------------------

In the repository, Python helpers live in `scripts/spsa/lakas/`. On a trainer
box, `lakas_nnue.py` may instead be copied side-by-side with `master.sh`,
`worker.sh`, and `common.sh`. The scripts check for that flat layout first.

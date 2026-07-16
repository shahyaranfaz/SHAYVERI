# Search-parameter tuning

## Purpose

Tune SHAYVERI search parameters with distributed CMAES evaluations and preserve
enough optimizer state to inspect, resume, and validate each batch.

## Motivation

Search parameters interact and individual game matches are noisy. CMAES can
explore related parameters jointly, while direct candidate-versus-baseline
games prevent noisy optimizer samples from becoming production defaults.

## Method

The master asks Nevergrad for candidates and writes evaluation jobs to shared
storage. Long-lived workers claim jobs and use fastchess to compare each
candidate with the batch baseline. Completed results update the saved CMA
distribution.

The master records the time control, games per evaluation, opening file, and
engine options in each job. Changing only batch definitions does not require
restarting updated workers.

Optimizer output is never promoted directly:

1. Inspect the decoded CMA distribution center.
2. Treat xbest and log aggregates as selection-biased diagnostics.
3. Validate worthwhile candidates in fresh direct games.
4. Promote only convincing improvements and leave rejected defaults unchanged.

## Usage

From the repository root or a trainer directory containing the copied scripts:

```bash
PASS_NAME=pass_v2_7 \
BATCH_ID=1 \
BUDGET=400 \
bash scripts/spsa/master.sh
```

Start workers and monitor the run:

```bash
bash scripts/spsa/worker.sh
bash scripts/spsa/watcher.sh
```

`BUDGET` is the total target, not an additional count. Restarting a master with
a larger budget resumes from the saved checkpoint and schedules only the
remainder.

## Inspecting results

Inspect measured outcomes and aggregate diagnostics:

```bash
python3 scripts/spsa/lakas/view_cmaes_log.py \
  scripts/spsa/outputs/pass_v2_7/batch1/batch1.log
```

Inspect the optimizer checkpoint and decoded center:

```bash
python3 scripts/spsa/lakas/view_cmaes_checkpoint.py \
  scripts/spsa/outputs/pass_v2_7/batch1/batch1.dat --show-best
```

The checkpoint viewer distinguishes completed updates, partial populations,
orphaned asks, and live jobs. A low sigma indicates concentration, not proven
strength.

## Layout and controls

- `common.sh`: batch parameters, time controls, and shared defaults
- `master.sh`: optimizer and job producer
- `worker.sh`: long-lived fastchess evaluator
- `watcher.sh`: queue and artifact summary
- `lakas/`: optimizer and inspection helpers
- `outputs/<pass>/`: ignored checkpoints, logs, jobs, and decision tracker

Important controls are `PASS_NAME`, `BATCH_ID`, `BUDGET`, `MASTER_JOBS`,
`WORKER_JOBS`, `JOB_CLAIMS`, `GAMES_PER_BUDGET`, `OPENING_FILE`, and
`FASTCHESS`. Tuning matches default to `OwnBook=false` and
`Book_Info_Depth=0` so the configured external opening set is the only book.

On trainer machines, `lakas_nnue.py` may be copied beside the shell scripts or
kept under `lakas/`; the launcher supports both layouts.

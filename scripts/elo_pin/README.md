# Release Elo pin

## Purpose

Place a release on SHAYVERI's anchored STC and LTC rating scales with exact,
reproducible game quotas.

## Motivation

A full STC and LTC pin requires 36,000 games. Distributing deterministic game
shards across multiple machines makes that workload practical and lets faster
workers complete more of it. The master keeps the final game quotas exact while
the available workers reduce the total wall-clock time.

## Method

The master freezes one worker roster, runs STC to completion, analyzes it with
Ordo, and then opens LTC. Workers claim paired-game shards until each phase's
exact quota is complete. Worker count and speed affect runtime, not the sample.

The fixed nine-engine pool contains release SHAYVERI with NNUE and HCE, five
external engines, and Stockfish 18 limited to 2850 and 3000 Elo.

| Phase | Time control | Games per pairing | Total games |
|------:|-------------:|------------------:|------------:|
|   STC |     `10+0.1` |               800 |      28,800 |
|   LTC |     `90+0.5` |               200 |       7,200 |

Every opening is played with both colors. Seeds derive from `BASE_SEED`, phase,
and shard number. Fastchess recovers transient engine failures. A failed
Fastchess process or incomplete PGN discards and requeues that shard. Illegal
moves remain a hard failure and prevent publication.

## Usage

Start the master on the shared filesystem:

```bash
cd ~/elo_pin

PIN_ROOT="$PWD" \
RELEASE_ID=v0.0.0 \
NET= \
REGISTER_SECONDS=300 \
./master.sh
```

Start one worker on each machine during registration:

```bash
cd ~/elo_pin
PIN_ROOT="$PWD" ./worker.sh
```

Monitor the run:

```bash
cd ~/elo_pin
PIN_ROOT="$PWD" ./watch.sh
```

Status distinguishes the frozen registration roster from workers that
currently own an active shard.

Resume an interrupted run without changing its roster, shards, or seeds:

```bash
cd ~/elo_pin
PIN_ROOT="$PWD" \
RESUME_RUN_ID=v0.0.0_YYYYMMDD_HHMMSS \
./master.sh
```

Resume automatically discards obsolete transient failure markers but still
stops on an illegal-move marker. Existing working shards are preserved by
default so a master can restart while workers continue. If the workers were
stopped, `RECLAIM_WORKING=1` returns their abandoned shards to the queues before
the original registered workers are restarted.

Run the orchestration check after changing the harness:

```bash
bash scripts/elo_pin/check.sh
```

## Layout and outputs

Engines live under `engines/`; the book, anchors, and Ordo default to `books/`,
`anchors`, and `ordo`. Temporary state lives under `.work/`.

Successful runs publish `outputs/<release>/{stc,ltc}`. Each phase retains the
combined `rating_pool.pgn` and every Ordo output. Registrations, logs, shard
definitions, and individual shard PGNs are then removed. Failed runs keep
their work directory for diagnosis.

## Main controls

- `STC_GAMES_PER_PAIR` and `LTC_GAMES_PER_PAIR`: exact quotas
- `STC_SHARD_PAIR_GAMES` and `LTC_SHARD_PAIR_GAMES`: shard sizes
- `BASE_SEED`: deterministic seed sequence
- `CONCURRENCY`: local fastchess concurrency, default 23
- `NET=`: use the embedded default network
- `RELEASE_ID`: full public engine version used by both SHAYVERI entries and
  its output directory
- `EXPECTED_WORKERS`: close registration as soon as this many workers arrive,
  default 10
- `REGISTER_SECONDS`: maximum registration wait if fewer workers arrive
- `OVERWRITE=1`: replace an existing release output
- `RESUME_RUN_ID`: continue an interrupted run from its retained work directory
- `RECLAIM_WORKING=1`: requeue abandoned working shards while resuming
- `SHAYVERI_OPTIONS`: options shared by both SHAYVERI entries

Game and shard quotas must be positive even numbers because openings are
color-paired.

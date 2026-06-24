Elo pin harness
===============

Purpose
=======

Coordinate multi-worker cutechess matches for a properly pinned Elo run.

Flow
====

1. Start the master on the shared filesystem.
2. Start workers within the master's registration window.
3. Each worker runs a full 9-engine round-robin tournament and writes one PGN.
4. Master waits for all registered workers to finish.
5. Master concatenates worker PGNs into one PGN.
6. Master runs Ordo on the combined PGN.

Default layout:

- `PIN_ROOT` is the shared directory containing these scripts.
- Engines live in `$PIN_ROOT/engines`.
- Engine commands use `dir=engines`.
- The book defaults to `$PIN_ROOT/books/UHO_2024_8mvs.epd`.
- Anchors default to `$PIN_ROOT/anchors`.
- Ordo defaults to `$PIN_ROOT/ordo`.
- Runs are written to `$PIN_ROOT/results/$RUN_ID`.

Typical STC run
===============

On master:

```bash
cd ~/elo_pin

PIN_ROOT="$PWD" \
NAME_ID=rm500 \
NET=robotmoon_500M.nnue \
TC=stc \
GAMES_PER_PAIR_PER_WORKER=40 \
REGISTER_SECONDS=300 \
./master.sh
```

On each worker, within 5 minutes:

```bash
cd ~/elo_pin

PIN_ROOT="$PWD" \
./worker.sh
```

Useful knobs
============

- The pool is fixed: `SHAYVERI v1.0`, the current/configured SHAYVERI NNUE, plus `Alexandria9,Berserk13,Ethereal14,PlentyChess7,Weiss2,SF2850,SF3000`.
- Every worker runs a full round robin over all 9 engines.
- `SHAYVERI v1.0` is always included with `UseNNUE=false`.
- `NET=` uses SHAYVERI's default current NNUE file.
- `NET=SHAYVERI2_2_0.nnue` pins the NNUE slot with that file.
- `NAME_ID` controls the NNUE SHAYVERI name written into PGNs/Ordo.
- `HCE_NAME` defaults to `SHAYVERI v1.0`.
- `NNUE_NAME` defaults to `NAME_ID`.
- Run IDs are always `NAME_ID_TC_YYYYmmdd_HHMMSS`.
- `TC=stc` means `10+0.1`.
- `TC=ltc` means `90+0.5`.
- `GAMES_PER_PAIR_PER_WORKER` is total games per worker for each engine pair.
- Total games are `workers * 36 * GAMES_PER_PAIR_PER_WORKER`.
- `REGISTER_SECONDS` is the worker join window.
- `CONCURRENCY` defaults to 23.
- `SHAYVERI_OPTIONS` can append extra UCI options to the SHAYVERI engine.

Main outputs
============

Under `$PIN_ROOT/results/$RUN_ID`:

- `workers/`: worker registration files.
- `games/`: worker PGNs while running.
- `done/`: worker completion markers.
- `rating_pool.pgn`: concatenated final PGN.
- `anchors`: copied Ordo anchor file.
- `ordo`: copied Ordo executable.
- `results.txt`: Ordo text output.
- `h2h.txt`: pairwise sanity output.
- `results.csv`: Ordo CSV output.
- `cfs.csv` and `err.csv`: Ordo support diagnostics.
- `master.log`: master progress.

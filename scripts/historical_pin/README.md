# Historical Elo pins

## Purpose

Estimate the strength of releases that predate SHAYVERI's current release-pin
process on one shared rating scale.

## Motivation

Historical ratings were not measured when those engines were developed. A
fixed retrospective pool permits useful comparison without implying that the
numbers were contemporaneous release results.

## Method

The v2.6.0 STC and LTC PGNs provide the external-engine reference games. Games
involving SHAYVERI are removed, leaving the same reference pool for every
historical engine. Each tested release then plays all seven external opponents
with paired openings.

- STC: `10+0.1`, 200 games per opponent
- LTC: `90+0.5`, 100 games per opponent
- Ordo anchors: `SF2850=2850` and `SF3000=3000`
- Internal SHAYVERI books are disabled when the release supports it

Only the tested SHAYVERI binary and its new games change between pins.

## Usage

```bash
STC_SOURCE_PGN=/path/to/stc/rating_pool.pgn \
LTC_SOURCE_PGN=/path/to/ltc/rating_pool.pgn \
scripts/historical_pin/run_all.sh
```

The runner downloads and verifies historical packages, checks their UCI
handshake, creates shared reference PGNs, runs the games, and analyzes each
combined pool with Ordo.

Run individual stages when resuming or diagnosing:

```bash
scripts/historical_pin/download_binaries.sh
python3 scripts/historical_pin/parse_games.py
scripts/historical_pin/run_games.sh
scripts/historical_pin/run_ordo.sh
```

Restrict work with `TAGS` or `TCS`:

```bash
TAGS="v2.1.0 v2.2.0" TCS=stc scripts/historical_pin/run_all.sh
```

Generated binaries, games, logs, and reports stay under `outputs/`.

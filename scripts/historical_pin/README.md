# Historical Elo Pins

## Motivation

SHAYVERI's releases prior to v2.3.0 predate the current release and 
Elo-pinning process. This harness retrospectively places every release before 
v2.3.0 into one shared reference pool so their strength can be compared 
consistently.

These results are retrospective estimates. They do not claim that the ratings
were known or formally pinned when the historical releases were developed.

## Method

The v2.6.0 STC and LTC pin PGNs provide the reference games. The parser removes
all games involving SHAYVERI and selects the same balanced base for every
historical pin:

- 200 games for every STC external-engine pairing
- 100 games for every LTC external-engine pairing
- opening positions played with both colors
- fixed `SF2850=2850` and `SF3000=3000` Ordo anchors

Each historical release then replaces the removed SHAYVERI slot and plays all
seven external opponents:

- 200 games per opponent at STC `10+0.1`
- 100 games per opponent at LTC `90+0.5`
- paired openings from the same book
- the same opening seed, hardware, engine configuration, and engine pool
- the internal SHAYVERI opening book disabled where the option exists

Every rating uses the exact same external reference PGN. Only the tested
SHAYVERI release and its newly played games change.

Each STC pool contains 4,200 external games and 1,400 SHAYVERI games, for
5,600 games total. Each LTC pool contains 2,100 external games and 700
SHAYVERI games, for 2,800 games total.

## Requirements

By default, the parser expects the original PGNs at their preserved output
locations under `scripts/elo_pin/outputs`. Other paths can be supplied through
`STC_SOURCE_PGN` and `LTC_SOURCE_PGN`.

## Complete Run

```bash
chmod +x scripts/historical_pin/*.sh

STC_SOURCE_PGN=/path/to/stc/rating_pool.pgn \
LTC_SOURCE_PGN=/path/to/ltc/rating_pool.pgn \
scripts/historical_pin/run_all.sh
```

The complete runner performs four stages:

1. Downloads every Linux release package from v1.0.0 through v2.2.0, verifies
   its published SHA-256 checksum, extracts it, and runs a UCI check.
2. Creates the shared STC and LTC reference PGNs.
3. Runs the historical STC and LTC gauntlets.
4. Combines each release's games with the shared reference PGN and runs Ordo.

Shared paths, tags, time controls, game counts, and helper functions live in
`common.sh`. The other scripts only implement their individual pipeline stage.

## Partial and Resumed Runs

The stages can be run separately:

```bash
scripts/historical_pin/download_binaries.sh
python3 scripts/historical_pin/parse_games.py
scripts/historical_pin/run_games.sh
scripts/historical_pin/run_ordo.sh
```

The game runner stores one PGN per release, time control, and opponent. A
completed pairing is skipped when rerun. An incomplete pairing is rejected so
it cannot silently enter the final rating pool. Remove that pairing's PGN and
log before deliberately rerunning it.

Restrict a run with `TAGS` or `TCS`:

```bash
TAGS="v2.1.0 v2.2.0" TCS="stc" scripts/historical_pin/run_all.sh
```

Generated binaries, PGNs, logs, and Ordo reports are stored beneath
`scripts/historical_pin/outputs`. They are local testing artifacts and are not
committed.

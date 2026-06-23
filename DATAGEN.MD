# SHAYVERI Datagen

The goal is to emit training data from SHAYVERI search with an intended NNUE
loaded, while keeping the data auditable enough that bad labels or corpus
collapse are obvious before we waste a training run.

## Output

Supported output formats:

- `shayveri-plain-v1`
- `bullet-v1`

Example direct Bullet run:

```bash
./SHAYVERI datagen \
  --threads 1 \
  --positions 10000 \
  --output-prefix /tmp/shayveri_datagen/smoke \
  --output-format bullet-v1 \
  --eval-file SHAYVERI2_2_0.nnue \
  --nodes 1000 \
  --seed 1
```

`shayveri-plain-v1` is the readable bridge format:

```text
<full FEN> | <white-pov cp> | <white-pov WDL>
```

The bridge format is intentionally lossless for the information we need:

- full FEN
- centipawn label from White's perspective
- game result/WDL from White's perspective

Datagen writes one shard per worker thread:

```text
<output-prefix>_<worker-id>.plain
<output-prefix>_<worker-id>.bullet.bin
```

It also writes:

```text
<output-prefix>.summary.txt
<output-prefix>.DONE
```

`bullet-v1` writes 32-byte records compatible with the `jw1912/bulletformat`
chess `ChessBoard` layout:

```text
u64 occ
u8  pcs[16]
i16 score
u8  result
u8  ksq
u8  opp_ksq
u8  extra[3]
```

The Bullet record is side-to-move normalized. For black-to-move positions,
datagen mirrors the board vertically, swaps colours, negates the score, and
flips the result, matching `bulletformat` semantics. `score` is centipawns from
the side-to-move-normalized perspective. `result` is encoded as `0` black win,
`1` draw, `2` white win after the same normalization.

## Defaults

The defaults are set to match the current external-data recipe as closely as is
reasonable:

- `--max-abs-cp 3000`
- qsearch/static label path, not raw search score labels
- quiet-ish position filtering by default
- TWIC-book starts by default
- adjudication enabled and explicit
- summaries written for every run
- DONE marker written only after successful C++ datagen completion

If `--eval-file` is omitted, the engine default net is used. For serious runs,
always pass the intended seed net explicitly. Use `--eval-file "<hce>"` only
for no-NNUE debug runs.

## Start Positions

The baseline root distribution is the compiled TWIC-derived opening book. That
book was built from 2600+ games across available TWIC instances at the time, so
it gives normal chess starts without turning the self-loop into a narrow test
suite.

External FEN/EPD starts are optional:

- `--start-file <path>`
- `--start-file-prob <0..1>`

External starts are parsed as the first six FEN fields. Illegal, malformed,
structurally impossible, or already-terminal starts are rejected before use and
counted in the summary.

## Filters

Default filters are intended to produce quiet, bounded, NNUE-ready training
positions, similar in shape to public RobotMoon/Stockfish-style datasets:

- reject positions where side to move is in check unless `--include-checks true`
- reject captures, promotions, and en-passant moves unless
  `--include-captures true`
- reject mate-like sentinel scores unless `--include-mate-scores true`
- reject `abs(cp) > --max-abs-cp`
- optional ply bounds with `--min-ply` and `--max-ply`
- optional sampling stride with `--sample-stride`
- optional per-game cap with `--max-samples-per-game`
- optional duplicate filtering with `--include-duplicates false`

Duplicate filtering is off by default because repeated positions/motifs can be
legitimate training signal. When enabled, duplicate checks and duplicate rate
are reported.

## Adjudication

Adjudication is deliberately simple and auditable:

- `--enable-adjudication <bool>`
- `--adjudication-cp <cp>`
- `--adjudication-plies <n>`

A game can end by checkmate, draw, or adjudication. The summary splits those
ending types and records adjudicated win direction. If future labels look
strange, inspect these fields before changing training params.

This is not claimed to be a byte-for-byte copy of Stockfish datagen policy. It
is explicit, knobbed, and auditable.

## Run Controls

Use at least one of:

- `--positions <n>`
- `--games <n>`

Useful debug controls:

- `--print-interval <games>`
- `--threads <n>`
- `--nodes <n>`
- `--seed <n>`

`--games` is mainly for fast sanity runs and debugging positions/game behavior.
Real runs should still be position-targeted.

## Summary Fields

Each run writes `<output-prefix>.summary.txt` with:

- total positions and games
- average positions per game
- max positions from one game
- TWIC-book vs external start counts and percentages
- invalid and terminal external start counts
- full option snapshot
- game ending breakdown
- adjudication direction and ply stats
- filter counts by reason
- duplicate checks and duplicate rate when applicable
- cp buckets
- WDL buckets
- side-to-move buckets
- phase buckets

These fields are the first line of defense against self-play collapse, weird
result labels, narrow opening families, and over-filtering.

## Pipeline Responsibility

C++ datagen writes shard content, summary, and DONE. The outer pipeline still
owns:

- `.incomplete` directories
- atomic rename into completed shard directories
- resume and skip logic
- conversion to Bullet format when the datagen output is not already `bullet-v1`
- shuffle
- training

Do not consume a shard as complete unless the pipeline-level completion marker
and datagen DONE marker agree.

## HCE Role

HCE is not a training teacher.

Use HCE only as:

- a style/flavour reference
- a no-net regression opponent
- a later diagnostic for positions where SHAYVERI NNUE/search disagrees hard

Do not blend HCE eval into labels by default, and do not choose a weaker net
only because it is more HCE-like.

# SHAYVERI Elo

This file records published ratings. The release-pin workflow and configuration
are documented in [`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md).
Ratings are pool-relative rather than universal CCRL ratings.

Version labels now separate SHAYVERI source, evaluator, and network:

- SHAYVERI source release: `SHAYVERI v2.8.0`.
- NNUE evaluator: `NNUE`, using network `SHAYVERI2_5_0`.
- Classical evaluator: `HCE-classical`, the handcrafted evaluation path with
  the UCI option `UseNNUE=false`.

`HCE-classical` is not a SHAYVERI version.

## Current Public Ratings

|          Engine |    Evaluation |       Network | Time Control | Rating |   Error | Gap to SF2850 | Gap to SF3000 |
|----------------:|--------------:|--------------:|-------------:|-------:|--------:|--------------:|--------------:|
| SHAYVERI v2.8.0 |          NNUE | SHAYVERI2_5_0 |   STC 10+0.1 | 3130.0 | +/-15.0 |        +280.0 |        +130.0 |
| SHAYVERI v2.8.0 |          NNUE | SHAYVERI2_5_0 |   LTC 90+0.5 | 3235.6 | +/-29.6 |        +385.6 |        +235.6 |
| SHAYVERI v2.8.0 | HCE-classical |          none |   STC 10+0.1 | 2643.6 | +/-18.8 |        -206.4 |        -356.4 |
| SHAYVERI v2.8.0 | HCE-classical |          none |   LTC 90+0.5 | 2771.4 | +/-31.3 |         -78.6 |        -228.6 |

On this shared scale, the v2.8.0 NNUE configuration is about **+486.4 Elo over
HCE-classical at STC** and **+464.2 Elo over HCE-classical at LTC**.

The previous public NNUE baseline,
`SHAYVERI v2.7.0 / NNUE SHAYVERI2_5_0`, pinned at `3094.7 +/-14.9` STC and
`3235.7 +/-29.8` LTC.

Compared with that baseline, v2.8.0 changed by `+35.3 +/-21.1` at STC and
`-0.1 +/-42.0` at LTC using the combined rating uncertainties.

## STC Pool

Time control: `10+0.1`

Total games: `28800`

Per pairing: `800`

| # | Player                          | Rating | Error | Points | Played | Score | CFS |
|--:|---------------------------------|-------:|------:|-------:|-------:|------:|----:|
| 1 | PlentyChess7                    | 3690.0 |  21.6 | 5432.0 |   6400 |   85% | 100 |
| 2 | Alexandria9                     | 3670.9 |  21.2 | 5353.5 |   6400 |   84% | 100 |
| 3 | Berserk13                       | 3621.0 |  20.1 | 5138.5 |   6400 |   80% | 100 |
| 4 | Weiss2                          | 3298.5 |  16.5 | 3499.5 |   6400 |   55% | 100 |
| 5 | Ethereal14                      | 3241.8 |  15.8 | 3192.0 |   6400 |   50% | 100 |
| 6 | SHAYVERI v2.8.0 / NNUE          | 3130.0 |  15.0 | 2591.0 |   6400 |   40% | 100 |
| 7 | SF3000                          | 3000.0 |  ---- | 1978.5 |   6400 |   31% | 100 |
| 8 | SF2850                          | 2850.0 |  ---- | 1148.5 |   6400 |   18% | 100 |
| 9 | SHAYVERI v2.8.0 / HCE-classical | 2643.6 |  18.8 |  466.5 |   6400 |    7% | --- |

White advantage: `162.30 +/- 2.81`

Draw rate: `37.59% +/- 0.52`

### STC Head To Head

| Match                                     | Games |       W-L-D | Score | Elo Diff |
|-------------------------------------------|------:|------------:|------:|---------:|
| SHAYVERI v2.8.0 / NNUE vs HCE-classical   |   800 |   735-18-47 | 94.8% |   +486.4 |
| SHAYVERI v2.8.0 / NNUE vs SF2850          |   800 |  584-123-93 | 78.8% |   +280.0 |
| SHAYVERI v2.8.0 / NNUE vs SF3000          |   800 | 407-217-176 | 61.9% |   +130.0 |
| SHAYVERI v2.8.0 / HCE-classical vs SF2850 |   800 | 195-488-117 | 31.7% |   -206.4 |
| SHAYVERI v2.8.0 / HCE-classical vs SF3000 |   800 |  79-604-117 | 17.2% |   -356.4 |

## LTC Pool

Time control: `90+0.5`

Total games: `7200`

Per pairing: `200`

| # |                          Player | Rating | Error | Points | Played | Score | CFS |
|--:|--------------------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |                    PlentyChess7 | 3687.7 |  39.4 | 1336.5 |   1600 |   84% |  99 |
| 2 |                     Alexandria9 | 3659.8 |  38.4 | 1305.5 |   1600 |   82% | 100 |
| 3 |                       Berserk13 | 3594.7 |  36.1 | 1228.5 |   1600 |   77% | 100 |
| 4 |                          Weiss2 | 3363.1 |  31.0 |  916.5 |   1600 |   57% | 100 |
| 5 |                      Ethereal14 | 3295.8 |  30.3 |  821.5 |   1600 |   51% | 100 |
| 6 |          SHAYVERI v2.8.0 / NNUE | 3235.6 |  29.6 |  737.0 |   1600 |   46% | 100 |
| 7 |                          SF3000 | 3000.0 |  ---- |  459.5 |   1600 |   29% | 100 |
| 8 |                          SF2850 | 2850.0 |  ---- |  212.5 |   1600 |   13% | 100 |
| 9 | SHAYVERI v2.8.0 / HCE-classical | 2771.4 |  31.3 |  182.5 |   1600 |   11% | --- |

White advantage: `183.73 +/- 5.29`

Draw rate: `47.55% +/- 1.13`

### LTC Head To Head

|                                     Match | Games |     W-L-D | Score | Elo Diff |
|------------------------------------------:|------:|----------:|------:|---------:|
|   SHAYVERI v2.8.0 / NNUE vs HCE-classical |   200 |  171-6-23 | 91.2% |   +464.2 |
|          SHAYVERI v2.8.0 / NNUE vs SF2850 |   200 |  173-9-18 | 91.0% |   +385.6 |
|          SHAYVERI v2.8.0 / NNUE vs SF3000 |   200 | 105-40-55 | 66.2% |   +235.6 |
| SHAYVERI v2.8.0 / HCE-classical vs SF2850 |   200 |  75-93-32 | 45.5% |    -78.6 |
| SHAYVERI v2.8.0 / HCE-classical vs SF3000 |   200 | 31-121-48 | 27.5% |   -228.6 |

## Release Notes

- SHAYVERI source release: `SHAYVERI v2.8.0`.
- Embedded default network: `SHAYVERI2_5_0.nnue`.
- External network files can still be loaded through the UCI option `EvalFile`.
- HCE-classical is retained as a debug and comparison evaluator, not as a
  separate SHAYVERI version.

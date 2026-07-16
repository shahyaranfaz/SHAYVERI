# SHAYVERI Elo

This file records published ratings. The release-pin workflow and configuration
are documented in [`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md).
Ratings are pool-relative rather than universal CCRL ratings.

Version labels now separate SHAYVERI source, evaluator, and network:

- SHAYVERI source release: `SHAYVERI v2.7.0`.
- NNUE evaluator: `NNUE`, using network `SHAYVERI2_5_0`.
- Classical evaluator: `HCE-classical`, the handcrafted evaluation path with
  the UCI option `UseNNUE=false`.

`HCE-classical` is not a SHAYVERI version.

## Current Public Ratings

|          Engine |    Evaluation |       Network | Time Control | Rating |   Error | Gap to SF2850 | Gap to SF3000 |
|----------------:|--------------:|--------------:|-------------:|-------:|--------:|--------------:|--------------:|
| SHAYVERI v2.7.0 |          NNUE | SHAYVERI2_5_0 |   STC 10+0.1 | 3094.7 | +/-14.9 |        +244.7 |         +94.7 |
| SHAYVERI v2.7.0 |          NNUE | SHAYVERI2_5_0 |   LTC 90+0.5 | 3235.7 | +/-29.8 |        +385.7 |        +235.7 |
| SHAYVERI v2.7.0 | HCE-classical |          none |   STC 10+0.1 | 2626.5 | +/-19.3 |        -223.5 |        -373.5 |
| SHAYVERI v2.7.0 | HCE-classical |          none |   LTC 90+0.5 | 2751.3 | +/-32.1 |         -98.7 |        -248.7 |

On this shared scale, the v2.7.0 NNUE configuration is about **+468.2 Elo over
HCE-classical at STC** and **+484.4 Elo over HCE-classical at LTC**.

The previous public NNUE baseline,
`SHAYVERI v2.6.0 / NNUE SHAYVERI2_5_0`, pinned at `3045.7 +/-14.8` STC and
`3136.5 +/-28.9` LTC before the v2.7.0 search overhaul.

Compared with that baseline, v2.7.0 improved by `+49.0 +/-21.0` at STC and
`+99.2 +/-41.5` at LTC using the combined rating uncertainties.

## STC Pool

Time control: `10+0.1`

Total games: `28800`

Per pairing: `800`

| # | Player                          | Rating | Error | Points | Played | Score | CFS |
|--:|---------------------------------|-------:|------:|-------:|-------:|------:|----:|
| 1 | PlentyChess7                    | 3683.1 |  21.6 | 5449.5 |   6400 |   85% | 100 |
| 2 | Alexandria9                     | 3658.5 |  21.2 | 5349.0 |   6400 |   84% | 100 |
| 3 | Berserk13                       | 3614.4 |  20.4 | 5160.0 |   6400 |   81% | 100 |
| 4 | Weiss2                          | 3281.7 |  16.1 | 3475.5 |   6400 |   54% | 100 |
| 5 | Ethereal14                      | 3235.0 |  16.0 | 3222.5 |   6400 |   50% | 100 |
| 6 | SHAYVERI v2.7.0 / NNUE          | 3094.7 |  14.9 | 2468.0 |   6400 |   39% | 100 |
| 7 | SF3000                          | 3000.0 |  ---- | 2052.5 |   6400 |   32% | 100 |
| 8 | SF2850                          | 2850.0 |  ---- | 1178.5 |   6400 |   18% | 100 |
| 9 | SHAYVERI v2.7.0 / HCE-classical | 2626.5 |  19.3 |  444.5 |   6400 |    7% | --- |

White advantage: `161.35 +/- 2.84`

Draw rate: `37.65% +/- 0.52`

### STC Head To Head

| Match                                     | Games |       W-L-D | Score | Elo Diff |
|-------------------------------------------|------:|------------:|------:|---------:|
| SHAYVERI v2.7.0 / NNUE vs HCE-classical   |   800 |   725-23-52 | 93.9% |   +468.2 |
| SHAYVERI v2.7.0 / NNUE vs SF2850          |   800 | 555-131-114 | 76.5% |   +244.7 |
| SHAYVERI v2.7.0 / NNUE vs SF3000          |   800 | 381-241-178 | 58.8% |    +94.7 |
| SHAYVERI v2.7.0 / HCE-classical vs SF2850 |   800 | 170-491-139 | 29.9% |   -223.5 |
| SHAYVERI v2.7.0 / HCE-classical vs SF3000 |   800 |  78-621-101 | 16.1% |   -373.5 |

## LTC Pool

Time control: `90+0.5`

Total games: `7200`

Per pairing: `200`

| # |                          Player | Rating | Error | Points | Played | Score | CFS |
|--:|--------------------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |                    PlentyChess7 | 3722.1 |  41.9 | 1340.0 |   1600 |   84% |  99 |
| 2 |                     Alexandria9 | 3688.3 |  39.6 | 1304.0 |   1600 |   82% | 100 |
| 3 |                       Berserk13 | 3630.9 |  38.3 | 1239.0 |   1600 |   77% | 100 |
| 4 |                          Weiss2 | 3397.8 |  32.6 |  939.0 |   1600 |   59% | 100 |
| 5 |                      Ethereal14 | 3306.0 |  31.2 |  814.5 |   1600 |   51% | 100 |
| 6 |          SHAYVERI v2.7.0 / NNUE | 3235.7 |  29.8 |  720.0 |   1600 |   45% | 100 |
| 7 |                          SF3000 | 3000.0 |  ---- |  454.0 |   1600 |   28% | 100 |
| 8 |                          SF2850 | 2850.0 |  ---- |  218.5 |   1600 |   14% | 100 |
| 9 | SHAYVERI v2.7.0 / HCE-classical | 2751.3 |  32.1 |  171.0 |   1600 |   11% | --- |

White advantage: `198.26 +/- 5.45`

Draw rate: `49.83% +/- 1.19`

### LTC Head To Head

|                                     Match | Games |     W-L-D | Score | Elo Diff |
|------------------------------------------:|------:|----------:|------:|---------:|
|   SHAYVERI v2.7.0 / NNUE vs HCE-classical |   200 |  179-6-15 | 93.2% |   +484.4 |
|          SHAYVERI v2.7.0 / NNUE vs SF2850 |   200 | 164-11-25 | 88.2% |   +385.7 |
|          SHAYVERI v2.7.0 / NNUE vs SF3000 |   200 | 119-28-53 | 72.8% |   +235.7 |
| SHAYVERI v2.7.0 / HCE-classical vs SF2850 |   200 |  69-93-38 | 44.0% |    -98.7 |
| SHAYVERI v2.7.0 / HCE-classical vs SF3000 |   200 | 34-125-41 | 27.2% |   -248.7 |

## Release Notes

- SHAYVERI source release: `SHAYVERI v2.7.0`.
- Embedded default network: `SHAYVERI2_5_0.nnue`.
- External network files can still be loaded through the UCI option `EvalFile`.
- HCE-classical is retained as a debug and comparison evaluator, not as a
  separate SHAYVERI version.

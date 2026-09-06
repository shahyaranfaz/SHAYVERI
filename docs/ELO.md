# SHAYVERI Elo

This file records published ratings for the latest SHAYVERI release.

The release-pin workflow and configuration are documented in
[`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md). Ratings are
pool-relative rather than universal CCRL ratings.

- SHAYVERI source release: `SHAYVERI v2.10.0`.
- Embedded default network: `SHAYVERI2_10_4.nnue`.

## Current Public Ratings

|           Engine | Evaluation | Time Control | Rating |   Error |
|-----------------:|-----------:|-------------:|-------:|--------:|
| SHAYVERI v2.10.0 |       NNUE |   STC 10+0.1 | 3261.4 | +/-16.0 |
| SHAYVERI v2.10.0 |       NNUE |   LTC 90+0.5 | 3320.6 | +/-30.3 |
| SHAYVERI v2.10.0 |        HCE |   STC 10+0.1 | 2674.8 | +/-18.1 |
| SHAYVERI v2.10.0 |        HCE |   LTC 90+0.5 | 2794.1 | +/-30.9 |

On this shared scale, SHAYVERI v2.10.0 NNUE gained **+586.6 Elo over v2.10.0
HCE at STC** and **+526.5 Elo over v2.10.0 HCE at LTC**. Against the identically
anchored SHAYVERI v2.9.0, it gained **+67.0 Elo at STC** and **+50.3 Elo at
LTC**.

## Engine Pool

Both the STC and LTC results are based on pools with the following opponents:

|        Name |             Engine | Evaluation |
|------------:|-------------------:|-----------:|
|  Alexandria |       Alexandria 9 |       NNUE |
|     Berserk |         Berserk 13 |       NNUE |
|    Ethereal | Ethereal 14 (Free) |        HCE |
| PlentyChess |      PlentyChess 7 |       NNUE |
|      SF2850 |       Stockfish 18 |       NNUE |
|      SF3000 |       Stockfish 18 |       NNUE |
|       Weiss |            Weiss 2 |        HCE |

The Stockfish entries use `UCI_LimitStrength=true` with the corresponding
`UCI_Elo` value. The two engines are used to anchor the rating pool but do not
necessarily represent actual 2850 or 3000 Elo strength. All later tables use
only the names defined above.

## STC Results

Time control: `10+0.1`

Games per pairing: `800`

Total games: `28800`

Opening book: `UHO_2024_8mvs.epd`

| # |                Player | Rating | Error | Points | Played | Score | CFS |
|--:|----------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |           PlentyChess | 3698.6 |  21.3 | 5385.0 |   6400 |   84% | 100 |
| 2 |            Alexandria | 3672.0 |  21.0 | 5272.0 |   6400 |   82% | 100 |
| 3 |               Berserk | 3634.9 |  20.3 | 5108.0 |   6400 |   80% | 100 |
| 4 |                 Weiss | 3303.5 |  16.3 | 3385.0 |   6400 |   53% | 100 |
| 5 | SHAYVERI v2.10.0 NNUE | 3261.4 |  16.0 | 3156.0 |   6400 |   49% | 100 |
| 6 |              Ethereal | 3243.7 |  15.9 | 3060.0 |   6400 |   48% | 100 |
| 7 |                SF3000 | 3000.0 |  ---- | 1824.0 |   6400 |   28% | 100 |
| 8 |                SF2850 | 2850.0 |  ---- | 1098.5 |   6400 |   17% | 100 |
| 9 |  SHAYVERI v2.10.0 HCE | 2674.8 |  18.1 |  511.5 |   6400 |    8% | --- |

White advantage: `173.08 +/- 2.82`

Draw rate: `39.67% +/- 0.54`

## LTC Results

Time control: `90+0.5`

Games per pairing: `200`

Total games: `7200`

Opening book: `UHO_2024_8mvs.epd`

| # |                Player | Rating | Error | Points | Played | Score | CFS |
|--:|----------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |           PlentyChess | 3679.4 |  38.4 | 1316.0 |   1600 |   82% |  84 |
| 2 |            Alexandria | 3667.2 |  38.4 | 1302.0 |   1600 |   81% | 100 |
| 3 |               Berserk | 3607.9 |  36.4 | 1231.0 |   1600 |   77% | 100 |
| 4 |                 Weiss | 3356.6 |  30.5 |  889.0 |   1600 |   56% | 100 |
| 5 | SHAYVERI v2.10.0 NNUE | 3320.6 |  30.3 |  838.0 |   1600 |   52% | 100 |
| 6 |              Ethereal | 3289.6 |  29.6 |  794.0 |   1600 |   50% | 100 |
| 7 |                SF3000 | 3000.0 |  ---- |  454.5 |   1600 |   28% | 100 |
| 8 |                SF2850 | 2850.0 |  ---- |  184.0 |   1600 |   12% | 100 |
| 9 |  SHAYVERI v2.10.0 HCE | 2794.1 |  30.9 |  191.5 |   1600 |   12% | --- |

White advantage: `188.39 +/- 5.20`

Draw rate: `49.71% +/- 1.17`

## Head-To-Head Results

### NNUE at STC:

|             Opponent | Games |       W-L-D | Score | Pool Diff |
|---------------------:|------:|------------:|------:|----------:|
| SHAYVERI v2.10.0 HCE |   800 |    752-9-39 | 96.4% |    +586.6 |
|               SF2850 |   800 |   667-67-66 | 87.5% |    +411.4 |
|               SF3000 |   800 | 532-126-142 | 75.4% |    +261.4 |
|             Ethereal |   800 | 285-258-257 | 51.7% |     +17.7 |
|                Weiss |   800 | 235-328-237 | 44.2% |     -42.1 |
|              Berserk |   800 |   6-610-184 | 12.2% |    -373.5 |
|           Alexandria |   800 |   1-592-207 | 13.1% |    -410.6 |
|          PlentyChess |   800 |   4-580-216 | 14.0% |    -437.3 |

### HCE at STC:

|              Opponent | Games |       W-L-D | Score | Pool Diff |
|----------------------:|------:|------------:|------:|----------:|
|                SF2850 |   800 | 208-462-130 | 34.1% |    -175.2 |
|                SF3000 |   800 |  99-576-125 | 20.2% |    -325.2 |
| SHAYVERI v2.10.0 NNUE |   800 |    9-752-39 |  3.6% |    -586.6 |
|              Ethereal |   800 |    4-777-19 |  1.7% |    -568.9 |
|                 Weiss |   800 |    0-778-22 |  1.4% |    -628.7 |
|               Berserk |   800 |    1-787-12 |  0.9% |    -960.2 |
|            Alexandria |   800 |    0-784-16 |  1.0% |    -997.2 |
|           PlentyChess |   800 |    0-782-18 |  1.1% |   -1023.9 |

### NNUE at LTC:

|             Opponent | Games |     W-L-D | Score | Pool Diff |
|---------------------:|------:|----------:|------:|----------:|
| SHAYVERI v2.10.0 HCE |   200 |  184-3-13 | 95.2% |    +526.5 |
|               SF2850 |   200 |  177-4-19 | 93.2% |    +470.6 |
|               SF3000 |   200 | 134-21-45 | 78.2% |    +320.6 |
|             Ethereal |   200 |  71-56-73 | 53.8% |     +31.1 |
|                Weiss |   200 |  52-67-81 | 46.2% |     -35.9 |
|              Berserk |   200 |  0-126-74 | 18.5% |    -287.3 |
|           Alexandria |   200 |  0-137-63 | 15.8% |    -346.6 |
|          PlentyChess |   200 |  0-128-72 | 18.0% |    -358.8 |

### HCE at LTC:

|              Opponent | Games |     W-L-D | Score | Pool Diff |
|----------------------:|------:|----------:|------:|----------:|
|                SF2850 |   200 |  88-70-42 | 54.5% |     -55.9 |
|                SF3000 |   200 | 30-132-38 | 24.5% |    -205.9 |
| SHAYVERI v2.10.0 NNUE |   200 |  3-184-13 |  4.8% |    -526.5 |
|              Ethereal |   200 |   0-196-4 |  1.0% |    -495.4 |
|                 Weiss |   200 |   0-193-7 |  1.8% |    -562.5 |
|               Berserk |   200 |  0-187-13 |  3.2% |    -813.8 |
|            Alexandria |   200 |  0-188-12 |  3.0% |    -873.1 |
|           PlentyChess |   200 |  0-188-12 |  3.0% |    -885.3 |

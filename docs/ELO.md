# SHAYVERI Elo

This file records published ratings for the latest SHAYVERI release.

The release-pin workflow and configuration are documented in
[`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md). Ratings are
pool-relative rather than universal CCRL ratings.

- SHAYVERI source release: `SHAYVERI v2.9.1`.
- Embedded default network: `SHAYVERI2_10_4.nnue`.

The current version, v2.9.1, has identical engine behaviour to v2.9.0 and has
thus not been rigorously Elo-pinned. The results for the previous version are
still applicable and included below.

## Current Public Ratings

|          Engine | Evaluation | Time Control | Rating |   Error | Gap to SF2850* | Gap to SF3000* |
|----------------:|-----------:|-------------:|-------:|--------:|---------------:|---------------:|
| SHAYVERI v2.9.0 |       NNUE |   STC 10+0.1 | 3194.4 | +/-15.3 |         +344.4 |         +194.4 |
| SHAYVERI v2.9.0 |       NNUE |   LTC 90+0.5 | 3270.3 | +/-30.0 |         +420.3 |         +270.3 |
| SHAYVERI v2.9.0 |        HCE |   STC 10+0.1 | 2692.8 | +/-17.7 |         -157.2 |         -307.2 |
| SHAYVERI v2.9.0 |        HCE |   LTC 90+0.5 | 2779.7 | +/-31.2 |          -70.3 |         -220.3 |

On this shared scale, the v2.9.0 NNUE configuration is about **+501.6 Elo over
HCE at STC** and **+490.6 Elo over HCE at LTC**.

\*SF2850 and SF3000 are Stockfish instances with their Elo set to 2850 and 3000,
respectively. These labels anchor the rating pool and do not represent actual
2850 or 3000 Elo strength.

## STC Results

Time control: `10+0.1`

Total games: `28800`

Per pairing: `800`

| # |                 Player | Rating | Error | Points | Played | Score | CFS |
|--:|-----------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |           PlentyChess7 | 3695.4 |  21.0 | 5415.0 |   6400 |   85% | 100 |
| 2 |            Alexandria9 | 3677.0 |  20.9 | 5338.0 |   6400 |   83% | 100 |
| 3 |              Berserk13 | 3624.1 |  19.8 | 5106.0 |   6400 |   80% | 100 |
| 4 |                 Weiss2 | 3301.0 |  16.0 | 3430.0 |   6400 |   54% | 100 |
| 5 |             Ethereal14 | 3252.0 |  15.6 | 3161.5 |   6400 |   49% | 100 |
| 6 | SHAYVERI v2.9.0 / NNUE | 3194.4 |  15.3 | 2847.5 |   6400 |   44% | 100 |
| 7 |                 SF3000 | 3000.0 |  ---- | 1898.5 |   6400 |   30% | 100 |
| 8 |                 SF2850 | 2850.0 |  ---- | 1050.5 |   6400 |   16% | 100 |
| 9 |  SHAYVERI v2.9.0 / HCE | 2692.8 |  17.7 |  553.0 |   6400 |    9% | --- |

White advantage: `165.88 +/- 2.77`

Draw rate: `38.79% +/- 0.53`

## LTC Results

Time control: `90+0.5`

Total games: `7200`

Per pairing: `200`

| # |                 Player | Rating | Error | Points | Played | Score | CFS |
|--:|-----------------------:|-------:|------:|-------:|-------:|------:|----:|
| 1 |           PlentyChess7 | 3726.8 |  40.4 | 1345.0 |   1600 |   84% | 100 |
| 2 |            Alexandria9 | 3669.8 |  38.2 | 1282.5 |   1600 |   80% | 100 |
| 3 |              Berserk13 | 3638.1 |  37.9 | 1245.5 |   1600 |   78% | 100 |
| 4 |                 Weiss2 | 3396.0 |  32.2 |  929.0 |   1600 |   58% | 100 |
| 5 |             Ethereal14 | 3309.5 |  30.4 |  810.0 |   1600 |   51% | 100 |
| 6 | SHAYVERI v2.9.0 / NNUE | 3270.3 |  30.0 |  756.5 |   1600 |   47% | 100 |
| 7 |                 SF3000 | 3000.0 |  ---- |  440.5 |   1600 |   28% | 100 |
| 8 |                 SF2850 | 2850.0 |  ---- |  207.0 |   1600 |   13% | 100 |
| 9 |  SHAYVERI v2.9.0 / HCE | 2779.7 |  31.2 |  184.0 |   1600 |   12% | --- |

White advantage: `191.58 +/- 5.37`

Draw rate: `49.29% +/- 1.17`

## Head-To-Head Results

### STC

| Match                                  | Games |       W-L-D | Score | Elo Diff |
|----------------------------------------|------:|------------:|------:|---------:|
| SHAYVERI v2.9.0 / NNUE vs HCE          |   800 |   750-12-38 | 96.1% |   +501.7 |
| SHAYVERI v2.9.0 / NNUE vs SF2850       |   800 |  612-86-102 | 82.9% |   +344.4 |
| SHAYVERI v2.9.0 / NNUE vs SF3000       |   800 | 468-163-169 | 69.1% |   +194.4 |
| SHAYVERI v2.9.0 / NNUE vs Ethereal14   |   800 | 207-359-234 | 40.5% |    -57.6 |
| SHAYVERI v2.9.0 / NNUE vs Weiss2       |   800 | 139-383-278 | 34.8% |   -106.5 |
| SHAYVERI v2.9.0 / NNUE vs Berserk13    |   800 |   9-632-159 | 11.1% |   -429.7 |
| SHAYVERI v2.9.0 / NNUE vs Alexandria9  |   800 |   1-630-169 | 10.7% |   -482.5 |
| SHAYVERI v2.9.0 / NNUE vs PlentyChess7 |   800 |   3-629-168 | 10.9% |   -501.0 |

| Match                                 | Games |       W-L-D | Score | Elo Diff |
|---------------------------------------|------:|------------:|------:|---------:|
| SHAYVERI v2.9.0 / HCE vs SF2850       |   800 | 262-427-111 | 39.7% |   -157.2 |
| SHAYVERI v2.9.0 / HCE vs SF3000       |   800 |  98-577-125 | 20.1% |   -307.2 |
| SHAYVERI v2.9.0 / HCE vs NNUE         |   800 |   12-750-38 |  3.9% |   -501.7 |
| SHAYVERI v2.9.0 / HCE vs Ethereal14   |   800 |    2-779-19 |  1.4% |   -559.2 |
| SHAYVERI v2.9.0 / HCE vs Weiss2       |   800 |    3-766-31 |  2.3% |   -608.2 |
| SHAYVERI v2.9.0 / HCE vs Berserk13    |   800 |     0-791-9 |  0.6% |   -931.4 |
| SHAYVERI v2.9.0 / HCE vs Alexandria9  |   800 |    0-790-10 |  0.6% |   -984.2 |
| SHAYVERI v2.9.0 / HCE vs PlentyChess7 |   800 |     0-791-9 |  0.6% |  -1002.7 |

### LTC

| Match                                  | Games |     W-L-D | Score | Elo Diff |
|----------------------------------------|------:|----------:|------:|---------:|
| SHAYVERI v2.9.0 / NNUE vs HCE          |   200 |  178-1-21 | 94.2% |   +490.6 |
| SHAYVERI v2.9.0 / NNUE vs SF2850       |   200 |  173-6-21 | 91.8% |   +420.3 |
| SHAYVERI v2.9.0 / NNUE vs SF3000       |   200 | 126-33-41 | 73.2% |   +270.3 |
| SHAYVERI v2.9.0 / NNUE vs Ethereal14   |   200 |  51-74-75 | 44.2% |    -39.2 |
| SHAYVERI v2.9.0 / NNUE vs Weiss2       |   200 |  22-93-85 | 32.2% |   -125.7 |
| SHAYVERI v2.9.0 / NNUE vs Berserk13    |   200 |  0-138-62 | 15.5% |   -367.8 |
| SHAYVERI v2.9.0 / NNUE vs Alexandria9  |   200 |  1-141-58 | 15.0% |   -399.5 |
| SHAYVERI v2.9.0 / NNUE vs PlentyChess7 |   200 |  0-152-48 | 12.0% |   -456.5 |

| Match                                  | Games |     W-L-D | Score | Elo Diff |
|----------------------------------------|------:|----------:|------:|---------:|
| SHAYVERI v2.9.0 / HCE vs SF2850        |   200 |  75-81-44 | 48.5% |    -70.3 |
| SHAYVERI v2.9.0 / HCE vs SF3000        |   200 | 27-116-57 | 27.8% |   -220.3 |
| SHAYVERI v2.9.0 / HCE vs NNUE          |   200 |  1-178-21 |  5.8% |   -490.6 |
| SHAYVERI v2.9.0 / HCE vs Ethereal14    |   200 |   1-193-6 |  2.0% |   -529.8 |
| SHAYVERI v2.9.0 / HCE vs Weiss2        |   200 |   0-194-6 |  1.5% |   -616.3 |
| SHAYVERI v2.9.0 / HCE vs Berserk13     |   200 |   0-194-6 |  1.5% |   -858.4 |
| SHAYVERI v2.9.0 / HCE vs Alexandria9   |   200 |  0-187-13 |  3.2% |   -890.1 |
| SHAYVERI v2.9.0 / HCE vs PlentyChess7  |   200 |   0-193-7 |  1.8% |   -947.1 |

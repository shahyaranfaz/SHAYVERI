# SHAYVERI Elo

These ratings are from local multi-engine round-robin gauntlets analyzed with
Ordo. They are pool-relative and anchored to fixed-strength `SF2850=2850` and
`SF3000=3000`; they are not universal CCRL ratings.

Version labels now separate engine/source, evaluator, and network:

- Engine release: `SHAYVERI v2.6`.
- NNUE evaluator: `NNUE`, using network `SHAYVERI2_5_0`.
- Classical evaluator: `HCE-classical`, the handcrafted evaluation path with
  `UseNNUE=false`.

The classical evaluator is not an engine version.

## Current Public Ratings

| Engine        | Evaluation    | Network        | Time Control | Rating | Error   | Gap to SF2850 | Gap to SF3000 |
|---------------|---------------|----------------|--------------|--------|---------|---------------|---------------|
| SHAYVERI v2.6 | NNUE          | SHAYVERI2_5_0  | STC 10+0.1   | 3045.7 | +/-14.8 | +195.7        | +45.7         |
| SHAYVERI v2.6 | NNUE          | SHAYVERI2_5_0  | LTC 90+0.5   | 3136.5 | +/-28.9 | +286.5        | +136.5        |
| SHAYVERI v2.6 | HCE-classical | none           | STC 10+0.1   | 2594.5 | +/-19.8 | -255.5        | -405.5        |
| SHAYVERI v2.6 | HCE-classical | none           | LTC 90+0.5   | 2718.8 | +/-33.2 | -131.2        | -281.2        |

On this shared scale, the v2.6 NNUE configuration is about **+451.2 Elo over
HCE-classical at STC** and **+417.7 Elo over HCE-classical at LTC**.

The previous public NNUE baseline, `SHAYVERI v2.5 / NNUE SHAYVERI2_5_0`, pinned
at `2952.4 +/-14.7` STC and `3081.9 +/-28.4` LTC before the v2.6 search
overhaul.

## STC Pool

Time control: `10+0.1`

Total games: `28800`

Per pairing: `800`

| # | Player                         | Rating | Error  | Points | Played | Score | CFS |
|---|--------------------------------|--------|--------|--------|--------|-------|-----|
| 1 | PlentyChess7                   | 3666.5 | 21.1   | 5445.5 | 6400   | 85%   | 100 |
| 2 | Alexandria9                    | 3636.0 | 20.6   | 5319.0 | 6400   | 83%   | 100 |
| 3 | Berserk13                      | 3594.8 | 19.5   | 5140.0 | 6400   | 80%   | 100 |
| 4 | Weiss2                         | 3284.6 | 16.2   | 3564.0 | 6400   | 56%   | 100 |
| 5 | Ethereal14                     | 3232.6 | 15.9   | 3281.5 | 6400   | 51%   | 100 |
| 6 | SHAYVERI v2.6 / NNUE           | 3045.7 | 14.8   | 2281.0 | 6400   | 36%   | 100 |
| 7 | SF3000                         | 3000.0 | ----   | 2118.5 | 6400   | 33%   | 100 |
| 8 | SF2850                         | 2850.0 | ----   | 1249.0 | 6400   | 20%   | 100 |
| 9 | SHAYVERI v2.6 / HCE-classical  | 2594.5 | 19.8   | 401.5  | 6400   | 6%    | --- |

White advantage: `161.83 +/- 2.79`

Draw rate: `37.54% +/- 0.53`

### STC Head To Head

| Match                                      | Games | W-L-D       | Score | Elo Diff |
|--------------------------------------------|-------|-------------|-------|----------|
| SHAYVERI v2.6 / NNUE vs HCE-classical      | 800   | 732-27-41   | 94.1% | +451.2   |
| SHAYVERI v2.6 / NNUE vs SF2850             | 800   | 517-169-114 | 71.8% | +195.7   |
| SHAYVERI v2.6 / NNUE vs SF3000             | 800   | 329-275-196 | 53.4% | +45.7    |
| SHAYVERI v2.6 / HCE-classical vs SF2850    | 800   | 160-520-120 | 27.5% | -255.5   |
| SHAYVERI v2.6 / HCE-classical vs SF3000    | 800   | 46-644-110  | 12.6% | -405.5   |

## LTC Pool

Time control: `90+0.5`

Total games: `7200`

Per pairing: `200`

| # | Player                         | Rating | Error  | Points | Played | Score | CFS |
|---|--------------------------------|--------|--------|--------|--------|-------|-----|
| 1 | PlentyChess7                   | 3690.8 | 40.9   | 1338.0 | 1600   | 84%   | 97  |
| 2 | Alexandria9                    | 3666.7 | 39.8   | 1312.5 | 1600   | 82%   | 100 |
| 3 | Berserk13                      | 3614.7 | 38.4   | 1254.5 | 1600   | 78%   | 100 |
| 4 | Weiss2                         | 3374.7 | 32.5   | 948.0  | 1600   | 59%   | 100 |
| 5 | Ethereal14                     | 3289.2 | 31.2   | 831.5  | 1600   | 52%   | 100 |
| 6 | SHAYVERI v2.6 / NNUE           | 3136.5 | 28.9   | 626.0  | 1600   | 39%   | 100 |
| 7 | SF3000                         | 3000.0 | ----   | 491.5  | 1600   | 31%   | 100 |
| 8 | SF2850                         | 2850.0 | ----   | 237.0  | 1600   | 15%   | 100 |
| 9 | SHAYVERI v2.6 / HCE-classical  | 2718.8 | 33.2   | 161.0  | 1600   | 10%   | --- |

White advantage: `192.60 +/- 5.49`

Draw rate: `45.77% +/- 1.15`

### LTC Head To Head

| Match                                      | Games | W-L-D     | Score | Elo Diff |
|--------------------------------------------|-------|-----------|-------|----------|
| SHAYVERI v2.6 / NNUE vs HCE-classical      | 200   | 177-8-15  | 92.2% | +417.7   |
| SHAYVERI v2.6 / NNUE vs SF2850             | 200   | 152-23-25 | 82.2% | +286.5   |
| SHAYVERI v2.6 / NNUE vs SF3000             | 200   | 99-52-49  | 61.8% | +136.5   |
| SHAYVERI v2.6 / HCE-classical vs SF2850    | 200   | 78-102-20 | 44.0% | -131.2   |
| SHAYVERI v2.6 / HCE-classical vs SF3000    | 200   | 30-142-28 | 22.0% | -281.2   |

## Release Notes

- Engine/source release: `SHAYVERI v2.6`.
- Embedded default network: `SHAYVERI2_5_0.nnue`.
- External network files can still be loaded through `EvalFile`.
- HCE-classical is retained as a debug and comparison evaluator, not as a
  separate engine version.

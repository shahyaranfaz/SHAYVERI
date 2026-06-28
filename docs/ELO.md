# SHAYVERI Elo

These ratings are from local multi-engine round-robin gauntlets analyzed with
Ordo. They are pool-relative and anchored to fixed-strength `SF2850=2850` and
`SF3000=3000`; they are not universal CCRL ratings.

Both SHAYVERI evaluation modes are included in the same pool:

- `SHAYVERI v1.0`: classical handcrafted evaluation.
- `SHAYVERI v2.5.0`: current NNUE, default file `SHAYVERI2_5_0.nnue`.

Previous NNUE baseline: `SHAYVERI v2.2.0` was `2938.3 +/-14.9` at STC and
`3046.2 +/-28.6` at LTC.

## Current Public Ratings

| Configuration   | Evaluation    | Time Control | Rating | Error   | Gap to SF2850 | Gap to SF3000 |
|-----------------|---------------|--------------|--------|---------|---------------|---------------|
| SHAYVERI v1.0   | Classical HCE | STC 10+0.1   | 2572.4 | +/-19.9 | -277.6        | -427.6        |
| SHAYVERI v1.0   | Classical HCE | LTC 90+0.5   | 2694.3 | +/-33.6 | -155.7        | -305.7        |
| SHAYVERI v2.5.0 | NNUE          | STC 10+0.1   | 2952.4 | +/-14.7 | +102.4        | -47.6         |
| SHAYVERI v2.5.0 | NNUE          | LTC 90+0.5   | 3081.9 | +/-28.4 | +231.9        | +81.9         |

On this shared scale, v2.5.0 is about **+380.0 Elo over v1.0 at STC** and
**+387.6 Elo over v1.0 at LTC**.

Compared with the previous v2.2.0 NNUE baseline, v2.5.0 is **+14.1 Elo at STC**
and **+35.7 Elo at LTC**.

## STC Pool

Time control: `10+0.1`

Total games: `28800`

Per pairing: `800`

| # | Player          | Rating | Error  | Points | Played | Score | CFS |
|---|-----------------|--------|--------|--------|--------|-------|-----|
| 1 | PlentyChess7    | 3661.5 | 21.4   | 5458.5 | 6400   | 85%   | 100 |
| 2 | Alexandria9     | 3641.2 | 21.2   | 5376.5 | 6400   | 84%   | 100 |
| 3 | Berserk13       | 3584.5 | 19.8   | 5136.0 | 6400   | 80%   | 100 |
| 4 | Weiss2          | 3271.3 | 16.3   | 3585.0 | 6400   | 56%   | 100 |
| 5 | Ethereal14      | 3223.1 | 15.9   | 3330.0 | 6400   | 52%   | 100 |
| 6 | SF3000          | 3000.0 | anchor | 2224.0 | 6400   | 35%   | 100 |
| 7 | SHAYVERI v2.5.0 | 2952.4 | 14.7   | 1922.0 | 6400   | 30%   | 100 |
| 8 | SF2850          | 2850.0 | anchor | 1365.0 | 6400   | 21%   | 100 |
| 9 | SHAYVERI v1.0   | 2572.4 | 19.9   | 403.0  | 6400   | 6%    | --- |

White advantage: `160.28 +/- 2.84`

Draw rate: `37.05% +/- 0.53`

### STC Head-To-Head

| Pairing                          | Games | W-D-L       | Score | Elo Diff |
|----------------------------------|-------|-------------|-------|----------|
| SHAYVERI v2.5.0 vs SHAYVERI v1.0 | 800   | 688-64-48   | 90.0% | +380.0   |
| SHAYVERI v2.5.0 vs SF2850        | 800   | 452-145-203 | 65.6% | +102.4   |
| SHAYVERI v2.5.0 vs SF3000        | 800   | 234-197-369 | 41.6% | -47.6    |
| SHAYVERI v1.0 vs SF2850          | 800   | 148-111-541 | 25.4% | -277.6   |
| SHAYVERI v1.0 vs SF3000          | 800   | 56-92-652   | 12.8% | -427.6   |

## LTC Pool

Time control: `90+0.5`

Total games: `7200`

Per pairing: `200`

| #  | Player          | Rating | Error  | Points | Played | Score | CFS |
|----|-----------------|--------|--------|--------|--------|-------|-----|
| 1  | PlentyChess7    | 3696.8 | 41.2   | 1348.0 | 1600   | 84%   | 100 |
| 2  | Alexandria9     | 3655.7 | 39.7   | 1305.0 | 1600   | 82%   | 99  |
| 3  | Berserk13       | 3625.1 | 38.9   | 1271.5 | 1600   | 79%   | 100 |
| 4  | Weiss2          | 3364.3 | 32.3   | 945.5  | 1600   | 59%   | 100 |
| 5  | Ethereal14      | 3289.4 | 31.3   | 845.0  | 1600   | 53%   | 100 |
| 6  | SHAYVERI v2.5.0 | 3081.9 | 28.4   | 571.0  | 1600   | 36%   | 100 |
| 7  | SF3000          | 3000.0 | anchor | 503.5  | 1600   | 31%   | 100 |
| 8  | SF2850          | 2850.0 | anchor | 258.0  | 1600   | 16%   | 100 |
| 9  | SHAYVERI v1.0   | 2694.3 | 33.6   | 152.5  | 1600   | 10%   | --- |

White advantage: `191.22 +/- 5.52`

Draw rate: `45.82% +/- 1.15`

### LTC Head-To-Head

| Pairing                          | Games | W-D-L     | Score | Elo Diff |
|----------------------------------|-------|-----------|-------|----------|
| SHAYVERI v2.5.0 vs SHAYVERI v1.0 | 200   | 171-17-12 | 89.8% | +387.6   |
| SHAYVERI v2.5.0 vs SF2850        | 200   | 143-27-30 | 78.2% | +231.9   |
| SHAYVERI v2.5.0 vs SF3000        | 200   | 84-50-66  | 54.5% | +81.9    |
| SHAYVERI v1.0 vs SF2850          | 200   | 58-37-105 | 38.2% | -155.7   |
| SHAYVERI v1.0 vs SF3000          | 200   | 22-46-132 | 22.5% | -305.7   |

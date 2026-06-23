# SHAYVERI Elo

These ratings are from local multi-engine round-robin gauntlets analyzed with
Ordo. They are pool-relative and anchored to fixed-strength `SF2850=2850` and
`SF3000=3000`; they are not universal CCRL ratings.

Both SHAYVERI evaluation modes are included in the same pool:

- `SHAYVERI v1.0`: classical handcrafted evaluation.
- `SHAYVERI v2.2.0`: current NNUE, default file `SHAYVERI2_2_0.nnue`.

## Current Public Ratings

| Configuration   | Evaluation    | Time Control | Rating | Error   | Gap to SF2850 | Gap to SF3000 |
|-----------------|---------------|--------------|--------|---------|---------------|---------------|
| SHAYVERI v1.0   | Classical HCE | STC 10+0.1   | 2578.6 | +/-19.9 | -271.4        | -421.4        |
| SHAYVERI v1.0   | Classical HCE | LTC 90+0.5   | 2699.7 | +/-33.2 | -150.3        | -300.3        |
| SHAYVERI v2.2.0 | NNUE          | STC 10+0.1   | 2938.3 | +/-14.9 | +88.3         | -61.7         |
| SHAYVERI v2.2.0 | NNUE          | LTC 90+0.5   | 3046.2 | +/-28.6 | +196.2        | +46.2         |

On this shared scale, v2.2.0 is about **+359.7 Elo over v1.0 at STC** and
**+346.5 Elo over v1.0 at LTC**.

## STC Pool

Time control: `10+0.1`
Total games: `28800`
Per pairing: `800`

| # | Player          | Rating | Error  | Points | Played | Score | CFS |
|---|-----------------|--------|--------|--------|--------|-------|-----|
| 1 | PlentyChess7    | 3689.3 | 22.1   | 5470.0 | 6400   | 85%   | 100 |
| 2 | Alexandria9     | 3665.9 | 21.9   | 5377.5 | 6400   | 84%   | 100 |
| 3 | Berserk13       | 3610.5 | 20.5   | 5148.0 | 6400   | 80%   | 100 |
| 4 | Weiss2          | 3295.5 | 16.8   | 3633.0 | 6400   | 57%   | 100 |
| 5 | Ethereal14      | 3244.0 | 16.3   | 3369.5 | 6400   | 53%   | 100 |
| 6 | SF3000          | 3000.0 | anchor | 2190.5 | 6400   | 34%   | 100 |
| 7 | SHAYVERI v2.2.0 | 2938.3 | 14.9   | 1831.0 | 6400   | 29%   | 100 |
| 8 | SF2850          | 2850.0 | anchor | 1357.0 | 6400   | 21%   | 100 |
| 9 | SHAYVERI v1.0   | 2578.6 | 19.9   | 423.5  | 6400   | 7%    | --- |

White advantage: `167.14 +/- 2.92`
Draw rate: `37.45% +/- 0.54`
Game split: `14789` White wins, `4400` draws, `9611` Black wins, `0` discarded.

### STC Head-To-Head

| Pairing                          | Games | W-D-L       | Score | Elo Diff |
|----------------------------------|-------|-------------|-------|----------|
| SHAYVERI v2.2.0 vs SHAYVERI v1.0 | 800   | 662-73-65   | 87.3% | +359.7   |
| SHAYVERI v2.2.0 vs SF2850        | 800   | 427-139-234 | 62.1% | +88.3    |
| SHAYVERI v2.2.0 vs SF3000        | 800   | 219-220-361 | 41.1% | -61.7    |
| SHAYVERI v1.0 vs SF2850          | 800   | 149-85-566  | 23.9% | -271.4   |
| SHAYVERI v1.0 vs SF3000          | 800   | 49-112-639  | 13.1% | -421.4   |

## LTC Pool

Time control: `90+0.5`
Total games: `7200`
Per pairing: `200`

| #  | Player          | Rating | Error  | Points | Played | Score | CFS |
|----|-----------------|--------|--------|--------|--------|-------|-----|
| 1  | PlentyChess7    | 3675.0 | 40.5   | 1342.0 | 1600   | 84%   | 97  |
| 2  | Alexandria9     | 3649.4 | 39.4   | 1314.5 | 1600   | 82%   | 100 |
| 3  | Berserk13       | 3600.9 | 38.4   | 1260.0 | 1600   | 79%   | 100 |
| 4  | Weiss2          | 3363.3 | 32.3   | 957.5  | 1600   | 60%   | 100 |
| 5  | Ethereal14      | 3297.7 | 31.5   | 868.5  | 1600   | 54%   | 100 |
| 6  | SHAYVERI v2.2.0 | 3046.2 | 28.6   | 533.0  | 1600   | 33%   | 100 |
| 7  | SF3000          | 3000.0 | anchor | 520.0  | 1600   | 32%   | 100 |
| 8  | SF2850          | 2850.0 | anchor | 248.5  | 1600   | 16%   | 100 |
| 9  | SHAYVERI v1.0   | 2699.7 | 33.2   | 156.0  | 1600   | 10%   | --- |

White advantage: `181.74 +/- 5.46`
Draw rate: `44.42% +/- 1.13`
Game split: `3692` White wins, `1346` draws, `2162` Black wins, `0` discarded.

### LTC Head-To-Head

| Pairing                          | Games | W-D-L     | Score | Elo Diff |
|----------------------------------|-------|-----------|-------|----------|
| SHAYVERI v2.2.0 vs SHAYVERI v1.0 | 200   | 170-21-9  | 90.2% | +346.6   |
| SHAYVERI v2.2.0 vs SF2850        | 200   | 135-26-39 | 74.0% | +196.2   |
| SHAYVERI v2.2.0 vs SF3000        | 200   | 71-59-70  | 50.2% | +46.2    |
| SHAYVERI v1.0 vs SF2850          | 200   | 69-36-95  | 43.5% | -150.3   |
| SHAYVERI v1.0 vs SF3000          | 200   | 18-36-146 | 18.0% | -300.3   |

## Notes

For public reporting, the useful information is the Ordo rating table, selected
head-to-head rows against the anchors and v1.0, and the run summary: total games,
discarded games, white advantage, draw rate, and anchor confirmation.

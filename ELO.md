# Engine Benchmarks and Overview

This section provides the expected world-standard ratings (CCRL) and technical details for the engines used in this tournament. Ratings are mapped from CCRL Blitz (STC) and CCRL 40/15 (LTC) to provide a performance baseline.

### Expected World Ratings (CCRL)
| Engine Version    | Eval Type               | Expected STC (10+0.1) | Expected LTC (90+0.5) |
|-------------------|-------------------------|----------------------:|----------------------:|
| Stockfish 18      | NNUE                    |                 ~4100 |                 ~3650 |
| PlentyChess 7.0.0 | NNUE                    |                 ~4050 |                 ~3640 |
| Alexandria 9      | NNUE                    |                 ~4000 |                 ~3580 |
| Berserk 13        | NNUE                    |                 ~4010 |                 ~3510 |
| Ethereal 14       | Classical + NNUE Hybrid |                 ~3450 |                 ~3350 |
| Weiss 2.0         | Classical HCE           |                 ~3335 |                 ~3265 |
| SHAYVERI          | Classical HCE           |                 ~2650 |                 ~2735 |


*\*Note: While commercial versions of Ethereal 14 use NNUE, the open-source GitHub version typically defaults to its powerful Hand-Crafted Evaluation (HCE) unless a specific network is provided.*

---

### Engine Technical Profiles

All engines were compiled from source (Git) on the testing hardware.

* **Stockfish 18**: The world's strongest CPU engine. Uses a hybrid NNUE architecture.
* **PlentyChess 7.0.0**: A top-tier NNUE engine trained on over 15 billion positions.
* **Alexandria 9**: A bitboard-based engine. **Verified**: Version 9 uses NNUE (trained via Bullet trainer).
* **Berserk 13**: **Verified**: Uses a proprietary NNUE architecture (16-bucket mirrored).
* **Ethereal 14**: **Verified**: The open-source version is one of the strongest **HCE** engines. (NNUE is available in commercial/paid builds).
* **Weiss 2.0**: **Verified**: A pure **HCE** engine and descendant of the VICE engine.
* **SHAYVERI**: Developmental engine under test.

---

# SHAYVERI Elo Testing Results

## Overview

SHAYVERI was evaluated in a controlled multi-engine round-robin tournament using Ordo rating analysis. Two time controls were tested: **STC (10+0.1)** and **LTC (90+0.5)**. All engines ran single-threaded under identical hardware conditions.

Ratings are pool-relative and anchored to SF2850 = 2850 and SF3000 = 3000. They are **not** universal chess ratings.

---

## Final Ratings

### STC — 10+0.1 (17,000+ games per engine)

| # | Engine       | Rating     | Error     | Points   | Played  | Score % | CFS %     |
|---|--------------|------------|-----------|----------|---------|---------|-----------|
| 1 | PlentyChess  | 3686.2     | ±13.2     | 14279.0  | 17074   | 84%     | 100       |
| 2 | Alexandria   | 3669.9     | ±13.2     | 14092.0  | 17069   | 83%     | 100       |
| 3 | Berserk      | 3611.1     | ±12.6     | 13380.5  | 17072   | 78%     | 100       |
| 4 | Weiss        | 3280.4     | ±9.9      | 8856.0   | 17069   | 52%     | 100       |
| 5 | Ethereal     | 3224.2     | ±9.8      | 8074.5   | 17071   | 47%     | 100       |
| 6 | SF3000       | 3000.0     | anchor    | 5216.0   | 17073   | 31%     | 100       |
| 7 | SF2850       | 2850.0     | anchor    | 3078.5   | 17073   | 18%     | 100       |
| 8 | **SHAYVERI** | **2652.7** | **±11.5** | 1314.5   | 17081   | **8%**  | ----      |

**White advantage:** 169.43 ±1.85  
**Draw rate (equal opponents):** 40.47% ±0.38

---

### LTC — 90+0.5 (600 games per pairing, 4200 total per engine)

| # | Engine       | Rating     | Error     | Points | Played | Score % | CFS % |
|---|--------------|------------|-----------|--------|--------|---------|-------|
| 1 | PlentyChess  | 3698.0     | ±25.5     | 3487.5 | 4200   | 83%     | 100   |
| 2 | Alexandria   | 3668.1     | ±25.1     | 3399.0 | 4200   | 81%     | 100   |
| 3 | Berserk      | 3604.5     | ±24.0     | 3201.0 | 4200   | 76%     | 100   |
| 4 | Weiss        | 3363.4     | ±20.1     | 2371.0 | 4200   | 56%     | 100   |
| 5 | Ethereal     | 3279.3     | ±18.7     | 2074.5 | 4200   | 49%     | 100   |
| 6 | SF3000       | 3000.0     | anchor    | 1245.5 | 4200   | 30%     | 100   |
| 7 | SF2850       | 2850.0     | anchor    | 591.5  | 4200   | 14%     | 100   |
| 8 | **SHAYVERI** | **2734.5** | **±20.3** | 430.0  | 4200   | **10%** | ---   |

**White advantage:** 187.40 ±3.47  
**Draw rate (equal opponents):** 49.80% ±0.82

---

## SHAYVERI Summary

| Time Control | Rating    | Error | Gap to SF2850 | Gap to SF3000 |
|--------------|-----------|-------|---------------|---------------|
| STC 10+0.1   | 2652.7    | ±11.5 | −197.3        | −347.3        |
| LTC 90+0.5   | 2734.5    | ±20.3 | −115.5        | −265.5        |
| **Gain**     | **+81.8** | ----  | **+81.8**     | **+81.8**     |

SHAYVERI gains approximately **+82 Elo** moving from STC to LTC, suggesting it benefits meaningfully from deeper search. The gap to SF2850 closes from 197 points down to 116 at longer time controls.

---

## SHAYVERI Head-to-Head Results

### vs. SF2850

| TC  | Games | W   | D   | L    | Score % | Rating Diff | CFS % |
|-----|-------|-----|-----|------|---------|-------------|-------|
| STC | 2432  | 584 | 391 | 1457 | 32.1%   | −197.3      | 0.0   |
| LTC | 600   | 209 | 95  | 296  | 42.8%   | −115.5      | 0.0   |

### vs. SF3000

| TC  | Games | W   | D   | L    | Score % | Rating Diff | CFS % |
|-----|-------|-----|-----|------|---------|-------------|-------|
| STC | 2434  | 237 | 352 | 1845 | 17.0%   | −347.3      | 0.0   |
| LTC | 600   | 62  | 127 | 411  | 20.9%   | −265.5      | 0.0   |

### vs. All Opponents — STC

| Opponent    | Games | W    | D   | L    | Score % | Diff    | CFS % |
|-------------|-------|------|-----|------|---------|---------|-------|
| PlentyChess | 2450  | 0    | 43  | 2407 | 0.9%    | −1033.5 | 0.0   |
| Alexandria  | 2444  | 1    | 29  | 2414 | 0.6%    | −1017.3 | 0.0   |
| Berserk     | 2438  | 0    | 27  | 2411 | 0.6%    | −958.4  | 0.0   |
| Weiss       | 2442  | 4    | 51  | 2387 | 1.2%    | −627.7  | 0.0   |
| Ethereal    | 2441  | 7    | 70  | 2364 | 1.7%    | −571.6  | 0.0   |
| SF3000      | 2434  | 237  | 352 | 1845 | 17.0%   | −347.3  | 0.0   |
| SF2850      | 2432  | 584  | 391 | 1457 | 32.1%   | −197.3  | 0.0   |

### vs. All Opponents — LTC

| Opponent    | Games | W   | D   | L   | Score % | Diff    | CFS % |
|-------------|-------|-----|-----|-----|---------|---------|-------|
| PlentyChess | 600   | 0   | 16  | 584 | 1.3%    | −963.5  | 0.0   |
| Alexandria  | 600   | 0   | 20  | 580 | 1.7%    | −933.6  | 0.0   |
| Berserk     | 600   | 0   | 21  | 579 | 1.8%    | −870.0  | 0.0   |
| Weiss       | 600   | 1   | 13  | 586 | 1.2%    | −628.9  | 0.0   |
| Ethereal    | 600   | 1   | 22  | 577 | 2.0%    | −544.8  | 0.0   |
| SF3000      | 600   | 62  | 127 | 411 | 20.9%   | −265.5  | 0.0   |
| SF2850      | 600   | 209 | 95  | 296 | 42.8%   | −115.5  | 0.0   |

---

## Pool Rankings — Full Comparison

### STC Rating Gaps (relative to SF3000 = 3000)

| Engine      | Rating | vs SF3000 |
|-------------|--------|-----------|
| PlentyChess | 3686.2 | +686      |
| Alexandria  | 3669.9 | +670      |
| Berserk     | 3611.1 | +611      |
| Weiss       | 3280.4 | +280      |
| Ethereal    | 3224.2 | +224      |
| SF3000      | 3000.0 | 0         |
| SF2850      | 2850.0 | −150      |
| SHAYVERI    | 2652.7 | −347      |

### STC vs LTC Rating Shift by Engine

| Engine      | STC     | LTC     | Δ         |
|-------------|---------|---------|-----------|
| PlentyChess | 3686.2  | 3698.0  | +11.8     |
| Alexandria  | 3669.9  | 3668.1  | −1.8      |
| Berserk     | 3611.1  | 3604.5  | −6.6      |
| Weiss       | 3280.4  | 3363.4  | +83.0     |
| Ethereal    | 3224.2  | 3279.3  | +55.1     |
| SF3000      | 3000.0  | 3000.0  | 0.0       |
| SF2850      | 2850.0  | 2850.0  | 0.0       |
| SHAYVERI    | 2652.7  | 2734.5  | **+81.8** |

SHAYVERI's STC→LTC gain (+81.8) is the largest in the pool (excluding the fixed anchors), on par with Weiss (+83.0). This indicates SHAYVERI's eval or search benefits more from additional time than most pool engines.

---

## Draw Rate & White Advantage Analysis

| TC  | White Advantage | Draw Rate     |
|-----|-----------------|---------------|
| STC | 169.43 ±1.85    | 40.47% ±0.38% |
| LTC | 187.40 ±3.47    | 49.80% ±0.82% |

The white advantage is notably high at both time controls (typical engine testing shows ~15–35 Elo). This may reflect the UHO opening book providing unbalanced positions that favor White. Draw rate rises substantially at LTC (+9.3 pp), consistent with deeper search resolving positions more accurately and producing more drawn outcomes.

---

## Observations & Notes

**SHAYVERI's current strength** places it approximately 197 Elo below SF2850 at STC and 116 Elo below at LTC. It is not yet competitive with the mid-pool engines (Ethereal, Weiss) at either time control.

**LTC improvement** is encouraging. SHAYVERI's +82 Elo gain from STC to LTC is the largest relative gain in the pool, suggesting the engine's search or evaluation scales well with time and is not primarily relying on tactical shortcuts that faster engines exploit.

**SHAYVERI's draw profile** is unusual — at STC it scored almost exclusively losses against top engines (0 wins vs. PlentyChess, Alexandria, Berserk), with draws being the only survival mechanism against stronger opposition. Draw counts increase at LTC, which is a positive sign.

**Sample size:** STC results (~17,000 games per engine, ~2,400 per pairing) are statistically robust. LTC results (~600 per pairing) carry larger error margins (±20 Elo vs ±11 Elo) and should be treated as directionally reliable but not final.

---

## Testing Configuration Reference

| Parameter          | STC                             | LTC           |
|--------------------|---------------------------------|---------------|
| Time control       | 10+0.1                          | 90+0.5        |
| Threads            | 1                               | 1             |
| Hash               | 64 MB                           | 128 MB        |
| Opening book       | UHO_2024_8mvs_big_+085_+104.epd | same          |
| Paired games       | Yes (−repeat)                   | Yes (−repeat) |
| Total games (pool) | ~136,000                        | ~33,600       |
| Anchors            | SF2850=2850, SF3000=3000        | same          |
| Simulations        | 1000+                           | 1000+         |
| Confidence         | 99%                             | 99%           |

# Engine Benchmarks and Overview

This section provides the expected world-standard ratings (CCRL) and technical details for the engines used in this tournament. Ratings are mapped from CCRL Blitz/STC and CCRL 40/15-style LTC lists only as a sanity baseline. The Ordo ratings below are from this local gauntlet and should not be compared directly to CCRL numbers.

### Expected World Ratings (CCRL)
| Engine Version    | Eval Type               |   Expected STC (10+0.1) |   Expected LTC (90+0.5) |
|-------------------|-------------------------|------------------------:|------------------------:|
| PlentyChess 7.0.0 | NNUE                    |                   ~3750 |                   ~3610 |
| Alexandria 9      | NNUE                    |                   ~3760 |                   ~3630 |
| Berserk 13        | NNUE                    |                   ~3730 |                   ~3620 |
| Ethereal 14       | Classical HCE           |              ~3530-3710 |              ~3530-3580 |
| Weiss 2.0         | Classical HCE           |                   ~3325 |              ~3260-3380 |
| SHAYVERI v1.0     | Classical HCE           |                  2652.7 |                  2734.5 |
| SHAYVERI v2.2.0   | NNUE                    |                  2716.2 |                  2792.3 |


Notes:
- Weiss and the tested Ethereal build are **not NNUE**. The rest of the non-SHAYVERI opponents in this gauntlet are NNUE engines.
- `SF2850` and `SF3000` are fixed-strength Stockfish anchors, not full-strength Stockfish 18.
- Ethereal is awkward in rating tables because CCRL contains several Ethereal 14.x entries and CPU-count variants. For this gauntlet, treat the tested binary as the open/free HCE line, not the commercial NNUE line.
- CCRL references used for sanity only: CCRL 404 lists PlentyChess 7.0.0 around 3753, Alexandria 8.x around 3730-3764, Berserk 13 around 3732, Ethereal 14.00 around 3709 on an 8CPU entry, and Weiss 2.0 around 3325. CCRL 4040 lists PlentyChess 7.0.0 around 3606, Alexandria 8.x around 3588-3634, Berserk 13 around 3617, Ethereal 14.00 around 3531-3579 depending on CPU entry, and Weiss 2.0 around 3263-3381 depending on CPU entry.

---

### Engine Technical Profiles

All engines were compiled from source (Git) on the testing hardware.

* **Stockfish 18**: The world's strongest CPU engine. Uses a hybrid NNUE architecture.
* **PlentyChess 7.0.0**: A top-tier NNUE engine trained on over 15 billion positions.
* **Alexandria 9**: A bitboard-based engine. **Verified**: Version 9 uses NNUE (trained via Bullet trainer).
* **Berserk 13**: **Verified**: Uses a proprietary NNUE architecture (16-bucket mirrored).
* **Ethereal 14**: **Verified**: The tested binary is **HCE**. (NNUE is available in commercial/paid builds).
* **Weiss 2.0**: **Verified**: A pure **HCE** engine and descendant of the VICE engine.
* **SHAYVERI**: Developmental engine under test.

---

# SHAYVERI Elo Testing Results

## Overview

SHAYVERI was evaluated in controlled multi-engine round-robin tournaments using Ordo rating analysis. Two time controls were tested: **STC (10+0.1)** and **LTC (90+0.5)**. All engines ran single-threaded under identical hardware conditions.

Ratings are pool-relative and anchored to SF2850 = 2850 and SF3000 = 3000. They are **not** universal chess ratings.

Two SHAYVERI configurations are now recorded: **SHAYVERI HCE** and **SHAYVERI NNUE**.

The same opponent binaries and same tournament setup were used for both HCE and NNUE gauntlets. The opening book was `UHO_2024_8mvs.epd`, and Ordo was run as:

```bash
./ordo -p rating_pool.pgn -m anchors -W -D -s 5000 -n 8 -J -j h2h.txt -C cfs.csv -e err.csv -o results.txt -c results.csv -F 99
```

---

## HCE Final Ratings

### HCE STC — 10+0.1 (17,000+ games per engine)

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

### HCE LTC — 90+0.5 (600 games per pairing, 4200 total per engine)

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

## NNUE Final Ratings

### NNUE STC — 10+0.1 (~7,000 games per engine)

| # | Engine            | Rating     | Error     | Points  | Played | Score % | CFS % |
|---|-------------------|------------|-----------|---------|--------|---------|-------|
| 1 | PlentyChess       | 3674.2     | ±20.6     | 5827.5  | 6998   | 83%     | 91    |
| 2 | Alexandria        | 3665.7     | ±20.3     | 5789.0  | 6994   | 83%     | 100   |
| 3 | Berserk           | 3601.6     | ±19.3     | 5468.0  | 6998   | 78%     | 100   |
| 4 | Weiss             | 3280.7     | ±15.7     | 3634.0  | 7001   | 52%     | 100   |
| 5 | Ethereal          | 3226.0     | ±14.9     | 3312.5  | 6998   | 47%     | 100   |
| 6 | SF3000            | 3000.0     | anchor    | 2102.5  | 6998   | 30%     | 100   |
| 7 | SF2850            | 2850.0     | anchor    | 1174.5  | 6999   | 17%     | 100   |
| 8 | **SHAYVERI NNUE** | **2716.2** | **±16.4** | 688.0   | 7006   | **10%** | ---   |

**White advantage:** 165.15 ±2.85<br>
**Draw rate (equal opponents):** 40.48% ±0.56

### NNUE LTC — 90+0.5 (~950 games per engine)

| # | Engine            | Rating     | Error     | Points  | Played | Score % | CFS % |
|---|-------------------|------------|-----------|---------|--------|---------|-------|
| 1 | PlentyChess       | 3750.3     | ±57.6     | 779.0   | 942    | 83%     | 95    |
| 2 | Alexandria        | 3723.2     | ±57.1     | 770.5   | 943    | 82%     | 100   |
| 3 | Berserk           | 3675.9     | ±55.3     | 738.5   | 949    | 78%     | 100   |
| 4 | Weiss             | 3409.2     | ±45.8     | 536.5   | 943    | 57%     | 100   |
| 5 | Ethereal          | 3314.9     | ±42.8     | 468.5   | 944    | 50%     | 100   |
| 6 | SF3000            | 3000.0     | anchor    | 251.5   | 948    | 27%     | 100   |
| 7 | SF2850            | 2850.0     | anchor    | 128.5   | 947    | 14%     | 100   |
| 8 | **SHAYVERI NNUE** | **2792.3** | **±41.5** | 111.0   | 952    | **12%** | ---   |

**White advantage:** 190.63 ±7.83<br>
**Draw rate (equal opponents):** 49.19% ±1.76

---

## SHAYVERI HCE vs NNUE Summary

| Eval | Time Control | Rating | Error | Gap to SF2850 | Gap to SF3000 |
|------|--------------|-------:|------:|--------------:|--------------:|
| HCE  | STC 10+0.1   | 2652.7 | ±11.5 |        −197.3 |        −347.3 |
| NNUE | STC 10+0.1   | 2716.2 | ±16.4 |        −133.8 |        −283.8 |
| HCE  | LTC 90+0.5   | 2734.5 | ±20.3 |        −115.5 |        −265.5 |
| NNUE | LTC 90+0.5   | 2792.3 | ±41.5 |         −57.7 |        −207.7 |

| Comparison    |     STC Δ |     LTC Δ |
|---------------|----------:|----------:|
| NNUE over HCE | **+63.5** | **+57.8** |

NNUE is a real improvement over HCE in this pool. The gauntlet says SHAYVERI remains below SF2850 at both time controls, though it gets close at LTC.

---

## Original HCE SHAYVERI Summary

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

## SHAYVERI NNUE Head-to-Head Results

### vs. SF2850

| TC  | Games | W   | D   | L   | Score % | Rating Diff | CFS % |
|-----|-------|-----|-----|-----|---------|-------------|-------|
| STC | 990   | 298 | 198 | 494 | 40.1%   | −133.8      | 0.0   |
| LTC | 124   | 45  | 32  | 47  | 49.2%   | −57.7       | 0.0   |

### vs. SF3000

| TC  | Games | W   | D   | L   | Score % | Rating Diff | CFS % |
|-----|-------|-----|-----|-----|---------|-------------|-------|
| STC | 996   | 115 | 191 | 690 | 21.1%   | −283.8      | 0.0   |
| LTC | 131   | 20  | 33  | 78  | 27.9%   | −207.7      | 0.0   |

### vs. All Opponents — STC

| Opponent    | Games | W   | D   | L   | Score % | Diff    | CFS % |
|-------------|-------|-----|-----|-----|---------|---------|-------|
| PlentyChess | 1010  | 0   | 24  | 986 | 1.2%    | −958.0  | 0.0   |
| Alexandria  | 1005  | 1   | 21  | 983 | 1.1%    | −949.5  | 0.0   |
| Berserk     | 1000  | 0   | 8   | 992 | 0.4%    | −885.4  | 0.0   |
| Weiss       | 1003  | 3   | 39  | 961 | 2.2%    | −564.5  | 0.0   |
| Ethereal    | 1002  | 5   | 51  | 946 | 3.0%    | −509.8  | 0.0   |
| SF3000      | 996   | 115 | 191 | 690 | 21.1%   | −283.8  | 0.0   |
| SF2850      | 990   | 298 | 198 | 494 | 40.1%   | −133.8  | 0.0   |

### vs. All Opponents — LTC

| Opponent    | Games | W  | D  | L   | Score % | Diff    | CFS % |
|-------------|-------|----|----|-----|---------|---------|-------|
| PlentyChess | 142   | 0  | 7  | 135 | 2.5%    | −958.0  | 0.0   |
| Alexandria  | 140   | 0  | 3  | 137 | 1.1%    | −930.9  | 0.0   |
| Berserk     | 135   | 0  | 6  | 129 | 2.2%    | −883.6  | 0.0   |
| Weiss       | 140   | 1  | 3  | 136 | 1.8%    | −616.9  | 0.0   |
| Ethereal    | 140   | 0  | 6  | 134 | 2.1%    | −522.6  | 0.0   |
| SF3000      | 131   | 20 | 33 | 78  | 27.9%   | −207.7  | 0.0   |
| SF2850      | 124   | 45 | 32 | 47  | 49.2%   | −57.7   | 0.0   |

---

## Pool Rankings — Full Comparison

### HCE STC Rating Gaps (relative to SF3000 = 3000)

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

### NNUE STC Rating Gaps (relative to SF3000 = 3000)

| Engine      | Rating | vs SF3000 |
|-------------|--------|-----------|
| PlentyChess | 3674.2 | +674      |
| Alexandria  | 3665.7 | +666      |
| Berserk     | 3601.6 | +602      |
| Weiss       | 3280.7 | +281      |
| Ethereal    | 3226.0 | +226      |
| SF3000      | 3000.0 | 0         |
| SF2850      | 2850.0 | −150      |
| SHAYVERI    | 2716.2 | −284      |

### HCE STC vs LTC Rating Shift by Engine

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

### NNUE STC vs LTC Rating Shift by Engine

| Engine      | STC     | LTC     | Δ         |
|-------------|---------|---------|-----------|
| PlentyChess | 3674.2  | 3750.3  | +76.1     |
| Alexandria  | 3665.7  | 3723.2  | +57.5     |
| Berserk     | 3601.6  | 3675.9  | +74.3     |
| Weiss       | 3280.7  | 3409.2  | +128.5    |
| Ethereal    | 3226.0  | 3314.9  | +88.9     |
| SF3000      | 3000.0  | 3000.0  | 0.0       |
| SF2850      | 2850.0  | 2850.0  | 0.0       |
| SHAYVERI    | 2716.2  | 2792.3  | **+76.1** |

NNUE keeps the same broad STC→LTC pattern as HCE. It scales better with time than the fixed anchors, but not enough to close the gap to SF3000 or the HCE mid-pool engines.

---

## Draw Rate & White Advantage Analysis

| Eval | TC  | White Advantage | Draw Rate     |
|------|-----|-----------------|---------------|
| HCE  | STC | 169.43 ±1.85    | 40.47% ±0.38% |
| HCE  | LTC | 187.40 ±3.47    | 49.80% ±0.82% |
| NNUE | STC | 165.15 ±2.85    | 40.48% ±0.56% |
| NNUE | LTC | 190.63 ±7.83    | 49.19% ±1.76% |

The white advantage is notably high at both time controls (typical engine testing shows ~15–35 Elo). This may reflect the UHO opening book providing unbalanced positions that favor White. Draw rate rises substantially at LTC (+9.3 pp), consistent with deeper search resolving positions more accurately and producing more drawn outcomes.

---

## Observations & Notes

**SHAYVERI HCE strength** places it approximately 197 Elo below SF2850 at STC and 116 Elo below at LTC.

**SHAYVERI NNUE strength** places it approximately 134 Elo below SF2850 at STC and 58 Elo below SF2850 at LTC. It is still not competitive with the mid-pool HCE engines Ethereal and Weiss, but it is clearly closer to the anchors than HCE.

**NNUE gain:** NNUE adds about +64 Elo at STC and +58 Elo at LTC over HCE in this anchored pool. This is a real gain, but it is nowhere near the +300 Elo hoped for from the NNUE project.

**LTC improvement** is encouraging. SHAYVERI's +82 Elo gain from STC to LTC is the largest relative gain in the pool, suggesting the engine's search or evaluation scales well with time and is not primarily relying on tactical shortcuts that faster engines exploit.

**SHAYVERI's draw profile** is unusual — at STC it scored almost exclusively losses against top engines (0 wins vs. PlentyChess, Alexandria, Berserk), with draws being the only survival mechanism against stronger opposition. Draw counts increase at LTC, which is a positive sign.

**Sample size:** HCE STC results (~17,000 games per engine, ~2,400 per pairing) are statistically robust. HCE LTC results (~600 per pairing) carry larger error margins. NNUE STC results (~7,000 games per engine, ~1,000 per pairing) are solid. NNUE LTC results (~950 games per engine, ~125-140 per pairing) are directionally useful but still noisy.

---

## Testing Configuration Reference

| Parameter          | STC                             | LTC           |
|--------------------|---------------------------------|---------------|
| Time control       | 10+0.1                          | 90+0.5        |
| Threads            | 1                               | 1             |
| Hash               | 64 MB                           | 128 MB        |
| Opening book       | UHO_2024_8mvs.epd               | same          |
| Paired games       | Yes (−repeat)                   | Yes (−repeat) |
| HCE total games    | ~136,000                        | ~33,600       |
| NNUE total games   | ~56,000                         | ~7,600        |
| Anchors            | SF2850=2850, SF3000=3000        | same          |
| Simulations        | 1000+                           | 1000+         |
| Confidence         | 99%                             | 99%           |

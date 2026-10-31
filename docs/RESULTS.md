# Results

All numbers below come from `build/campaign` runs of **v2** of the model: own home bases, a 14 × 16 h daily window (06:00–22:00), time decay and the soft clash rule (see [CHANGES_V2.md](CHANGES_V2.md)). The raw CSVs are in [results/sweep/](../results/sweep/). The v1 results are in git history (commit `94aa4dd`). They used a different reach model, so they can't be compared number for number.

**Note (2026-10-31):** every section and every CSV in `results/sweep/` was re-run after the daily window was widened from 08:00–20:00 (12 h) to 06:00–22:00 (16 h), so the campaign has 224 active hours instead of 168 (ASSUMPTIONS V3). This changed every number, and it changed one conclusion: budgeted nearest neighbour now beats A at the defaults (finding 2). Figures in earlier commits and issue comments used 12-hour days and can't be compared with these, except the 14 × 12 h rows of section 6, which reproduce them.

**Versions of B (2026-10-31):** five experiments from the literature (GitHub issues #13–#17) are kept as their own approaches, so the results show what each one adds: `ils-cache`, `ils-revert`, `ils-alns`, `ils-moves`, `ils-full` and `multistart`. Sections 1–7 compare the benchmarks, A and **B as published** (`ils`, unchanged); section 8 compares the versions of B. Every CSV in `results/sweep/` was re-run with all of them; the reach of the approaches that already existed is unchanged.

Runtimes are from a single run on an Apple Silicon Mac (Apple clang 21, `-O2`). They will differ on other machines and vary a little between runs. The other numbers are deterministic.

Column meanings:
- **Reach:** people reached, in millions, with time decay and clash penalties applied.
- **% max:** reach as a share of the bound where every politician rallies in every town and reaches its whole population (no decay, no penalty). No real plan gets near 100%, because the polling-day weight alone cuts early rallies to about 0.4.
- **NN:** the budgeted nearest-neighbour benchmark (`nearest-budget`): the brief's classic heuristic, stopping each politician when no nearby town leaves time to get home.
- **Rallies:** total number of rallies across all politicians.
- **Clashes:** pairs of rallies in the same town at overlapping times. They are allowed, but they cost reach.
- **Travel / wait:** total hours driving or flying, and hours spent waiting (for a town to be free, or for the next morning because a rally wouldn't finish before 22:00).
- **Fairness:** lowest politician's reach ÷ highest politician's reach.

## 1. Default run

90 Peninsular towns, k = 2 (homes: Kuala Lumpur and George Town), alpha = 0.5, 14 days × 16 h, tauRepeat = 48 h, tauPoll = 336 h, beta = 0.5.

| Approach | Reach | % max | Rallies | Clashes | Travel (h) | Wait (h) | Fairness | Valid | Runtime |
|---|---|---|---|---|---|---|---|---|---|
| Classic nearest neighbour, full tour | 21.58M | 50.5% | 180 | 1 (1.1 h) | 94.8 | 28.0 | 0.91 | **no** | < 0.1 ms |
| Classic nearest neighbour, budgeted | 21.14M | 49.5% | 154 | 1 (1.1 h) | 82.1 | 23.2 | 0.89 | yes | < 0.1 ms |
| Uncoordinated (baseline) | 15.08M | 35.3% | 124 | **13** (34.2 h) | 140.5 | 13.1 | 0.72 | yes | 0.5 ms |
| A: one at a time | 18.36M | 43.0% | 122 | 0 | 148.8 | 8.9 | 0.87 | yes | 0.7 ms |
| B: iterated local search, as published | **23.62M** | **55.3%** | 113 | 0 | 172.8 | 4.7 | **0.92** | yes | ~150 ms |
| B, final version (`ils-full`, section 8) | 26.71M | 62.5% | 135 | 0 | 130.9 | 1.2 | 0.95 | yes | ~740 ms |

- **Full tour:** invalid. Each tour visits all 90 towns and needs **260.5 active hours (16.3 days of 16 h)** to get everyone home, against a 224 h campaign (260.5 h for the KL politician, 258.7 h for George Town). 22 of its 180 rallies are after polling day and reach nobody (#4), so its reach comes from the first 158 alone.
- **Budgeted nearest neighbour:** the same rule, but each politician stops when no town leaves time to get home. It is valid and reaches 21.14M, **15% more than A**. It holds more rallies than A (154 vs 122) and drives a little over half as far (82 h vs 149 h), because the big towns cluster around KL, JB and Penang. Its two routes meet once, in one 1.1 h clash.
- **Baseline:** both politicians plan as if alone and end up in some of the same towns at the same times. The 13 clashes cost it reach: it reaches 18% less than A and 29% less than budgeted nearest neighbour, and its fairness is the lowest (0.72).
- **A:** valid, never chooses to clash, and fairness is 0.87. It now falls behind budgeted nearest neighbour, with 32 fewer rallies and 1.8 times the driving.
- **B:** reaches 29% more than A and 12% more than budgeted nearest neighbour, with the best fairness (0.92). It travels the most (173 h). The polling-day weight rewards rallying in big towns late, and B is better at fitting those in. Its result here comes from the search that starts from empty routes; the one that starts from A's plan reaches 23.08M.

The route map for this run is `results/default/campaign_map.html` (see [viz/README.md](../viz/README.md)). It can show the budgeted nearest-neighbour plan, the baseline, A, multi-start A and every version of B except `ils-cache`, whose routes are the same as B's.

## 2. Across settings

90 towns, default homes and decay. k ∈ {2, 3, 5}, alpha ∈ {0, 0.5, 1}, campaign of 6 or 14 days. Reach is in millions. Every NN, baseline, A and B plan was valid. A clashed 3 times in both k = 5, alpha = 1 runs; B clashed 3 times in the 6-day one and never otherwise.

| k | alpha | days | NN reach | A reach | B reach | A / NN | B / A | A fairness | B fairness | A wait (h) | B wait (h) | Baseline reach (clashes) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 6 | 12.98 | 17.43 | 17.89 | 1.343 | 1.026 | 0.95 | 0.99 | 4.8 | 4.8 | 10.62 (10) |
| 2 | 0 | 14 | 20.12 | 18.26 | 22.71 | **0.907** | 1.244 | 0.89 | 0.98 | 11.3 | 3.2 | 12.45 (13) |
| 2 | 0.5 | 6 | 13.64 | 17.69 | 18.55 | 1.297 | 1.049 | 0.89 | 0.89 | 6.9 | 1.5 | 12.48 (10) |
| 2 | 0.5 | 14 | 21.14 | 18.36 | 23.62 | **0.868** | 1.287 | 0.87 | 0.92 | 8.9 | 4.7 | 15.08 (13) |
| 2 | 1 | 6 | 14.30 | 19.57 | 20.00 | 1.369 | 1.022 | 0.95 | 0.95 | 6.5 | 2.1 | 14.35 (10) |
| 2 | 1 | 14 | 22.15 | 21.02 | 24.46 | **0.949** | 1.164 | 0.99 | 1.00 | 9.7 | 5.0 | 17.70 (13) |
| 3 | 0 | 6 | 19.35 | 21.62 | 22.23 | 1.117 | 1.028 | 0.68 | 0.77 | 8.0 | 4.8 | 12.57 (10) |
| 3 | 0 | 14 | 26.67 | 24.38 | 29.65 | **0.914** | 1.216 | 0.75 | 0.86 | 14.2 | 6.2 | 13.04 (20) |
| 3 | 0.5 | 6 | 20.55 | 23.46 | 24.37 | 1.142 | 1.039 | 0.68 | 0.86 | 6.9 | 4.7 | 17.23 (10) |
| 3 | 0.5 | 14 | 30.04 | 25.14 | 32.14 | **0.837** | 1.278 | 0.75 | 0.89 | 11.3 | 6.5 | 19.21 (20) |
| 3 | 1 | 6 | 21.74 | 29.29 | 29.61 | 1.347 | 1.011 | 0.95 | 0.95 | 8.6 | 4.2 | 24.06 (10) |
| 3 | 1 | 14 | 33.42 | 31.39 | 35.50 | **0.939** | 1.131 | 0.98 | 0.96 | 13.9 | 7.2 | 27.63 (20) |
| 5 | 0 | 6 | 20.86 | 28.69 | 29.09 | 1.375 | 1.014 | **0.44** | **0.41** | 10.6 | 6.2 | 14.26 (27) |
| 5 | 0 | 14 | 29.01 | 35.52 | 40.12 | 1.224 | 1.130 | 0.66 | 0.69 | 28.4 | 16.7 | 15.39 (43) |
| 5 | 0.5 | 6 | 28.71 | 32.42 | 34.34 | 1.129 | 1.059 | 0.52 | 0.62 | 13.4 | 9.3 | 22.16 (27) |
| 5 | 0.5 | 14 | 40.89 | 36.66 | 46.46 | **0.897** | 1.268 | 0.67 | 0.86 | 24.3 | 10.9 | 26.08 (43) |
| 5 | 1 | 6 | 37.56 | 48.16 | 48.94 | 1.282 | 1.016 | 0.94 | 0.93 | 19.0 | 7.7 | 38.39 (27) |
| 5 | 1 | 14 | 54.11 | 51.82 | 58.59 | **0.958** | 1.131 | 0.97 | 0.89 | 29.0 | 17.8 | 45.26 (43) |

- **Reach rises steadily with alpha and with campaign length** for A and B in every row.
- **A's lead over budgeted nearest neighbour depends on the campaign length.** Over 6 days A is 12–38% ahead. Over 14 days it is behind in 8 of the 9 settings, by 4–16%, and ahead only at k = 5 with alpha = 0 (by 22%). In a long campaign, nearest neighbour fits in many more rallies with little driving, and A's reach-per-hour choice, made from day 1 when the polling-day weight is lowest, doesn't make up for it.
- **B beats A in every setting,** by 1.1–28.7%, and budgeted nearest neighbour by 6–40%.
- **Budgeted nearest neighbour now depends on alpha** even at k = 2 over 6 days. Alpha only matters when a town gets more than one rally, so its routes now share towns; with 12-hour days they didn't.

## 3. Soft clash rule: does anyone choose to clash?

k = 2, alpha = 0.5, 14 days. Shared home (both in KL, the case most likely to clash) vs the default homes.

| beta | Homes | Baseline reach (clashes) | A reach | A clashes | A wait (h) | B reach | B clashes | B wait (h) |
|---|---|---|---|---|---|---|---|---|
| 0 | KL, KL | 20.93 (64) | 20.93 | **64** | 10.3 | 23.54 | 0 | 6.4 |
| 0 | KL, George Town | 18.31 (13) | 18.92 | **2** | 12.2 | 23.16 | 0 | 4.0 |
| 0.25 | KL, KL | 15.70 (64) | 18.31 | **7** | 7.7 | 23.04 | 0 | 5.2 |
| 0.25 | KL, George Town | 16.69 (13) | 18.36 | 0 | 8.9 | 23.62 | 0 | 4.7 |
| 0.5 | KL, KL | 10.47 (64) | 18.43 | 0 | 10.6 | 23.46 | 0 | 2.8 |
| 0.5 | KL, George Town | 15.08 (13) | 18.36 | 0 | 8.9 | 23.62 | 0 | 4.7 |
| 1 | KL, KL | 0.00 (64) | 18.43 | 0 | 10.6 | 23.46 | 0 | 2.8 |
| 1 | KL, George Town | 11.85 (13) | 18.36 | 0 | 8.9 | 23.62 | 0 | 4.7 |

- **Clashing is a real trade-off.** While two rallies overlap, the crowd is split (beta) instead of counted as already reached (ASSUMPTIONS V12). A full overlap reaches `2 × (1 − beta)` crowds in total, against `1 + alpha` for two rallies one after the other, so on reach alone clashing wins when beta < (1 − alpha) / 2 (0.25 at the default alpha). A clashes at or below that, and never above it. At beta = 0.25 exactly, the two options tie on reach.
- **At beta = 0 a full overlap costs nothing**, so A with both homes in KL follows the same route as the baseline (64 clashes). With separate homes, A clashes twice.
- **B's best plan never clashes in this table,** even when clashing is free. With 16-hour days it finds enough free slots and other towns to do better without (23.54M vs A's 20.93M at beta = 0 from a shared home). With 12-hour days it clashed 37 times in that cell.
- **The baseline shows what coordination is worth.** At beta = 1 with a shared home, both uncoordinated politicians follow identical routes, overlap completely, and reach nobody.

## 4. Starting points

Default settings. Fixed homes vs five random seeds (`--starts random --seed N`).

| Homes | NN reach | A reach | A fairness | A travel (h) | B reach | B fairness | B travel (h) |
|---|---|---|---|---|---|---|---|
| Kuala Lumpur, George Town (fixed) | 21.14 | 18.36 | 0.87 | 149 | 23.62 | 0.92 | 173 |
| Yan Besar, Kampong Baharu Cheras Batu Sebelas (seed 1) | 20.81 | 17.95 | 0.99 | 157 | 22.96 | 0.97 | 186 |
| Setia Alam, Hulu Langat (seed 2) | 13.04 | 18.43 | 0.88 | 137 | 23.10 | 0.99 | 162 |
| Parit Sulung, Paya Terubong (seed 3) | 20.66 | 18.47 | 0.90 | 159 | 22.90 | 0.97 | 198 |
| Bukit Mertajam, Kampung Sungai Ara (seed 4) | 11.82 | 18.71 | 0.90 | 151 | 22.73 | 0.87 | 195 |
| Simpang Empat, Kuantan (seed 5) | 19.32 | 18.49 | 0.89 | 153 | 23.36 | 0.93 | 174 |

Reach barely depends on where politicians start for A and B: each varies by about 4% across the six. Fourteen days is long enough to reach the big towns from anywhere on the peninsula. B beats A and NN for every set of homes. NN varies far more (11.82M to 21.14M), and the fixed homes are its best case: it does worst when the two homes are close (Bukit Mertajam and Kampung Sungai Ara, 22 km apart, reach 11.82M).

### 4.1 Homes weighted by population, k = 2 to 10

Default settings except k. For each k from 2 to 10, ten runs with `--starts weighted --seed 1` to `10`. These draw k distinct rally towns, each with probability proportional to its population, so big towns (KL, Puchong, JB) are home more often and many teams have two politicians in the Klang Valley. The same seed gives the same team plus one more politician at the next k, so each k adds one politician to the same ten teams. Raw rows: [results/sweep/weighted_starts.csv](../results/sweep/weighted_starts.csv); the tables come from `python3 scripts/average_by_k.py results/sweep/weighted_starts.csv`. All 900 plans (NN, the baseline, A, multi-start A and the six versions of B) are valid.

Mean reach in millions over the ten runs (standard deviation in brackets):

| k | NN | Uncoordinated | A | B |
|---|---|---|---|---|
| 2 | 19.31 (2.74) | 14.95 (2.93) | 18.39 (0.21) | **22.96** (0.47) |
| 3 | 26.40 (4.57) | 17.17 (3.45) | 25.12 (0.40) | **32.18** (0.58) |
| 4 | 30.83 (6.21) | 18.32 (4.21) | 31.13 (0.47) | **40.16** (0.43) |
| 5 | 33.96 (5.42) | 18.71 (3.63) | 36.66 (0.55) | **46.49** (0.43) |
| 6 | 36.21 (5.64) | 18.78 (3.51) | 41.73 (0.84) | **51.83** (0.63) |
| 7 | 36.98 (6.10) | 18.30 (2.74) | 46.16 (0.94) | **56.59** (0.46) |
| 8 | 39.02 (5.65) | 18.16 (2.87) | 50.27 (1.10) | **60.36** (0.68) |
| 9 | 39.76 (5.56) | 17.56 (2.70) | 54.00 (1.28) | **64.24** (0.59) |
| 10 | 40.76 (5.58) | 17.31 (2.63) | 56.98 (1.46) | **67.35** (0.60) |

Leads, as the mean over the ten runs (lowest to highest in brackets):

| k | B vs A | B vs NN | A vs NN | Runs where A beats NN |
|---|---|---|---|---|
| 2 | +24.9% (+18.9 to +28.5) | +21.7% (+4.4 to +62.7) | −2.7% (−14.5 to +29.9) | 3 of 10 |
| 3 | +28.1% (+23.4 to +30.6) | +26.4% (+6.5 to +100.7) | −1.4% (−16.5 to +56.6) | 3 of 10 |
| 4 | +29.0% (+26.1 to +32.8) | +37.8% (+10.5 to +151.3) | +6.9% (−13.9 to +96.7) | 4 of 10 |
| 5 | +26.8% (+24.3 to +29.2) | +40.7% (+15.9 to +88.3) | +10.8% (−9.8 to +46.9) | 6 of 10 |
| 6 | +24.3% (+19.3 to +29.0) | +46.9% (+17.5 to +106.2) | +18.1% (−7.2 to +64.4) | 9 of 10 |
| 7 | +22.6% (+16.8 to +27.1) | +57.2% (+26.7 to +113.7) | +28.2% (+1.3 to +72.2) | 10 of 10 |
| 8 | +20.1% (+13.9 to +25.1) | +57.8% (+33.5 to +94.5) | +31.5% (+10.6 to +61.6) | 10 of 10 |
| 9 | +19.0% (+15.3 to +21.8) | +64.7% (+33.9 to +106.3) | +38.3% (+14.3 to +70.4) | 10 of 10 |
| 10 | +18.3% (+11.3 to +21.8) | +68.3% (+41.4 to +114.8) | +42.3% (+18.2 to +77.6) | 10 of 10 |

Mean fairness and clashes:

| k | NN fairness | Uncoord. fairness | A fairness | B fairness | NN clashes | Uncoord. clashes | A clashes | B clashes | B runtime |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 0.91 | 0.87 | 0.91 | 0.97 | 6.6 | 22.3 | 0.1 | 0.0 | 0.18 s |
| 4 | 0.64 | 0.59 | 0.78 | 0.91 | 46.6 | 124.0 | 0.3 | 0.0 | 0.55 s |
| 6 | 0.45 | 0.41 | 0.67 | 0.85 | 140.6 | 287.1 | 0.5 | 0.1 | 1.03 s |
| 8 | 0.39 | 0.24 | 0.58 | 0.79 | 233.0 | 553.1 | 0.5 | 0.5 | 1.08 s |
| 10 | 0.31 | 0.18 | 0.48 | 0.74 | 372.8 | 1002.8 | 0.6 | 0.5 | 1.93 s |

(The script prints every k; odd k follow the same trend.)

- **B beats A and NN in all 90 runs,** by at least 11.3% over A and 4.4% over NN. Its lead over A peaks at 29.0% at k = 4 and falls to 18.3% at k = 10.
- **NN beats A on average at k = 2 and 3; A is ahead from k = 4, and in every run from k = 7.** NN wins 25 of the 50 runs at k = 2 to 6, with A up to 16.5% behind. A's lead over NN grows to 42% at k = 10 as NN's clashes grow from 7 to 373: NN's politicians ignore each other.
- **The default homes favour NN.** At k = 2, NN's mean is 19.31M against 21.14M with the fixed KL and George Town homes, which are 300 km apart. NN does worst when two homes are close: its lowest run at k = 2, Selayang Baru Utara and Hulu Langat (23 km apart, both next to KL), reaches 14.19M with 32 clashes, while A reaches 18.43M. A's and B's reach hardly depends on the homes (standard deviation at most 3% of the mean).
- **Uncoordinated planning stops gaining after k = 6.** It falls from 18.78M at k = 6 to 17.31M at k = 10, while its clashes grow from 287 to 1,003.
- **Fairness falls with k for every approach,** and B is the fairest at every k (0.74 at k = 10, against 0.48 for A).

## 5. Time-decay parameters

Default settings, k = 2. Reach in millions. tauPoll = 100,000 h effectively turns the polling-day weight off.

| tauRepeat | tauPoll = 168 h | tauPoll = 336 h | tauPoll off |
|---|---|---|---|
| 12 h | NN 14.33 / A **11.72** / B 17.83 | NN 22.07 / A **20.35** / B 24.81 | NN 36.67 / A 37.33 / B 37.57 |
| 48 h (default) | NN 13.70 / A **10.71** / B 16.58 | NN 21.14 / A **18.36** / B 23.62 | NN 35.22 / A **33.98** / B 35.45 |
| 168 h | NN 12.45 / A **9.74** / B 14.53 | NN 19.23 / A **17.11** / B 21.30 | NN 32.22 / A **31.01** / B 32.58 |

Bold: A is behind budgeted nearest neighbour.

- **tauPoll has the most effect on reach.** Halving it from 336 h to 168 h cuts reach by 28–43%, and switching it off raises reach about 1.5–1.85×.
- **Faster repeat fading (smaller tauRepeat) helps a little,** because second rallies days apart become worth almost a full crowd.
- **The stronger the polling-day weight, the worse A does against nearest neighbour.** With tauPoll = 168 h, budgeted nearest neighbour beats A by 18–22%; at 336 h, by 8–13%. A is ahead in only one cell, with the weight off and tauRepeat = 12 h (by 1.8%). A's reach-per-hour choice doesn't look ahead to the rallies that will be worth more later.
- **B's advantage also depends on the polling-day weight.** With it on, B beats A by 22–55% and NN by 11–24%. With it off, B is only 0.6–5.1% ahead of A and 0.7–2.5% ahead of NN, and at tauRepeat = 12 h B's search from empty routes loses to A (−1.9%); B's figure in that cell comes from its second run, which starts from A's plan (#7). Ranking insertions on the exact knock-on reach was tried and didn't close the gap (see [DESIGN.md §6.3](DESIGN.md#63-approach-b-iterated-local-search-solveils)).

## 6. Daily window

Default settings, with four campaign clocks. 7 × 24 h and 14 × 12 h have the same 168 active hours (7 × 24 h is v1's continuous clock); 14 × 12 h was the default before the window was widened; 14 × 16 h is the default now; 14 × 24 h drops the nights altogether.

| Clock | Active hours | Approach | Reach | Rallies | Wait (h) | Travel (h) |
|---|---|---|---|---|---|---|
| 7 × 24 h | 168 | NN | 20.44 | 117 | 14.5 | 59.0 |
| 7 × 24 h | 168 | A | 21.24 | 87 | 7.1 | 109.3 |
| 7 × 24 h | 168 | B | 24.31 | 83 | 2.8 | 123.0 |
| 14 × 12 h | 168 | NN | 16.89 | 115 | 20.6 | 57.9 |
| 14 × 12 h | 168 | A | 17.06 | 84 | 8.8 | 112.8 |
| 14 × 12 h | 168 | B | 19.96 | 80 | 1.6 | 133.8 |
| 14 × 16 h | 224 | NN | 21.14 | 154 | 23.2 | 82.1 |
| 14 × 16 h | 224 | A | 18.36 | 122 | 8.9 | 148.8 |
| 14 × 16 h | 224 | B | 23.62 | 113 | 4.7 | 172.8 |
| 14 × 24 h | 336 | NN | 20.43 | 180 | 22.4 | 94.8 |
| 14 × 24 h | 336 | A | 18.18 | 180 | 10.5 | 245.9 |
| 14 × 24 h | 336 | B | 26.02 | 166 | 11.8 | 289.5 |

- **Same hours, with and without nights (7 × 24 h vs 14 × 12 h).** The daily window increases waiting for nearest neighbour (14.5 h to 20.6 h) and A (7.1 h to 8.8 h), because a rally that won't finish in the window has to wait until the next morning. B's waiting falls (2.8 h to 1.6 h), because it can fit rallies around the evenings. Reach is lower partly because of the window and partly because a 14-day campaign spreads rallies over a longer stretch of the polling-day weight than a 7-day one. These two effects are not separated here.
- **Widening the window from 12 h to 16 h** (a third more active hours) raises NN's reach by 25%, B's by 18%, and A's by only 8%, although A holds 38 more rallies. This is what puts NN ahead of A at the defaults.
- **With 24-hour days,** NN and A rally in every town (180 rallies), yet both reach slightly less than with 16-hour days. Only B gains (26.02M). We haven't looked into why.

## 7. Scaling with instance size

k = 2, default settings. Larger instances were made by lowering the population cut-off.

| Towns | NN reach | A reach | B reach | A runtime | B runtime |
|---|---|---|---|---|---|
| 15 | 7.15M | 7.24M | 9.41M | < 0.1 ms | 4 ms |
| 30 | 12.49M | 12.34M | 17.98M | 0.1 ms | 7 ms |
| 60 | 20.68M | 18.05M | 22.92M | 0.4 ms | 60 ms |
| 90 | 21.14M | 18.36M | 23.62M | 0.7 ms | 150 ms |
| 173 (pop ≥ 20k) | 18.49M | 18.35M | 23.08M | 1.3 ms | 683 ms |
| 253 (pop ≥ 10k) | 12.41M | 18.35M | 23.16M | 1.8 ms | 2,645 ms |

- **Nearest neighbour peaks at 90 towns and then falls**, to 12.41M at 253 towns: with smaller towns in the list, the nearest town is often a small one, and it ignores population. A and B pick by reach, so smaller towns barely change their results.
- **Nearest neighbour beats A on the 30-, 60-, 90- and 173-town instances** (by 1%, 15%, 15% and 1%).
- **B is about 1.2× slower than in v1 at 90 towns** (150 ms vs 125 ms) and 2.5× slower at 253 (2.65 s vs 1.05 s). Each candidate now tries two timings and scores reach with the full model, since #7 the whole search runs twice (from empty routes and from A's plan), and 16-hour days make routes longer. The pruning in `tryInsertion` (skip the exact reach calculation when even a full crowd couldn't win) keeps the cost down. A is still more than 100× faster from 60 towns up. Section 8 has the faster and better versions of B.
- **B's reach isn't monotone in the number of towns** (23.62M at 90, 23.08M at 173): it is a heuristic, and a larger list gives it more ways to settle on a slightly worse plan.

## 8. Improving B: the experiments

B as published (Vansteenwegen et al. 2009) only inserts towns and shakes the plan. Five ideas from the later literature were each tried as an experiment (GitHub issues #13–#17) and are kept as their own approaches, so the effect of each one can be measured on the same runs:

| Approach | What it adds to B | Source | Issue |
|---|---|---|---|
| `ils` | Nothing: B as published | Vansteenwegen et al. 2009 | — |
| `ils-cache` | Keeps each candidate insertion's timing and reach between steps, and works out again only those the last insertion changed. Same plans as `ils` | Granular neighbourhoods, Labadie et al. 2012 | #16 |
| `ils-revert` | After each shake and refill, goes back to the plan before the shake if the new one reaches fewer people | SAILS, Gunawan et al. 2017 (simulated annealing at temperature 0) | #14 |
| `ils-alns` | Several destroy and repair operators, picked more often the more they improve the plan | ALNS, Ropke & Pisinger 2006; Hammami et al. 2020 | #15 |
| `ils-moves` | After each refill, tries replacing a rally, moving or swapping one between politicians, and swapping two in one route; keeps any move that raises reach | VND moves, Shahmanzari et al. 2022 | #13 |
| `ils-full` | `ils-moves` and `ils-revert` together: the final version | — | #13, #14 |
| `multistart` | Not B: A run 200 times with random politician orders and slightly random choices, keeping the best | Biased randomisation, Panadero et al. 2021 | #17 |

Every version except `ils` and `multistart` also uses the cache, so their runtimes can be compared with each other.

**Default run** (as section 1):

| Approach | Reach | vs B | Rallies | Travel (h) | Wait (h) | Fairness | Runtime |
|---|---|---|---|---|---|---|---|
| A (`sequential`) | 18.36M | −22.3% | 122 | 148.8 | 8.9 | 0.87 | 0.7 ms |
| `multistart` | 19.25M | −18.5% | 107 | 179.6 | 8.4 | 0.95 | 121 ms |
| `ils` | 23.62M | — | 113 | 172.8 | 4.7 | 0.92 | 150 ms |
| `ils-cache` | 23.62M | same plan | 113 | 172.8 | 4.7 | 0.92 | 55 ms |
| `ils-alns` | 23.49M | −0.5% | 119 | 159.8 | 7.3 | 0.86 | 176 ms |
| `ils-revert` | 24.75M | +4.8% | 122 | 155.4 | 3.0 | 0.95 | 298 ms |
| `ils-moves` | 26.07M | +10.4% | 124 | 152.9 | 3.0 | 0.90 | 340 ms |
| `ils-full` | **26.71M** | **+13.1%** | 135 | 130.9 | 1.2 | 0.95 | 738 ms |

**Across the 27 settings of sections 2 and 5, and the 90 weighted-home runs of section 4.1** (reach and runtime as a ratio to `ils`; runtime is the median):

| Approach | Reach, 27 settings: mean (min–max) | Better / worse | Reach, 90 runs: mean (min–max) | Better / worse | Runtime vs `ils` | Slowest run |
|---|---|---|---|---|---|---|
| `multistart` | 0.890 (0.706–0.994) | 0 / 27 | 0.872 (0.803–0.939) | 0 / 90 | 0.6–0.8× | 2.3 s |
| `ils-cache` | 1.000 | equal in all | 1.000 | equal in all | **0.3×** | 0.6 s |
| `ils-alns` | 1.002 (0.956–1.060) | 12 / 13 | 1.014 (0.975–1.053) | 75 / 15 | 0.8× | 3.0 s |
| `ils-revert` | 1.052 (1.000–1.144) | 26 / 0 | 1.067 (1.007–1.116) | 90 / 0 | 0.6–0.8× | 2.1 s |
| `ils-moves` | 1.115 (1.047–1.213) | 27 / 0 | 1.137 (1.095–1.181) | 90 / 0 | 1.3–1.8× | 5.7 s |
| `ils-full` | **1.139** (1.062–1.220) | **27 / 0** | **1.161** (1.114–1.210) | **90 / 0** | 2.9–4.3× | 15.5 s |

**By team size** (section 4.1's weighted homes; mean reach in millions):

| k | `multistart` | `ils` | `ils-revert` | `ils-alns` | `ils-moves` | `ils-full` | `ils-full` vs `ils` | `ils` runtime | `ils-full` runtime |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 19.08 | 22.96 | 24.47 | 23.25 | 26.25 | **26.82** | +16.9% (+13.3 to +20.2) | 0.18 s | 0.52 s |
| 4 | 33.31 | 40.16 | 42.54 | 40.37 | 45.31 | **46.88** | +16.8% (+13.0 to +19.1) | 0.55 s | 2.03 s |
| 6 | 45.01 | 51.83 | 55.44 | 52.68 | 59.27 | **60.78** | +17.3% (+15.1 to +19.7) | 1.03 s | 4.24 s |
| 8 | 55.01 | 60.36 | 64.84 | 61.87 | 69.19 | **70.32** | +16.5% (+13.6 to +18.3) | 1.08 s | 6.17 s |
| 10 | 62.38 | 67.35 | 71.08 | 68.02 | 75.16 | **75.94** | +12.8% (+11.4 to +14.9) | 1.93 s | 10.72 s |

(`average_by_k.py` prints every k; odd k follow the same trend.)

- **Each adopted change adds something, and they add up.** The cache makes B about 3× faster (2.7× at 90 towns, 4.9× at 253) with exactly the same plans. Going back to the plan before the shake adds about 5–7%. The local-search moves add 11–14%. Together they reach 14–16% more than B, better in all 117 runs, by at least 6.2%.
- **Most of the moves' gain comes from swapping two rallies in one route** so that the bigger town comes later, which the polling-day weight rewards (#13). Moving the big towns to the end of the campaign is exactly what A can't do.
- **The final version is not the best in every run.** `ils-moves` alone beats `ils-full` in 3 of the 27 settings (and ties in 1) and in 1 of the 90 runs, by at most 1.1%. Going back to the earlier plan probably sometimes stops the search from moving on to a better area. `ils-revert` on its own loses to `ils` on the two smallest instances (−2.1% at 15 towns, −1.4% at 30).
- **Two ideas didn't pay off.** ALNS (#15) is about even with B: +0.2% on the 27 settings, +1.4% on the 90 runs, and −0.5% at the defaults. Its adaptive weights probably have little to learn from while every plan is accepted. Multi-start A (#17) beats A by 2–8% but stays 11–13% below B even with B's runtime: A only appends towns, so randomising its choices can't move big towns later.
- **The cost is runtime.** `ils-full` takes 2.9–4.3× as long as B (median), up to 15.5 s at k = 10. At 253 towns it reaches 26.30M (+13.6% over B) in 2.7 s, about the same time B needs.
- **Against the benchmarks,** `ils-full` reaches on average 35% more than budgeted nearest neighbour and 34% more than A over the 27 settings, and 70% and 44% more over the 90 weighted-home runs.
- **Every plan is valid:** all 117 benchmark runs, and all 360 stress-test settings (DESIGN §8) for every approach. The shake in `ils-revert` and `ils-full` also skips removals that would make a later stop late; that can only happen with Borneo flights, which break the triangle inequality (#14).

Runtimes come from single runs while other benchmarks were running, so they are rough; the reach figures are exact.

## 9. Findings

1. **The classic TSP heuristic can't be used unchanged.** The full nearest-neighbour tour needs 16.3 days of 16 h for a 14-day campaign, and 22 of its rallies fall after polling day. With one check added (only go where there's still time to get home), it is valid.
2. **A valid classic heuristic is a strong benchmark, and beats A at the defaults.** Budgeted nearest neighbour reaches 15% more than A at the defaults, and beats A in 16 of the 27 settings in sections 2 and 5: 8 of the 9 14-day campaigns in section 2, and 8 of the 9 cells in section 5. It wins by holding many more rallies with much less driving in the big-town clusters. A's advantage is clear only in 6-day campaigns (12–38%), at k = 5 with alpha = 0 (22%), and with more politicians from random homes (section 4.1). With 12-hour days A was narrowly ahead at the defaults; the wider window helps NN far more than A (section 6).
3. **B beats both in every setting.** It is 1.1–28.7% ahead of A in section 2, and 0.7–40% ahead of budgeted nearest neighbour in sections 2 and 5. B's search from empty routes alone fell below A in 4 of the 27 settings in sections 2 and 5 (by up to 6.1%, at k = 5 with alpha = 0 over 6 days); B also runs from A's plan and keeps the better result (#7), which closes those gaps. The run from A's plan gives B's figure in 17 of the 27 settings; the default run's figure comes from the empty start.
4. **Uncoordinated planning costs reach.** At the defaults the baseline reaches 18% less than A and 29% less than budgeted nearest neighbour, despite choosing towns by reach. At k = 5 over 14 days it reaches less than half of A (15.39M vs 35.52M at alpha = 0). At beta = 1 with a shared home it reaches nobody.
5. **The soft clash rule is used when the overlap penalty is mild.** A clashes when beta ≤ (1 − alpha) / 2 (0.25 at the default alpha); above that, it waits or goes elsewhere. In sections 2 and 3 B never clashes more than A, and in section 3 its best plans never clash at all: with 16-hour days it can spread the politicians out instead.
6. **The daily window makes waiting matter.** Waiting goes from about 0 in v1 to as much as 29 h (A at k = 5, alpha = 1, 14 days), because a rally that won't finish by 22:00 waits for the next morning.
7. **Where politicians start barely matters to A and B** (about 4% across the homes in section 4, and a standard deviation of at most 3% of the mean in section 4.1), but it does help fairness. With separate homes, A's fairness is 0.87, against 0.68 in v1. It matters a lot to NN and the baseline, which ignore the other politicians: NN ranges from 11.82M to 21.14M across the six homes in section 4.
8. **Averaged over homes weighted by population, B is first at every k from 2 to 10** and the baseline last. B beats both in all 90 runs. NN is ahead of A on average at k = 2 and 3; A is ahead from k = 4, and in every run from k = 7 (section 4.1).
9. **Maximising reach still isn't fair.** Fairness drops as low as 0.44 for A and 0.41 for B (k = 5, alpha = 0, 6 days). Nothing in the objective rewards using every politician.
10. **The decay parameters matter more than the algorithm.** tauPoll moves reach by about 1.5–1.85× and B vs A by 0.6–55%; it also decides how far A falls behind nearest neighbour. The parameters should be justified, or swept, in the report.
11. **Ideas from the literature improve B step by step** (section 8). Cached insertion scores make it about 3× faster with the same plans; going back to the plan before a bad shake adds 5–7%; local-search moves add 11–14%; together they add 14–16%, better in all 117 runs. ALNS and a randomised multi-start A did not help.

## 10. Reproduce

```sh
make
# Section 2
for k in 2 3 5; do for a in 0 0.5 1; do for d in 6 14; do
  ./build/campaign --k $k --alpha $a --days $d --summary results/sweep/settings.csv > /dev/null
done; done; done
# Section 3
for b in 0 0.25 0.5 1; do for st in "Kuala Lumpur,Kuala Lumpur" fixed; do
  ./build/campaign --clash-penalty $b --starts "$st" --summary results/sweep/clash_penalty.csv > /dev/null
done; done
# Section 4
./build/campaign --summary results/sweep/starts.csv > /dev/null
for s in 1 2 3 4 5; do ./build/campaign --starts random --seed $s --summary results/sweep/starts.csv > /dev/null; done
# Section 4.1
for k in 2 3 4 5 6 7 8 9 10; do for s in 1 2 3 4 5 6 7 8 9 10; do
  ./build/campaign --k $k --starts weighted --seed $s --summary results/sweep/weighted_starts.csv > /dev/null
done; done
python3 scripts/average_by_k.py results/sweep/weighted_starts.csv
# Section 5
for tr in 12 48 168; do for tp in 168 336 100000; do
  ./build/campaign --tau-repeat $tr --tau-poll $tp --summary results/sweep/decay.csv > /dev/null
done; done
# Section 6
./build/campaign --days 7 --day-hours 24 --summary results/sweep/window.csv > /dev/null
./build/campaign --day-hours 12 --day-start 8 --summary results/sweep/window.csv > /dev/null
./build/campaign --summary results/sweep/window.csv > /dev/null
./build/campaign --day-hours 24 --day-start 0 --summary results/sweep/window.csv > /dev/null
# Section 7
for opt in "--max-towns 15" "--max-towns 30" "--max-towns 60" "" "--min-pop 20000" "--min-pop 10000"; do
  ./build/campaign $opt --summary results/sweep/scaling.csv > /dev/null   # in zsh use ${=opt}
done
```

Delete the old CSVs in `results/sweep/` first: `--summary` appends. Each run includes every approach; section 8's comparisons come from the same CSVs (`python3 scripts/compare.py` compares two `scripts/bench.sh` outputs, as used in the experiment issues).

## 11. Not done yet

- Gap to the optimal plan on small instances (needs an exact solver).
- Separating the effect of the daily window from the effect of campaign length on the polling-day weight (section 6).
- Why NN and A reach less with 24-hour days than with 16-hour days (section 6).
- A look-ahead for A, so it can save big towns for the last days when the polling-day weight is high. Without one, it falls behind budgeted nearest neighbour in long campaigns (findings 2 and 10).
- A better search for B when the polling-day weight is off or k = 5. From empty routes it still loses to A in some of those settings (sections 2 and 5), and exact knock-on ranking didn't fix it (#7). The versions in section 8 beat B in every one of these settings.
- Combining ALNS with the go-back acceptance rule, and adding the local-search moves to the run that starts from A's plan only where it helps (section 8).
- Repeated runs to give runtime averages instead of single measurements.

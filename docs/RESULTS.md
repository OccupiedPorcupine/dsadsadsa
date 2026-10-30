# Results

All numbers below come from `build/campaign` runs of **v2** of the model: own home bases, a 14 × 12 h daily window, time decay and the soft clash rule (see [CHANGES_V2.md](CHANGES_V2.md)). The raw CSVs are in [results/sweep/](../results/sweep/). The v1 results are in git history (commit `94aa4dd`). They used a different reach model, so they can't be compared number for number.

**Note (2026-10-31):** every section and every CSV in `results/sweep/` was re-run after all the fixes since v2 (CHANGES_V2 §11). The last two changed every number: five places that double-counted a city were dropped from the data (#11, ASSUMPTIONS D6), so the default instance has 90 towns instead of 92, and a budgeted nearest-neighbour benchmark was added (#12). Figures in earlier commits and issue comments used the old data and can't be compared with these.

Runtimes are from a single run on an Apple Silicon Mac (Apple clang 21, `-O2`). They will differ on other machines and vary a little between runs. The other numbers are deterministic.

Column meanings:
- **Reach:** people reached, in millions, with time decay and clash penalties applied.
- **% max:** reach as a share of the bound where every politician rallies in every town and reaches its whole population (no decay, no penalty). No real plan gets near 100%, because the polling-day weight alone cuts early rallies to about 0.4.
- **NN:** the budgeted nearest-neighbour benchmark (`nearest-budget`): the brief's classic heuristic, stopping each politician when no nearby town leaves time to get home.
- **Rallies:** total number of rallies across all politicians.
- **Clashes:** pairs of rallies in the same town at overlapping times. They are allowed, but they cost reach.
- **Travel / wait:** total hours driving or flying, and hours spent waiting (for a town to be free, or for the next morning because a rally wouldn't finish before 20:00).
- **Fairness:** lowest politician's reach ÷ highest politician's reach.

## 1. Default run

90 Peninsular towns, k = 2 (homes: Kuala Lumpur and George Town), alpha = 0.5, 14 days × 12 h, tauRepeat = 48 h, tauPoll = 336 h, beta = 0.5.

| Approach | Reach | % max | Rallies | Clashes | Travel (h) | Wait (h) | Fairness | Valid | Runtime |
|---|---|---|---|---|---|---|---|---|---|
| Classic nearest neighbour, full tour | 18.00M | 42.1% | 180 | 0 | 94.8 | 31.0 | 0.93 | **no** | < 0.1 ms |
| Classic nearest neighbour, budgeted | 16.89M | 39.5% | 115 | 0 | 57.9 | 20.6 | **0.95** | yes | < 0.1 ms |
| Uncoordinated (baseline) | 15.03M | 35.2% | 82 | **6** (6.6 h) | 117.2 | 8.6 | 0.69 | yes | 0.3 ms |
| A: one at a time | 17.06M | 39.9% | 84 | 0 | 112.8 | 8.8 | 0.91 | yes | 0.4 ms |
| B: iterated local search | **19.96M** | **46.7%** | 80 | 0 | 133.8 | 1.6 | 0.90 | yes | ~215 ms |

- **Full tour:** invalid. Each tour visits all 90 towns and needs **264 active hours (22 days of 12 h)** to get everyone home, against a 168 h campaign (258 h for the KL politician, 264 h for George Town). 62 of its 180 rallies are after polling day and reach nobody (#4), so its reach comes from the first 118 alone.
- **Budgeted nearest neighbour:** the same rule, but each politician stops when no town leaves time to get home. It is valid and reaches 16.89M, **within 1% of A**. It holds far more rallies than A (115 vs 84) and drives half as far (58 h vs 113 h), because the big towns cluster around KL, JB and Penang.
- **Baseline:** both politicians plan as if alone and end up in some of the same towns at the same times. The 6 clashes cost it reach, and it reaches 12% less than A and 11% less than budgeted nearest neighbour. Its fairness is also the lowest (0.69).
- **A:** valid, never chooses to clash, and fairness is 0.91. It barely beats budgeted nearest neighbour, with 31 fewer rallies and twice the driving.
- **B:** reaches 17% more than A and 18% more than budgeted nearest neighbour, with fairness 0.90 and the least waiting (1.6 h). It travels the most (134 h). The polling-day weight rewards rallying in big towns late, and B is better at fitting those in. Its result here comes from the search that starts from empty routes; the one that starts from A's plan reaches 19.92M.

The route map for this run is `results/default/campaign_map.html` (see [viz/README.md](../viz/README.md)). It can show the budgeted nearest-neighbour plan, the baseline, A and B.

## 2. Across settings

90 towns, default homes and decay. k ∈ {2, 3, 5}, alpha ∈ {0, 0.5, 1}, campaign of 6 or 14 days. Reach is in millions. Every NN, baseline, A and B plan was valid. A clashed once in each of the two k = 5, 14-day runs with alpha 0.5 and 1; B never clashed.

| k | alpha | days | NN reach | A reach | B reach | A / NN | B / A | A fairness | B fairness | A wait (h) | B wait (h) | Baseline reach (clashes) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 6 | 10.62 | 14.27 | 15.13 | 1.343 | 1.060 | 0.88 | 0.83 | 4.7 | 1.3 | 8.73 (1) |
| 2 | 0 | 14 | 16.38 | 16.22 | 19.85 | **0.990** | 1.224 | 0.77 | 0.86 | 10.3 | 2.8 | 12.11 (6) |
| 2 | 0.5 | 6 | 10.62 | 14.31 | 15.15 | 1.348 | 1.058 | 0.89 | 0.96 | 3.6 | 2.2 | 11.82 (1) |
| 2 | 0.5 | 14 | 16.89 | 17.06 | 19.96 | 1.010 | 1.170 | 0.91 | 0.90 | 8.8 | 1.6 | 15.03 (6) |
| 2 | 1 | 6 | 10.62 | 15.16 | 16.55 | 1.428 | 1.091 | 0.95 | 0.94 | 3.0 | 0.8 | 14.91 (1) |
| 2 | 1 | 14 | 17.41 | 19.01 | 21.38 | 1.092 | 1.125 | 0.99 | 0.97 | 9.8 | 2.5 | 17.95 (6) |
| 3 | 0 | 6 | 14.93 | 18.43 | 18.84 | 1.235 | 1.022 | 0.69 | 0.58 | 5.0 | 0.4 | 10.05 (1) |
| 3 | 0 | 14 | 21.85 | 23.33 | 26.59 | 1.068 | 1.140 | 0.75 | 0.96 | 15.3 | 7.5 | 12.83 (9) |
| 3 | 0.5 | 6 | 14.93 | 19.97 | 20.78 | 1.338 | 1.040 | 0.78 | 0.85 | 4.4 | 3.3 | 15.64 (1) |
| 3 | 0.5 | 14 | 24.26 | 24.05 | 27.95 | **0.991** | 1.162 | 0.83 | 0.92 | 11.4 | 7.7 | 19.21 (9) |
| 3 | 1 | 6 | 14.93 | 23.21 | 24.30 | 1.555 | 1.047 | 0.92 | 0.95 | 3.6 | 4.3 | 22.47 (1) |
| 3 | 1 | 14 | 26.68 | 28.06 | 31.18 | 1.052 | 1.111 | 0.95 | 0.93 | 10.6 | 10.5 | 26.95 (9) |
| 5 | 0 | 6 | 20.68 | 24.62 | 24.84 | 1.191 | **1.009** | **0.47** | **0.47** | 7.0 | 1.3 | 13.38 (20) |
| 5 | 0 | 14 | 27.00 | 35.35 | 36.72 | 1.309 | 1.039 | 0.65 | 0.66 | 26.9 | 17.4 | 14.67 (35) |
| 5 | 0.5 | 6 | 24.39 | 28.36 | 28.98 | 1.163 | 1.022 | 0.52 | 0.60 | 5.7 | 3.4 | 19.54 (20) |
| 5 | 0.5 | 14 | 35.38 | 35.66 | 40.78 | 1.008 | 1.144 | 0.71 | 0.79 | 19.5 | 9.4 | 25.14 (35) |
| 5 | 1 | 6 | 28.27 | 38.87 | 39.78 | 1.375 | 1.023 | 0.92 | 0.80 | 12.1 | 5.1 | 29.61 (20) |
| 5 | 1 | 14 | 44.01 | 47.32 | 51.21 | 1.075 | 1.082 | 0.92 | 0.94 | 24.4 | 9.3 | 40.05 (35) |

- **Reach rises steadily with alpha and with campaign length** for A and B in every row.
- **A's lead over budgeted nearest neighbour depends on the campaign length.** Over 6 days A is 16–56% ahead. Over 14 days it is 1–31% ahead, and behind in two settings (k = 2 with alpha = 0, and k = 3 with alpha = 0.5, both by about 1%). In a long campaign, nearest neighbour fits in many more rallies with little driving, and A's reach-per-hour choice, made from day 1 when the polling-day weight is lowest, doesn't make up for it.
- **B beats A in every setting,** by 0.9–22.4%, and budgeted nearest neighbour by 15–63%.
- **Budgeted nearest neighbour ignores alpha** when the politicians' routes don't share towns, which is why its reach is the same for every alpha at k = 2 and 3 over 6 days.

## 3. Soft clash rule: does anyone choose to clash?

k = 2, alpha = 0.5, 14 days. Shared home (both in KL, the case most likely to clash) vs the default homes.

| beta | Homes | Baseline reach (clashes) | A reach | A clashes | A wait (h) | B reach | B clashes | B wait (h) |
|---|---|---|---|---|---|---|---|---|
| 0 | KL, KL | 19.07 (42) | 19.07 | **42** | 11.1 | 21.19 | **37** | 9.2 |
| 0 | KL, George Town | 15.77 (6) | 17.29 | **5** | 9.2 | 20.14 | 0 | 3.9 |
| 0.25 | KL, KL | 14.30 (42) | 17.38 | **1** | 8.0 | 20.67 | **1** | 2.2 |
| 0.25 | KL, George Town | 15.40 (6) | 17.06 | 0 | 8.8 | 20.32 | 0 | 1.5 |
| 0.5 | KL, KL | 9.54 (42) | 17.04 | 0 | 10.7 | 20.83 | 0 | 4.4 |
| 0.5 | KL, George Town | 15.03 (6) | 17.06 | 0 | 8.8 | 19.96 | 0 | 1.6 |
| 1 | KL, KL | 0.00 (42) | 17.04 | 0 | 10.7 | 20.48 | 0 | 3.7 |
| 1 | KL, George Town | 14.29 (6) | 17.06 | 0 | 8.8 | 20.19 | 0 | 2.1 |

- **Clashing is a real trade-off.** While two rallies overlap, the crowd is split (beta) instead of counted as already reached (ASSUMPTIONS V12). A full overlap reaches `2 × (1 − beta)` crowds in total, against `1 + alpha` for two rallies one after the other, so on reach alone clashing wins when beta < (1 − alpha) / 2 (0.25 at the default alpha). At or below that, A and B clash; above it, they wait or go elsewhere.
- **At beta = 0 a full overlap costs nothing**, so A with both homes in KL follows the same route as the baseline (42 clashes). B's best plan there clashes almost as often (37), because the clashes cost it nothing. With separate homes, A clashes 5 times and B never.
- **The baseline shows what coordination is worth.** At beta = 1 with a shared home, both uncoordinated politicians follow identical routes, overlap completely, and reach nobody.

## 4. Starting points

Default settings. Fixed homes vs five random seeds (`--starts random --seed N`).

| Homes | A reach | A fairness | A travel (h) | B reach | B fairness | B travel (h) |
|---|---|---|---|---|---|---|
| Kuala Lumpur, George Town (fixed) | 17.06 | 0.91 | 113 | 19.96 | 0.90 | 134 |
| Yan Besar, Kampong Baharu Cheras Batu Sebelas (seed 1) | 16.67 | 1.00 | 116 | 20.46 | 0.87 | 122 |
| Setia Alam, Hulu Langat (seed 2) | 17.03 | 0.83 | 110 | 21.13 | 0.90 | 133 |
| Parit Sulung, Paya Terubong (seed 3) | 17.87 | 0.93 | 111 | 20.37 | 0.93 | 127 |
| Bukit Mertajam, Kampung Sungai Ara (seed 4) | 17.32 | 0.98 | 118 | 19.82 | 0.97 | 141 |
| Simpang Empat, Kuantan (seed 5) | 17.35 | 0.87 | 119 | 20.14 | 0.87 | 133 |

Reach barely depends on where politicians start: A and B each vary by about 7% across the six. Fourteen days is long enough to reach the big towns from anywhere on the peninsula. B beats A for every set of homes. (The random homes differ from earlier versions of this table, because they are drawn from the town list, which changed with #11.)

## 5. Time-decay parameters

Default settings, k = 2. Reach in millions. tauPoll = 100,000 h effectively turns the polling-day weight off.

| tauRepeat | tauPoll = 168 h | tauPoll = 336 h | tauPoll off |
|---|---|---|---|
| 12 h | NN 11.20 / A 11.56 / B 15.34 | NN 17.36 / A 18.88 / B 20.69 | NN 28.77 / A 33.66 / B 34.05 |
| 48 h (default) | NN 10.90 / A **10.36** / B 14.12 | NN 16.89 / A 17.06 / B 19.96 | NN 28.04 / A 30.94 / B 32.06 |
| 168 h | NN 9.75 / A **9.13** / B 12.86 | NN 15.25 / A 15.55 / B 18.46 | NN 25.67 / A 28.35 / B 29.01 |

- **tauPoll has the most effect on reach.** Halving it from 336 h to 168 h cuts reach by 26–41%, and switching it off raises reach about 1.6–1.8×.
- **Faster repeat fading (smaller tauRepeat) helps a little,** because second rallies days apart become worth almost a full crowd.
- **The stronger the polling-day weight, the worse A does against nearest neighbour.** With tauPoll = 168 h, budgeted nearest neighbour beats A (by 5–7% at tauRepeat 48 h and 168 h). With the weight off, A is 10–17% ahead. A's reach-per-hour choice doesn't look ahead to the rallies that will be worth more later.
- **B's advantage over A also depends on the polling-day weight.** With it on, B beats A by 10–41%. With it off, B is only 1.2–3.6% ahead, and at tauRepeat = 12 h B's search from empty routes loses to A (−3.8%); B's figure in that cell comes from its second run, which starts from A's plan (#7). Ranking insertions on the exact knock-on reach was tried and didn't close the gap (see [DESIGN.md §6.3](DESIGN.md#63-approach-b-iterated-local-search-solveils)).

## 6. Daily window vs continuous hours

Default settings. The same 168 hours of campaigning, either as 7 days × 24 h (no nights, like v1) or 14 days × 12 h.

| Clock | Approach | Reach | Rallies | Wait (h) | Travel (h) |
|---|---|---|---|---|---|
| 7 × 24 h | NN | 20.44 | 117 | 14.5 | 59.0 |
| 7 × 24 h | A | 21.24 | 87 | 7.1 | 109.3 |
| 7 × 24 h | B | 24.31 | 83 | 2.8 | 123.0 |
| 14 × 12 h | NN | 16.89 | 115 | 20.6 | 57.9 |
| 14 × 12 h | A | 17.06 | 84 | 8.8 | 112.8 |
| 14 × 12 h | B | 19.96 | 80 | 1.6 | 133.8 |

The daily window increases waiting for nearest neighbour (14.5 h to 20.6 h) and A (7.1 h to 8.8 h), because a rally that won't finish by 20:00 has to wait until the next morning. B's waiting falls (2.8 h to 1.6 h), because it can fit rallies around the evenings. Reach is lower partly because of the window and partly because a 14-day campaign spreads rallies over a longer stretch of the polling-day weight than a 7-day one. These two effects are not separated here.

## 7. Scaling with instance size

k = 2, default settings. Larger instances were made by lowering the population cut-off.

| Towns | NN reach | A reach | B reach | A runtime | B runtime |
|---|---|---|---|---|---|
| 15 | 7.72M | 7.74M | 10.77M | < 0.1 ms | 3 ms |
| 30 | 13.90M | 13.73M | 19.03M | 0.1 ms | 8 ms |
| 60 | 19.67M | 17.00M | 20.21M | 0.3 ms | 72 ms |
| 90 | 16.89M | 17.06M | 19.96M | 0.4 ms | 217 ms |
| 173 (pop ≥ 20k) | 12.01M | 17.06M | 20.45M | 0.8 ms | 808 ms |
| 253 (pop ≥ 10k) | 10.19M | 17.09M | 20.15M | 1.3 ms | 1,484 ms |

- **Nearest neighbour peaks at 60 towns and then falls**, to 10.19M at 253 towns: with smaller towns in the list, the nearest town is often a small one, and it ignores population. A and B pick by reach, so smaller towns barely change their results.
- **On the 30- and 60-town instances, nearest neighbour beats A** (by 1% and 16%).
- **B is about 1.7× slower than in v1 at 90 towns** (217 ms vs 125 ms) and 1.4× slower at 253 (1.48 s vs 1.05 s). Each candidate now tries two timings and scores reach with the full model, and since #7 the whole search runs twice (from empty routes and from A's plan). The pruning in `tryInsertion` (skip the exact reach calculation when even a full crowd couldn't win) keeps the cost down. A is still more than 100× faster.
- **B's reach isn't monotone in the number of towns** (20.21M at 60, 19.96M at 90): it is a heuristic, and a larger list gives it more ways to settle on a slightly worse plan.

## 8. Findings

1. **The classic TSP heuristic can't be used unchanged.** The full nearest-neighbour tour needs 22 days for a 14-day campaign. With one check added (only go where there's still time to get home), it is valid.
2. **A valid classic heuristic is a strong benchmark.** Budgeted nearest neighbour reaches within 1% of A at the defaults, and beats A in 4 of the 27 settings in sections 2 and 5, all 14-day campaigns: two in section 2, and the two with tauPoll = 168 h and tauRepeat ≥ 48 h in section 5. It wins by holding many more rallies with much less driving in the big-town clusters. A's advantage is clear only in short campaigns (16–56% over 6 days) or with the polling-day weight off (10–17%).
3. **B beats both in every setting.** It is 0.9–22.4% ahead of A in section 2, and 13–63% ahead of budgeted nearest neighbour in sections 2 and 5. B's search from empty routes alone fell below A in 4 of the 27 settings in sections 2 and 5 (by up to 3.8%); B also runs from A's plan and keeps the better result (#7), which closes those gaps. The run from A's plan gives B's figure in 16 of the 27 settings; the default run's figure comes from the empty start.
4. **Uncoordinated planning costs reach.** At the defaults the baseline reaches 12% less than A, and less than budgeted nearest neighbour too, despite choosing towns by reach. At k = 5 over 14 days it reaches less than half of A (14.67M vs 35.35M at alpha = 0). At beta = 1 with a shared home it reaches nobody.
5. **The soft clash rule is used when the overlap penalty is mild.** A and B clash when beta < (1 − alpha) / 2 (beta ≤ 0.25 at the default alpha); above that, they wait or go elsewhere. B never clashes more than A when the politicians have separate homes, because it can spread them out instead.
6. **The daily window makes waiting matter.** Waiting goes from about 0 in v1 to as much as 27 h (A at k = 5, alpha = 0, 14 days), because every evening blocks out the late slots.
7. **Where politicians start barely matters** for total reach (about ±3.5%), but it does help fairness. With separate homes, A's fairness rises from 0.68 (v1) to 0.91.
8. **Maximising reach still isn't fair.** Fairness drops as low as 0.47 for both A and B (k = 5, alpha = 0, 6 days). Nothing in the objective rewards using every politician.
9. **The decay parameters matter more than the algorithm.** tauPoll moves reach by about 1.8× and B vs A by 1–41%; it also decides whether A beats nearest neighbour at all. The parameters should be justified, or swept, in the report.

## 9. Reproduce

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
# Section 5
for tr in 12 48 168; do for tp in 168 336 100000; do
  ./build/campaign --tau-repeat $tr --tau-poll $tp --summary results/sweep/decay.csv > /dev/null
done; done
# Section 6
./build/campaign --days 7 --day-hours 24 --summary results/sweep/window.csv > /dev/null
./build/campaign --summary results/sweep/window.csv > /dev/null
# Section 7
for opt in "--max-towns 15" "--max-towns 30" "--max-towns 60" "" "--min-pop 20000" "--min-pop 10000"; do
  ./build/campaign $opt --summary results/sweep/scaling.csv > /dev/null   # in zsh use ${=opt}
done
```

Delete the old CSVs in `results/sweep/` first: `--summary` appends.

## 10. Not done yet

- Gap to the optimal plan on small instances (needs an exact solver).
- Separating the effect of the daily window from the effect of campaign length on the polling-day weight (section 6).
- A look-ahead for A, so it can save big towns for the last days when the polling-day weight is high. Without one, it falls behind budgeted nearest neighbour in long campaigns (findings 2 and 9).
- A better search for B when the polling-day weight is off or k = 5. From empty routes it still loses to A in some of those settings (sections 2 and 5), and exact knock-on ranking didn't fix it (#7).
- Repeated runs to give runtime averages instead of single measurements.

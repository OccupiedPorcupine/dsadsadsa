# Results

All numbers below come from `build/campaign` runs of **v2** of the model: own home bases, a 14 × 12 h daily window, time decay and the soft clash rule (see [CHANGES_V2.md](CHANGES_V2.md)). The raw CSVs are in [results/sweep/](../results/sweep/). The v1 results are in git history (commit `94aa4dd`). They used a different reach model, so they can't be compared number for number.

Runtimes are from a single run on an Apple Silicon Mac (Apple clang 21, `-O2`). They will differ on other machines and vary a little between runs. The other numbers are deterministic.

Column meanings:
- **Reach:** people reached, in millions, with time decay and clash penalties applied.
- **% max:** reach as a share of the bound where every politician rallies in every town and reaches its whole population (no decay, no penalty). No real plan gets near 100%, because the polling-day weight alone cuts early rallies to about 0.4.
- **Rallies:** total number of rallies across all politicians.
- **Clashes:** pairs of rallies in the same town at overlapping times. They are allowed, but they cost reach.
- **Travel / wait:** total hours driving or flying, and hours spent waiting (for a town to be free, or for the next morning because a rally wouldn't finish before 20:00).
- **Fairness:** lowest politician's reach ÷ highest politician's reach.

## 1. Default run

92 Peninsular towns, k = 2 (homes: Kuala Lumpur and George Town), alpha = 0.5, 14 days × 12 h, tauRepeat = 48 h, tauPoll = 336 h, beta = 0.5.

| Approach | Reach | % max | Rallies | Clashes | Travel (h) | Wait (h) | Fairness | Valid | Runtime |
|---|---|---|---|---|---|---|---|---|---|
| Classic nearest neighbour (benchmark) | *33.64M* | *70.2%* | 184 | 1 | 95.5 | 43.8 | 0.88 | **no** | < 0.1 ms |
| Uncoordinated (baseline) | 12.02M | 25.1% | 85 | **28** (69.5 h) | 108.7 | 6.5 | 0.63 | yes | 0.3 ms |
| A: one at a time | 19.02M | 39.7% | 84 | 0 | 113.5 | 4.1 | 0.90 | yes | 0.4 ms |
| B: iterated local search | **22.06M** | **46.0%** | 79 | 0 | 122.0 | 7.7 | **0.93** | yes | ~215 ms |

- **Classic nearest neighbour:** invalid. Each tour visits all 92 towns, and politicians get home at 269 h and 283 h against a 168 h budget. Its reach is in italics because it counts rallies held after polling day. Because the politicians now start from different homes, their tours hardly ever coincide (1 clash instead of 92 in v1).
- **Baseline:** both politicians plan as if alone and end up on largely the same big towns at the same times. Under the soft rule this is no longer invalid, but the 28 clashes cost a lot: the baseline reaches only **63% of A's figure**.
- **A:** valid, never chooses to clash, and fairness is 0.90. Fairness is much better than v1's 0.68, because separate homes stop P1 from taking every nearby town first.
- **B:** reaches 16% more people than A. It travels a little more than A (122 h vs 114 h), which reverses v1. The polling-day weight rewards rallying in big towns late, and B is better at fitting those in.

The route map for this run is `results/default/campaign_map.html` (see [viz/README.md](../viz/README.md)).

## 2. Across settings

92 towns, default homes and decay. k ∈ {2, 3, 5}, alpha ∈ {0, 0.5, 1}, campaign of 6 or 14 days. Reach is in millions. Every A and B plan was valid. Neither A nor B chose to clash in any of these runs.

| k | alpha | days | A reach | B reach | B / A | A fairness | B fairness | A wait (h) | B wait (h) | Baseline reach (clashes) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 6 | 13.00 | 14.46 | 1.112 | 0.55 | 0.66 | 0.7 | 1.3 | 7.73 (3) |
| 2 | 0 | 14 | 13.12 | 15.91 | 1.213 | 0.22 | 0.44 | 11.3 | 10.9 | 7.33 (28) |
| 2 | 0.5 | 6 | 15.33 | 16.53 | 1.078 | 0.86 | 0.82 | 1.2 | 0.7 | 12.64 (3) |
| 2 | 0.5 | 14 | 19.02 | 22.06 | 1.160 | 0.90 | 0.93 | 4.1 | 7.7 | 12.02 (28) |
| 2 | 1 | 6 | 15.98 | 17.42 | 1.090 | 0.91 | 0.85 | 0.9 | 2.5 | 15.23 (3) |
| 2 | 1 | 14 | 21.01 | 23.08 | 1.099 | 0.95 | 0.92 | 6.0 | 3.5 | 14.99 (28) |
| 3 | 0 | 6 | 15.94 | 17.85 | 1.120 | 0.35 | 0.53 | 1.3 | 2.7 | 9.13 (9) |
| 3 | 0 | 14 | 13.24 | 16.36 | 1.236 | 0.01 | 0.06 | 12.7 | 12.9 | 8.49 (42) |
| 3 | 0.5 | 6 | 21.47 | 22.89 | 1.066 | 0.79 | 0.77 | 3.2 | 2.7 | 15.94 (9) |
| 3 | 0.5 | 14 | 26.22 | 30.65 | 1.169 | 0.82 | 0.85 | 6.9 | 3.7 | 16.81 (42) |
| 3 | 1 | 6 | 24.21 | 26.33 | 1.088 | 0.91 | 0.88 | 4.1 | 4.5 | 21.89 (9) |
| 3 | 1 | 14 | 31.58 | 33.45 | 1.059 | 0.95 | 0.96 | 14.5 | 8.1 | 24.07 (42) |
| 5 | 0 | 6 | 18.64 | 20.36 | 1.092 | 0.13 | 0.34 | 3.3 | 9.1 | 10.95 (30) |
| 5 | 0 | 14 | 13.24 | 16.44 | 1.242 | 0.00 | **0.00** | 12.7 | 14.2 | 9.97 (76) |
| 5 | 0.5 | 6 | 30.94 | **29.77** | **0.962** | 0.64 | 0.61 | 6.5 | 5.5 | 20.94 (30) |
| 5 | 0.5 | 14 | 38.60 | 43.27 | 1.121 | 0.70 | 0.89 | 15.3 | 14.1 | 24.65 (76) |
| 5 | 1 | 6 | 41.53 | 42.49 | 1.023 | 0.83 | 0.87 | 9.6 | 6.6 | 33.33 (30) |
| 5 | 1 | 14 | 52.85 | 54.28 | 1.027 | 0.95 | 0.86 | 26.5 | 12.9 | 40.32 (76) |

**A longer campaign can lower reach when alpha = 0** (k = 3 and 5: 6 days gives more than 14). Under the polling-day weight, a rally is worth more the closer it is to the end. With alpha = 0 each town is only worth its first rally, so a long campaign spends its big towns early, at a low weight.

## 3. Soft clash rule: does anyone choose to clash?

k = 2, alpha = 0.5, 14 days. Shared home (both in KL, the case most likely to clash) vs the default homes.

| beta | Homes | Baseline reach (clashes) | A reach | A clashes | A wait (h) | B reach | B clashes | B wait (h) |
|---|---|---|---|---|---|---|---|---|
| 0 | KL, KL | 16.13 (43) | 19.05 | **2** | 9.1 | 23.42 | 0 | 1.6 |
| 0 | KL, George Town | 17.00 (28) | 19.02 | 0 | 4.1 | 21.79 | 0 | 1.6 |
| 0.25 | KL, KL | 12.10 (43) | 19.20 | 0 | 7.8 | 22.78 | 0 | 3.3 |
| 0.25 | KL, George Town | 14.51 (28) | 19.02 | 0 | 4.1 | 22.67 | 0 | 2.1 |
| 0.5 | KL, KL | 8.06 (43) | 19.20 | 0 | 7.8 | 22.78 | 0 | 3.3 |
| 0.5 | KL, George Town | 12.02 (28) | 19.02 | 0 | 4.1 | 22.06 | 0 | 7.7 |
| 1 | KL, KL | 0.00 (43) | 19.20 | 0 | 7.8 | 22.78 | 0 | 3.3 |
| 1 | KL, George Town | 7.04 (28) | 19.02 | 0 | 4.1 | 22.10 | 0 | 3.5 |

- **Clashing is almost never worth it.** Even at beta = 0, a clash only saves the waiting time, and the clashing rally still pays the full repeat penalty, because the other rally is happening at that moment. With beta > 0, A and B always wait or go elsewhere.
- **The exception:** with alpha = 1 and beta = 0, clashing is free. Both homes in KL then gives 43 clashes for A (k = 2). With k = 5 and everyone in KL, A clashes 430 times and B only twice.
- **The baseline shows what coordination is worth.** At beta = 1 with a shared home, both uncoordinated politicians follow identical routes, overlap completely, and reach nobody.

## 4. Starting points

Default settings. Fixed homes vs five random seeds (`--starts random --seed N`).

| Homes | A reach | A fairness | A travel (h) | B reach | B fairness | B travel (h) |
|---|---|---|---|---|---|---|
| Kuala Lumpur, George Town (fixed) | 19.02 | 0.90 | 114 | 22.06 | 0.93 | 122 |
| Bandar Saujana Utama, Yan Besar (seed 1) | 19.16 | 0.94 | 110 | 21.69 | 0.87 | 134 |
| Parit, Shah Alam (seed 2) | 19.40 | 0.94 | 99 | 22.18 | 0.97 | 134 |
| Subang Jaya, Tasek Glugor (seed 3) | 19.16 | 0.94 | 109 | 22.45 | 0.91 | 125 |
| Kuala Lumpur, Pasir Mas (seed 4) | 19.15 | 0.89 | 113 | 21.90 | 1.00 | 128 |
| Pasir Puteh, Puchong (seed 5) | 19.55 | 0.98 | 109 | 21.70 | 0.97 | 138 |

Reach barely depends on where politicians start: A varies by 3% and B by 3.5% across the six. Fourteen days is long enough to reach the big towns from anywhere on the peninsula. B beats A for every set of homes.

## 5. Time-decay parameters

Default settings, k = 2. Reach in millions (rallies in brackets). tauPoll = 100,000 h effectively turns the polling-day weight off.

| tauRepeat | tauPoll = 168 h | tauPoll = 336 h | tauPoll off |
|---|---|---|---|
| 12 h | A 12.46 / B 15.79 | A 21.17 / B 23.37 | A 37.69 / B **35.27** |
| 48 h (default) | A 11.19 / B 15.80 | A 19.02 / B 22.06 | A 34.04 / B 34.57 |
| 168 h | A 10.34 / B 14.08 | A 17.52 / B 19.94 | A 31.35 / B 31.14 |

- **tauPoll has the most effect on reach.** Halving it from 336 h to 168 h cuts reach by 28–41%, and switching it off raises reach about 1.6–1.8×.
- **Faster repeat fading (smaller tauRepeat) helps a little,** because second rallies days apart become worth almost a full crowd.
- **B's advantage depends on the polling-day weight.** With it switched off, B loses to A at tauRepeat = 12 h (−6%) and roughly ties at 168 h. B's `reach² / delay` score was designed for fixed rewards, and it misjudges how much a rally is worth when that depends on timing. This is a known weakness (see [DESIGN.md §6.3](DESIGN.md#63-approach-b-iterated-local-search-solveils)).

## 6. Daily window vs continuous hours

Default settings. The same 168 hours of campaigning, either as 7 days × 24 h (no nights, like v1) or 14 days × 12 h.

| Clock | Approach | Reach | Rallies | Wait (h) | Travel (h) |
|---|---|---|---|---|---|
| 7 × 24 h | A | 23.75 | 89 | 0.3 | 107.2 |
| 7 × 24 h | B | 25.88 | 76 | 0.7 | 136.4 |
| 14 × 12 h | A | 19.02 | 84 | 4.1 | 113.5 |
| 14 × 12 h | B | 22.06 | 79 | 7.7 | 122.0 |

The daily window increases waiting about 10× (to 4–8 h), because a rally that won't finish by 20:00 has to wait until the next morning. Reach is lower partly because of the window and partly because a 14-day campaign spreads rallies over a longer stretch of the polling-day weight than a 7-day one. These two effects are not separated here.

## 7. Scaling with instance size

k = 2, default settings. Larger instances were made by lowering the population cut-off.

| Towns | A reach | B reach | A runtime | B runtime |
|---|---|---|---|---|
| 15 | 8.48M | 11.89M | < 0.1 ms | 2 ms |
| 30 | 15.08M | 20.70M | 0.1 ms | 3 ms |
| 60 | 19.02M | 21.48M | 0.3 ms | 26 ms |
| 92 | 19.02M | 22.06M | 0.4 ms | 216 ms |
| 175 (pop ≥ 20k) | 19.02M | 21.98M | 0.8 ms | 420 ms |
| 255 (pop ≥ 10k) | 19.02M | 21.79M | 1.2 ms | 794 ms |

B is about 1.7× slower than in v1 at 92 towns, because each candidate now tries two timings and scores reach with the full model. The pruning in `tryInsertion` (skip the exact reach calculation when even a full crowd couldn't win) keeps the cost down. A is still more than 100× faster.

## 8. Findings

1. **Classic single-traveller planning still fails.** Nearest neighbour overruns the budget by more than 100 h. The uncoordinated baseline is now *valid*, but it reaches 37% less than A at the default settings, mostly because of its 28 clashes. At beta = 1 with a shared home it reaches nobody.
2. **B beats A in 17 of 18 settings** (by 2–24%). It loses only at k = 5, alpha = 0.5, 6 days (−3.8%). Its biggest wins come with alpha = 0 and long campaigns, where *when* each town is visited matters most.
3. **The soft clash rule is almost never used** by A or B, except when clashing is free (beta = 0 and alpha = 1). The repeat penalty already applies in full to two simultaneous rallies, so waiting a little, or going elsewhere and coming back days later, is nearly always worth more. This supports the original hard rule as a good approximation.
4. **The daily window makes coordination matter.** Waiting goes from about 0 in v1 to as much as 27 h (4–8 h at the default settings), because every evening blocks out the late slots.
5. **Where politicians start barely matters** for total reach (±3%), but it does help fairness. With separate homes, A's fairness rises from 0.68 (v1) to 0.90.
6. **Maximising reach still isn't fair.** With alpha = 0 and a long campaign, fairness drops to 0.00–0.06 at k = 3 and 5. Nothing in the objective rewards using every politician.
7. **The decay parameters matter more than the algorithm.** tauPoll moves reach by about 2× and B vs A by about 10%. They should be justified, or swept, in the report.

## 9. Reproduce

```sh
make
mkdir -p results/sweep
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

## 10. Not done yet

- Gap to the optimal plan on small instances (needs an exact solver).
- Separating the effect of the daily window from the effect of campaign length on the polling-day weight (section 6).
- A scoring rule for B that accounts for time-dependent reach (finding 7, section 5).
- Repeated runs to give runtime averages instead of single measurements.

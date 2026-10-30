# Results

All numbers below come from `build/campaign` runs of **v2** of the model: own home bases, a 14 × 12 h daily window, time decay and the soft clash rule (see [CHANGES_V2.md](CHANGES_V2.md)). The raw CSVs are in [results/sweep/](../results/sweep/). The v1 results are in git history (commit `94aa4dd`). They used a different reach model, so they can't be compared number for number.

**Note (2026-10-31):** every section and every CSV in `results/sweep/` was re-run after five changes: the overlap rule (#3, ASSUMPTIONS V12), zero reach for rallies after polling day (#4, ASSUMPTIONS V5), B also running from A's plan (#7, DESIGN §6.3), a repeat penalty that fades at alpha = 0 too (#5, ASSUMPTIONS V13), and splitting the crowd by how many rallies overlap at once (#2, ASSUMPTIONS V6). #2 only changed the baseline at k ≥ 3.

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
| Classic nearest neighbour (benchmark) | 18.77M | 39.2% | 184 | 1 | 95.5 | 43.8 | 0.94 | **no** | < 0.1 ms |
| Uncoordinated (baseline) | 13.83M | 28.9% | 85 | **28** (69.5 h) | 108.7 | 6.5 | 0.88 | yes | 0.4 ms |
| A: one at a time | 19.33M | 40.3% | 84 | 0 | 113.3 | 4.2 | **0.91** | yes | 0.4 ms |
| B: iterated local search | **22.30M** | **46.5%** | 78 | 0 | 130.0 | 3.1 | 0.87 | yes | ~225 ms |

- **Classic nearest neighbour:** invalid. Each tour visits all 92 towns, and politicians get home at 269 h and 283 h against a 168 h budget. 67 of its 184 rallies are after polling day and reach nobody (#4), so its reach comes from the first 117 alone. That is close to A's, because it holds every rally it can, but it ignores population and the time budget. Because the politicians now start from different homes, their tours hardly ever coincide (1 clash instead of 92 in v1).
- **Baseline:** both politicians plan as if alone and end up on largely the same big towns at the same times. Under the soft rule this is no longer invalid, but the 28 clashes cost a lot: the baseline reaches only **72% of A's figure**.
- **A:** valid, never chooses to clash, and fairness is 0.91. Fairness is much better than v1's 0.68, because separate homes stop P1 from taking every nearby town first.
- **B:** reaches 15% more people than A, with fairness 0.87, a little below A's. It travels more than A (130 h vs 113 h), which reverses v1. The polling-day weight rewards rallying in big towns late, and B is better at fitting those in.

The route map for this run is `results/default/campaign_map.html` (see [viz/README.md](../viz/README.md)).

## 2. Across settings

92 towns, default homes and decay. k ∈ {2, 3, 5}, alpha ∈ {0, 0.5, 1}, campaign of 6 or 14 days. Reach is in millions. Every A and B plan was valid. Neither A nor B chose to clash in any of these runs.

| k | alpha | days | A reach | B reach | B / A | A fairness | B fairness | A wait (h) | B wait (h) | Baseline reach (clashes) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 6 | 14.18 | 16.55 | 1.167 | 0.76 | 0.85 | 5.1 | 0.8 | 11.14 (3) |
| 2 | 0 | 14 | 18.84 | 21.81 | 1.158 | 0.85 | 0.82 | 6.9 | 2.2 | 12.68 (28) |
| 2 | 0.5 | 6 | 15.52 | 16.60 | 1.069 | 0.88 | 0.95 | 1.2 | 1.8 | 13.19 (3) |
| 2 | 0.5 | 14 | 19.33 | 22.30 | 1.154 | 0.91 | 0.87 | 4.2 | 3.1 | 13.83 (28) |
| 2 | 1 | 6 | 15.98 | 17.80 | 1.114 | 0.91 | 0.89 | 0.9 | 1.5 | 15.23 (3) |
| 2 | 1 | 14 | 21.01 | 23.11 | 1.100 | 0.95 | 0.94 | 6.0 | 6.6 | 14.99 (28) |
| 3 | 0 | 6 | 20.27 | 21.14 | 1.043 | 0.65 | 0.74 | 5.5 | 2.7 | 12.74 (9) |
| 3 | 0 | 14 | 26.81 | 28.71 | 1.071 | 0.83 | 0.90 | 13.9 | 7.1 | 14.52 (42) |
| 3 | 0.5 | 6 | 21.97 | 22.81 | 1.038 | 0.81 | 0.97 | 1.5 | 2.1 | 16.91 (9) |
| 3 | 0.5 | 14 | 26.97 | 30.97 | 1.148 | 0.81 | 0.93 | 7.2 | 8.1 | 18.89 (42) |
| 3 | 1 | 6 | 24.21 | 27.32 | 1.129 | 0.91 | 0.89 | 4.1 | 3.4 | 21.89 (9) |
| 3 | 1 | 14 | 31.58 | 34.08 | 1.079 | 0.95 | 0.89 | 14.5 | 9.5 | 23.98 (42) |
| 5 | 0 | 6 | 26.50 | 27.19 | 1.026 | **0.38** | 0.47 | 12.2 | 5.1 | 16.23 (30) |
| 5 | 0 | 14 | 40.14 | 42.81 | 1.066 | 0.76 | 0.83 | 31.1 | 16.7 | 18.39 (76) |
| 5 | 0.5 | 6 | 31.41 | 32.15 | **1.024** | 0.54 | 0.67 | 4.1 | 3.2 | 22.22 (30) |
| 5 | 0.5 | 14 | 40.59 | 45.31 | 1.116 | 0.79 | 0.85 | 16.4 | 13.7 | 26.96 (76) |
| 5 | 1 | 6 | 41.53 | 42.52 | **1.024** | 0.83 | 0.90 | 9.6 | 7.5 | 32.35 (30) |
| 5 | 1 | 14 | 52.85 | 54.98 | 1.040 | 0.95 | 0.94 | 26.5 | 11.8 | 39.27 (76) |

**Reach rises steadily with alpha and with campaign length** for both A and B in every row. alpha = 0 is no longer a special case: a repeat rally straight after another reaches nobody, but the penalty fades over the following days as it does for any alpha (#5, ASSUMPTIONS V13). Before #5, plain `alpha^E` never faded at alpha = 0, so at k = 3 and 5 a 14-day campaign reached *less* than a 6-day one and fairness fell to 0.00–0.06.

## 3. Soft clash rule: does anyone choose to clash?

k = 2, alpha = 0.5, 14 days. Shared home (both in KL, the case most likely to clash) vs the default homes.

| beta | Homes | Baseline reach (clashes) | A reach | A clashes | A wait (h) | B reach | B clashes | B wait (h) |
|---|---|---|---|---|---|---|---|---|
| 0 | KL, KL | 21.50 (43) | 21.50 | **43** | 6.5 | 23.14 | **45** | 6.7 |
| 0 | KL, George Town | 20.47 (28) | 18.99 | **2** | 8.7 | 22.45 | **2** | 3.2 |
| 0.25 | KL, KL | 16.13 (43) | 19.52 | **1** | 12.2 | 22.75 | 0 | 2.4 |
| 0.25 | KL, George Town | 17.15 (28) | 19.33 | 0 | 4.2 | 22.36 | 0 | 5.9 |
| 0.5 | KL, KL | 10.75 (43) | 19.56 | 0 | 7.8 | 23.46 | 0 | 1.6 |
| 0.5 | KL, George Town | 13.83 (28) | 19.33 | 0 | 4.2 | 22.30 | 0 | 3.1 |
| 1 | KL, KL | 0.00 (43) | 19.56 | 0 | 7.8 | 23.46 | 0 | 1.6 |
| 1 | KL, George Town | 7.19 (28) | 19.33 | 0 | 4.2 | 22.30 | 0 | 3.1 |

- **Clashing is a real trade-off.** While two rallies overlap, the crowd is split (beta) instead of counted as already reached (ASSUMPTIONS V12). A full overlap reaches `2 × (1 − beta)` crowds in total, against `1 + alpha` for two rallies one after the other, so on reach alone clashing wins when beta < (1 − alpha) / 2 (0.25 at the default alpha). At or below that, A clashes; above it, A and B wait or go elsewhere.
- **At beta = 0 a full overlap costs nothing**, so A with both homes in KL follows the same route as the baseline (43 clashes). B's best plan there clashes about as often (45), because the clashes cost it nothing. With separate homes, A and B clash twice each.
- **The baseline shows what coordination is worth.** At beta = 1 with a shared home, both uncoordinated politicians follow identical routes, overlap completely, and reach nobody.

## 4. Starting points

Default settings. Fixed homes vs five random seeds (`--starts random --seed N`).

| Homes | A reach | A fairness | A travel (h) | B reach | B fairness | B travel (h) |
|---|---|---|---|---|---|---|
| Kuala Lumpur, George Town (fixed) | 19.33 | 0.91 | 113 | 22.30 | 0.87 | 130 |
| Bandar Saujana Utama, Yan Besar (seed 1) | 19.16 | 0.92 | 110 | 22.65 | 0.99 | 131 |
| Parit, Shah Alam (seed 2) | 19.71 | 0.95 | 99 | 23.00 | 0.90 | 120 |
| Subang Jaya, Tasek Glugor (seed 3) | 19.13 | 0.92 | 110 | 22.46 | 0.88 | 123 |
| Kuala Lumpur, Pasir Mas (seed 4) | 19.15 | 0.91 | 109 | 22.85 | 0.90 | 137 |
| Pasir Puteh, Puchong (seed 5) | 19.46 | 0.96 | 113 | 22.19 | 0.98 | 124 |

Reach barely depends on where politicians start: A varies by 3% and B by 4% across the six. Fourteen days is long enough to reach the big towns from anywhere on the peninsula. B beats A for every set of homes.

## 5. Time-decay parameters

Default settings, k = 2. Reach in millions (rallies in brackets). tauPoll = 100,000 h effectively turns the polling-day weight off.

| tauRepeat | tauPoll = 168 h | tauPoll = 336 h | tauPoll off |
|---|---|---|---|
| 12 h | A 12.53 / B 17.19 | A 20.79 / B 23.28 | A 37.32 / B 37.45 |
| 48 h (default) | A 11.41 / B 15.90 | A 19.33 / B 22.30 | A 34.54 / B 35.03 |
| 168 h | A 10.47 / B 14.25 | A 17.90 / B 20.20 | A 32.08 / B 32.81 |

- **tauPoll has the most effect on reach.** Halving it from 336 h to 168 h cuts reach by 26–42%, and switching it off raises reach about 1.6–1.8×.
- **Faster repeat fading (smaller tauRepeat) helps a little,** because second rallies days apart become worth almost a full crowd.
- **B's advantage depends on the polling-day weight.** With it on, B beats A by 12–39%. With it off, B is only 0.3–2.3% ahead. B's search from empty routes loses to A at tauRepeat = 12 h (36.32 vs 37.32, −2.7%); B's figure in that cell comes from its second run, which starts from A's plan (#7). Ranking insertions on the exact knock-on reach was tried and didn't close the gap (see [DESIGN.md §6.3](DESIGN.md#63-approach-b-iterated-local-search-solveils)).

## 6. Daily window vs continuous hours

Default settings. The same 168 hours of campaigning, either as 7 days × 24 h (no nights, like v1) or 14 days × 12 h.

| Clock | Approach | Reach | Rallies | Wait (h) | Travel (h) |
|---|---|---|---|---|---|
| 7 × 24 h | A | 24.27 | 87 | 0.5 | 110.4 |
| 7 × 24 h | B | 26.64 | 80 | 3.3 | 123.4 |
| 14 × 12 h | A | 19.33 | 84 | 4.2 | 113.3 |
| 14 × 12 h | B | 22.30 | 78 | 3.1 | 130.0 |

The daily window increases A's waiting from 0.5 h to 4.2 h, because a rally that won't finish by 20:00 has to wait until the next morning. B's waiting doesn't rise (3.3 h to 3.1 h), because it can fit rallies around the evenings. Reach is lower partly because of the window and partly because a 14-day campaign spreads rallies over a longer stretch of the polling-day weight than a 7-day one. These two effects are not separated here.

## 7. Scaling with instance size

k = 2, default settings. Larger instances were made by lowering the population cut-off.

| Towns | A reach | B reach | A runtime | B runtime |
|---|---|---|---|---|
| 15 | 8.66M | 11.71M | < 0.1 ms | 3 ms |
| 30 | 15.30M | 21.09M | 0.1 ms | 12 ms |
| 60 | 19.33M | 22.05M | 0.3 ms | 102 ms |
| 92 | 19.33M | 22.30M | 0.5 ms | 225 ms |
| 175 (pop ≥ 20k) | 19.33M | 22.46M | 0.8 ms | 622 ms |
| 255 (pop ≥ 10k) | 19.33M | 22.95M | 1.2 ms | 969 ms |

B is about 1.8× slower than in v1 at 92 towns (225 ms vs 125 ms), but no slower at 255 (969 ms vs 1,054 ms). Each candidate now tries two timings and scores reach with the full model, and since #7 the whole search runs twice (from empty routes and from A's plan). The pruning in `tryInsertion` (skip the exact reach calculation when even a full crowd couldn't win) keeps the cost down. A is still more than 100× faster.

## 8. Findings

1. **Classic single-traveller planning still fails.** Nearest neighbour overruns the budget by more than 100 h. The uncoordinated baseline is now *valid*, but it reaches 28% less than A at the default settings, mostly because of its 28 clashes. At beta = 1 with a shared home it reaches nobody.
2. **B is never below A.** It beats A in every setting in section 2, by 2.4–16.7%. Its biggest wins come with k = 2 (up to 16.7%) and its smallest with k = 5 and 6 days (2.4–2.6%). B's search from empty routes alone lost to A in 3 of the 27 settings in sections 2 and 5 (by up to 5.2%, at k = 5, alpha = 0, 14 days). B also runs from A's plan and keeps the better result (#7), which closes those gaps. The run from A's plan gives B's figure in 19 of the 27 settings, including the default run.
3. **The soft clash rule is used when the overlap penalty is mild.** A clashes when beta < (1 − alpha) / 2 (beta ≤ 0.25 at the default alpha); above that, A and B wait or go elsewhere. B never clashes more than A when the politicians have separate homes, because it can spread them out instead.
4. **The daily window makes coordination matter.** Waiting goes from about 0 in v1 to as much as 31 h (3–4 h at the default settings), because every evening blocks out the late slots.
5. **Where politicians start barely matters** for total reach (±3%), but it does help fairness. With separate homes, A's fairness rises from 0.68 (v1) to 0.91.
6. **Maximising reach still isn't fair.** Fairness drops as low as 0.38 for A and 0.47 for B (k = 5, alpha = 0, 6 days). Nothing in the objective rewards using every politician. (Before #5, alpha = 0 runs reached 0.00–0.06, but that came from the penalty never fading.)
7. **The decay parameters matter more than the algorithm.** tauPoll moves reach by about 2× and B vs A by about 10%. They should be justified, or swept, in the report.

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

## 10. Not done yet

- Gap to the optimal plan on small instances (needs an exact solver).
- Separating the effect of the daily window from the effect of campaign length on the polling-day weight (section 6).
- A better search for B when the polling-day weight is off or k = 5. From empty routes it still loses to A in some of those settings (sections 2 and 5), and exact knock-on ranking didn't fix it (#7).
- Repeated runs to give runtime averages instead of single measurements.

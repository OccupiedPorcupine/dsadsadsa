# Results

All numbers below come from `build/campaign` runs. The raw CSVs are in [results/sweep/](../results/sweep/).
Runtimes are from a single run on an Apple Silicon Mac (Apple clang 21, `-O2`). They will differ on other machines and vary a little between runs. The other numbers are deterministic.

Column meanings:
- **Reach:** people reached, in millions.
- **% max:** reach as a share of the case where every politician rallies in every town.
- **Rallies:** total number of rallies across all politicians.
- **Clashes:** pairs of rallies in the same town at overlapping times. The baseline is invalid whenever this is above 0.
- **Travel / wait:** total hours driving or flying, and hours spent waiting for a town to be free.
- **Fairness:** lowest politician's reach ÷ highest politician's reach.

## 1. Default run

92 Peninsular towns, k = 2, alpha = 0.5, 168 hours.

| Approach | Reach | % max | Rallies | Clashes | Travel (h) | Fairness | Runtime |
|---|---|---|---|---|---|---|---|
| Classic nearest neighbour (benchmark) | *35.94M* | *100%* | 184 | **92** | 96.3 | 0.50 | < 0.1 ms |
| Uncoordinated (baseline) | 29.24M | 81.4% | 90 | **45** | 106.1 | 0.50 | < 0.1 ms |
| A: one at a time | 30.43M | 84.7% | 95 | 0 | 96.1 | 0.68 | < 0.1 ms |
| B: iterated local search | **31.87M** | **88.7%** | 108 | 0 | **68.1** | **0.93** | ~125 ms |

- **Classic nearest neighbour:** invalid twice over. Its tour takes **254.3 h** (48 h driving + 206 h of rallies) against a 168 h budget, and both politicians follow it, so all 92 towns clash. Its reach is in italics because it counts rallies held after the deadline. Cut off at 168 h, a politician gets through 60 of the 92 towns, reaching 16.04M people (67% of the population). Johor Bahru, the second-largest town, is 83rd on the tour and is never reached. One politician on this route reaches about half of what A or B reach with two.
- **Baseline:** both politicians choose the same 45 towns in the same order, so every rally clashes.
- **A:** valid, but P1 gets the best towns, so fairness is only 0.68.
- **B:** reaches 4.7% more people than A, in 29% less travel time, and is much fairer.

The route map for this run is `results/default/campaign_map.html` (see [viz/README.md](../viz/README.md)).

## 2. Across settings

92 towns. k ∈ {2, 3, 5}, alpha ∈ {0, 0.5, 1}, T ∈ {72, 168} hours. Reach is in millions.

| k | alpha | T (h) | A reach | B reach | B / A | A fairness | B fairness | A wait (h) | B wait (h) |
|---|---|---|---|---|---|---|---|---|---|
| 2 | 0 | 72 | 18.60 | 19.15 | 1.029 | 0.69 | 0.77 | 0 | 0 |
| 2 | 0 | 168 | 23.96 | 23.96 | 1.000 | 0.23 | 0.16 | 0 | 0 |
| 2 | 0.5 | 72 | 19.96 | 20.13 | 1.009 | 0.88 | 0.99 | 0 | 0 |
| 2 | 0.5 | 168 | 30.43 | 31.87 | 1.047 | 0.68 | 0.93 | 0 | 0 |
| 2 | 1 | 72 | 21.85 | 24.58 | 1.125 | 0.98 | 0.98 | 0 | 0 |
| 2 | 1 | 168 | 38.92 | 41.83 | 1.075 | 1.00 | 0.99 | 0.5 | 0 |
| 3 | 0 | 72 | 21.57 | 22.58 | 1.047 | 0.27 | 0.92 | 0 | 0 |
| 3 | 0 | 168 | 23.96 | 23.96 | 1.000 | 0.21 | **0.00** | 3.0 | 0 |
| 3 | 0.5 | 72 | 24.63 | 25.50 | 1.035 | 0.61 | 0.77 | 0 | 0 |
| 3 | 0.5 | 168 | 36.59 | 37.48 | 1.024 | 0.74 | 0.91 | 0 | 0.4 |
| 3 | 1 | 72 | 33.75 | 36.66 | 1.086 | 0.91 | 0.97 | 0 | 0 |
| 3 | 1 | 168 | 58.57 | 61.60 | 1.052 | 0.99 | 0.98 | 0.5 | 0.1 |
| 5 | 0 | 72 | 23.96 | 23.96 | 1.000 | 0.06 | 0.05 | 0 | 0 |
| 5 | 0 | 168 | 23.96 | 23.96 | 1.000 | 0.00 | **0.00** | 24.6 | 0 |
| 5 | 0.5 | 72 | 32.14 | 32.44 | 1.009 | 0.59 | 0.63 | 0.5 | 0 |
| 5 | 0.5 | 168 | 43.00 | 43.19 | 1.005 | 0.41 | 0.84 | 2.7 | 1.1 |
| 5 | 1 | 72 | 56.97 | 59.71 | 1.048 | 0.91 | 0.93 | 3.5 | 1.2 |
| 5 | 1 | 168 | 97.43 | **96.28** | **0.988** | 0.98 | 0.92 | 9.4 | 0 |

The baseline clashed in every one of these runs (from 18 clashes up to 450). The nearest-neighbour benchmark is left out of this table: it builds the same tour whatever k and alpha are, so it is invalid in every setting (back at HQ at 254.3 h, with every rally clashing).

## 3. Scaling with instance size

k = 2, alpha = 0.5, 168 hours. Larger instances were made by lowering the population cut-off.

| Towns | A reach | B reach | A runtime | B runtime | Nearest-neighbour tour length |
|---|---|---|---|---|---|
| 15 | 16.07M | 16.07M | < 0.1 ms | 0.5 ms | 72.2 h |
| 30 | 25.09M | 25.09M | < 0.1 ms | 2.1 ms | 113.4 h |
| 60 | 30.21M | 31.65M | < 0.1 ms | 35 ms | 188.6 h |
| 92 | 30.43M | 31.87M | < 0.1 ms | 125 ms | 254.3 h |
| 175 (pop ≥ 20k) | 30.43M | 31.85M | < 0.1 ms | 437 ms | 408.3 h |
| 255 (pop ≥ 10k) | 30.43M | 31.88M | 0.1 ms | 1,054 ms | 511.1 h |

The nearest-neighbour tour fits in 168 h only up to 30 towns, and even there every rally clashes because both politicians follow the same tour.

## 4. Findings

1. **Classic single-traveller planning fails outright.** The textbook nearest-neighbour tour overruns the 168 h budget by 86 h and visits towns in distance order, not by size. The uncoordinated baseline fixes the budget and the town choice, but still clashes in every town in every setting.
2. **B beats A on reach in 13 of 18 settings, ties in 4, and loses in 1** (k = 5, alpha = 1, 168 h, by 1.2%). In the ties, both visit every town once, which is the maximum possible when alpha = 0.
3. **B travels far less.** In the default run it travels 68 h vs A's 96 h, because inserting a town between two existing rallies avoids long detours.
4. **The clash rule rarely forces anyone to wait.** Waiting is 0 in most runs and at most 25 h (A, k = 5). With continuous 24-hour days, there is almost always another free slot. Adding daytime-only rallies (assumption T6) should make the rule bite much harder.
5. **Maximising reach does not mean being fair.** When alpha = 0 and time is generous, B gives every town to one politician (fairness 0.00). Nothing in the objective rewards using everyone. This could be fixed with a fairness term or a per-politician minimum.
6. **More towns stop helping quickly.** Going from 92 to 255 towns adds almost nothing to reach, because time, not the choice of towns, is the limit. The extra towns are small. B's runtime still grows about 8× over that range.
7. **A is more than 1,000× faster.** It is the right choice if plans must be recomputed live, for example after a cancellation.

## 5. Reproduce

```sh
make
mkdir -p results/sweep
# Section 2
for k in 2 3 5; do for a in 0 0.5 1; do for h in 72 168; do
  ./build/campaign --k $k --alpha $a --hours $h --summary results/sweep/settings.csv > /dev/null
done; done; done
# Section 3
for opt in "--max-towns 15" "--max-towns 30" "--max-towns 60" "" "--min-pop 20000" "--min-pop 10000"; do
  ./build/campaign $opt --summary results/sweep/scaling.csv > /dev/null   # in zsh use ${=opt}
done
```

## 6. Not done yet

- Gap to the optimal plan on small instances (needs an exact solver).
- Experiments with daytime-only rallies.
- Repeated runs to give runtime averages instead of single measurements.

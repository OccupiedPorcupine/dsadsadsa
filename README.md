# Campaign routing: Team Orienteering with clash-free rallies

k politicians campaign across Malaysia within a fixed time budget. Each politician starts and ends at party HQ in Kuala Lumpur.
- **Goal:** maximise population reach. A town's first rally earns its population, and each later rally there earns `alpha` times the previous one.
- **Constraint:** two politicians may never hold a rally in the same town at the same time.

## Build and run

Requires a C++17 compiler and `make`. No third-party libraries.

```sh
make                      # builds build/campaign
./build/campaign          # runs all approaches with the defaults below
./build/campaign --help   # all options
```

Example: 3 politicians, no decay, 72 hours, write routes and a summary row:

```sh
./build/campaign --k 3 --alpha 1 --hours 72 --routes-dir results --summary results/summary.csv
```

## Folder structure

| Path | Contents |
|---|---|
| `src/instance.*` | Loads GeoNames data, filters and merges towns, builds the travel-time matrix |
| `src/booking.*` | `BookingTable`: per-town sorted rally bookings, used to detect and avoid clashes |
| `src/sequential.cpp` | Baseline (uncoordinated) and Approach A (sequential greedy with booking table) |
| `src/ils.cpp` | Approach B (iterated local search with clash-aware `maxShift`) |
| `src/solution.*` | Independent validator: timing, horizon, repeat visits, clashes, reach |
| `src/main.cpp` | Command-line driver |
| `CS201_2026_project/` | Provided GeoNames dataset (`MY.txt`) |
| `docs/papers/` | Reference papers and reading list |

## Approaches

| | What it does | Data structures |
|---|---|---|
| **Baseline: uncoordinated** | Each politician plans greedily as if alone, which is classic single-traveller thinking. Violates the clash rule by design. | Visited flags |
| **A: sequential greedy** | Politicians are planned one after another. Each appends the town with the best `reach / (travel + wait + rally hours)`, avoiding times already booked. | `BookingTable` (per-town sorted intervals) |
| **B: iterated local search** | All politicians at once. Insert the best `(town, politician, position)` by `gain^2 / extra time`, then shake (remove runs of visits) and repeat. Based on Vansteenwegen et al. (2009). | `BookingTable` + per-visit `{arrive, start, finish, maxShift}`. `maxShift` includes the slack before the next clash, so each insertion is checked in O(1) |

## Data and preprocessing

- **Source:** `CS201_2026_project/MY.txt` (GeoNames, Malaysia).
- **Columns used:** geonameid, asciiname, latitude, longitude, feature class, feature code, population.
- **Records:** feature class `P` (populated places) with population ≥ `--min-pop` (default 50,000). Peninsular Malaysia only unless `--borneo` is given.
- **HQ:** the record with feature code `PPLC` (Kuala Lumpur).
- **Merging:** GeoNames lists cities and their districts separately, each with a population. Places are sorted by population, and any place within `--merge-km` (default 5 km) of a larger kept place is dropped.

## Derived attributes and assumptions

| Attribute | Definition |
|---|---|
| Travel time (same landmass) | Great-circle km × 1.3 road factor ÷ 70 km/h |
| Travel time (peninsula ↔ Borneo) | 2.5 h airport overhead + km ÷ 700 km/h |
| Rally length | `1 + log10(population / 10,000)` hours, clamped to [1, 3] |
| Reach of the v-th rally in a town | `population × alpha^(v-1)` |

- Each politician rallies in a town at most once.
- Time is continuous hours, with no overnight or daytime-only window yet.

## Output columns

| Column | Meaning |
|---|---|
| `reach`, `%max` | Total reach, and as a share of the bound where every politician visits every town |
| `clashes` | Pairs of overlapping rallies in the same town; must be 0 to be valid |
| `travel_h`, `wait_h` | Hours driving or flying, and hours waiting for a town to be free |
| `fairness` | Lowest politician's reach ÷ highest politician's reach |
| `valid` | Passed every check in the independent validator |

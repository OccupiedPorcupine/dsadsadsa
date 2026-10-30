# Campaign routing: Team Orienteering with clash-free rallies

k politicians campaign across Malaysia for 14 days, rallying and travelling only between 08:00 and 20:00. Each politician starts from their own home base and must be back there by the end of the campaign.
- **Goal:** maximise population reach. Rallies are worth more closer to polling day. A repeat rally in a town is discounted by `alpha`, but the discount fades as time passes since the last one.
- **Soft clash rule:** two politicians *may* rally in the same town at the same time, but both lose reach in proportion to the overlap. Each planner chooses whether to wait, clash, or go somewhere else.

What changed from the first version, and why: [docs/CHANGES_V2.md](docs/CHANGES_V2.md).

## Documentation

| Document | Read it for |
|---|---|
| [docs/CHANGES_V2.md](docs/CHANGES_V2.md) | Every change from v1 to v2: model, options, output files, code, and results |
| [docs/DESIGN.md](docs/DESIGN.md) | The problem, why classic TSP doesn't fit, data structures, both algorithms with pseudocode, complexity, how results are checked, limitations, and a code map |
| [docs/RESULTS.md](docs/RESULTS.md) | Experiment results, findings, and the commands to reproduce them |
| [docs/ASSUMPTIONS.md](docs/ASSUMPTIONS.md) | Every modelling assumption and decision, with reasons |
| [docs/AI_USAGE.md](docs/AI_USAGE.md) | Draft AI acknowledgement for the submission (team must complete) |
| [docs/papers/README.md](docs/papers/README.md) | Reading list and downloaded papers |
| [viz/README.md](viz/README.md) | How the route map works |

## Build and run

Requires a C++17 compiler and `make`. No third-party libraries.

```sh
make                      # builds build/campaign
./build/campaign          # runs all approaches with the defaults below
./build/campaign --help   # all options
```

Example: 3 politicians, no repeat penalty, a 6-day campaign, write routes and a summary row:

```sh
mkdir -p results/k3
./build/campaign --k 3 --alpha 1 --days 6 --routes-dir results/k3 --summary results/summary.csv
```

Other examples:

```sh
./build/campaign --starts random --seed 4                 # random home bases, reproducible
./build/campaign --starts "Kuala Lumpur,Ipoh,Kuantan" --k 3
./build/campaign --clash-penalty 0                        # clashing costs nothing
./build/campaign --tau-repeat 12 --tau-poll 168           # faster-fading decay
```

## Route map

The visualisation is kept separate from the solver, in `viz/`. It reads the CSVs that `--routes-dir` writes and needs only Python 3, with no extra packages.

```sh
mkdir -p results/default
./build/campaign --routes-dir results/default
python3 viz/make_map.py results/default      # writes results/default/campaign_map.html
```

Open `campaign_map.html` in a browser. It needs an internet connection for the map tiles.

What the map shows:
- **Towns:** coloured and sized by population. Faded towns weren't visited.
- **Homes:** an `H` badge in each politician's colour.
- **Routes:** each politician's route in their own colour, with arrows showing the direction of travel.
- **Clashes:** marked with a dashed red ring. They are allowed, but cost reach.
- **Controls:** switch between approaches, show or hide individual politicians, and turn on visit numbers.
- **Timeline:** press Play or drag the slider to see where each politician is on any day and time.
- **Route table:** every rally with its day, times and reach.

## Folder structure

| Path | Contents |
|---|---|
| `src/instance.*` | Loads GeoNames data, filters and merges towns, picks home bases, builds the travel-time matrix; `Clock` (daily window) |
| `src/booking.*` | `BookingTable`: per-town sorted rally bookings, aware of the daily window; finds free slots and measures overlaps |
| `src/reach.*` | The reach model: time decay, polling-day weight, clash penalty |
| `src/nearest.cpp` | Benchmark: classic nearest-neighbour TSP tour |
| `src/sequential.cpp` | Baseline (uncoordinated) and Approach A (sequential greedy with booking table) |
| `src/ils.cpp` | Approach B (iterated local search with clash-aware `maxShift`) |
| `src/solution.*` | Independent validator: timing, daily window, return home, repeat visits, clashes, reach |
| `src/main.cpp` | Command-line driver; `--routes-dir` exports towns, settings and routes as CSV |
| `viz/` | Route map: `make_map.py` turns a results folder into an interactive HTML map (`map_template.html`) |
| `results/` | Solver output (CSVs) and generated maps |
| `CS201_2026_project/` | Provided GeoNames dataset (`MY.txt`) |
| `notebooks/explore_geonames.ipynb` | First look at the GeoNames spreadsheet with pandas |
| `docs/ASSUMPTIONS.md` | Every modelling assumption and decision, with reasons |
| `docs/papers/` | Reference papers and reading list |

## Approaches

| | What it does | Data structures |
|---|---|---|
| **Benchmark: classic nearest neighbour** | The brief's textbook TSP heuristic: a tour through every town, closest first, from each politician's home. Ignores population, the time budget and other politicians. | Visited flags |
| **Baseline: uncoordinated** | Each politician plans greedily as if alone, which is classic single-traveller thinking. Clashes freely and pays the penalty. | Visited flags |
| **A: sequential greedy** | Politicians are planned one after another. Each appends the town and timing (wait or clash) with the best `reach gain / (travel + wait + rally hours)`. | `BookingTable` (per-town sorted intervals) |
| **B: iterated local search** | All politicians at once. Insert the best `(town, politician, position, timing)` by `gain^2 / extra time`, then shake (remove runs of visits) and repeat. Based on Vansteenwegen et al. (2009). Runs twice, from empty routes and from A's plan, and keeps the better plan, so it never ends below A. | `BookingTable` + per-visit `{arrive, start, finish, mode, maxShift}`. `maxShift` includes the slack before the next clash and before the day's window closes, so each insertion is checked in O(1) |

## Data and preprocessing

- **Source:** `CS201_2026_project/MY.txt` (GeoNames, Malaysia).
- **Columns used:** geonameid, asciiname, latitude, longitude, feature class, feature code, population.
- **Records:** feature class `P` (populated places) with population ≥ `--min-pop` (default 50,000). Peninsular Malaysia only unless `--borneo` is given.
- **Home bases:** by default KL, George Town, Johor Bahru, Kota Bharu, Ipoh, Kuantan, Alor Setar, in that order (`--starts`). `--starts random --seed N` picks random towns.
- **Merging:** GeoNames lists cities and their districts separately, each with a population. Places are sorted by population, and any place within `--merge-km` (default 5 km) of a larger kept place is dropped.

## Derived attributes and assumptions

The full list, with the reasoning behind each one, is in [docs/ASSUMPTIONS.md](docs/ASSUMPTIONS.md). The main ones:

| Attribute | Definition |
|---|---|
| Travel time (same landmass) | Great-circle km × 1.3 road factor ÷ 70 km/h |
| Travel time (peninsula ↔ Borneo) | 2.5 h airport overhead + km ÷ 700 km/h |
| Rally length | `1 + log10(population / 10,000)` hours, clamped to [1, 3] |
| Reach of a rally | `population × P(E) × w(start) × share` |
| `E` (fading repeat count) | `Σ exp(−gap / tauRepeat)` over earlier rallies in the town (default tauRepeat = 48 h) |
| `P` (repeat penalty) | `alpha^E` at whole E, straight lines in between, so it fades even at alpha = 0 |
| `w` (polling-day weight) | `exp(−(polling − start) / tauPoll)` (default tauPoll = 336 h) |
| `share` (crowd share) | While m other politicians rally in the town at the same time, the rally keeps `(1 − beta) / (1 − beta + m × beta)` of its crowd, averaged over the rally: `1 − beta` with one other, `1 / (m + 1)` at beta = 0.5 |
| Clash penalty `beta` | 0.5: a fully overlapped rally loses half its reach |

- Each politician rallies in a town at most once.
- Campaigning happens 08:00–20:00 for 14 days. Rallies must fit within one day; drives can be split across a night.

## Output columns

| Column | Meaning |
|---|---|
| `reach`, `%max` | Total reach, and as a share of the bound where every politician reaches every town's full population |
| `clashes`, `clash_h` | Pairs of overlapping rallies in the same town, and their total overlap in hours. Allowed, but they cost reach |
| `travel_h`, `wait_h` | Hours driving or flying, and hours waiting for a town to be free or for the next morning |
| `fairness` | Lowest politician's reach ÷ highest politician's reach |
| `valid` | Passed every check in the independent validator (timing, daily window, back home in time) |

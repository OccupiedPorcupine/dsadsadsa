# Route map

An interactive map of the planned routes. It is kept separate from the solver: it only reads the CSV files the solver exports and never calls the C++ code.

## Make a map

```sh
mkdir -p results/default
./build/campaign --routes-dir results/default      # solver writes the CSVs
python3 viz/make_map.py results/default            # writes results/default/campaign_map.html
```

Open the HTML file in a browser. The map tiles load from the internet. Everything else is inside the file, so you can share it as a single file.

Requirements: Python 3 standard library only. The page loads [Leaflet](https://leafletjs.com) 1.9.4 from unpkg and grey basemap tiles from Esri, neither of which needs an API key.

## Files

| File | What it does |
|---|---|
| `make_map.py` | Reads `meta.csv`, `towns.csv` and `<approach>_routes.csv` from a results folder, then puts the data into the template as JSON. |
| `map_template.html` | The page: map, controls, legend, timeline and table. The data goes where the `/*DATA*/null` placeholder is. |

## Input files (written by `--routes-dir`)

| File | Columns |
|---|---|
| `meta.csv` | `key,value`: politicians, alpha, campaign_hours, hq_name, hq_lat, hq_lon |
| `towns.csv` | every town in the instance: geonameid, town, lat, lon, population, rally_h |
| `<approach>_routes.csv` | one row per rally: politician, seq, geonameid, town, lat, lon, population, arrive_h, start_h, finish_h, return_h |

## What the map shows

| Element | Meaning |
|---|---|
| Dot colour and size | Town population, in 5 bands. Lighter and smaller means fewer people. |
| Faded dot | Town not visited by any shown politician |
| Coloured line with arrows | A politician's route, in travel order, from HQ and back |
| Dashed red ring | Clash: two rallies in this town overlap in time. Also counted in the ⚠ Clashes tile |
| `HQ` badge | Party HQ, Kuala Lumpur |
| Numbered badges (optional) | Stop number in each politician's route |
| `P1`, `P2` markers (timeline) | Where each politician is at the selected hour |

**Controls**
- Switch approach (baseline, A, B).
- Show or hide each politician.
- Turn visit numbers on or off.
- Play the timeline, or drag the slider.
- Hover over a town for its population and rally times.
- Open **Route table** for every rally as text.

**Stats tiles** are recomputed in the browser using the same rules as the solver's validator, so they should match the solver's output table.

## Colour choices

- **Population** uses one blue hue from light to dark, because it is a magnitude. Dot size changes too, so the information isn't carried by colour alone.
- **Politicians** use orange, then aqua, then further categorical colours. Blue is skipped because it already means population.
- **Clashes** use a reserved red, always with the ⚠ symbol and a text label.
- The colours pass a colour-blindness check (CVD separation ΔE ≥ 9) in both light and dark mode.
- We also tried a light-yellow-to-dark-purple ramp and single-hue amber, purple and grey ramps. We kept blue.
- The page follows the system light/dark setting.
- **If you change a colour, change it in all three blocks** at the top of `map_template.html` (one for light mode, two for dark mode), then rerun `make_map.py`.

## Known limits

- With more than about 3 politicians, the route colours get harder to tell apart. Use the Show checkboxes to compare a few at a time.
- Arrows point along straight lines between towns, not real roads.
- The timeline moves politicians in a straight line between towns, at a constant speed for each leg.

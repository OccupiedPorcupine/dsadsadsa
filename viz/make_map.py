#!/usr/bin/env python3
"""Builds an interactive map of the campaign routes from the solver's CSV output.

Usage:
    ./build/campaign --routes-dir results/default
    python3 viz/make_map.py results/default

This writes results/default/campaign_map.html. Open it in a browser
(it needs an internet connection for the map tiles).

Only uses the Python standard library.
"""

import csv
import json
import sys
from pathlib import Path

APPROACHES = ["uncoordinated", "sequential", "ils"]
TEMPLATE = Path(__file__).parent / "map_template.html"


def read_csv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_run(run_dir):
    meta = {row["key"]: row["value"] for row in read_csv(run_dir / "meta.csv")}

    towns = []
    index_of = {}  # geonameid -> position in `towns`
    for row in read_csv(run_dir / "towns.csv"):
        index_of[row["geonameid"]] = len(towns)
        towns.append({
            "name": row["town"],
            "lat": float(row["lat"]),
            "lon": float(row["lon"]),
            "pop": int(row["population"]),
            "rallyHours": float(row["rally_h"]),
        })

    routes = {}
    returns = {}
    for approach in APPROACHES:
        path = run_dir / f"{approach}_routes.csv"
        if not path.exists():
            continue
        visits = []
        back_home = {}
        for row in read_csv(path):
            politician = int(row["politician"])
            visits.append({
                "p": politician,
                "seq": int(row["seq"]),
                "town": index_of[row["geonameid"]],
                "arrive": float(row["arrive_h"]),
                "start": float(row["start_h"]),
                "finish": float(row["finish_h"]),
                "clash": row["clash"] == "1",
                "reach": float(row["reach"]),
            })
            back_home[politician] = float(row["return_h"])
        routes[approach] = visits
        returns[approach] = back_home

    homes = [
        {"name": row["name"], "lat": float(row["lat"]), "lon": float(row["lon"])}
        for row in sorted(read_csv(run_dir / "homes.csv"), key=lambda r: int(r["politician"]))
    ]

    return {
        "meta": {
            "politicians": int(meta["politicians"]),
            "alpha": float(meta["alpha"]),
            "campaignHours": float(meta["campaign_hours"]),
            "days": int(meta["days"]),
            "dayHours": float(meta["day_hours"]),
            "dayStart": float(meta["day_start"]),
            "tauRepeat": float(meta["tau_repeat"]),
            "tauPoll": float(meta["tau_poll"]),
            "clashPenalty": float(meta["clash_penalty"]),
            "maxReach": float(meta["max_reach"]),
        },
        "homes": homes,
        "towns": towns,
        "routes": routes,
        "returns": returns,
    }


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)

    run_dir = Path(sys.argv[1])
    data = load_run(run_dir)
    if not data["routes"]:
        sys.exit(f"no *_routes.csv files found in {run_dir}")

    # "</" inside a <script> block would end it early, so escape it.
    data_json = json.dumps(data).replace("</", "<\\/")
    html = TEMPLATE.read_text(encoding="utf-8").replace("/*DATA*/null", data_json)

    out_path = run_dir / "campaign_map.html"
    out_path.write_text(html, encoding="utf-8")
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()

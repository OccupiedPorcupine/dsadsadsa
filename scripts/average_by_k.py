"""Averages a --summary CSV over the runs for each k: one row per k, one column per approach.

Usage: python3 scripts/average_by_k.py results/sweep/weighted_starts.csv
Prints Markdown tables of mean reach, B's lead, fairness and runtime, and how often each
approach's plan was valid, then a table comparing the versions of B (and multi-start A). The full-tour nearest-neighbour benchmark is left out, because
it is invalid by design.
"""

import csv
import statistics
import sys
from collections import defaultdict

APPROACHES = ["nearest-budget", "uncoordinated", "sequential", "ils"]
NAMES = {"nearest-budget": "NN", "uncoordinated": "Uncoord.", "sequential": "A", "ils": "B"}
# Versions of B from the experiments in issues #13-#17. ils-cache is left out: same plans as ils.
VERSIONS = ["multistart", "ils", "ils-revert", "ils-alns", "ils-moves", "ils-full"]


def main(path):
    # runs[k][homes][approach] = row
    runs = defaultdict(lambda: defaultdict(dict))
    with open(path, newline="") as file:
        for row in csv.DictReader(file):
            if row["approach"] in APPROACHES + VERSIONS:
                runs[int(row["k"])][row["homes"]][row["approach"]] = row

    def column(k, approach, field):
        return [float(run[approach][field]) for run in runs[k].values()]

    print("Mean reach in millions (standard deviation in brackets)\n")
    print("| k | runs | " + " | ".join(NAMES[a] for a in APPROACHES) + " |")
    print("|---|---|" + "---|" * len(APPROACHES))
    for k in sorted(runs):
        cells = []
        for a in APPROACHES:
            reach = [r / 1e6 for r in column(k, a, "reach")]
            cells.append(f"{statistics.mean(reach):.2f} ({statistics.stdev(reach):.2f})")
        print(f"| {k} | {len(runs[k])} | " + " | ".join(cells) + " |")

    print("\nB's lead (mean, min to max), and how many runs A beat NN\n")
    print("| k | B vs A | B vs NN | A vs NN | A beats NN |")
    print("|---|---|---|---|---|")
    for k in sorted(runs):
        def lead(x, y):
            gains = [100 * (a / b - 1) for a, b in zip(column(k, x, "reach"), column(k, y, "reach"))]
            return f"{statistics.mean(gains):+.1f}% ({min(gains):+.1f} to {max(gains):+.1f})"
        wins = sum(a > n for a, n in zip(column(k, "sequential", "reach"), column(k, "nearest-budget", "reach")))
        print(f"| {k} | {lead('ils', 'sequential')} | {lead('ils', 'nearest-budget')} | "
              f"{lead('sequential', 'nearest-budget')} | {wins} of {len(runs[k])} |")

    print("\nMean fairness, clashes and runtime\n")
    print("| k | " + " | ".join(f"{NAMES[a]} fairness" for a in APPROACHES)
          + " | " + " | ".join(f"{NAMES[a]} clashes" for a in APPROACHES) + " | B runtime (ms) |")
    print("|---|" + "---|" * (2 * len(APPROACHES) + 1))
    for k in sorted(runs):
        fair = [f"{statistics.mean(column(k, a, 'fairness')):.2f}" for a in APPROACHES]
        clash = [f"{statistics.mean(column(k, a, 'clashes')):.1f}" for a in APPROACHES]
        runtime = statistics.mean(column(k, "ils", "runtime_ms"))
        print(f"| {k} | " + " | ".join(fair) + " | " + " | ".join(clash) + f" | {runtime:.0f} |")

    versions = [v for v in VERSIONS if all(v in run for k in runs for run in runs[k].values())]
    if versions:
        print("\nVersions of B: mean reach in millions, the final version's lead over B, and mean runtime\n")
        print("| k | " + " | ".join(versions) + " | ils-full vs ils | ils runtime (ms) | ils-full runtime (ms) |")
        print("|---|" + "---|" * (len(versions) + 3))
        for k in sorted(runs):
            means = [f"{statistics.mean(column(k, v, 'reach')) / 1e6:.2f}" for v in versions]
            gains = [100 * (a / b - 1) for a, b in zip(column(k, "ils-full", "reach"), column(k, "ils", "reach"))]
            lead = f"{statistics.mean(gains):+.1f}% ({min(gains):+.1f} to {max(gains):+.1f})"
            print(f"| {k} | " + " | ".join(means) + f" | {lead} | {statistics.mean(column(k, 'ils', 'runtime_ms')):.0f}"
                  f" | {statistics.mean(column(k, 'ils-full', 'runtime_ms')):.0f} |")

    checked = APPROACHES + versions
    invalid = [(k, a) for k in runs for run in runs[k].values() for a in checked if run[a]["valid"] != "1"]
    total = sum(len(runs[k]) for k in runs) * len(checked)
    print(f"\nValid plans: {total - len(invalid)} of {total}")


if __name__ == "__main__":
    main(sys.argv[1])

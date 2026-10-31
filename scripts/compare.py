"""Compare two scripts/bench.sh outputs row by row.

    python3 scripts/compare.py BASE.csv NEW.csv [APPROACH]

Rows are matched by position (both files come from the same benchmark). Prints reach and runtime
ratios NEW / BASE for the 27 settings and for the 90 weighted-home runs, and any invalid plans.
"""
import csv
import statistics
import sys


def rows(path, approach):
    return [r for r in csv.DictReader(open(path)) if approach is None or r["approach"] == approach]


def summary(name, pairs):
    ratios = [float(n["reach"]) / float(b["reach"]) for b, n in pairs]
    times = [float(n["runtime_ms"]) / max(float(b["runtime_ms"]), 1e-3) for b, n in pairs]
    better = sum(r > 1 + 1e-9 for r in ratios)
    worse = sum(r < 1 - 1e-9 for r in ratios)
    print(f"{name}: {len(pairs)} runs, reach NEW/BASE mean {statistics.mean(ratios):.4f}, "
          f"min {min(ratios):.4f}, max {max(ratios):.4f}; better {better}, worse {worse}, "
          f"equal {len(pairs) - better - worse}; runtime NEW/BASE median {statistics.median(times):.2f}x, "
          f"max {max(times):.2f}x")


base_path, new_path = sys.argv[1], sys.argv[2]
approach = sys.argv[3] if len(sys.argv) > 3 else None
base, new = rows(base_path, approach), rows(new_path, approach)
assert len(base) == len(new), f"{len(base)} vs {len(new)} rows"
pairs = list(zip(base, new))
summary("27 settings (RESULTS §2, §5)", pairs[:27])
summary("90 weighted-home runs (RESULTS §4.1)", pairs[27:])
invalid = [i for i, (_, n) in enumerate(pairs) if n["valid"] != "1"]
print(f"invalid NEW plans: {invalid if invalid else 'none'}")

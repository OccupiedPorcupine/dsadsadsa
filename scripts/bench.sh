#!/bin/zsh
# Runs one approach over the experiment benchmark and appends a summary CSV.
#   scripts/bench.sh OUT.csv APPROACH [extra campaign options...]
# The benchmark: the 27 settings of RESULTS §2 and §5, then the 90 weighted-home runs of §4.1.
# Compare two outputs with scripts/compare.py.
set -e
out=$1; approach=$2; shift 2
rm -f "$out"
run() { ./build/campaign --approach "$approach" "$@" --summary "$out" > /dev/null; }
for k in 2 3 5; do for a in 0 0.5 1; do for d in 6 14; do run --k $k --alpha $a --days $d "$@"; done; done; done
for tr in 12 48 168; do for tp in 168 336 100000; do run --tau-repeat $tr --tau-poll $tp "$@"; done; done
for k in 2 3 4 5 6 7 8 9 10; do for s in 1 2 3 4 5 6 7 8 9 10; do run --k $k --starts weighted --seed $s "$@"; done; done

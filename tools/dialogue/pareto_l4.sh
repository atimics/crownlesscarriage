#!/bin/bash
# Run the L4 Pareto sweep on one machine: one evolution per prosperity weight, all in parallel.
# Needs cmake, a C compiler, sqlite headers and python3 with numpy. Results land in ./results.
set -euo pipefail
cd "$(dirname "$0")/../.."
WEIGHTS=${WEIGHTS:-"0 1 2 4 8 16 32 64"}
WORKERS=${WORKERS:-8}
GENERATIONS=${GENERATIONS:-120}
mkdir -p results
cmake -S . -B build -DCC_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release > results/cmake.log 2>&1
cmake --build build --target crowdsim -j "$(nproc)" >> results/cmake.log 2>&1
python3 tools/dialogue/fastworld.py >/dev/null 2>&1 || true
for w in $WEIGHTS; do
  python3 tools/dialogue/evolve_life.py --stage "l4:$w" --output "results/l4-$w" \
    --generations "$GENERATIONS" --pairs 32 --batch 48 --workers "$WORKERS" --fresh 400 \
    > "results/l4-$w.log" 2>&1 &
done
wait
python3 tools/dialogue/pareto_table.py results | tee results/pareto.txt
echo finished > results/DONE

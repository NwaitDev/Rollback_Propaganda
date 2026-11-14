#!/usr/bin/env bash
# Writes the reference outputs of ./simulation that test_programs.py compares
# with (tests/integration/golden/). Run it ONLY after a change of the game that
# is meant to change how games unfold, then check the differences with git diff.
#   tests/integration/make_golden.sh          (from the project root, after make)
set -euo pipefail
out=tests/integration/golden
mkdir -p "$out"
rm -f "$out"/*.txt
for deck in all_spells implemented modifiers empty; do
    for seed in 1 2 3 4 5 6; do
        ./simulation --play --seed "$seed" --deck "decks/$deck.json" \
            --contracts contracts/mixed.json > "$out/${deck}_${seed}.txt"
    done
done
echo "reference outputs written in $out"

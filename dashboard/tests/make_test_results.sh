#!/usr/bin/env bash
# Plays the small campaigns the dashboard's tests run on (a few seconds).
#   dashboard/tests/make_test_results.sh <folder>      (from the project root)
set -euo pipefail
out=${1:-build/dashboard_test}
rm -rf "$out"
mkdir -p "$out/experiments" "$out/results"
python3 - "$out" <<'PY'
import json, sys
out = sys.argv[1]
base = dict(seed=1, games={"random_only": 40, "with_smart": 40}, players=3,
            random_greed=[8, 13, 17], hand_size=4, deck="decks/empty.json",
            events="events/control.json", contracts="contracts/two_per_faction.json")
campaigns = {
    "control": {},
    "control_seed2": {"seed": 2},                       # same parameters: pooled
    "card_EUPHORIA": {"extra_card": "EUPHORIA"},
    "card_IMMUNITY": {"extra_card": "IMMUNITY"},
    "card_PRECIPITATION": {"extra_card": "PRECIPITATION"},
    "impl_hand4": {"deck": "decks/implemented.json"},
    "impl_hand3": {"deck": "decks/implemented.json", "hand_size": 3},
    "impl_hand5": {"deck": "decks/implemented.json", "hand_size": 5},
    "impl_players4": {"deck": "decks/implemented.json", "players": 4, "random_greed": [8, 13]},
    "impl_mixed": {"deck": "decks/implemented.json", "contracts": "contracts/mixed.json"},
}
for name, changes in campaigns.items():
    experiment = {**base, **changes, "name": name, "output": f"{out}/results/{name}"}
    with open(f"{out}/experiments/{name}.json", "w") as f:
        json.dump(experiment, f, indent=1)
PY
for experiment in "$out"/experiments/*.json; do
    ./campaign "$experiment" > /dev/null 2>&1
done
echo "test campaigns in $out/results"

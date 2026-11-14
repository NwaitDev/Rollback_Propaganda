"""Tests of the programs themselves (./simulation and ./campaign): options and
error messages, reproducibility, the CSV files of a campaign, and reference
outputs of whole games.

    make test             (builds the programs, then runs these tests)
    python3 -m pytest -q tests/integration      (from the project root)

The campaigns played here are tiny and written in a temporary folder, never
in results/.
"""

import csv
import json
import math
import os
import statistics
import subprocess
from pathlib import Path

import pytest

PROJECT = Path(__file__).resolve().parents[2]
SIMULATION = PROJECT / "simulation"
CAMPAIGN = PROJECT / "campaign"
GOLDEN = Path(__file__).parent / "golden"

pytestmark = pytest.mark.skipif(not (SIMULATION.exists() and CAMPAIGN.exists()),
                                reason="run make first")


def run(program, *args):
    return subprocess.run([str(program), *map(str, args)], cwd=PROJECT,
                          capture_output=True, text=True, timeout=300)


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


# --- options and errors --------------------------------------------------------------

@pytest.mark.parametrize("args, message", [
    (["--seed", "-3"], "--seed expects an integer between 0 and 4294967295"),
    (["--seed", "abc"], "--seed expects an integer"),
    (["--seed", "99999999999"], "--seed expects an integer"),
    (["--popularity", "abc"], "--popularity expects an integer"),
    (["--deck", "no_such_file.json"], "cannot open"),
    (["--deck", "decks/empty.json", "--deck", "decks/empty.json"], "one --deck for every mage, or one per mage"),
    (["--contracts", "decks/empty.json"], "contracts"),
    (["--events", "nope.json"], "cannot open"),
    (["--foo"], "usage"),
])
def test_simulation_errors(args, message):
    result = run(SIMULATION, *args)
    assert result.returncode != 0
    assert message in result.stderr


def test_simulation_is_reproducible():
    first = run(SIMULATION, "--play", "--seed", 11, "--deck", "decks/all_spells.json")
    again = run(SIMULATION, "--play", "--seed", 11, "--deck", "decks/all_spells.json")
    other = run(SIMULATION, "--play", "--seed", 12, "--deck", "decks/all_spells.json")
    assert first.returncode == 0 and first.stdout == again.stdout != other.stdout
    assert "winner: player n°" in first.stdout


def test_simulation_prints_a_drawn_seed():
    result = run(SIMULATION)
    assert result.returncode == 0 and result.stdout.startswith("seed: ")


@pytest.mark.parametrize("name", sorted(p.stem for p in GOLDEN.glob("*.txt")))
def test_reference_outputs(name):
    """Whole games, byte for byte. A change of the rules or of the players'
    decisions changes them on purpose: then regenerate them with
    tests/integration/make_golden.sh and check the differences."""
    deck, seed = name.rsplit("_", 1)
    result = run(SIMULATION, "--play", "--seed", seed, "--deck", f"decks/{deck}.json",
                 "--contracts", "contracts/mixed.json")
    assert result.stdout == (GOLDEN / f"{name}.txt").read_text()


def test_campaign_lists_the_cards():
    result = run(CAMPAIGN, "--cards")
    assert result.returncode == 0
    assert "EUPHORIA" in result.stdout.split() and len(result.stdout.split()) == 10


# --- a tiny campaign ---------------------------------------------------------------------

@pytest.fixture(scope="module")
def campaign(tmp_path_factory):
    folder = tmp_path_factory.mktemp("campaign")
    experiment = {
        "name": "tiny", "seed": 5, "games": {"random_only": 6, "with_smart": 6},
        "players": 3, "random_greed": [8, 13], "hand_size": 4,
        "deck": "decks/all_spells.json", "events": "events/control.json",
        "contracts": "contracts/mixed.json", "extra_card": "EUPHORIA",
        "output": str(folder / "results"),
    }
    path = folder / "tiny.json"
    path.write_text(json.dumps(experiment))
    result = run(CAMPAIGN, path)
    assert result.returncode == 0, result.stderr
    return path, folder / "results"


def test_campaign_headers_match_the_templates(campaign):
    _, results = campaign
    for name in ("campaigns.csv", "details.csv"):
        produced = (results / name).read_text().splitlines()[0]
        template = (PROJECT / "metrics" / name).read_text().splitlines()[0]
        assert produced == template
    documented = {(r["file"], r["column"]) for r in read_csv(PROJECT / "metrics" / "columns.csv")}
    for name in ("campaigns.csv", "details.csv"):
        header = (PROJECT / "metrics" / name).read_text().splitlines()[0].split(",")
        assert {(name, c) for c in header} == {d for d in documented if d[0] == name}


def test_campaign_metrics_match_games_csv(campaign):
    _, results = campaign
    rows = read_csv(results / "campaigns.csv")
    games = read_csv(results / "games.csv")
    details = read_csv(results / "details.csv")
    assert len(rows) == 4 and len(games) == 24
    for row in rows:
        mine = [g for g in games if g["random_greed"] == row["random_greed"] and g["type"] == row["game_type"]]
        finished = [g for g in mine if g["finished"] == "1"]
        assert int(row["finished_games"]) == len(finished)
        turns = [int(g["turns"]) for g in finished]
        assert math.isclose(float(row["turns_mean"]), statistics.mean(turns), abs_tol=1e-3)
        assert math.isclose(float(row["turns_sd"]), statistics.stdev(turns), abs_tol=1e-3)
        assert int(row["holder_wins"]) == sum(g["winner"] == g["holder_seat"] for g in finished)
        margins = []
        for g in finished:
            points = sorted(int(g[f"points_{i}"]) for i in range(3))
            margins.append(points[-1] - points[-2])
        assert math.isclose(float(row["final_margin_mean"]), statistics.mean(margins), abs_tol=1e-3)
        key = lambda d: d["random_greed"] == row["random_greed"] and d["game_type"] == row["game_type"]
        seats = [d for d in details if key(d) and d["kind"] == "seat"]
        assert sum(int(d["wins"]) for d in seats) == len(finished)
        contracts = [d for d in details if key(d) and d["kind"] == "contract"]
        assert sum(int(d["occurrences"]) for d in contracts) == 3 * len(finished)
        cards = {d["item"]: d for d in details if key(d) and d["kind"] == "card"}
        # all_spells: 6 ETHER and one of each spell per mage, plus the extra EUPHORIA
        assert int(cards["ETHER"]["occurrences"]) == 6 * 3 * len(finished)
        assert int(cards["EUPHORIA"]["occurrences"]) == (3 + 1) * len(finished)
        assert cards["ETHER"]["failed"] == ""
        if row["game_type"] == "with_smart":
            assert row["smart_wins"] == row["holder_wins"]   # the smart player holds the card


def test_campaign_is_reproducible(campaign):
    path, results = campaign
    before = {name: (results / name).read_text() for name in ("games.csv", "campaigns.csv", "details.csv")}
    assert run(CAMPAIGN, path).returncode == 0
    for name, content in before.items():
        assert (results / name).read_text() == content


def test_replay_matches_games_csv(campaign):
    path, results = campaign
    line = next(g for g in read_csv(results / "games.csv")
                if g["random_greed"] == "13" and g["type"] == "with_smart" and g["index"] == "2")
    replay = run(CAMPAIGN, path, "--replay", "with_smart", 2, 13)
    assert replay.returncode == 0
    assert f"(seed {line['seed']})" in replay.stdout
    assert f"winner: player n°{line['winner']}" in replay.stdout


@pytest.mark.parametrize("args, message", [
    (["--replay", "with_smart", 2], "several greed levels"),
    (["--replay", "with_smart", 2, 9], "no greed level 9"),
    (["--replay", "with_smart", 99, 8], "has 6 with_smart games"),
])
def test_replay_errors(campaign, args, message):
    path, _ = campaign
    result = run(CAMPAIGN, path, *args)
    assert result.returncode != 0 and message in result.stderr


def test_campaign_usage():
    result = run(CAMPAIGN)
    assert result.returncode != 0 and "usage" in result.stderr

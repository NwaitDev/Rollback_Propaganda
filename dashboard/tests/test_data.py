"""Tests of the data layer, on small campaigns played by ./campaign.

    ROLLBACK_TEST_RESULTS=<folder> pytest dashboard/tests

The folder holds campaigns made for the tests (see make_test_results.sh):
control and control_seed2 (same parameters), card_* (extra cards),
impl_* (other decks, hands, players and contracts).
"""

import math
import os
import statistics
import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import data  # noqa: E402

RESULTS = Path(os.environ.get("ROLLBACK_TEST_RESULTS", ""))
pytestmark = pytest.mark.skipif(not (RESULTS / "control" / "campaigns.csv").exists(),
                                reason="ROLLBACK_TEST_RESULTS is not set")

CONTROL = dict(game_type=["random_only"], players=[3], random_greed=[8],
               smart_greed=[data.SAME_GREED], hand_size=[4], deck=["empty"], events=["control"],
               contracts=["two_per_faction"], extra_card=[data.NO_EXTRA_CARD], max_stealth=[21],
               popularity=[0], max_turns=[1000])


@pytest.fixture(scope="module")
def results():
    return data.load_results(RESULTS)


def games(name):
    return pd.read_csv(RESULTS / name / "games.csv")


def test_loading(results):
    assert not results.empty and not results.problems
    assert set(results.campaigns["campaign"]) >= {"control", "control_seed2", "card_EUPHORIA"}
    assert (results.campaigns.loc[results.campaigns["game_type"] == "random_only", "smart_greed"]
            == data.NOT_APPLICABLE).all()
    assert set(results.campaigns["deck"]) == {"empty", "implemented"}


def test_single_value_pools_identical_campaigns(results):
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], CONTROL)
    assert view.status == "value"
    point = view.points.iloc[0]
    turns = pd.concat([games(n) for n in ("control", "control_seed2")])
    turns = turns[(turns["random_greed"] == 8) & (turns["type"] == "random_only")]["turns"]
    assert point["n"] == len(turns) == 80
    assert point["value"] == pytest.approx(turns.mean(), abs=1e-3)
    half = 1.959964 * statistics.stdev(turns) / math.sqrt(len(turns))
    assert point["high"] - point["value"] == pytest.approx(half, rel=1e-3)
    assert point["campaigns"] == "control, control_seed2"


def test_rate_matches_games_csv(results):
    selection = dict(CONTROL, game_type=["with_smart"])
    point = data.evaluate(results, data.METRICS_BY_KEY["smart_win_rate"], selection).points.iloc[0]
    g = pd.concat([games(n) for n in ("control", "control_seed2")])
    g = g[(g["random_greed"] == 8) & (g["type"] == "with_smart") & (g["finished"] == 1)]
    assert point["value"] == pytest.approx((g["winner"] == g["smart_seat"]).mean())
    assert point["low"] < point["value"] < point["high"]


def test_one_free_parameter_gives_2d(results):
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], dict(CONTROL, random_greed=[8, 13, 17]))
    assert view.status == "2d" and [p.column for p in view.free] == ["random_greed"]
    assert list(view.points["random_greed"]) == [8, 13, 17] and not view.missing


def test_two_free_parameters_give_3d_and_list_the_missing_cases(results):
    selection = dict(CONTROL, deck=["implemented"], hand_size=[3, 4, 5], players=[3, 4])
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], selection)
    assert view.status == "3d"
    assert len(view.points) == 4   # hands 3, 4, 5 with 3 players, hand 4 with 4 players
    assert sorted((m["hand_size"], m["players"]) for m in view.missing) == [(3, 4), (5, 4)]


def test_more_than_two_free_parameters(results):
    selection = dict(CONTROL, random_greed=[8, 13], hand_size=[3, 4], deck=["empty", "implemented"])
    assert data.evaluate(results, data.METRICS_BY_KEY["turns"], selection).status == "too_many"


def test_never_studied(results):
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], dict(CONTROL, hand_size=[3]))
    assert view.status == "nothing" and view.message == "Ce cas de figure n'a jamais été étudié."


def test_empty_selection(results):
    assert data.evaluate(results, data.METRICS_BY_KEY["turns"], dict(CONTROL, deck=[])).status == "incomplete"


def test_smart_greed_follows_random_greed(results):
    selection = dict(CONTROL, game_type=["with_smart"], random_greed=[8, 13])
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], selection)
    assert view.status == "2d" and len(view.points) == 2
    # a fixed smart greed only exists at the matching random greed here
    view = data.evaluate(results, data.METRICS_BY_KEY["turns"], dict(selection, smart_greed=[13]))
    assert len(view.points) == 1 and len(view.missing) == 1


def test_item_is_a_parameter(results):
    metric = data.METRICS_BY_KEY["card_reveal_rate"]
    cards = data.options(results, metric)["item"]
    selection = dict(CONTROL, deck=["implemented"], item=cards)
    view = data.evaluate(results, metric, selection)
    implemented = {"ETHER", "EUPHORIA", "NEURASTHENIA", "PRECIPITATION", "MEMORY_LAPSE"}
    assert view.status == "2d" and set(view.points["item"]) == implemented
    # IMMUNITY was only ever an extra card of the empty deck
    assert [m["item"] for m in view.missing] == ["IMMUNITY"]
    fail = data.options(results, data.METRICS_BY_KEY["card_fail_rate"])["item"]
    assert "ETHER" not in fail


def test_seat_wins_add_up(results):
    metric = data.METRICS_BY_KEY["seat_win_rate"]
    view = data.evaluate(results, metric, dict(CONTROL, item=["0", "1", "2"]))
    assert view.points["value"].sum() == pytest.approx(1.0)


def test_comparison_to_the_control_for_an_extra_card(results):
    metric = data.METRICS_BY_KEY["holder_win_rate"]
    selection = dict(CONTROL, extra_card=["EUPHORIA"], game_type=["random_only"])
    point = data.evaluate(results, metric, selection, reference="control").points.iloc[0]
    g = pd.concat([games(n) for n in ("control", "control_seed2")])
    g = g[(g["random_greed"] == 8) & (g["type"] == "random_only") & (g["finished"] == 1)]
    reference = (g["winner"] == g["index"] % 3).mean()   # the seat that would hold the card
    assert point["reference_value"] == pytest.approx(reference)
    assert point["difference"] == pytest.approx(point["value"] - reference)
    assert 0 <= point["p_value"] <= 1 and point["diff_low"] < point["difference"] < point["diff_high"]


def test_comparison_needs_the_same_greed_and_type(results):
    metric = data.METRICS_BY_KEY["turns"]
    selection = dict(CONTROL, deck=["implemented"], players=[4])
    point = data.evaluate(results, metric, selection, reference="control").points.iloc[0]
    assert point["difference"] is not None   # control has greed 8 / random_only too
    # impl_players4 only has the greed levels 8 and 13
    point = data.evaluate(results, metric, dict(CONTROL, random_greed=[17]),
                          reference="impl_players4").points.iloc[0]
    assert point["difference"] is None and "témoin" in point["comparison_note"]


def test_percentiles_are_not_pooled(results):
    metric = data.METRICS_BY_KEY["max_popularity_p50"]
    pooled = data.evaluate(results, metric, CONTROL).points.iloc[0]
    assert pooled["value"] is None and "fusionne" in pooled["note"]
    single = data.evaluate(results, metric, dict(CONTROL, deck=["implemented"])).points.iloc[0]
    assert single["value"] is not None


def test_statistics():
    e = data.wilson(0, 10)
    assert e.value == 0 and e.low == pytest.approx(0, abs=1e-12) and 0 < e.high < 0.35
    e = data.mean_interval(1, 5, 25)
    assert e.value == 5 and e.low is None
    c = data.compare(data.wilson(30, 100), data.wilson(30, 100))
    assert c.difference == 0 and c.p_value == pytest.approx(1.0) and c.significant is False
    c = data.compare(data.wilson(80, 100), data.wilson(30, 100))
    assert c.significant is True

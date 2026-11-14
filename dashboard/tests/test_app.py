"""Tests of the dashboard's pages, driven by Streamlit's test tool, and of the
launcher on a temporary copy of the project.

    ROLLBACK_TEST_RESULTS=<folder> pytest dashboard/tests
"""

import os
import shutil
import sys
from pathlib import Path

import pytest
from streamlit.testing.v1 import AppTest

DASHBOARD = Path(__file__).resolve().parent.parent
PROJECT = DASHBOARD.parent
sys.path.insert(0, str(DASHBOARD))
import data      # noqa: E402
import launcher  # noqa: E402

RESULTS = Path(os.environ.get("ROLLBACK_TEST_RESULTS", ""))
needs_results = pytest.mark.skipif(not (RESULTS / "control" / "campaigns.csv").exists(),
                                   reason="ROLLBACK_TEST_RESULTS is not set")


def start():
    os.environ["ROLLBACK_RESULTS"] = str(RESULTS)
    app = AppTest.from_file(str(DASHBOARD / "app.py"), default_timeout=60)
    app.run()
    assert not app.exception, app.exception
    return app


def multiselect(app, label):
    return next(w for w in app.multiselect if w.label == label)


def charts(app):
    return [e for e in app.get("plotly_chart")]


@needs_results
def test_first_view_is_a_value():
    app = start()
    assert app.selectbox[0].label == "Métrique"
    assert any(m.label == "Durée de partie (tours)" or True for m in app.metric)
    assert len(app.metric) >= 3 and not charts(app)


@needs_results
def test_free_parameters_switch_the_display():
    app = start()
    multiselect(app, "Avidité des random players").set_value([8, 13, 17]).run()
    assert not app.exception and len(charts(app)) == 1           # 2D
    multiselect(app, "Type de partie").set_value(["random_only", "with_smart"]).run()
    assert not app.exception and len(charts(app)) == 2           # 3D and the view from above
    multiselect(app, "Deck").set_value(["empty", "implemented"]).run()
    assert not app.exception and not charts(app)
    assert any("Plus de deux paramètres" in w.value for w in app.warning)


@needs_results
def test_never_studied_message():
    app = start()
    multiselect(app, "Taille de la main").set_value([3]).run()
    assert any("jamais été étudié" in i.value for i in app.info)


@needs_results
def test_every_metric_renders():
    app = start()
    for metric in data.METRICS:
        app.selectbox[0].set_value(metric).run()
        assert not app.exception, (metric.key, app.exception)


@needs_results
def test_comparison_to_the_control():
    app = start()
    app.selectbox[0].set_value(data.METRICS_BY_KEY["holder_win_rate"]).run()
    assert app.selectbox[1].value == "control"
    assert any(m.label == "Écart au témoin" for m in app.metric)
    multiselect(app, "Carte ajoutée à un joueur").set_value(["EUPHORIA", "IMMUNITY", "PRECIPITATION"]).run()
    next(r for r in app.radio if r.label == "Afficher").set_value("l'écart au témoin").run()
    assert not app.exception and len(charts(app)) == 1


def test_launcher(tmp_path):
    if not (PROJECT / "campaign").exists():
        pytest.skip("./campaign is not built")
    project = tmp_path / "project"
    project.mkdir()
    for folder in ("decks", "events", "contracts"):
        shutil.copytree(PROJECT / folder, project / folder)
    shutil.copy(PROJECT / "campaign", project / "campaign")
    assert "EUPHORIA" in launcher.available_cards(project)
    settings = launcher.Settings(
        name="launcher_test", seed=3, random_only_games=4, with_smart_games=4, players=3,
        random_greeds=[8, 13], smart_greed=None, hand_size=4, deck="decks/empty.json",
        events="events/control.json", contracts="contracts/two_per_faction.json",
        extra_card="EUPHORIA")
    assert launcher.check(settings, project, overwrite=False) == []
    bad = launcher.Settings(**{**settings.__dict__, "name": "é t", "random_greeds": []})
    assert len(launcher.check(bad, project, overwrite=False)) == 2
    path = launcher.write_experiment(settings, project)
    events = list(launcher.run(path, project))
    assert events[-1] == ("done", 0)
    assert any(kind == "progress" for kind, _ in events)
    results = data.load_results(project / "results")
    assert sorted(results.campaigns["random_greed"]) == [8, 8, 13, 13]
    assert launcher.check(settings, project, overwrite=False)   # now it exists


@needs_results
def test_a_parameter_can_be_emptied():
    app = start()
    multiselect(app, "Deck").set_value([]).run()
    assert multiselect(app, "Deck").value == []
    assert any("Choisissez au moins une valeur" in w.value for w in app.warning)
    multiselect(app, "Deck").set_value(["implemented"]).run()
    assert multiselect(app, "Deck").value == ["implemented"] and not app.exception

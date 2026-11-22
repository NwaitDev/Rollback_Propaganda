"""Tests of ./server and ./bot against the protocol (docs/protocol.md): the
shape of the messages, the errors, and the same game whoever plays it
(./simulation, the strategies inside the engine, or ./bot processes).

    make test             (builds the programs, then runs these tests)
    python3 -m pytest -q tests/integration      (from the project root)
"""

import json
import subprocess
import sys
from pathlib import Path

import pytest

PROJECT = Path(__file__).resolve().parents[2]
SIMULATION = PROJECT / "simulation"
SERVER = PROJECT / "server"
BOT = PROJECT / "bot"
ROUTER = PROJECT / "scripts" / "play_with_bots.py"

pytestmark = pytest.mark.skipif(not all(p.exists() for p in (SIMULATION, SERVER, BOT)),
                                reason="run make first")

GAME = ["--deck", "decks/all_spells.json", "--contracts", "contracts/mixed.json"]


def run(program, *args, stdin=""):
    return subprocess.run([str(program), *map(str, args)], cwd=PROJECT, input=stdin,
                          capture_output=True, text=True, timeout=300)


def messages(text):
    return [json.loads(line) for line in text.splitlines()]


def play_section(text):
    """The lines of the text log between the start and the end of the game."""
    lines = text.splitlines()
    start = lines.index("The game is running...")
    end = lines.index("the game is over!")
    return lines[start:end + 1]


def without_state(message):
    return {k: v for k, v in message.items() if k != "state"}


# --- the same game -------------------------------------------------------------------

@pytest.mark.parametrize("deck", ["all_spells", "implemented", "modifiers", "empty"])
@pytest.mark.parametrize("seed", [1, 2, 3])
def test_the_engine_plays_the_simulation_game(deck, seed):
    """Random players inside the engine play the game of ./simulation."""
    options = ["--seed", seed, "--deck", f"decks/{deck}.json", "--contracts", "contracts/mixed.json"]
    simulation = run(SIMULATION, "--play", *options)
    server = run(SERVER, "--light", *options,
                 "--seat", "0=random", "--seat", "1=random", "--seat", "2=random")
    assert server.returncode == 0, server.stderr
    assert play_section(server.stderr) == play_section(simulation.stdout)
    over = messages(server.stdout)[-1]
    scores = [int(line.split("score :")[1].split(")")[0])
              for line in simulation.stdout.splitlines()[-4:-1]]
    assert over["type"] == "game_over" and over["scores"] == scores


@pytest.mark.parametrize("seed", [1, 2, 3, 4])
def test_bots_play_like_the_strategies_of_the_engine(tmp_path, seed):
    """./bot processes take the decisions the same strategies take inside the
    engine: the same announcements, an ask where the engine announces a
    decided, with the same id and the same choice."""
    seats = {0: "smart", 1: "random:14", 2: "smart:8"}
    transcript = tmp_path / "bots.jsonl"
    routed = subprocess.run(
        [sys.executable, str(ROUTER), "--transcript", str(transcript),
         *[f"--bot={k}={v}" for k, v in seats.items()], "--", "--light", "--seed", str(seed), *GAME],
        cwd=PROJECT, capture_output=True, text=True, timeout=300)
    assert routed.returncode == 0, routed.stderr
    inside = run(SERVER, "--light", "--quiet", "--seed", seed, *GAME,
                 *[a for k, v in seats.items() for a in ("--seat", f"{k}={v}")])
    assert inside.returncode == 0

    remote = messages(transcript.read_text())
    engine = messages(inside.stdout)
    answers = {m["id"]: m["choice"] for m in remote if "type" not in m}
    announcements = [without_state(m) for m in remote if "type" in m]
    assert len(announcements) == len(engine)
    for asked, decided in zip(announcements, engine):
        if asked["type"] == "ask":
            assert decided["type"] == "decided"
            for key in ("id", "seat", "question", "context", "options"):
                assert asked[key] == decided[key]
            assert answers[asked["id"]] == decided["choice"]
        else:
            # the seat_setup tells who plays the seat
            if asked["type"] == "seat_setup":
                asked.pop("controller"), decided.pop("controller")
            assert asked == decided


# --- the messages --------------------------------------------------------------------

@pytest.fixture(scope="module")
def full_game():
    server = run(SERVER, "--quiet", "--seed", 7, *GAME,
                 "--seat", "0=smart", "--seat", "1=random", "--seat", "2=random")
    assert server.returncode == 0
    return messages(server.stdout)


def test_a_game_starts_and_ends(full_game):
    assert [m["type"] for m in full_game[:4]] == ["game_start", "seat_setup", "seat_setup", "seat_setup"]
    assert full_game[0]["protocol"] == 1 and full_game[0]["seed"] == 7
    assert full_game[-1]["type"] == "game_over" and full_game[-1]["finished"]
    assert all("state" in m for m in full_game)


def test_every_turn_goes_through_its_phases(full_game):
    order = ["schemes", "reveal", "spells", "resolution", "cleanup"]
    phases = []
    for m in full_game:
        if m["type"] == "turn_start":
            phases = []
        elif m["type"] == "phase":
            phases.append(m["name"])
            assert m["state"]["phase"] == m["name"]
        elif m["type"] == "turn_end":
            expected = order if m["outcome"] == "resolved" else ["schemes", "reveal", "cleanup"]
            # a rollback can also happen during the spells (Precipitation)
            assert phases in (expected, ["schemes", "reveal", "spells", "cleanup"])


def test_decisions_have_increasing_ids_and_valid_choices(full_game):
    decided = [m for m in full_game if m["type"] == "decided"]
    assert [m["id"] for m in decided] == list(range(len(decided)))
    for m in decided:
        assert m["options"] and 0 <= m["choice"] < len(m["options"])


def test_the_state_follows_the_announcements(full_game):
    """A few facts Godot reads in the state instead of computing them."""
    for m in full_game:
        state = m["state"]
        if m["type"] == "event_revealed":
            assert state["present"][m["present"]] == m["event"]
            assert state["stealth"] == m["stealth"]
        elif m["type"] == "event_resolved":
            assert state["past"][-1]["id"] == m["event"]["id"]
            assert state["popularity"] == m["popularity"]
        elif m["type"] == "cards_drawn":
            hand = state["seats"][m["seat"]]["hand"]
            assert hand[len(hand) - len(m["cards"]):] == m["cards"]
        elif m["type"] == "election":
            assert [s["score"] for s in state["seats"]] == m["scores"]


# --- errors --------------------------------------------------------------------------

def test_two_invalid_answers_abort_the_game():
    server = run(SERVER, "--quiet", "--light", "--seed", 1,
                 stdin='not json\n{"seat": 0, "id": 0, "choice": 99}\n')
    assert server.returncode == 3
    sent = [without_state(m) for m in messages(server.stdout)]
    asks = [m for m in sent if m["type"] == "ask"]
    errors = [m for m in sent if m["type"] == "error"]
    assert len(asks) == 2 and asks[0] == asks[1]
    assert len(errors) == 2 and all(e["id"] == 0 and e["seat"] == 0 for e in errors)
    assert sent[-1]["type"] == "game_aborted" and sent[-1]["seat"] == 0


def test_an_invalid_answer_gets_the_question_again():
    server = run(SERVER, "--quiet", "--light", "--seed", 1,
                 stdin='{"seat": 1, "id": 0, "choice": 0}\n{"seat": 0, "id": 0, "choice": 0}\n')
    sent = [m for m in messages(server.stdout) if m["type"] in ("ask", "error", "game_aborted")]
    assert [m["type"] for m in sent] == ["ask", "error", "ask", "ask", "game_aborted"]
    assert sent[3]["id"] == 1
    assert server.returncode == 2      # then the input is closed
    assert sent[-1]["seat"] is None


@pytest.mark.parametrize("args, message", [
    (["--seat", "3=random"], "there are only 3 seats"),
    (["--seat", "0=foo"], "expects remote, random[:greed] or smart[:greed]"),
    (["--seat", "0=remote:4"], "expects no greed"),
    (["--players", "0"], "--players expects an integer"),
    (["--deck", "no_such_file.json"], "cannot open"),
    (["--foo"], "usage"),
])
def test_server_errors(args, message):
    server = run(SERVER, *args)
    assert server.returncode == 1
    assert message in server.stderr
    assert server.stdout == ""

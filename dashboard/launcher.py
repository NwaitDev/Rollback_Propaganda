"""Builds an experiment file from the dashboard's form and runs ./campaign on it.

Nothing here depends on Streamlit, so it can be tested alone.
"""

from __future__ import annotations

import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

NAME_PATTERN = re.compile(r"^[A-Za-z0-9_-]+$")


def available_files(project: Path, folder: str) -> list[str]:
    """decks/, events/ or contracts/ files, as written in an experiment."""
    return sorted(f"{folder}/{p.name}" for p in (project / folder).glob("*.json"))


def available_cards(project: Path) -> list[str]:
    """Card names known to ./campaign (empty if it is not built)."""
    binary = project / "campaign"
    if not binary.exists():
        return []
    result = subprocess.run([str(binary), "--cards"], cwd=project,
                            capture_output=True, text=True, timeout=30)
    return result.stdout.split() if result.returncode == 0 else []


@dataclass
class Settings:
    name: str
    seed: int
    random_only_games: int
    with_smart_games: int
    players: int
    random_greeds: list[int]
    smart_greed: int | None        # None: the random players' greed
    hand_size: int
    deck: str
    events: str
    contracts: str
    extra_card: str | None         # None: no extra card
    max_stealth: int = 21
    popularity: int = 0
    max_turns: int = 1000


def check(settings: Settings, project: Path, overwrite: bool) -> list[str]:
    """Problems that prevent the campaign from running (empty if none)."""
    problems = []
    if not NAME_PATTERN.match(settings.name):
        problems.append("Le nom ne peut contenir que des lettres sans accent, des chiffres, _ et -.")
    if settings.random_only_games + settings.with_smart_games <= 0:
        problems.append("La campagne doit jouer au moins une partie.")
    if not settings.random_greeds:
        problems.append("Choisissez au moins un niveau d'avidité.")
    if len(set(settings.random_greeds)) != len(settings.random_greeds):
        problems.append("Un niveau d'avidité est répété.")
    if not overwrite and (experiment_path(settings, project).exists()
                          or (project / "results" / settings.name).exists()):
        problems.append(f"Une campagne « {settings.name} » existe déjà : cochez la case pour la remplacer.")
    if not (project / "campaign").exists():
        problems.append("Le programme ./campaign n'est pas compilé : lancez make dans le projet.")
    return problems


def experiment_path(settings: Settings, project: Path) -> Path:
    return project / "experiments" / f"{settings.name}.json"


def experiment(settings: Settings) -> dict:
    """The experiment file's content (see experiments/ and campaign.hpp)."""
    content = {
        "name": settings.name,
        "seed": settings.seed,
        "games": {"random_only": settings.random_only_games,
                  "with_smart": settings.with_smart_games},
        "players": settings.players,
        "random_greed": settings.random_greeds if len(settings.random_greeds) > 1
                        else settings.random_greeds[0],
        "hand_size": settings.hand_size,
        "deck": settings.deck,
        "events": settings.events,
        "contracts": settings.contracts,
        "max_stealth": settings.max_stealth,
        "popularity": settings.popularity,
        "max_turns": settings.max_turns,
    }
    if settings.smart_greed is not None:
        content["smart_greed"] = settings.smart_greed
    if settings.extra_card:
        content["extra_card"] = settings.extra_card
    return content


def write_experiment(settings: Settings, project: Path) -> Path:
    path = experiment_path(settings, project)
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps(experiment(settings), indent=2, ensure_ascii=False) + "\n")
    return path


def run(experiment_file: Path, project: Path):
    """Runs the campaign; yields ("progress", percent), then ("line", text) for
    each line of its summary, and finally ("done", return code)."""
    process = subprocess.Popen([str(project / "campaign"), str(experiment_file.relative_to(project))],
                               cwd=project, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, bufsize=1)
    # the progress goes to stderr as "\r37 %"
    buffer = ""
    errors = []
    while True:
        char = process.stderr.read(1)
        if not char:
            break
        if char in "\r\n":
            text = buffer.strip()
            if text.endswith("%") and text[:-1].strip().isdigit():
                yield "progress", int(text[:-1])
            elif text:
                errors.append(text)
            buffer = ""
        else:
            buffer += char
    for line in process.stdout:
        yield "line", line.rstrip("\n")
    for line in errors:
        yield "line", line
    yield "done", process.wait()

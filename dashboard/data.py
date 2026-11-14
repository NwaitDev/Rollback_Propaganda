"""Data layer of the dashboard: reads the campaign results, pools them and
computes the metrics with their 95 % confidence intervals.

Every campaign writes results/<name>/campaigns.csv and details.csv (see
metrics/columns.csv). Rows with exactly the same parameters are pooled: their
games add up. Nothing here depends on Streamlit, so it can be tested alone.
"""

from __future__ import annotations

import itertools
import math
from dataclasses import dataclass, field
from pathlib import Path
from statistics import NormalDist

import pandas as pd

Z95 = NormalDist().inv_cdf(0.975)  # 1.96

NOT_APPLICABLE = "sans objet"   # smart_greed of the random-only games
SAME_GREED = "= avidité des random"   # smart_greed selection: follows random_greed
NO_EXTRA_CARD = "aucune"        # extra_card of the campaigns without one


# --- parameters --------------------------------------------------------------------

@dataclass(frozen=True)
class Parameter:
    column: str
    label: str
    numeric: bool


PARAMETERS = [
    Parameter("game_type", "Type de partie", False),
    Parameter("players", "Nombre de joueurs", True),
    Parameter("random_greed", "Avidité des random players", True),
    Parameter("smart_greed", "Avidité du smart player", True),
    Parameter("hand_size", "Taille de la main", True),
    Parameter("deck", "Deck", False),
    Parameter("events", "Cartes événements", False),
    Parameter("contracts", "Contrats", False),
    Parameter("extra_card", "Carte ajoutée à un joueur", False),
    Parameter("max_stealth", "Seuil de furtivité", True),
    Parameter("popularity", "Popularité de départ", True),
    Parameter("max_turns", "Limite de tours", True),
]
PARAMETER_COLUMNS = [p.column for p in PARAMETERS]

# the element a per-element metric (details.csv) is computed for
ITEM_PARAMETERS = {
    "seat": Parameter("item", "Place", True),
    "contract": Parameter("item", "Contrat", False),
    "card": Parameter("item", "Carte", False),
}


# --- metrics --------------------------------------------------------------------------

@dataclass(frozen=True)
class Metric:
    key: str
    code: str               # M1 ... M22
    label: str
    source: str             # "campaigns" or "details"
    kind: str               # "rate", "mean", "ratio" or "value"
    help: str
    item_kind: str | None = None        # details.csv: seat, contract or card
    only_game_type: str | None = None   # e.g. the smart player's win rate
    needs_extra_card: bool = False
    percent: bool = False   # shown as a percentage
    # rate: successes / trials; ratio: numerator / denominator
    successes: str | None = None
    trials: str | None = None
    # mean: column of the mean and of the standard deviation; n = number of
    # values behind the mean ("finished_games", "finished_players", or a column)
    mean: str | None = None
    sd: str | None = None
    n: str = "finished_games"
    # value: a column that cannot be pooled (a percentile)
    value: str | None = None
    exclude_items: tuple[str, ...] = ()
    # rate compared to the témoin: its successes column there (by default the
    # same as `successes`)
    reference_successes: str | None = None


METRICS = [
    Metric("seat_win_rate", "M1", "Victoires par place", "details", "rate",
           "Part des parties terminées gagnées par chaque place.",
           item_kind="seat", percent=True, successes="wins", trials="occurrences"),
    Metric("contract_win_rate", "M2", "Victoires par contrat", "details", "rate",
           "Part des parties où un joueur tenant ce contrat a gagné.",
           item_kind="contract", percent=True, successes="wins", trials="occurrences"),
    Metric("contract_points", "M2", "Points par contrat", "details", "mean",
           "Points moyens marqués par un joueur tenant ce contrat.",
           item_kind="contract", mean="points_mean", sd="points_sd", n="occurrences"),
    Metric("tiebreak_grimory_rate", "M4", "Parties départagées par le grimoire", "campaigns", "rate",
           "Part des parties terminées à égalité de points, départagées par la taille du grimoire.",
           percent=True, successes="tiebreak_grimory_games", trials="finished_games"),
    Metric("tiebreak_draw_rate", "M4", "Parties départagées par tirage", "campaigns", "rate",
           "Part des parties terminées à égalité de points et de grimoire, départagées par tirage.",
           percent=True, successes="tiebreak_draw_games", trials="finished_games"),
    Metric("smart_win_rate", "M5", "Victoires du smart player", "campaigns", "rate",
           "Part des parties terminées gagnées par le smart player. M6 : comparez-la au témoin.",
           only_game_type="with_smart", percent=True, successes="smart_wins", trials="finished_games"),
    Metric("holder_win_rate", "M8", "Victoires du détenteur de la carte ajoutée", "campaigns", "rate",
           "Part des parties terminées gagnées par le joueur qui a reçu la carte ajoutée. "
           "Comparez-la au témoin pour mesurer l'effet de la carte.",
           needs_extra_card=True, percent=True, successes="holder_wins", trials="finished_games",
           reference_successes="rotating_seat_wins"),
    Metric("card_reveal_rate", "M9", "Taux de révélation", "details", "rate",
           "Part des fois où la carte manigancée a été révélée plutôt que remise sous le grimoire.",
           item_kind="card", percent=True, successes="revealed", trials="schemed"),
    Metric("card_fail_rate", "M10", "Taux d'échec", "details", "rate",
           "Part des sorts révélés sans effet, faute de cible.",
           item_kind="card", percent=True, successes="failed", trials="revealed",
           exclude_items=("ETHER",)),
    Metric("card_dead_leading", "M13", "Temps mort en main, détenteur seul en tête", "details", "mean",
           "Tours passés en main sans être manigancée, par exemplaire et par partie, "
           "quand le détenteur est seul en tête des points.",
           item_kind="card", mean="dead_turns_leading_mean", sd="dead_turns_leading_sd"),
    Metric("card_dead_trailing", "M13", "Temps mort en main, détenteur pas seul en tête", "details", "mean",
           "Tours passés en main sans être manigancée, par exemplaire et par partie, "
           "quand le détenteur n'est pas seul en tête des points.",
           item_kind="card", mean="dead_turns_trailing_mean", sd="dead_turns_trailing_sd"),
    Metric("turns", "M14", "Durée de partie (tours)", "campaigns", "mean",
           "Nombre moyen de tours par partie terminée.",
           mean="turns_mean", sd="turns_sd"),
    Metric("resolved_turns", "M14", "Tours sans Rollback", "campaigns", "mean",
           "Nombre moyen de tours allés jusqu'à la Résolution, par partie terminée.",
           mean="resolved_turns_mean", sd="resolved_turns_sd"),
    Metric("elections_resolved", "M14", "Élections résolues", "campaigns", "mean",
           "Nombre moyen d'élections résolues par partie terminée.",
           mean="elections_resolved_mean", sd="elections_resolved_sd"),
    Metric("stealth_rollbacks_per_turn", "M15", "Rollbacks de furtivité par tour", "campaigns", "rate",
           "Part des tours finis par un dépassement de furtivité (les Explosions ne comptent pas).",
           percent=True, successes="stealth_rollbacks", trials="all_turns"),
    Metric("events_per_resolved_turn", "M16", "Événements par tour sans Rollback", "campaigns", "ratio",
           "Nombre moyen d'événements résolus par tour allé jusqu'à la Résolution "
           "(pas d'intervalle de confiance : la donnée est un rapport de totaux).",
           successes="events_resolved", trials="all_resolved_turns"),
    Metric("lead_changes", "M17", "Changements de faction en tête", "campaigns", "mean",
           "Nombre moyen de changements de faction en tête par partie ; "
           "le passage à une égalité en tête compte comme un changement.",
           mean="lead_changes_mean", sd="lead_changes_sd"),
    Metric("max_popularity", "M17", "Popularité maximale atteinte (moyenne)", "campaigns", "mean",
           "Moyenne, sur les parties, de la plus haute popularité atteinte par une faction.",
           mean="max_popularity_mean", sd="max_popularity_sd"),
    Metric("max_popularity_p10", "M17", "Popularité maximale atteinte (10e centile)", "campaigns", "value",
           "10e centile de la plus haute popularité atteinte par partie "
           "(non fusionnable : affiché seulement pour une campagne seule).",
           value="max_popularity_p10"),
    Metric("max_popularity_p50", "M17", "Popularité maximale atteinte (médiane)", "campaigns", "value",
           "Médiane de la plus haute popularité atteinte par partie "
           "(non fusionnable : affichée seulement pour une campagne seule).",
           value="max_popularity_p50"),
    Metric("max_popularity_p90", "M17", "Popularité maximale atteinte (90e centile)", "campaigns", "value",
           "90e centile de la plus haute popularité atteinte par partie "
           "(non fusionnable : affiché seulement pour une campagne seule).",
           value="max_popularity_p90"),
    Metric("decided_turn", "M18", "Tour où la partie est jouée", "campaigns", "mean",
           "Tour moyen à partir duquel le futur gagnant garde le plus de points (égalités comprises).",
           mean="decided_turn_mean", sd="decided_turn_sd"),
    Metric("decided_turn_ratio", "M18", "Moment où la partie est jouée (part de la durée)", "campaigns", "mean",
           "Tour décisif rapporté à la durée de la partie : 0 = joué d'avance, 1 = joué au dernier tour.",
           mean="decided_turn_ratio_mean", sd="decided_turn_ratio_sd"),
    Metric("final_margin", "M19", "Écart final entre le premier et le deuxième", "campaigns", "mean",
           "Écart moyen de points entre le premier et le deuxième en fin de partie.",
           mean="final_margin_mean", sd="final_margin_sd"),
    Metric("blocked_rate", "M20", "Parties bloquées", "campaigns", "rate",
           "Part des parties arrêtées à la limite de tours.",
           percent=True, successes="blocked_games", trials="games"),
    Metric("end_grimory", "M21", "Grimoire en fin de partie", "campaigns", "mean",
           "Cartes restantes dans le grimoire en fin de partie, par joueur.",
           mean="end_grimory_mean", sd="end_grimory_sd", n="finished_players"),
    Metric("end_hand", "M21", "Main en fin de partie", "campaigns", "mean",
           "Cartes restantes en main en fin de partie, par joueur.",
           mean="end_hand_mean", sd="end_hand_sd", n="finished_players"),
    Metric("end_cosmos", "M21", "Cosmos en fin de partie", "campaigns", "mean",
           "Cartes dans le cosmos en fin de partie.",
           mean="end_cosmos_mean", sd="end_cosmos_sd"),
    Metric("memory_lapses", "M22", "Trous de mémoire reçus", "campaigns", "mean",
           "Trous de mémoire reçus d'un adversaire, par partie.",
           mean="memory_lapses_received_mean", sd="memory_lapses_received_sd"),
    Metric("ethers_leaked", "M22", "Éthers perdus par Fuite d'éther", "campaigns", "mean",
           "Éthers perdus par Fuite d'éther, par partie.",
           mean="ethers_leaked_mean", sd="ethers_leaked_sd"),
    Metric("immunities", "M22", "Immunités déclenchées", "campaigns", "mean",
           "Choix adverses annulés par une Immunité, par partie.",
           mean="immunities_triggered_mean", sd="immunities_triggered_sd"),
]
METRICS_BY_KEY = {m.key: m for m in METRICS}


# --- loading --------------------------------------------------------------------------

@dataclass
class Results:
    campaigns: pd.DataFrame        # one row per campaign, greed level and game type
    details: pd.DataFrame          # same, per seat, contract and card (with the parameters)
    problems: list[str] = field(default_factory=list)

    @property
    def empty(self) -> bool:
        return self.campaigns.empty


def _name_of(path: str) -> str:
    """decks/empty.json -> empty"""
    return Path(str(path)).stem


def _rotating_seat_wins(path: Path, campaigns: pd.DataFrame) -> list:
    """Wins of the seat that changes at every game (index % players), by row of
    campaigns.csv: the player who would hold the extra card (M8)."""
    if not path.exists():
        return [None] * len(campaigns)
    games = pd.read_csv(path, usecols=["random_greed", "type", "index", "finished", "winner"])
    players = int(campaigns["players"].iloc[0])
    games = games[games["finished"] == 1]
    won = games[games["winner"] == games["index"] % players]
    counts = won.groupby(["random_greed", "type"]).size()
    return [int(counts.get((g, t), 0)) for g, t in zip(campaigns["random_greed"], campaigns["game_type"])]


def load_results(root: str | Path) -> Results:
    """Reads every <root>/<campaign>/campaigns.csv and details.csv."""
    root = Path(root)
    campaigns, details, problems = [], [], []
    for directory in sorted(p for p in root.glob("*") if p.is_dir()) if root.is_dir() else []:
        c_path, d_path = directory / "campaigns.csv", directory / "details.csv"
        if not c_path.exists():
            continue   # an older campaign, or one still running
        try:
            c = pd.read_csv(c_path, dtype={"extra_card": str, "smart_greed": "Int64"},
                            keep_default_na=False, na_values=[""])
            d = pd.read_csv(d_path, dtype={"item": str}, keep_default_na=False, na_values=[""]) \
                if d_path.exists() else pd.DataFrame()
        except (pd.errors.ParserError, pd.errors.EmptyDataError, ValueError) as error:
            problems.append(f"{directory.name} : illisible ({error})")
            continue
        if c.empty:
            continue
        c["source"] = directory.name
        c["rotating_seat_wins"] = _rotating_seat_wins(directory / "games.csv", c)
        if not d.empty:
            d["source"] = directory.name
        campaigns.append(c)
        details.append(d)
    if not campaigns:
        return Results(pd.DataFrame(), pd.DataFrame(), problems)

    c = pd.concat(campaigns, ignore_index=True)
    c["smart_greed"] = c["smart_greed"].astype(object).where(c["smart_greed"].notna(), NOT_APPLICABLE)
    c["extra_card"] = c["extra_card"].fillna(NO_EXTRA_CARD)
    for column in ("deck", "events", "contracts"):
        c[column] = c[column].map(_name_of)
    # totals needed to pool the ratios and the per-mage means
    c["all_turns"] = (c["turns_mean"] * c["finished_games"]).round()
    c["all_resolved_turns"] = (c["resolved_turns_mean"] * c["finished_games"]).round()
    c["stealth_rollbacks"] = (c["stealth_rollbacks_per_turn"] * c["all_turns"]).round()
    c["events_resolved"] = (c["events_per_resolved_turn"] * c["all_resolved_turns"]).round()
    c["finished_players"] = c["finished_games"] * c["players"]

    d = pd.concat([x for x in details if not x.empty], ignore_index=True) \
        if any(not x.empty for x in details) else pd.DataFrame()
    if not d.empty:
        keys = ["source", "campaign", "random_greed", "game_type"]
        d = d.merge(c[keys + [p for p in PARAMETER_COLUMNS if p not in keys] + ["finished_games"]],
                    on=keys, how="inner")
    return Results(c, d, problems)


# --- statistics ------------------------------------------------------------------------

@dataclass
class Estimate:
    value: float | None
    low: float | None = None
    high: float | None = None
    n: float = 0              # trials, values or games behind the estimate
    se: float | None = None   # standard error, for the comparisons
    note: str = ""


def wilson(successes: float, trials: float) -> Estimate:
    if trials <= 0:
        return Estimate(None, note="aucune observation")
    p = successes / trials
    denominator = 1 + Z95 ** 2 / trials
    centre = (p + Z95 ** 2 / (2 * trials)) / denominator
    half = Z95 * math.sqrt(p * (1 - p) / trials + Z95 ** 2 / (4 * trials ** 2)) / denominator
    return Estimate(p, max(0.0, centre - half), min(1.0, centre + half), trials,
                    math.sqrt(p * (1 - p) / trials))


def mean_interval(n: float, total: float, squares: float) -> Estimate:
    if n <= 0:
        return Estimate(None, note="aucune observation")
    mean = total / n
    if n < 2:
        return Estimate(mean, n=n, note="une seule valeur : pas d'intervalle")
    variance = max((squares - total * total / n) / (n - 1), 0.0)
    se = math.sqrt(variance / n)
    return Estimate(mean, mean - Z95 * se, mean + Z95 * se, n, se)


def _n_of(metric: Metric, row: pd.Series) -> float:
    return float(row[metric.n])


def sufficient_statistics(metric: Metric, rows: pd.DataFrame,
                          successes: str | None = None) -> dict | None:
    """Sums that pool exactly over rows with the same parameters."""
    successes = successes or metric.successes
    rows = rows.dropna(subset=[c for c in (successes, metric.trials, metric.mean,
                                           metric.value) if c])
    if rows.empty:
        return None
    if metric.kind in ("rate", "ratio"):
        return {"successes": float(rows[successes].sum()),
                "trials": float(rows[metric.trials].sum())}
    if metric.kind == "mean":
        n = total = squares = 0.0
        for _, row in rows.iterrows():
            k = _n_of(metric, row)
            m = float(row[metric.mean])
            s = 0.0 if pd.isna(row[metric.sd]) else float(row[metric.sd])
            n += k
            total += k * m
            squares += (k - 1) * s * s + k * m * m
        return {"n": n, "total": total, "squares": squares}
    # value: only for a single campaign
    if len(rows) > 1:
        return {"value": None, "games": float(rows["finished_games"].sum()),
                "note": "plusieurs campagnes : un centile ne se fusionne pas"}
    return {"value": float(rows[metric.value].iloc[0]),
            "games": float(rows["finished_games"].iloc[0])}


def estimate(metric: Metric, rows: pd.DataFrame, successes: str | None = None) -> Estimate:
    stats = sufficient_statistics(metric, rows, successes)
    if stats is None:
        return Estimate(None, note="pas de valeur (aucune partie terminée ou aucune observation)")
    if metric.kind == "rate":
        return wilson(stats["successes"], stats["trials"])
    if metric.kind == "ratio":
        if stats["trials"] <= 0:
            return Estimate(None, note="aucune observation")
        return Estimate(stats["successes"] / stats["trials"], n=stats["trials"],
                        note="pas d'intervalle pour un rapport de totaux")
    if metric.kind == "mean":
        return mean_interval(stats["n"], stats["total"], stats["squares"])
    return Estimate(stats["value"], n=stats["games"], note=stats.get("note", ""))


@dataclass
class Comparison:
    difference: float | None
    low: float | None = None
    high: float | None = None
    p_value: float | None = None
    note: str = ""

    @property
    def significant(self) -> bool | None:
        return None if self.p_value is None else self.p_value < 0.05


def compare(point: Estimate, reference: Estimate) -> Comparison:
    """point - reference, with a two-sided normal test when both have a standard error."""
    if point.value is None or reference.value is None:
        return Comparison(None, note="pas de valeur à comparer")
    difference = point.value - reference.value
    if point.se is None or reference.se is None:
        return Comparison(difference, note="pas de test possible pour cette métrique")
    se = math.sqrt(point.se ** 2 + reference.se ** 2)
    if se == 0:
        return Comparison(difference, difference, difference,
                          1.0 if difference == 0 else 0.0)
    z = difference / se
    p = 2 * (1 - NormalDist().cdf(abs(z)))
    return Comparison(difference, difference - Z95 * se, difference + Z95 * se, p)


# --- selection ---------------------------------------------------------------------------

def rows_for(results: Results, metric: Metric, as_reference: bool = False) -> pd.DataFrame:
    """The rows a metric can be computed on (the témoin has no extra card)."""
    if metric.source == "campaigns":
        rows = results.campaigns
    else:
        rows = results.details
        if rows.empty:
            return rows
        rows = rows[rows["kind"] == metric.item_kind]
        if metric.exclude_items:
            rows = rows[~rows["item"].isin(metric.exclude_items)]
    if rows.empty:
        return rows
    if metric.only_game_type:
        rows = rows[rows["game_type"] == metric.only_game_type]
    if metric.needs_extra_card and not as_reference:
        rows = rows[rows["extra_card"] != NO_EXTRA_CARD]
    return rows


def parameters_for(metric: Metric) -> list[Parameter]:
    parameters = list(PARAMETERS)
    if metric.item_kind:
        parameters.append(ITEM_PARAMETERS[metric.item_kind])
    return parameters


def _sort_key(value):
    try:
        return (0, float(value), "")
    except (TypeError, ValueError):
        return (1, 0.0, str(value))


def options(results: Results, metric: Metric) -> dict[str, list]:
    """The values each parameter takes in the rows of this metric."""
    rows = rows_for(results, metric)
    found = {}
    for p in parameters_for(metric):
        values = rows[p.column].dropna().unique().tolist() if not rows.empty else []
        if p.column == "smart_greed":
            numbers = sorted((v for v in values if v != NOT_APPLICABLE), key=_sort_key)
            values = [SAME_GREED] + numbers
        else:
            values = sorted(values, key=_sort_key)
        found[p.column] = values
    return found


def _smart_greed_applies(selection: dict[str, list]) -> bool:
    return "with_smart" in selection.get("game_type", [])


def free_parameters(metric: Metric, selection: dict[str, list]) -> list[Parameter]:
    """Parameters with several selected values."""
    free = []
    for p in parameters_for(metric):
        values = selection.get(p.column, [])
        if p.column == "smart_greed" and not _smart_greed_applies(selection):
            continue
        if len(values) > 1:
            free.append(p)
    return free


@dataclass
class View:
    metric: Metric
    free: list[Parameter]
    status: str               # "value", "2d", "3d", "too_many", "nothing", "incomplete"
    points: pd.DataFrame      # one row per studied combination
    missing: list[dict]       # combinations never studied
    reference: str | None = None
    message: str = ""


def _combinations(metric: Metric, selection: dict[str, list]):
    parameters = parameters_for(metric)
    columns = [p.column for p in parameters]
    lists = [selection.get(c, []) for c in columns]
    for values in itertools.product(*[l if l else [None] for l in lists]):
        combo = dict(zip(columns, values))
        if combo.get("game_type") == "random_only" and "smart_greed" in combo:
            combo["smart_greed"] = NOT_APPLICABLE
        elif combo.get("smart_greed") == SAME_GREED:
            combo["smart_greed"] = combo.get("random_greed")
        yield combo


def _match(rows: pd.DataFrame, combo: dict) -> pd.DataFrame:
    mask = pd.Series(True, index=rows.index)
    for column, value in combo.items():
        if value is None:
            return rows.iloc[0:0]
        mask &= rows[column].astype(str) == str(value)
    return rows[mask]


def evaluate(results: Results, metric: Metric, selection: dict[str, list],
             reference: str | None = None) -> View:
    """Computes the metric for every combination of the selected values."""
    free = free_parameters(metric, selection)
    missing_values = [p.label for p in parameters_for(metric)
                      if not selection.get(p.column)
                      and not (p.column == "smart_greed" and not _smart_greed_applies(selection))]
    if missing_values:
        return View(metric, free, "incomplete", pd.DataFrame(), [],
                    reference, "Choisissez au moins une valeur pour : " + ", ".join(missing_values))
    if len(free) > 2:
        return View(metric, free, "too_many", pd.DataFrame(), [], reference,
                    "Plus de deux paramètres ont plusieurs valeurs : ce cas n'est pas représenté. "
                    "Fixez-en au moins " + str(len(free) - 2) + ".")

    rows = rows_for(results, metric)
    reference_rows = reference_rows_for(results, metric, reference) if reference else None
    points, missing = [], []
    seen = set()
    for combo in _combinations(metric, selection):
        key = tuple(sorted(combo.items()))
        if key in seen:
            continue
        seen.add(key)
        matched = _match(rows, combo)
        shown = {p.column: combo[p.column] for p in free}
        if matched.empty:
            missing.append(combo)
            continue
        e = estimate(metric, matched)
        point = {**shown, "value": e.value, "low": e.low, "high": e.high, "n": e.n,
                 "campaigns": ", ".join(sorted(matched["campaign"].astype(str).unique())),
                 "note": e.note}
        if reference:
            point.update(_comparison_fields(metric, e, reference_rows, combo))
        points.append(point)

    frame = pd.DataFrame(points)
    if frame.empty:
        return View(metric, free, "nothing", frame, missing, reference,
                    "Ce cas de figure n'a jamais été étudié.")
    status = {0: "value", 1: "2d", 2: "3d"}[len(free)]
    return View(metric, free, status, frame, missing, reference)


# parameters a point is matched on with the témoin (the other ones may differ)
REFERENCE_MATCH = ("random_greed", "game_type", "smart_greed")


def reference_rows_for(results: Results, metric: Metric, reference: str) -> pd.DataFrame:
    """Rows of the témoin, pooled with the campaigns that have exactly the same
    parameters (the same rule as everywhere else)."""
    rows = rows_for(results, metric, as_reference=True)
    columns = [c for c in PARAMETER_COLUMNS if c not in REFERENCE_MATCH]
    chosen = results.campaigns[results.campaigns["campaign"] == reference]
    if chosen.empty or rows.empty:
        return rows.iloc[0:0]
    signatures = set(map(tuple, chosen[columns].astype(str).values))
    keep = rows[columns].astype(str).apply(tuple, axis=1).isin(signatures)
    return rows[keep]


def _comparison_fields(metric: Metric, point: Estimate, reference_rows: pd.DataFrame,
                       combo: dict) -> dict:
    match = {"random_greed": combo.get("random_greed"), "game_type": combo.get("game_type")}
    if metric.item_kind:
        match["item"] = combo.get("item")
    rows = _match(reference_rows, match)
    if rows.empty:
        return {"reference_value": None, "difference": None, "diff_low": None,
                "diff_high": None, "p_value": None, "significant": None,
                "comparison_note": "le témoin n'a pas ce niveau d'avidité et ce type de partie"}
    reference = estimate(metric, rows, metric.reference_successes)
    c = compare(point, reference)
    return {"reference_value": reference.value, "difference": c.difference,
            "diff_low": c.low, "diff_high": c.high, "p_value": c.p_value,
            "significant": c.significant, "comparison_note": c.note}


def describe(combo: dict, metric: Metric) -> str:
    """A never-studied combination, in words."""
    labels = {p.column: p.label for p in parameters_for(metric)}
    return ", ".join(f"{labels[c]} = {v}" for c, v in combo.items()
                     if v != NOT_APPLICABLE and c in labels)


def format_value(metric: Metric, value) -> str:
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return "—"
    if metric.percent:
        return f"{100 * value:.1f} %"
    return f"{value:.2f}"

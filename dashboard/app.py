"""Tableau de bord de Rollback Propaganda.

    streamlit run dashboard/app.py          (ou : make dashboard)

Explore the metrics of the campaigns in results/ and runs new campaigns.
The data logic lives in data.py and launcher.py.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import streamlit as st

sys.path.insert(0, str(Path(__file__).parent))
import data       # noqa: E402
import launcher   # noqa: E402

PROJECT = Path(__file__).resolve().parent.parent
DEFAULT_RESULTS = Path(os.environ.get("ROLLBACK_RESULTS", PROJECT / "results"))

# colours (dataviz reference palette): one hue per role, never cycled
VALUE_COLOR = "#2a78d6"        # the metric
REFERENCE_COLOR = "#eb6834"    # the témoin
MUTED_COLOR = "#898781"        # a difference that is not significant
SEQUENTIAL = ["#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf", "#184f95", "#0d366b"]
DIVERGING = [[0.0, "#2a78d6"], [0.5, "#f0efec"], [1.0, "#e34948"]]

st.set_page_config(page_title="Rollback Propaganda — tableau de bord", layout="wide")


# --- data ----------------------------------------------------------------------------------

def _signature(folder: Path) -> tuple:
    """Changes when a campaign is added or rewritten, to refresh the cache."""
    if not folder.is_dir():
        return ()
    return tuple(sorted((str(p), p.stat().st_mtime) for p in folder.glob("*/campaigns.csv")))


@st.cache_data(show_spinner="Lecture des résultats…")
def load(folder: str, signature: tuple) -> data.Results:
    return data.load_results(folder)


# --- figures -------------------------------------------------------------------------------

def _scale(metric: data.Metric, values):
    """Percentages are shown out of 100."""
    factor = 100 if metric.percent else 1
    return [None if v is None or pd.isna(v) else factor * v for v in values]


def _unit(metric: data.Metric, difference: bool = False) -> str:
    if metric.percent:
        return "écart (points de %)" if difference else "%"
    return "écart" if difference else ""


def _hover(metric: data.Metric, points: pd.DataFrame, difference: bool) -> list[str]:
    texts = []
    for _, p in points.iterrows():
        if difference:
            line = (f"écart au témoin : {_fmt(metric, p['difference'], diff=True)}"
                    f"<br>IC 95 % : [{_fmt(metric, p['diff_low'], diff=True)} ; "
                    f"{_fmt(metric, p['diff_high'], diff=True)}]"
                    f"<br>p = {_p(p['p_value'])} — {_significance(p)}")
        else:
            line = (f"{data.format_value(metric, p['value'])}"
                    f"<br>IC 95 % : [{data.format_value(metric, p['low'])} ; "
                    f"{data.format_value(metric, p['high'])}]")
        line += f"<br>n = {int(p['n'])}<br>campagnes : {p['campaigns']}"
        texts.append(line)
    return texts


def _fmt(metric: data.Metric, value, diff: bool = False) -> str:
    if value is None or pd.isna(value):
        return "—"
    if metric.percent:
        return f"{100 * value:+.1f} pts" if diff else f"{100 * value:.1f} %"
    return f"{value:+.2f}" if diff else f"{value:.2f}"


def _p(value) -> str:
    return "—" if value is None or pd.isna(value) else (f"{value:.3f}" if value >= 0.001 else "< 0,001")


def _significance(point) -> str:
    s = point.get("significant")
    if s is None or pd.isna(s):
        return point.get("comparison_note") or "pas de test"
    return "écart significatif" if s else "écart non significatif"


def _errors(metric, points, value, low, high):
    v = _scale(metric, points[value])
    lo = _scale(metric, points[low])
    hi = _scale(metric, points[high])
    plus = [None if a is None or b is None else b - a for a, b in zip(v, hi)]
    minus = [None if a is None or b is None else a - b for a, b in zip(v, lo)]
    return v, plus, minus


def figure_2d(metric: data.Metric, view: data.View, difference: bool) -> go.Figure:
    parameter = view.free[0]
    points = view.points.copy()
    if difference:
        points = points[points["difference"].notna()]
    points = points.sort_values(parameter.column, key=lambda s: s.map(data._sort_key))
    x = points[parameter.column].astype(str) if not parameter.numeric else points[parameter.column]
    figure = go.Figure()
    if difference:
        y, plus, minus = _errors(metric, points, "difference", "diff_low", "diff_high")
        significant = points["significant"].fillna(False).astype(bool).tolist()
        hover = _hover(metric, points, True)
        for flag, name, color, symbol in ((True, "écart significatif", VALUE_COLOR, "circle"),
                                          (False, "écart non significatif", MUTED_COLOR, "circle-open")):
            keep = [s == flag for s in significant]
            figure.add_trace(go.Scatter(
                x=[a for a, k in zip(x, keep) if k], y=[a for a, k in zip(y, keep) if k],
                mode="markers", name=name,
                marker=dict(size=11, color=color, symbol=symbol, line=dict(width=2, color=color)),
                error_y=dict(type="data", array=[a for a, k in zip(plus, keep) if k],
                             arrayminus=[a for a, k in zip(minus, keep) if k],
                             thickness=1.5, width=6, color=color),
                hovertext=[h for h, k in zip(hover, keep) if k], hoverinfo="text"))
        figure.add_hline(y=0, line=dict(width=1, color=MUTED_COLOR, dash="dash"))
        y_title = f"{metric.label} — {_unit(metric, True)}"
    else:
        y, plus, minus = _errors(metric, points, "value", "low", "high")
        hover = _hover(metric, points, False)
        error = dict(type="data", array=plus, arrayminus=minus, thickness=1.5, width=6,
                     color=VALUE_COLOR)
        if parameter.numeric:
            figure.add_trace(go.Scatter(x=x, y=y, mode="lines+markers", name=metric.label,
                                        line=dict(width=2, color=VALUE_COLOR),
                                        marker=dict(size=9, color=VALUE_COLOR),
                                        error_y=error, hovertext=hover, hoverinfo="text"))
        else:
            figure.add_trace(go.Bar(x=x, y=y, name=metric.label, marker=dict(color=VALUE_COLOR,
                                    cornerradius=4), error_y=error, hovertext=hover,
                                    hoverinfo="text"))
        if view.reference and "reference_value" in points:
            reference = _scale(metric, points["reference_value"])
            if any(r is not None for r in reference):
                figure.add_trace(go.Scatter(
                    x=x, y=reference, mode="markers", name=f"témoin ({view.reference})",
                    marker=dict(size=11, color=REFERENCE_COLOR, symbol="diamond",
                                line=dict(width=2, color="#fcfcfb")),
                    hovertemplate="témoin : %{y:.2f}<extra></extra>"))
        y_title = f"{metric.label}" + (f" ({_unit(metric)})" if _unit(metric) else "")
    figure.update_layout(
        xaxis_title=parameter.label, yaxis_title=y_title, height=460,
        margin=dict(l=10, r=10, t=30, b=10),
        showlegend=difference or bool(view.reference),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, x=0),
        bargap=0.45)
    if not parameter.numeric or len(points) < 2:
        figure.update_xaxes(type="category")
    return figure


def _axis(parameter: data.Parameter, values) -> tuple[list, dict]:
    """Positions on a 3D axis, and its ticks (categories become 0, 1, 2…)."""
    order = sorted(pd.unique(values), key=data._sort_key)
    if parameter.numeric:
        return [float(v) for v in values], dict(title=parameter.label)
    index = {v: i for i, v in enumerate(order)}
    return [index[v] for v in values], dict(title=parameter.label, tickvals=list(range(len(order))),
                                            ticktext=[str(v) for v in order])


def figure_3d(metric: data.Metric, view: data.View, difference: bool) -> go.Figure:
    a, b = view.free
    points = view.points.copy()
    column, low, high = ("difference", "diff_low", "diff_high") if difference else ("value", "low", "high")
    points = points[points[column].notna()]
    x, x_axis = _axis(a, points[a.column])
    y, y_axis = _axis(b, points[b.column])
    z, plus, minus = _errors(metric, points, column, low, high)
    hover = _hover(metric, points, difference)
    hover = [f"{a.label} : {va}<br>{b.label} : {vb}<br>{h}"
             for va, vb, h in zip(points[a.column], points[b.column], hover)]
    if difference:
        significant = points["significant"].fillna(False).astype(bool)
        colors = [VALUE_COLOR if s else MUTED_COLOR for s in significant]
        symbols = ["circle" if s else "circle-open" for s in significant]
    else:
        colors, symbols = VALUE_COLOR, "circle"
    figure = go.Figure(go.Scatter3d(
        x=x, y=y, z=z, mode="markers",
        marker=dict(size=6, color=colors, symbol=symbols, line=dict(width=2, color=colors)),
        error_z=dict(type="data", array=plus, arrayminus=minus, thickness=2, width=4,
                     color=MUTED_COLOR),
        hovertext=hover, hoverinfo="text", showlegend=False))
    z_title = metric.label + (f" ({_unit(metric, difference)})" if _unit(metric, difference) else "")
    figure.update_layout(height=560, margin=dict(l=0, r=0, t=10, b=0),
                         scene=dict(xaxis=x_axis, yaxis=y_axis, zaxis=dict(title=z_title)))
    return figure


def figure_top(metric: data.Metric, view: data.View, difference: bool) -> go.Figure:
    """The 3D view seen from above: a grid of cells, one per combination."""
    a, b = view.free
    points = view.points
    column = "difference" if difference else "value"
    xs = sorted(pd.unique(view.points[a.column]), key=data._sort_key)
    ys = sorted(pd.unique(view.points[b.column]), key=data._sort_key)
    grid = [[None] * len(xs) for _ in ys]
    text = [[""] * len(xs) for _ in ys]
    for _, p in points.iterrows():
        i, j = ys.index(p[b.column]), xs.index(p[a.column])
        value = p[column]
        if value is None or pd.isna(value):
            text[i][j] = "—"
            continue
        grid[i][j] = (100 if metric.percent else 1) * value
        label = _fmt(metric, value, diff=difference)
        if difference and p.get("significant") is not None and not pd.isna(p.get("significant")) \
                and not bool(p.get("significant")):
            label += " (n.s.)"
        text[i][j] = label
    numbers = [v for row in grid for v in row if v is not None]
    if difference:
        bound = max((abs(v) for v in numbers), default=1) or 1
        colors = dict(colorscale=DIVERGING, zmin=-bound, zmax=bound)
    else:
        colors = dict(colorscale=[[k / (len(SEQUENTIAL) - 1), c] for k, c in enumerate(SEQUENTIAL)])
    figure = go.Figure(go.Heatmap(
        x=[str(v) for v in xs], y=[str(v) for v in ys], z=grid, text=text,
        texttemplate="%{text}", hovertemplate=f"{a.label} : %{{x}}<br>{b.label} : %{{y}}<br>%{{text}}<extra></extra>",
        xgap=2, ygap=2, colorbar=dict(title=_unit(metric, difference) or ""), **colors))
    figure.update_layout(height=max(260, 70 * len(ys) + 120), margin=dict(l=10, r=10, t=10, b=10),
                         xaxis=dict(title=a.label, type="category"),
                         yaxis=dict(title=b.label, type="category"))
    return figure


# --- explorer page ---------------------------------------------------------------------------

def _default(values: list, rows: pd.DataFrame, column: str) -> list:
    """The most common value, so that the first view shows something."""
    if not values:
        return []
    if column == "smart_greed":
        return [data.SAME_GREED]
    counts = rows[column].astype(str).value_counts()
    for value in counts.index:
        for v in values:
            if str(v) == value:
                return [v]
    return values[:1]


def explorer(results: data.Results):
    left, right = st.columns([3, 2])
    with left:
        metric = st.selectbox(
            "Métrique", data.METRICS, format_func=lambda m: f"{m.code} · {m.label}",
            key="metric")
        st.caption(metric.help)
    with right:
        names = sorted(results.campaigns["campaign"].astype(str).unique())
        choices = ["(aucun)"] + names
        default = choices.index("control") if "control" in choices else 0
        reference = st.selectbox("Témoin", choices, index=default, key="reference",
                                 help="Chaque point est comparé à la ligne du témoin qui a le même "
                                      "niveau d'avidité et le même type de partie. Les campagnes "
                                      "qui ont exactement les mêmes paramètres que le témoin y sont "
                                      "fusionnées.")
        reference = None if reference == "(aucun)" else reference

    rows = data.rows_for(results, metric)
    if rows.empty:
        st.info("Aucune campagne ne contient de données pour cette métrique.")
        return
    options = data.options(results, metric)

    st.markdown("##### Paramètres")
    st.caption("Une valeur par paramètre donne un nombre ; plusieurs valeurs pour un seul "
               "paramètre donnent un graphique 2D ; pour deux paramètres, un graphique 3D.")
    selection = {}
    parameters = data.parameters_for(metric)
    several = [p for p in parameters if len(options[p.column]) > 1]
    single = [p for p in parameters if len(options[p.column]) <= 1]
    columns = st.columns(4)
    for i, p in enumerate(several):
        with columns[i % 4]:
            if p.column == "smart_greed" and "with_smart" not in st.session_state.get(
                    f"param_{metric.item_kind}_game_type", _default(options["game_type"], rows, "game_type")):
                st.multiselect(p.label, ["sans objet"], ["sans objet"], disabled=True,
                               key=f"param_{metric.item_kind}_smart_disabled",
                               help="Seules les parties avec un smart player ont cette avidité.")
                selection[p.column] = []
                continue
            key = f"param_{metric.item_kind}_{p.column}"
            # a value chosen for another metric may not exist for this one
            if key in st.session_state:
                stored = st.session_state[key]
                kept = [v for v in stored if v in options[p.column]]
                # an empty choice stays empty (the view asks for a value); a choice
                # whose values all vanished takes the default again
                st.session_state[key] = kept if kept or not stored else \
                    _default(options[p.column], rows, p.column)
            else:
                st.session_state[key] = _default(options[p.column], rows, p.column)
            selection[p.column] = st.multiselect(p.label, options[p.column], key=key,
                                                 format_func=lambda v: str(v))
    for p in single:
        selection[p.column] = options[p.column]
    if single:
        st.caption("Valeur unique dans les résultats : " + " · ".join(
            f"{p.label} = {options[p.column][0] if options[p.column] else '—'}" for p in single))

    view = data.evaluate(results, metric, selection, reference)
    st.divider()
    show(metric, view)


def show(metric: data.Metric, view: data.View):
    if view.status in ("incomplete", "too_many"):
        st.warning(view.message)
        return
    if view.status == "nothing":
        st.info("Ce cas de figure n'a jamais été étudié.")
        return

    difference = False
    if view.reference and view.status in ("2d", "3d"):
        difference = st.radio("Afficher", ["la valeur", "l'écart au témoin"], horizontal=True,
                              key="mode") == "l'écart au témoin"

    if view.status == "value":
        point = view.points.iloc[0]
        a, b, c = st.columns(3)
        a.metric(metric.label, data.format_value(metric, point["value"]))
        if point["low"] is not None and not pd.isna(point["low"]):
            b.metric("Intervalle de confiance à 95 %",
                     f"{data.format_value(metric, point['low'])} – {data.format_value(metric, point['high'])}")
        else:
            b.metric("Intervalle de confiance à 95 %", "—", help=point["note"] or None)
        c.metric("Observations", f"{int(point['n'])}", help="Parties, joueurs ou cartes derrière la valeur.")
        st.caption(f"Campagnes : {point['campaigns']}" + (f" — {point['note']}" if point["note"] else ""))
        if view.reference:
            comparison(metric, point, view.reference)
    elif view.status == "2d":
        st.plotly_chart(figure_2d(metric, view, difference), theme="streamlit")
    else:
        tab_3d, tab_top = st.tabs(["Vue 3D", "Vue de dessus"])
        with tab_3d:
            st.plotly_chart(figure_3d(metric, view, difference), theme="streamlit")
        with tab_top:
            st.plotly_chart(figure_top(metric, view, difference), theme="streamlit")

    if view.missing:
        with st.expander(f"{len(view.missing)} cas de figure jamais étudiés", expanded=view.status == "value"):
            for combo in view.missing:
                st.markdown(f"- {data.describe(combo, metric)}")

    if view.status in ("2d", "3d"):
        with st.expander("Tableau des valeurs"):
            table = table_of(metric, view)
            st.dataframe(table, hide_index=True)
            st.download_button("Télécharger (CSV)", table.to_csv(index=False).encode("utf-8"),
                               file_name=f"{metric.key}.csv", mime="text/csv")


def comparison(metric: data.Metric, point, reference: str):
    if point.get("difference") is None or pd.isna(point.get("difference")):
        st.info(f"Comparaison au témoin « {reference} » impossible : {point.get('comparison_note')}.")
        return
    a, b, c = st.columns(3)
    a.metric(f"Témoin ({reference})", data.format_value(metric, point["reference_value"]))
    b.metric("Écart au témoin", _fmt(metric, point["difference"], diff=True),
             help=f"IC 95 % : [{_fmt(metric, point['diff_low'], diff=True)} ; "
                  f"{_fmt(metric, point['diff_high'], diff=True)}]")
    c.metric("p-valeur", _p(point["p_value"]))
    significant = point.get("significant")
    if significant is None or pd.isna(significant):
        st.info(point.get("comparison_note") or "Pas de test possible pour cette métrique.")
    elif bool(significant):
        st.success("L'écart avec le témoin est significatif (p < 0,05).")
    else:
        st.warning("L'écart avec le témoin n'est pas significatif (p ≥ 0,05) : "
                   "il peut s'expliquer par le hasard.")


def table_of(metric: data.Metric, view: data.View) -> pd.DataFrame:
    columns = [p.column for p in view.free]
    table = view.points[columns + ["value", "low", "high", "n", "campaigns"]].copy()
    names = {p.column: p.label for p in view.free}
    names.update({"value": metric.label, "low": "IC 95 % bas", "high": "IC 95 % haut",
                  "n": "observations", "campaigns": "campagnes"})
    if view.reference:
        for c in ("reference_value", "difference", "diff_low", "diff_high", "p_value", "significant"):
            table[c] = view.points[c]
        names.update({"reference_value": "témoin", "difference": "écart", "diff_low": "écart IC bas",
                      "diff_high": "écart IC haut", "p_value": "p-valeur", "significant": "significatif"})
    if metric.percent:
        for c in ("value", "low", "high", "reference_value", "difference", "diff_low", "diff_high"):
            if c in table:
                table[c] = pd.to_numeric(table[c], errors="coerce") * 100
        names = {k: (v + " (%)" if k in ("value", "low", "high", "reference_value") else
                     v + " (pts)" if k in ("difference", "diff_low", "diff_high") else v)
                 for k, v in names.items()}
    return table.rename(columns=names)


# --- launch page ------------------------------------------------------------------------------

def launch_page(results_folder: Path):
    st.markdown("Remplissez les paramètres de la campagne, puis lancez-la. Elle écrit ses "
                "résultats dans `results/<nom>/` et son fichier d'expérience dans `experiments/`.")
    decks = launcher.available_files(PROJECT, "decks")
    events = launcher.available_files(PROJECT, "events")
    contracts = launcher.available_files(PROJECT, "contracts")
    cards = launcher.available_cards(PROJECT)
    if not cards:
        st.error("Le programme ./campaign n'est pas compilé : lancez `make` dans le projet.")
        return
    if results_folder.resolve() != (PROJECT / "results").resolve():
        st.info(f"Les campagnes lancées ici écrivent dans `{PROJECT / 'results'}`, "
                f"pas dans le dossier exploré (`{results_folder}`).")

    with st.form("campaign"):
        a, b, c = st.columns(3)
        name = a.text_input("Nom de la campagne", placeholder="ex. card_EUPHORIA")
        seed = b.number_input("Graine", min_value=0, max_value=4294967295, value=1, step=1)
        players = c.number_input("Nombre de joueurs", min_value=2, max_value=12, value=3, step=1)
        a, b, c = st.columns(3)
        random_only = a.number_input("Parties entre random players", min_value=0, value=5000, step=500)
        with_smart = b.number_input("Parties avec un smart player", min_value=0, value=5000, step=500)
        hand = c.number_input("Taille de la main", min_value=1, max_value=20, value=4, step=1)
        a, b, c = st.columns(3)
        greeds = a.multiselect("Avidités des random players", list(range(0, 31)), default=[8, 13, 17],
                               help="Les parties sont jouées pour chaque niveau, avec les mêmes graines.")
        same = b.checkbox("Smart player : même avidité que les random", value=True)
        smart = c.number_input("Avidité du smart player (si différente)", min_value=0, value=13, step=1)
        a, b, c = st.columns(3)
        deck = a.selectbox("Deck distribué à tous", decks,
                           index=decks.index("decks/empty.json") if "decks/empty.json" in decks else 0)
        event = b.selectbox("Cartes événements", events,
                            index=events.index("events/control.json") if "events/control.json" in events else 0)
        contract = c.selectbox("Contrats", contracts,
                               index=contracts.index("contracts/two_per_faction.json")
                               if "contracts/two_per_faction.json" in contracts else 0)
        a, b, c, d = st.columns(4)
        extra = a.selectbox("Carte ajoutée à un joueur", ["(aucune)"] + cards,
                            help="Seule différence entre les grimoires (M8). Elle va au smart player "
                                 "dans les parties qui en ont un.")
        max_stealth = b.number_input("Seuil de furtivité", min_value=0, value=21, step=1)
        popularity = c.number_input("Popularité de départ", value=0, step=1)
        max_turns = d.number_input("Limite de tours", min_value=1, value=1000, step=100)
        overwrite = st.checkbox("Remplacer une campagne existante du même nom")
        submitted = st.form_submit_button("Lancer la campagne", type="primary")

    if not submitted:
        return
    settings = launcher.Settings(
        name=name.strip(), seed=int(seed), random_only_games=int(random_only),
        with_smart_games=int(with_smart), players=int(players), random_greeds=sorted(greeds),
        smart_greed=None if same else int(smart), hand_size=int(hand), deck=deck, events=event,
        contracts=contract, extra_card=None if extra == "(aucune)" else extra,
        max_stealth=int(max_stealth), popularity=int(popularity), max_turns=int(max_turns))
    problems = launcher.check(settings, PROJECT, overwrite)
    if problems:
        for problem in problems:
            st.error(problem)
        return
    path = launcher.write_experiment(settings, PROJECT)
    total = (settings.random_only_games + settings.with_smart_games) * len(settings.random_greeds)
    st.write(f"Campagne « {settings.name} » : {total} parties ({path.relative_to(PROJECT)}).")
    bar = st.progress(0, text="Partie en cours…")
    lines = []
    code = None
    for kind, value in launcher.run(path, PROJECT):
        if kind == "progress":
            bar.progress(min(value, 100), text=f"{value} %")
        elif kind == "line":
            lines.append(value)
        else:
            code = value
    bar.progress(100, text="Terminé")
    st.code("\n".join(lines) or "(aucune sortie)")
    if code == 0:
        st.success("Campagne terminée. Ses résultats apparaissent dans « Explorer les résultats ».")
        load.clear()
    else:
        st.error(f"La campagne a échoué (code {code}).")


# --- page --------------------------------------------------------------------------------------

def main():
    st.title("Rollback Propaganda — tableau de bord")
    with st.sidebar:
        page = st.radio("Page", ["Explorer les résultats", "Lancer une campagne"])
        folder = Path(st.text_input("Dossier des résultats", str(DEFAULT_RESULTS)))
        if st.button("Recharger les résultats"):
            load.clear()
    if page == "Lancer une campagne":
        launch_page(folder)
        return
    results = load(str(folder), _signature(folder))
    for problem in results.problems:
        st.warning(problem)
    if results.empty:
        st.info(f"Aucun résultat dans `{folder}`. Lancez une campagne (page « Lancer une campagne », "
                "ou `make experiment`), puis rechargez.")
        return
    with st.sidebar:
        st.caption(f"{results.campaigns['source'].nunique()} campagnes chargées.")
    explorer(results)


main()

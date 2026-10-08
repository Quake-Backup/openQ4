"""The Remastered scoreboard (guis/scoreboard.gui; ui-visual-design.md 14.14).

Held on the scores button, instant and non-interactive. The stock board's
construction stays: the soft black backing, the server header in #FFFF8D,
your team's panel first, then the other team's and the spectators', each
opening with a header band in the team color carrying the faction mark, the
title and the team score, column headings at 0.40 over a white 0.10 rule, rows
on server-list bands whose pitch adapts to the entries, and the footer with the
game type, map, limits and timer. Remastered adds the rank line under the
header, the ◥ marker on your row, a DEATHS column after KILLS, ping as the
network-quality bars beside the number, dead players at 0.50 behind a skull and
a BOT tag for openQ4 bots. Beyond its height the rows keep the smallest pitch
and scroll by whole rows, each panel heading the rows it shows, over a count of
the rows below.

The game publishes the rows as fields (idMultiplayerGame::PublishRemasteredScoreboard):
oq4_sb{p}_kind/_score/_count per panel (0 your team or the players, 1 the other
team, 2 the spectators) and oq4_sb{p}_{r}_name/_clan/_score/_kills/_deaths/
_time/_ping/_flags per row, oq4_sb_pitch, oq4_sb_kills (the KILLS column shows),
oq4_sb_rank and, while the board scrolls, the rows below as oq4_sb_below and
its text oq4_sb_below_text. The panels it publishes are the window the scroll
shows. The header and footer read the stock keys (servername, serverip,
servergametype, servermap, serverlimit, servertimelimit, timeleft, infinity).
"""
from __future__ import annotations

import build_retained_screens as ui
from remastered_hud_base import (AND, NOT, OR, S, U, HudDocument, anchor, box, face_metrics, layout_state, num, op, pts_path,
                                 rgb, rounded_rect, sel, svg_path, text_box, u, vec, when)
from remastered_hud_mp import colour_of
from remastered_hud_status import full

NOTE, BAND, MARKER = "#FFFF8D", "#9A9C6E", "#E38900"
TEAM_COLOURS = {0: "#8B964B", 1: "#6AA42B", 2: "#FF7B04", 3: "#999999"}   # players, marine, strogg, spectators
TITLES = {0: "#str_200038", 1: "#str_200917", 2: "#str_200199", 3: "#str_200281"}
MARKS = {0: "M12 2a10 10 0 1 0 .01 0zM12 7a5 5 0 1 1-.01 0z", 1: "M12 1L22 7V17L12 23L2 17V7ZM12 6L7 16H17Z",
         2: "M2 6L10 10L12 2L14 10L22 6L16 14L18 22L12 17L6 22L8 14Z", 3: "M12 2a10 10 0 1 0 .01 0zM12 7a5 5 0 1 1-.01 0z"}
# Row slots per panel (your team or the players, the other team, the
# spectators): the most a panel shows at once, a panel alone at the smallest
# team pitch between the panels' top and the count of the rows below. The game
# publishes the window of rows the scroll shows (PublishRemasteredScoreboard).
ROWS = (20, 20, 16)
# The panels' viewport (u): from their top to the last row's bottom, with the
# count of the rows below under it while the board scrolls; Tourney's starts
# under the bracket tree. The game lays its window out on the same numbers.
VIEW_TOP, VIEW_BOTTOM, MORE_TOP = 74, 404, 396
TOURNEY_VIEW_TOP, TOURNEY_VIEW_BOTTOM, TOURNEY_MORE_TOP = 206, 468, 460
COLUMNS = {"name": 157, "clan": 300, "score": 372, "kills": 408, "deaths": 444, "time": 480}
PING_RIGHT, BARS_X = 537, 502
# Flags bits (PublishRemasteredScoreboard).
ME, DEAD, BOT, GREYED, READY_SHOWN, READY, MUTED, CARRIER = 1, 2, 4, 8, 16, 32, 64, 128
BAND_X, BAND_W = 93, 455
ROW_TEXT = 15.84              # Lowpixel 0.22
LINE_DP = 20.0


def bit(value, flag):
    return op("==", op("%", op("floor", op("/", value, flag)), 2), 1)


def shadow_text(ident: str, key: str, *, size: float, colour: list, x: float | None = None, right: float | None = None,
                baseline: float, width: float = 200, face: str = "lowpixel", extra: dict | None = None) -> list:
    """Text with the stock 1 u drop shadow, as every scoreboard line has."""
    kw = dict(face=face, size=size, baseline=u(baseline), width=u(width), extra=extra)
    if right is not None:
        kw["right"] = u(right)
    else:
        kw["x"] = u(x)
    shadow = text_box(ident + "-sh", key, colour=[0, 0, 0, colour[3]], **{**kw, "baseline": kw["baseline"] + u(1),
                                                                         **({"right": kw["right"] + u(1)} if right is not None else {"x": kw["x"] + u(1)})})
    return [shadow, text_box(ident, key, colour=colour, **kw)]


def soft_backing() -> dict:
    """The stock soft backing: black 0.60 over the 496x390 u board with a
    9 u Gaussian edge, as nested rounded rectangles whose composited alpha
    follows the blur's profile."""
    layers = [(-16, 0.0359), (-12, 0.0457), (-8, 0.0778), (-4, 0.1128), (0, 0.1402), (4, 0.1479), (8, 0.1297), (12, 0.0917),
              (16, 0.0644)]
    paths = []
    for index, (inset, alpha) in enumerate(layers):
        x, y, w, h = 72 + inset, 46 + inset, 496 - 2 * inset, 390 - 2 * inset
        r = max(1.0, 8 - inset * 0.5)
        paths.append(pts_path(f"l{index}", [(u(px), u(py)) for px, py in rounded_rect(x, y, w, h, r, steps=6)],
                              fill=ui.solid([0, 0, 0, alpha])))
    return vec("sb-back", paths)


def cell(ident: str, left: float, width: float, *, right: bool = False, colour=None) -> dict:
    """A row column: Lowpixel 0.22 with the drop shadow, vertically centred
    in the band."""
    colour = colour or [1, 1, 1, 1]
    props = {**ui.typeface("lowpixel", ROW_TEXT, LINE_DP, colour), "position": ui.keyword("absolute"),
             "left": ui.length(u(left)), "width": ui.length(u(width)), "top": ui.length(50, "%"),
             "margin-top": ui.length(-LINE_DP / 2), "height": ui.length(LINE_DP), "white-space": ui.keyword("nowrap"),
             "overflow": ui.keyword("hidden"), "text-overflow": ui.keyword("ellipsis"), "display": ui.keyword("block")}
    if right:
        props["text-align"] = ui.keyword("right")
    shadow = dict(props, **{"left": ui.length(u(left + 1)), "margin-top": ui.length(-LINE_DP / 2 + u(1)),
                            "color": ui.colour([0, 0, 0, 1])})
    return ui.group(ident + "-c", {**ui.FULL, "display": ui.keyword("block")},
                    [ui.label(ident + "-sh", "#str_230007", shadow), ui.label(ident, "#str_230007", props)])


def ping_bars(doc: HudDocument, ident: str, ping) -> list:
    """The network-quality bars (13.13): four steps by ping, #B5C784 for three
    or four, #E3AD36 for two, #E46D56 for one, unlit bars at 0.20."""
    level = sel(op("<=", ping, 60), 4, sel(op("<=", ping, 120), 3, sel(op("<=", ping, 200), 2, 1)))
    nodes = []
    for lit in (1, 2, 3, 4):
        colour = "#B5C784" if lit >= 3 else "#E3AD36" if lit == 2 else "#E46D56"
        paths = []
        for b in range(4):
            x, height = BARS_X - BAND_X + b * 3.4, 3 + b * 2.2
            top, bottom = {"fraction": 1, "dp": -u(height)}, {"fraction": 1}
            paths.append({"id": f"b{b}", "fill": ui.solid(rgb(colour, 1 if b < lit else 0.2)), "commands": [
                {"id": "a", "op": "move", "points": [[u(x), top]]}, {"id": "b", "op": "line", "points": [[u(x + 2.2), top]]},
                {"id": "c", "op": "line", "points": [[u(x + 2.2), bottom]]}, {"id": "d", "op": "line", "points": [[u(x), bottom]]},
                {"id": "e", "op": "close"}]})
        # The bars stand on the text's baseline, 3 u below the band's centre.
        node = vec(f"{ident}-bars{lit}", paths, {**ui.absolute(left=0, top=0, width=u(BAND_W)), "height": ui.length(50, "%"),
                                                  "margin-top": ui.length(u(3)), "display": ui.keyword("none"),
                                                  "overflow": ui.keyword("visible")})
        doc.show(node["id"], op("==", level, lit))
        nodes.append(node)
    return nodes


def panel(doc: HudDocument, p: int) -> dict:
    """One panel: the header band 3-24.5 u below its top with the faction mark,
    title and team score, the headings with their baseline on the first band's
    top edge over the rule, then the rows; the next panel starts 5 u on."""
    kind, count = S(f"oq4_sb{p}_kind"), S(f"oq4_sb{p}_count")
    doc.number(f"oq4_sb{p}_kind", -1)
    doc.string(f"oq4_sb{p}_score")
    doc.number(f"oq4_sb{p}_count")
    bands, marks, titles = [], [], []
    for k, colour in TEAM_COLOURS.items():
        band = vec(f"sb{p}-band{k}", [pts_path("band", [(u(100 - BAND_X + 0), u(3)), (u(557 - BAND_X), u(3)), (u(557 - BAND_X), u(24.5)),
                                                         (0, u(24.5)), (0, u(10))], fill=ui.solid(rgb(colour)))],
                   {"display": ui.keyword("none")})
        doc.show(band["id"], op("==", kind, k))
        mark = vec(f"sb{p}-mark{k}", [svg_path("m", MARKS[k], matrix=(U * 0.667, 0, 0, U * 0.667, u(102 - BAND_X), u(5.8)),
                                               fill=ui.solid([1, 1, 1, 1]), rule="evenodd")], {"display": ui.keyword("none")})
        doc.show(mark["id"], op("==", kind, k))
        title = text_box(f"sb{p}-title{k}", TITLES[k], face="lowpixel", size=22.3, colour=[1, 1, 1, 1], baseline=u(19),
                         x=u(123 - BAND_X), width=u(300))
        title["properties"]["display"] = ui.keyword("none")
        doc.show(title["id"], op("==", kind, k))
        bands.append(band); marks.append(mark); titles.append(title)
    score = text_box(f"sb{p}-score", "#str_230007", face="lowpixel", size=22.3, colour=[1, 1, 1, 1], baseline=u(19),
                     right=u(539 - BAND_X), width=u(120))
    doc.bind(f"sb{p}-score.text", f"sb{p}-score", "text", S(f"oq4_sb{p}_score"))
    spectators = op("==", kind, 3)
    heads = []
    heading = dict(size=11.52, colour=[1, 1, 1, 0.4], baseline=33, width=100)
    for name, key, shown in (("name", "#str_200242", None), ("clan", "#str_200256", None),
                             ("score", "#str_200198", NOT(spectators)), ("kills", "#str_200201", AND(NOT(spectators), S("oq4_sb_kills"))),
                             ("deaths", "#str_200202", NOT(spectators)), ("time", "#str_200244", NOT(spectators))):
        nodes = shadow_text(f"sb{p}-h-{name}", key, x=COLUMNS[name] - BAND_X, **heading)
        group = full(f"sb{p}-h-{name}-g", nodes)
        if shown is not None:
            doc.show(group["id"], shown)
        heads.append(group)
    heads.append(full(f"sb{p}-h-ping-g", shadow_text(f"sb{p}-h-ping", "#str_200037", right=PING_RIGHT - BAND_X, **heading)))
    rule = vec(f"sb{p}-rule", [pts_path("rule", [(u(2), u(34.6)), (u(448), u(34.6)), (u(448), u(35.7)), (u(2), u(35.7))],
                                        fill=ui.solid([1, 1, 1, 0.1]))])
    header = box(f"sb{p}-header", 0, 0, u(BAND_W), u(32), [*bands, *marks, *titles, score, *heads, rule], overflow=ui.keyword("visible"))
    header["properties"]["position"] = ui.keyword("relative")
    rows = []
    for r in range(ROWS[p]):
        ident = f"sb{p}r{r}"
        field = lambda name: S(f"oq4_sb{p}_{r}_{name}")
        for name, initial in (("name", ""), ("clan", ""), ("score", 0), ("kills", 0), ("deaths", 0), ("time", 0), ("ping", 0), ("flags", 0)):
            if isinstance(initial, str):
                doc.string(f"oq4_sb{p}_{r}_{name}", initial)
            else:
                doc.number(f"oq4_sb{p}_{r}_{name}", initial)
        flags = field("flags")
        me, dead, bot, greyed = bit(flags, ME), bit(flags, DEAD), bit(flags, BOT), bit(flags, GREYED)
        band_me = vec(f"{ident}-band-me", [rect_band(0.29)], {"display": ui.keyword("none")})
        band = vec(f"{ident}-band", [rect_band(0.08)])
        doc.show(band_me["id"], me)
        doc.show(band["id"], NOT(me))
        marker = vec(f"{ident}-marker", [pts_path("m", [(u(85 - BAND_X), 0), (u(92 - BAND_X), 0), (u(92 - BAND_X), u(7))],
                                                  fill=ui.solid(rgb(MARKER)))],
                     {**ui.absolute(left=0, width=u(BAND_W), height=u(7)), "top": ui.length(50, "%"), "margin-top": ui.length(-u(4)),
                      "display": ui.keyword("none"), "overflow": ui.keyword("visible")})
        doc.show(marker["id"], me)
        # The speaker, struck through when muted; the skull while dead; the flag a carrier holds.
        speaker = vec(f"{ident}-speaker", [svg_path("s", "M0 2h2.6L6 -0.8V8.8L2.6 6H0Z", matrix=(U, 0, 0, U, u(109 - BAND_X), 0),
                                                    fill=ui.solid(rgb("#8E8D70")))],
                      {**ui.absolute(left=0, width=u(BAND_W), height=u(8)), "top": ui.length(50, "%"), "margin-top": ui.length(-u(4))})
        muted = vec(f"{ident}-muted", [svg_path("x", "M7.5 0L11.5 6M11.5 0L7.5 6", matrix=(U, 0, 0, U, u(109 - BAND_X), u(1)),
                                                stroke={"paint": ui.solid(rgb("#E46D56")), "widthDp": u(1), "minimumPixels": 1,
                                                        "join": "miter", "cap": "butt"})],
                    {**ui.absolute(left=0, width=u(BAND_W), height=u(8)), "top": ui.length(50, "%"), "margin-top": ui.length(-u(4)),
                     "display": ui.keyword("none")})
        doc.show(muted["id"], bit(flags, MUTED))
        skull = vec(f"{ident}-skull", [svg_path("k", "M4 0a4 4 0 0 1 4 4c0 1.4-.7 2-1.3 2.7v1.3H1.3V6.7C.7 6 0 5.4 0 4a4 4 0 0 1 4-4zM2.4 3.4h1.2v1.2H2.4zM4.4 3.4h1.2v1.2H4.4z",
                                                matrix=(U, 0, 0, U, u(124 - BAND_X), 0), fill=ui.solid([1, 1, 1, 1]), rule="evenodd")],
                    {**ui.absolute(left=0, width=u(BAND_W), height=u(9)), "top": ui.length(50, "%"), "margin-top": ui.length(-u(4.5)),
                     "display": ui.keyword("none")})
        doc.show(skull["id"], dead)
        ready = vec(f"{ident}-ready", [svg_path("r", "M0 3.5L2.5 6L7 0.5", matrix=(U, 0, 0, U, u(96 - BAND_X), 0),
                                                stroke={"paint": ui.solid(rgb("#B5C784")), "widthDp": u(1.4), "minimumPixels": 1,
                                                        "join": "round", "cap": "round"})],
                    {**ui.absolute(left=0, width=u(BAND_W), height=u(7)), "top": ui.length(50, "%"), "margin-top": ui.length(-u(3.5)),
                     "display": ui.keyword("none")})
        doc.show(ready["id"], AND(bit(flags, READY_SHOWN), bit(flags, READY)))
        name = cell(f"{ident}-name", COLUMNS["name"] - BAND_X, 140)
        doc.bind(f"{ident}-name.text", f"{ident}-name", "text", field("name"))
        doc.bind(f"{ident}-name-sh.text", f"{ident}-name-sh", "text", field("name"))
        clan = cell(f"{ident}-clan", COLUMNS["clan"] - BAND_X, 70)
        doc.bind(f"{ident}-clan.text", f"{ident}-clan", "text", field("clan"))
        doc.bind(f"{ident}-clan-sh.text", f"{ident}-clan-sh", "text", field("clan"))
        doc.show(clan["id"], NOT(bot))
        # openQ4 bots carry a BOT tag in the header-band construction.
        tag = ui.group(f"{ident}-bot", {**ui.absolute(left=u(COLUMNS["clan"] - BAND_X), width=u(22), height=u(11)),
                                        "top": ui.length(50, "%"), "margin-top": ui.length(-u(5.5)), "display": ui.keyword("none")}, [
            vec(f"{ident}-bot-plate", [{"id": "t", "fill": ui.solid(rgb("#8B964B", 0.6)), "commands": [
                {"id": "a", "op": "move", "points": [[0, 0]]}, {"id": "b", "op": "line", "points": [[{"fraction": 1}, 0]]},
                {"id": "c", "op": "line", "points": [[{"fraction": 1}, {"fraction": 1}]]},
                {"id": "d", "op": "line", "points": [[u(4), {"fraction": 1}]]},
                {"id": "e", "op": "line", "points": [[0, {"fraction": 1, "dp": -u(4)}]]}, {"id": "f", "op": "close"}]}]),
            ui.label(f"{ident}-bot-text", "#str_231231", {**ui.FULL, **ui.typeface("lowpixel", 10.8, u(11), [1, 1, 1, 1]),
                     "text-align": ui.keyword("center"), "white-space": ui.keyword("nowrap"), "display": ui.keyword("block")})])
        doc.show(tag["id"], bot)
        numbers = []
        for col, shown in (("score", NOT(spectators)), ("kills", AND(NOT(spectators), S("oq4_sb_kills"))),
                           ("deaths", NOT(spectators)), ("time", NOT(spectators))):
            node = cell(f"{ident}-{col}", COLUMNS[col] - BAND_X, 34)
            doc.bind(f"{ident}-{col}.text", f"{ident}-{col}", "text", op("numberText", field(col)))
            doc.bind(f"{ident}-{col}-sh.text", f"{ident}-{col}-sh", "text", op("numberText", field(col)))
            doc.show(node["id"], shown)
            numbers.append(node)
        ping = cell(f"{ident}-ping", PING_RIGHT - BAND_X - 40, 40, right=True)
        doc.bind(f"{ident}-ping.text", f"{ident}-ping", "text", op("numberText", field("ping")))
        doc.bind(f"{ident}-ping-sh.text", f"{ident}-ping-sh", "text", op("numberText", field("ping")))
        bars = full(f"{ident}-bars", ping_bars(doc, ident, field("ping")))
        doc.show(bars["id"], NOT(bot))
        content = full(f"{ident}-content", [speaker, muted, skull, ready, name, clan, tag, *numbers, ping, bars], opacity=num(1))
        doc.bind(f"{ident}-content.dim", f"{ident}-content", "opacity", sel(OR(dead, greyed), 0.5, 1))
        lane = ui.group(f"{ident}-lane", {**ui.absolute(left=0, top=u(3), width=u(BAND_W)), "bottom": ui.length(0),
                                          "display": ui.keyword("block")}, [band, band_me, marker, content])
        row = ui.group(ident, {"position": ui.keyword("relative"), "display": ui.keyword("none"), "width": ui.length(u(BAND_W)),
                               "height": ui.length(u(30))}, [lane])
        doc.bind(f"{ident}.display", ident, "display", sel(op(">", count, r), "block", "none"))
        doc.bind(f"{ident}.height", ident, "height", op("*", S("oq4_sb_pitch"), U))
        rows.append(row)
    group = ui.group(f"sb{p}", {"position": ui.keyword("relative"), "display": ui.keyword("none"), "margin-left": ui.length(u(BAND_X)),
                                "width": ui.length(u(BAND_W)), "padding-bottom": ui.length(u(5))}, [header, *rows])
    doc.show(group["id"], op(">=", kind, 0))
    return group


def rect_band(alpha: float) -> dict:
    def p(fx, fy):
        return [{"fraction": fx}, {"fraction": fy}]
    return {"id": "band", "fill": ui.solid(rgb(BAND, alpha)), "commands": [
        {"id": "a", "op": "move", "points": [p(0, 0)]}, {"id": "b", "op": "line", "points": [p(1, 0)]},
        {"id": "c", "op": "line", "points": [p(1, 1)]}, {"id": "d", "op": "line", "points": [p(0, 1)]}, {"id": "e", "op": "close"}]}


# ------------------------------------------------------------------ Tourney

# The stock bracket tree (scoreboard.gui p_tourney): (round, bracket) -> rect.
BRACKETS = {(1, 1): (5, 20), (1, 2): (5, 56), (1, 3): (5, 104), (1, 4): (5, 140),
            (1, 5): (554, 20), (1, 6): (554, 56), (1, 7): (554, 104), (1, 8): (554, 140),
            (2, 1): (92, 38), (2, 2): (92, 126), (2, 3): (447, 38), (2, 4): (447, 126),
            (3, 1): (148, 82), (3, 2): (391, 82), (4, 1): (256, 67)}
# Connectors (tourn_bar1-3, closed lines at 0.40): rect and mirroring.
CONNECTORS = [((74, 27, 58, 16), 1, False, False), ((74, 65, 58, 16), 1, False, True), ((74, 115, 58, 16), 1, False, False),
              ((74, 153, 58, 16), 1, False, True), ((158, 68, 16, 16), 2, False, False), ((158, 112, 16, 16), 2, False, False),
              ((247, 90, 16, 16), 3, False, False), ((377, 90, 16, 16), 3, True, False), ((467, 68, 16, 16), 2, False, False),
              ((467, 112, 16, 16), 2, False, False), ((508, 27, 58, 16), 1, True, False), ((508, 65, 58, 16), 1, True, True),
              ((508, 115, 58, 16), 1, True, False), ((508, 153, 58, 16), 1, True, True)]
# Each connector's stroke in its texture (64x16 or 16x16 texels): rectangles.
CONNECTOR_STROKES = {1: (64, [(12, 4, 52, 6), (50, 4, 52, 12)]), 2: (16, [(7, 1, 9, 15)]), 3: (16, [(1, 7, 10, 9)])}
ROUND_TITLES = [((5, 170, 79, 14), "#str_200286", "right", 0.16), ((554, 170, 79, 14), "#str_200286", "left", 0.16),
                ((93, 156, 98, 14), "#str_200287", "right", 0.16), ((447, 156, 98, 14), "#str_200287", "left", 0.16),
                ((149, 111, 98, 14), "#str_200288", "right", 0.16), ((391, 111, 98, 14), "#str_200288", "left", 0.16),
                ((257, 129, 126, 19), "#str_200289", "center", 0.22)]
AQUA, FINISHED = "#5EB987", "#3E57B7"


def rect_points(x0: float, y0: float, x1: float, y1: float) -> list:
    return [(u(x0), u(y0)), (u(x1), u(y0)), (u(x1), u(y1)), (u(x0), u(y1))]


def stock_line(ident: str, key, rect: tuple, scale: float, colour, *, align: str = "left", shadow: bool = True) -> list:
    """A stock text window: Lowpixel at `scale`, its baseline 3/4 down the
    window, with the drop shadow when the stock asks for one."""
    x, y, w, h = rect
    size = scale * 72
    baseline = y + min(h, scale * 48) * 0.82 + max(0.0, (h - scale * 48) / 2)
    kw = dict(face="lowpixel", size=size, baseline=u(baseline), width=u(w))
    if align == "right":
        kw["right"] = u(x + w)
    elif align == "center":
        kw["center"] = u(x + w / 2)
    else:
        kw["x"] = u(x)
    static = isinstance(key, str)
    nodes = []
    if shadow:
        offset = {k: v + u(1) for k, v in kw.items() if k in ("x", "right", "center")}
        nodes.append(text_box(f"{ident}-sh", key if static else "#str_230007", colour=[0, 0, 0, 1],
                              **{**kw, **offset, "baseline": kw["baseline"] + u(1)}))
    nodes.append(text_box(ident, key if static else "#str_230007", colour=colour if isinstance(colour[0], (int, float)) else [1, 1, 1, 1], **kw))
    return nodes


def bracket(doc: HudDocument, r: int, b: int) -> dict:
    """One arena: a closed 1 u outline at 0.40, the finished blue fill, your
    arena's aqua (the whole box or your half), the two names in white and
    their scores in orange; once decided the winner's score turns white and
    the loser's name and score drop to 0.50; an empty arena reads EMPTY."""
    x, y = BRACKETS[(r, b)]
    finals = r == 4
    w, h = (128, 64) if finals else (80, 32)
    prefix = f"tb{r}{b}"
    for name in ("done", "empty"):
        doc.boolean(f"{prefix}_{name}")
    doc.number(f"{prefix}_winner")
    doc.number(f"{prefix}_focus")
    for part in ("name1", "score1", "name2", "score2"):
        doc.string(f"{part}_round{r}_bracket{b}")
    done, empty, winner, focus = (S(f"{prefix}_done"), S(f"{prefix}_empty"), S(f"{prefix}_winner"), S(f"{prefix}_focus"))
    inner = (x + 1, y + 1, x + w - 1, y + h - 1)
    fill = vec(f"{prefix}-done", [pts_path("f", rect_points(*inner), fill=ui.solid(rgb(FINISHED, 0.4)))], {"display": ui.keyword("none")})
    doc.show(fill["id"], done)
    whole = vec(f"{prefix}-focus", [pts_path("f", rect_points(*inner), fill=ui.solid(rgb(AQUA, 0.4)))], {"display": ui.keyword("none")})
    doc.show(whole["id"], op("==", focus, 3))
    half = (h - 2) / 2
    top = vec(f"{prefix}-sel1", [pts_path("f", rect_points(x + 2, y + 2, x + w - 2, y + 1 + half), fill=ui.solid(rgb(AQUA, 0.4)))],
              {"display": ui.keyword("none")})
    doc.show(top["id"], op("==", focus, 1))
    bottom = vec(f"{prefix}-sel2", [pts_path("f", rect_points(x + 2, y + 1 + half, x + w - 2, y + h - 2), fill=ui.solid(rgb(AQUA, 0.4)))],
                 {"display": ui.keyword("none")})
    doc.show(bottom["id"], op("==", focus, 2))
    outline = vec(f"{prefix}-box", [pts_path("o", rect_points(x + 1.5, y + 1.5, x + w - 0.5, y + h - 0.5), closed=True,
                                             stroke={"paint": ui.solid([1, 1, 1, 0.4]), "widthDp": u(1), "minimumPixels": 1,
                                                     "join": "miter", "cap": "butt"})])
    if finals:
        rows = {1: ((x + 5, y + 3, 103, 20), (x + 107, y + 3, 15, 18)), 2: ((x + 5, y + 41, 103, 20), (x + 107, y + 41, 15, 18))}
        scale, empty_rect = 0.28, (x, y + 23, 128, 19)
    else:
        rows = {1: ((x + 1, y, 66, 15), (x + 66, y, 11, 16)), 2: ((x + 1, y + 13, 66, 15), (x + 66, y + 13, 11, 16))}
        scale, empty_rect = 0.22, (x, y + 7, 80, 15)
    texts = []
    for player in (1, 2):
        other = 3 - player
        lost = AND(done, op("==", winner, other))
        won = AND(done, op("==", winner, player))
        name_rect, score_rect = rows[player]
        name = stock_line(f"{prefix}-n{player}", None, name_rect, scale, [1, 1, 1, 1])
        score = stock_line(f"{prefix}-s{player}", None, score_rect, scale, [1, 1, 1, 1], align="right")
        for node in name:
            doc.bind(node["id"] + ".text", node["id"], "text", S(f"name{player}_round{r}_bracket{b}"))
        for node in score:
            doc.bind(node["id"] + ".text", node["id"], "text", S(f"score{player}_round{r}_bracket{b}"))
        doc.bind(name[-1]["id"] + ".tint", name[-1]["id"], "color", colour_of([(lost, [1, 1, 1, 0.5])], [1, 1, 1, 1]))
        doc.bind(score[-1]["id"] + ".tint", score[-1]["id"], "color",
                 colour_of([(won, [1, 1, 1, 1]), (lost, [1, 1, 1, 0.5])], rgb("#FF8000")))
        texts += name + score
    words = full(f"{prefix}-words", texts)
    doc.show(words["id"], NOT(empty))
    blank = full(f"{prefix}-empty", stock_line(f"{prefix}-e", "#str_200952", empty_rect, 0.22, [1, 1, 1, 0.4], align="center"),
                 display=ui.keyword("none"))
    doc.show(blank["id"], empty)
    return full(prefix, [fill, whole, top, bottom, outline, words, blank])


def tourney_board(doc: HudDocument) -> dict:
    """Tourney (14.14): the bracket tree over the players and spectators
    lists on a flat black 0.60 fill, the round titles, the server, map and
    frag limit in the tree's centre and the tournament count, answering the
    stock arenaInit, arenaDone and arenaFocus events."""
    doc.number("round")
    doc.number("bracket")
    doc.number("winner")
    doc.number("sel")
    doc.boolean("empty")
    doc.string("tourney_frag_count")
    doc.string("tourney_count")
    nodes = [vec("sbt-fill", [rect_band_fill()])]
    nodes += stock_line("sbt-title", "#str_200285", (0, 8, 640, 21), 0.31, [1, 1, 1, 1], align="center", shadow=False)
    for index, (rect, kind, mirror_x, mirror_y) in enumerate(CONNECTORS):
        x, y, w, h = rect
        size, strokes = CONNECTOR_STROKES[kind]
        paths = []
        for s, (x0, y0, x1, y1) in enumerate(strokes):
            sx = w / size
            px0, px1 = (x + w - x1 * sx, x + w - x0 * sx) if mirror_x else (x + x0 * sx, x + x1 * sx)
            py0, py1 = (y + h - y1, y + h - y0) if mirror_y else (y + y0, y + y1)
            paths.append(pts_path(f"s{s}", rect_points(px0, py0, px1, py1), fill=ui.solid([1, 1, 1, 0.4])))
        nodes.append(vec(f"sbt-bar{index}", paths))
    for index, (rect, key, align, scale) in enumerate(ROUND_TITLES):
        nodes += stock_line(f"sbt-round{index}", key, rect, scale, [1, 1, 1, 0.5], align=align)
    for ident, key, rect in (("sbt-server", "servername", (192, 29, 256, 21)), ("sbt-address", "serverip", (192, 43, 256, 21)),
                             ("sbt-map", "servermap", (192, 147, 256, 21)), ("sbt-frags", "tourney_frag_count", (192, 161, 256, 21))):
        line = stock_line(ident, None, rect, 0.22, rgb(NOTE), align="center", shadow=False)
        for node in line:
            doc.bind(node["id"] + ".text", node["id"], "text", S(key))
        nodes += line
    count = stock_line("sbt-count", None, (310, 189, 323, 21), 0.22, [1, 1, 1, 1], align="right", shadow=False)
    for node in count:
        doc.bind(node["id"] + ".text", node["id"], "text", S("tourney_count"))
    nodes += count
    nodes += [bracket(doc, r, b) for (r, b) in BRACKETS]
    # The stock events address one arena by round and bracket.
    def at(r, b):
        return AND(op("==", S("round"), r), op("==", S("bracket"), b))
    init, finish, clear, focus = [], [], [], []
    for (r, b) in BRACKETS:
        p = f"tb{r}{b}"
        init.append(when(at(r, b), [{"op": "setState", "values": {f"{p}_done": False, f"{p}_winner": 0, f"{p}_focus": 0,
                                                                     f"{p}_empty": S("empty")}}]))
        finish.append(when(at(r, b), [{"op": "setState", "values": {f"{p}_done": True, f"{p}_winner": S("winner"), f"{p}_focus": 0}}]))
        clear.append(when(NOT(S(f"{p}_done")), [{"op": "setState", "values": {f"{p}_focus": 0}}]))
        focus.append(when(AND(at(r, b), NOT(S(f"{p}_done"))), [{"op": "setState", "values": {
            f"{p}_focus": sel(op("==", S("sel"), 1), 1, sel(op("==", S("sel"), 2), 2, 3))}}]))
    doc.on("arenaInit", *init)
    doc.on("arenaDone", *finish)
    doc.on("arenaFocus", *clear, *focus)
    group = full("sbt", nodes, display=ui.keyword("none"))
    doc.show("sbt", op("==", S("gametype"), 2))
    return group


def rect_band_fill() -> dict:
    """The Tourney board's flat black 0.60 over the whole view."""
    def p(fx, fy, dx, dy):
        return [{"fraction": fx, "dp": dx}, {"fraction": fy, "dp": dy}]
    big = u(400)
    return {"id": "fill", "fill": ui.solid([0, 0, 0, 0.6]), "commands": [
        {"id": "a", "op": "move", "points": [p(0, 0, -big, -big)]}, {"id": "b", "op": "line", "points": [p(1, 0, big, -big)]},
        {"id": "c", "op": "line", "points": [p(1, 1, big, big)]}, {"id": "d", "op": "line", "points": [p(0, 1, -big, big)]},
        {"id": "e", "op": "close"}]}


def more_rows(doc: HudDocument, tourney) -> dict:
    """While the board scrolls (14.14), under the last row it shows: a
    downward triangle and the count of the rows below, centred, in the
    headings' Lowpixel at white 0.40 with their drop shadow."""
    doc.number("oq4_sb_below")
    doc.string("oq4_sb_below_text")
    colour = [1, 1, 1, 0.4]
    triangle = vec("sb-more-arrow", [pts_path("a", [(0, u(1)), (u(6), u(1)), (u(3), u(4.5))], fill=ui.solid(colour))],
                   {"position": ui.keyword("relative"), "display": ui.keyword("block"), "width": ui.length(u(6)),
                    "height": ui.length(u(5.5)), "margin-right": ui.length(u(4))})
    face = ui.typeface("lowpixel", 11.52, u(10), colour)
    shadow = ui.label("sb-more-sh", "#str_230007", {**face, **ui.absolute(left=u(1), top=u(1)), "color": ui.colour([0, 0, 0, 0.4]),
                                                     "white-space": ui.keyword("nowrap"), "display": ui.keyword("block")})
    text = ui.label("sb-more-text", "#str_230007", {**face, "position": ui.keyword("relative"), "white-space": ui.keyword("nowrap"),
                                                     "display": ui.keyword("block")})
    for node in (shadow, text):
        doc.bind(node["id"] + ".text", node["id"], "text", S("oq4_sb_below_text"))
    words = ui.group("sb-more-words", {"position": ui.keyword("relative"), "display": ui.keyword("block")}, [shadow, text])
    more = ui.group("sb-more", {**ui.absolute(left=0, width=u(640), height=u(10)), "top": ui.length(u(MORE_TOP)),
                                "display": ui.keyword("none"), "flex-direction": ui.keyword("row"),
                                "justify-content": ui.keyword("center"), "align-items": ui.keyword("center")},
                    [triangle, words])
    doc.bind("sb-more.top", "sb-more", "top", sel(tourney, u(TOURNEY_MORE_TOP), u(MORE_TOP)))
    doc.bind("sb-more.display", "sb-more", "display", sel(op(">", S("oq4_sb_below"), 0), "flex", "none"))
    return more


def scoreboard_document() -> dict:
    doc = HudDocument("openq4.hud.remastered.scoreboard")
    layout_state(doc)
    doc.number("oq4_sb_pitch", 30)
    doc.boolean("oq4_sb_kills")
    doc.string("oq4_sb_rank")
    for key in ("servername", "serverip", "servergametype", "servermap", "serverlimit", "servertimelimit", "timeleft"):
        doc.string(key)
    doc.boolean("infinity")
    header = (shadow_text("sb-server", "#str_230007", size=15.84, colour=rgb(NOTE), x=98, baseline=56, width=300,
                          extra={"letter-spacing": ui.length(-0.45)}) +
              shadow_text("sb-address", "#str_230007", size=15.84, colour=rgb(NOTE), right=539, baseline=56, width=160))
    for node in header:
        source = "servername" if node["id"].startswith("sb-server") else "serverip"
        doc.bind(node["id"] + ".text", node["id"], "text", S(source))
    rank = shadow_text("sb-rank", "#str_230007", size=15.0, colour=[1, 1, 1, 1], x=170, baseline=70, width=300)
    for node in rank:
        node["properties"]["text-align"] = ui.keyword("center")
        doc.bind(node["id"] + ".text", node["id"], "text", S("oq4_sb_rank"))
    rank_group = full("sb-rank-g", rank)
    doc.show("sb-rank-g", op("!=", S("oq4_sb_rank"), ""))
    panels = ui.group("sb-panels", {**ui.absolute(left=0, top=u(VIEW_TOP), width=u(640)), "display": ui.keyword("block")},
                      [panel(doc, 0), panel(doc, 1), panel(doc, 2)])
    footer_rule = vec("sb-foot-rule", [pts_path("rule", [(u(95), u(408)), (u(541), u(408)), (u(541), u(409.3)), (u(95), u(409.3))],
                                                fill=ui.solid([1, 1, 1, 0.1]))])
    footer = [footer_rule]
    for ident, key, kw in (("sb-gametype", "servergametype", dict(x=94, baseline=421, size=15.84, extra={"letter-spacing": ui.length(-0.45)})),
                           ("sb-map", "servermap", dict(x=94, baseline=433, size=13.8)),
                           ("sb-limit", "serverlimit", dict(right=539, baseline=421, size=15.84)),
                           ("sb-timelimit", "servertimelimit", dict(right=539, baseline=433, size=15.84))):
        nodes = shadow_text(ident, "#str_230007", colour=rgb(NOTE), width=200, **kw)
        for node in nodes:
            doc.bind(node["id"] + ".text", node["id"], "text", S(key))
        footer += nodes
    clock = vec("sb-clock", [svg_path("c", "M294 418a7 7 0 1 0 .01 0z", fill={"type": "none"},
                                      stroke={"paint": ui.solid(rgb(NOTE)), "widthDp": u(1.5), "minimumPixels": 1, "join": "round", "cap": "butt"}),
                             svg_path("h", "M294 425l3 3M291 416h6", stroke={"paint": ui.solid(rgb(NOTE)), "widthDp": u(1.3), "minimumPixels": 1,
                                                                              "join": "miter", "cap": "butt"})])
    time = shadow_text("sb-time", "#str_230007", size=17.3, colour=[1, 1, 1, 1], x=305, baseline=429, width=80)
    for node in time:
        doc.bind(node["id"] + ".text", node["id"], "text", S("timeleft"))
    timer = full("sb-timer", [clock, *time])
    doc.show("sb-timer", NOT(S("infinity")))
    # Tourney trades the soft board, header, rank line and footer for its
    # bracket tree and lists the players below it.
    doc.number("gametype")
    tourney = op("==", S("gametype"), 2)
    doc.bind("sb-panels.top", "sb-panels", "top", sel(tourney, u(TOURNEY_VIEW_TOP), u(VIEW_TOP)))
    board = full("sb", [soft_backing(), *header, rank_group, *footer, timer])
    doc.show("sb", NOT(tourney))
    board = full("sb-all", [board, tourney_board(doc), panels, more_rows(doc, tourney)])
    doc.on("hudJoin", *doc.joins)
    doc.on("onInit", *doc.joins)
    root = full("scoreboard-root", [anchor(doc, "cc", [board], scale="hud.centerScale")])
    return doc.build(root, {"canvas": {"height": 720, "minWidth": 960},
                            "extensions": {"openq4": {"reference": "ui-visual-design.md 14.14", "stock": "guis/scoreboard.gui"}}})

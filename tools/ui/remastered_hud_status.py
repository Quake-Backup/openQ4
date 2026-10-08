"""The Remastered status bar: ammunition, health and armor (specification 14.3).

Marine construction from the atlas's marineGauge and Appendix B.5; the Strogg
construction from findings section 3 (hud_strogg.gui). Values arrive in the
stock HUD's own state keys.
"""
from __future__ import annotations

import math

import build_retained_screens as ui
import hud_svg
from remastered_hud_base import (
    AND, NOT, OR, S, U, HudDocument, box, num, op, play, pts_path, rgb, sel, shear_group, stroke, svg_path, text_box, tr,
    transform_value, u, vec)

# Measured Marine gauge geometry in gauge-local u inside the 125x59 rect.
BACK = "M1 10.4Q1 9.2 2.2 9.2H122.8Q124 9.2 124 10.4V54.7Q124 56.2 122.5 56.2H91.7L84.9 49.3H2.2Q1 49.3 1 48.1Z"
FILL = "M5.4 14.8Q5.4 13.8 6.4 13.8H118.6Q119.6 13.8 119.6 14.8V34.6H105.4A16.6 16.6 0 0 0 93.6 44.7H6.4Q5.4 44.7 5.4 43.7Z"
DIVIDERS = (15.6, 27.3, 39.1, 50.8, 62.5, 74.2, 85.9, 97.7, 108.9)
NOTCH_CENTER = (109.0, 50.8)   # the notch arc's centre; radius 16.6
EKG_BASE = 34.57               # 58% of the gauge height (texel rows 74-75)

# The Strogg gauge, 129x67 u (hud_strogg.gui s_backbar/s_valbar).
ST_BACK = "M1 22.5L26.8 7.3H69L72 11H128V53.4H1Z"
ST_FILL = "M46.7 15.7H124V40.3H69.3L56.3 48.7H5.1V39.8L15.8 33L24.2 32.5L33.4 26.8L33.9 24.1Z"

MARINE = {"fill": "#B3D06E", "armor": "#B2CF6D", "low": "#FF4C00", "ink": "#DDEBC3", "icon": rgb("#A8A360", 0.6),
          "gun": rgb("#C7BD78", 0.4), "ekg": "#161B0E", "ekg_low": "#4C0D00"}
STROGG = {"fill": "#FF9000", "armor": "#FF9000", "low": "#FF4C00", "ink": "#FCFFC8", "icon": rgb("#FFCC00", 0.6),
          "gun": rgb("#C6BC77", 0.4), "ekg": "#1D1403", "ekg_low": "#4C0D00", "static": "#161616"}
# The multiplayer ammunition gauge's weapon tints (hud.gui:200-234), grey
# before the first change; Appendix B.4 records where they differ from hud.mtr.
MP_AMMO = ["#00D9FF", "#FFFF00", "#FF8C00", "#0073FF", "#338F12", "#99CCCC", "#FF4000", "#00FF00", "#FFFF99", "#C433FF",
           "#FFBF40"]
MP_TEAM = {"marine": "#B2CF6D", "strogg": "#FF8000"}
MP_ROSTER = ["item_gauntlet", "item_machinegun", "item_shotgun", "item_hyperblaster", "item_grenade", "item_nailgun",
             "item_rocket", "item_railgun", "item_lightning", "item_darkmatter", "item_fire"]
# The weapon render's texel origin in its right-aligned 256x64 texture
# (x = origin x 106/256 in the 106x26 u current_gun window; atlas GUN_X0).
GUN_X0 = {"blaster": 141, "machinegun": 124, "shotgun": 29, "hyperblaster": 70, "grenadelauncher": 82, "nailgun": 128,
          "rocketlauncher": 92, "railgun": 93, "lightninggun": 82, "darkmatter": 95, "napalmgun": 125, "gauntlet": 113}
GUN_RENDERS = ["gun_blaster", "gun_machinegun", "gun_shotgun", "gun_hyperblaster", "gun_grenadelauncher", "gun_nailgun",
               "gun_rocketlauncher", "gun_railgun", "gun_lightninggun", "gun_darkmatter", "gun_napalmgun", "gun_gauntlet",
               "gun_corecannon", "gun_freezegun"]
# Weapon glyphs by their inv_icon base name (the peer forwards an icon
# material as its base name, without "_colored").
WEAPON_GLYPHS = ["item_blaster", "item_machinegun", "item_shotgun", "item_hyperblaster", "item_grenade", "item_nailgun",
                 "item_rocket", "item_railgun", "item_lightning", "item_darkmatter", "item_fire", "item_gauntlet",
                 "item_corecannon", "item_freeze"]


# ------------------------------------------------------------------ geometry

def arc_points(center, radius, a0, a1, steps=20):
    return [(center[0] + radius * math.cos(a0 + (a1 - a0) * i / steps),
             center[1] + radius * math.sin(a0 + (a1 - a0) * i / steps)) for i in range(steps + 1)]


def fill_outline(d: float = 0.0) -> list:
    """The Marine fill plate inset by d u: square corners once d passes the
    1 u radius, the ledge raised by d and the concave notch grown to 16.6+d."""
    left, top, right, bottom, ledge = 5.4 + d, 13.8 + d, 119.6 - d, 44.7 - d, 34.6 - d
    cx, cy = NOTCH_CENTER
    r = 16.6 + d
    x_ledge = cx - math.sqrt(max(0.0, r * r - (cy - ledge) ** 2))
    x_bottom = cx - math.sqrt(max(0.0, r * r - (cy - bottom) ** 2))
    a0 = math.atan2(ledge - cy, x_ledge - cx)
    a1 = math.atan2(bottom - cy, x_bottom - cx)
    return [(left, top), (right, top), (right, ledge)] + arc_points((cx, cy), r, a0, a1) + [(left, bottom)]


def polygon_offset(points: list, d: float) -> list:
    """A simple clockwise-on-screen polygon moved d inward (mitred joins);
    only for small d against the outline's features."""
    n = len(points)
    lines = []
    for i in range(n):
        (x1, y1), (x2, y2) = points[i], points[(i + 1) % n]
        dx, dy = x2 - x1, y2 - y1
        length = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / length, dx / length          # inward for clockwise-on-screen order
        lines.append(((x1 + nx * d, y1 + ny * d), (dx, dy)))
    result = []
    for i in range(n):
        (p, r), (q, s) = lines[i - 1], lines[i]
        den = r[0] * s[1] - r[1] * s[0]
        if abs(den) < 1e-9:
            result.append(q)
            continue
        t = ((q[0] - p[0]) * s[1] - (q[1] - p[1]) * s[0]) / den
        result.append((p[0] + t * r[0], p[1] + t * r[1]))
    return result


def clip_polygon(subject: list, clipper: list) -> list:
    """Sutherland-Hodgman: any simple subject inside a convex clipper given
    clockwise on screen."""
    def inside(p, a, b):
        return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= 0

    def cut(p1, p2, a, b):
        x1, y1 = p1; x2, y2 = p2; x3, y3 = a; x4, y4 = b
        den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
        if abs(den) < 1e-12:
            return p2
        t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den
        return (x1 + t * (x2 - x1), y1 + t * (y2 - y1))
    output = list(subject)
    for i in range(len(clipper)):
        a, b = clipper[i], clipper[(i + 1) % len(clipper)]
        source, output = output, []
        if not source:
            break
        prev = source[-1]
        for cur in source:
            if inside(cur, a, b):
                if not inside(prev, a, b):
                    output.append(cut(prev, cur, a, b))
                output.append(cur)
            elif inside(prev, a, b):
                output.append(cut(prev, cur, a, b))
            prev = cur
    return output


def scaled(points: list, s: float = U) -> list:
    return [(x * s, y * s) for x, y in points]


def ring_path(ident: str, outer_svg: str, inner_points: list, colour: list, blend=None) -> dict:
    """An even-odd ring between an exact outline and an inset polygon."""
    path = svg_path(ident, outer_svg, fill=ui.solid(colour), rule="evenodd", blend=blend)
    start = len(path["commands"])
    inner = scaled(inner_points)
    path["commands"].append({"id": f"c{start}", "op": "move", "points": [[round(inner[0][0], 4), round(inner[0][1], 4)]]})
    for i, (x, y) in enumerate(inner[1:], start=1):
        path["commands"].append({"id": f"c{start + i}", "op": "line", "points": [[round(x, 4), round(y, 4)]]})
    path["commands"].append({"id": f"c{start + len(inner)}", "op": "close"})
    return path


def svg_polygon(data: str) -> list:
    """Vertices of a straight-edged SVG outline (the Strogg plates)."""
    return [seg[1] for seg in hud_svg.parse_path(data) if seg[0] in ("M", "L")]


# ------------------------------------------------------------------ readouts

def digit_readout(doc: HudDocument, ident: str, value, *, face: str, size: float, right: float, baseline: float,
                  colour: list, digits: int = 3, pitch: float | None = None) -> dict:
    """Tabular figures (section 14.3): fixed cells from a fixed trailing
    edge, so the leading edge never moves as digits change. Leading zeros
    stay blank. `value` is an expression, clamped to 0..999."""
    pitch = pitch or round(0.62 * size, 3)
    value = op("clamp", op("floor", value), 0, 10 ** digits - 1)
    nodes = []
    for k in range(digits):
        cell_right = right - k * pitch
        node = text_box(f"{ident}-d{k}", "", face=face, size=size, colour=colour, baseline=baseline,
                        center=cell_right - pitch / 2, width=pitch * 1.8)
        del node["properties"]["color"]          # the readout's colour, for the gain pop
        node["properties"]["display"] = ui.keyword("block")
        nodes.append(node)
        doc.bind(f"{ident}-d{k}.text", node["id"], "text", op("numberText", op("%", op("floor", op("/", value, 10 ** k)), 10)))
        if k:
            doc.bind(f"{ident}-d{k}.show", node["id"], "display", sel(op(">=", value, 10 ** k), "block", "none"))
    return ui.group(ident, {**ui.FULL, "display": ui.keyword("block")}, nodes)


def gain_timeline(doc: HudDocument, ident: str, numerals: str, flash: str, pivot: tuple, size: tuple, ink: list) -> str:
    """The gain pop: the numerals rise to 120% in white and settle to 100%
    and the readout colour over 250 ms while the icon flashes white. Damage
    never animates a gauge. Reduced motion drops it (nonessential)."""
    px, py = pivot
    cx, cy = size[0] / 2, size[1] / 2

    def tf(s):
        return transform_value((px - cx) * (1 - s), (py - cy) * (1 - s), s, s)
    return doc.timeline(ident, 250, [
        tr(numerals, "transform", [(0, tf(1.2)), (250, tf(1.0))]),
        tr(numerals, "color", [(0, ui.colour([1, 1, 1, 1])), (250, ui.colour(ink))]),
        tr(flash, "opacity", [(0, num(1)), (250, num(0))]),
    ])


def glyph_choice(doc: HudDocument, ident: str, key: str, choices: list, x: float, y: float, h: float, *, tint,
                 opacity: float = 1.0, anchor: str = "start", extra=None) -> list:
    """One holder per possible glyph, shown while `key` names it."""
    nodes = []
    for name in choices:
        parts = hud_svg.icon_nodes(name, f"{ident}-{name}", x, y, h, tint=tint, opacity=opacity, anchor=anchor)
        holder = ui.group(f"{ident}-{name}-on", {**ui.FULL, "display": ui.keyword("none")}, parts)
        condition = op("==", S(key), name)
        doc.show(holder["id"], AND(condition, extra) if extra is not None else condition)
        nodes.append(holder)
    return nodes


def full(ident: str, children=(), mask: dict | None = None, **props) -> dict:
    """A group filling its parent; `mask` is a node mask (paths in dp)."""
    p = {**ui.FULL, "display": ui.keyword("block")}
    p.update(props)
    node = ui.group(ident, p, children)
    if mask is not None:
        node["mask"] = mask
    return node


# ------------------------------------------------------------------ gauges

def colour_variants(kind: str, strogg: bool, mp_variants: bool) -> list:
    """(name, colour, condition, track alpha, fill alpha) for each paint the
    gauge can take; paint is static, so each is its own node."""
    pal = STROGG if strogg else MARINE
    mp, low, team = S("mp"), S("oq4.low"), S("oq4.teamStrogg")
    base = pal["armor"] if kind == "armor" else pal["fill"]
    out = []
    if kind == "health":
        out += [("sp", base, AND(NOT(mp), NOT(low)), 0.2, 0.35), ("low", pal["low"], AND(NOT(mp), low), 0.35, 0.5)]
    else:
        out.append(("sp", base, NOT(mp), 0.2, 0.35))
    if mp_variants:
        if kind == "ammo":
            out.append(("mpgrey", "#808080", AND(mp, OR(op("<", S("player_weapon"), 0), op(">", S("player_weapon"), 10))), 0.2, 0.5))
            out += [(f"mp{i}", c, AND(mp, op("==", S("player_weapon"), i)), 0.2, 0.5) for i, c in enumerate(MP_AMMO)]
        else:
            out += [("mpm", MP_TEAM["marine"], AND(mp, NOT(team)), 0.2, 0.5), ("mps", MP_TEAM["strogg"], AND(mp, team), 0.2, 0.5)]
    return out


def fill_window(doc: HudDocument, prefix: str, width: float, height: float, pct_key: str, nodes: list,
                lead_at_full: float, span: float, trailing: float) -> dict:
    """The trailing-anchored fill: a window whose leading edge sits at
    max(lead, trailing - span * pct) while its content stays put."""
    pct = op("clamp", S(pct_key), 0, 1)
    lead = op("max", lead_at_full, op("-", trailing, op("*", span, pct)))
    # RmlUi clips only content that overflows toward the trailing or bottom
    # side (scroll size beyond the client box), so the content box reaches
    # past the window's trailing edge; the fill paths themselves are absolute.
    # Each vector inside keeps the gauge's own box.
    nodes = [dict(node, properties={**node["properties"], **ui.absolute(left=0, top=0, width=u(width), height=u(height))})
             for node in nodes]
    content = ui.group(prefix + "-content", {**ui.absolute(left=0, top=0, width=u(width) + 600, height=u(height)),
                                             "display": ui.keyword("block")}, nodes)
    window = ui.group(prefix + "-window", {**ui.absolute(left=0, top=0, width=u(width), height=u(height)),
                                           "display": ui.keyword("block"), "overflow": ui.keyword("hidden")}, [content])
    doc.bind(prefix + "-window.left", window["id"], "left", op("*", lead, U))
    doc.bind(prefix + "-window.width", window["id"], "width", op("*", op("-", width, lead), U))
    doc.bind(prefix + "-content.left", content["id"], "left", op("*", op("-", 0, lead), U))
    return window


def ekg_layer(doc: HudDocument, prefix: str, mask_svg: str, waves: list, low_colour: str, colour: str) -> dict:
    """The EKG (section 14.3): traces added through the fill's outline beneath
    the fill. `waves` lists (points per period, period u, duration ms). An
    instrument, so reduced motion keeps it (essential timelines)."""
    low = S("oq4.low")
    layers = []
    for variant, tint, shown in (("normal", colour, NOT(low)), ("low", low_colour, low)):
        for wave_index, (beat, period, duration) in enumerate(waves):
            paths = []
            for repeat in range(-1, 3):
                line = scaled([(x + repeat * period, y) for x, y in beat])
                for width, alpha in ((4.4, 0.1), (2.6, 0.3), (0.95, 1.0)):
                    paths.append(pts_path(f"r{repeat + 1}w{int(width * 100)}", line, closed=False,
                                          stroke=stroke(rgb(tint, alpha), u(width), join="round", cap="round"),
                                          blend="additive"))
            ident = f"{prefix}-ekg-{variant}-{wave_index}"
            mover = vec(ident, paths, {"transform": ui.transform()})
            run = doc.timeline(ident + "-run", duration,
                               [tr(ident, "transform", [(0, transform_value(u(-period))), (duration, transform_value(0))])],
                               iterations=0, essential=True)
            doc.joins.append(play(run))
            layers.append(mover)
        holder = full(f"{prefix}-ekg-{variant}", layers[-len(waves):])
        doc.show(holder["id"], shown)
        layers = layers[:-len(waves)] + [holder]
    return full(prefix + "-ekg", layers, mask={"paths": [svg_path("mask", mask_svg, fill=ui.solid([1, 1, 1, 1]))]})


def low_rim(doc: HudDocument, prefix: str, back_svg: str) -> dict:
    """Low health: an additive red glow along the inside of the backing,
    full at the edge and gone about 17 u in, its red pulsing between 100%
    and 50% at 2 Hz (backbar_add). Nested strokes clipped to the silhouette
    build the falloff."""
    paths = []
    steps = 8
    for i in range(steps):
        prev, reach = 17.0 * i / steps, 17.0 * (i + 1) / steps
        alpha = (1 - prev / 17.0) ** 2 - (1 - reach / 17.0) ** 2
        paths.append(svg_path(f"rim{i}", back_svg, stroke=stroke([1, 0, 0, round(alpha, 4)], u(reach * 2), join="round"),
                              blend="additive"))
    rim = vec(prefix + "-rim", paths, {"opacity": num(1)}, mask={"paths": [svg_path("mask", back_svg, fill=ui.solid([1, 1, 1, 1]))]})
    holder = full(prefix + "-rim-on", [rim])
    doc.show(holder["id"], S("oq4.low"))
    run = doc.timeline(prefix + "-rim-pulse", 500, [tr(rim["id"], "opacity", [(0, num(1)), (250, num(0.5)), (500, num(1))])],
                       iterations=0)
    doc.joins.append(play(run))
    return holder


def backing_nodes(doc: HudDocument, kind: str, outline: str) -> list:
    """The black backing at 0.50, or 0.85 with the opaque-backing option.
    Paint alpha, not node opacity, so no composition layer is needed."""
    normal = vec(kind + "-back", [svg_path("back", outline, fill=ui.solid([0, 0, 0, 0.5]))])
    opaque = vec(kind + "-back-opaque", [svg_path("back", outline, fill=ui.solid([0, 0, 0, 0.85]))], {"display": ui.keyword("none")})
    doc.show(normal["id"], NOT(S("oq4.opaque")))
    doc.show(opaque["id"], S("oq4.opaque"))
    return [normal, opaque]


def marine_gauge(doc: HudDocument, kind: str, gx: float, gy: float, *, mp_variants: bool) -> dict:
    """One Marine gauge at (gx, gy) u (atlas marineGauge; findings 1.2-1.4)."""
    w, h, k = 125, 59, -0.22
    mp = S("mp")
    plate = []
    plate += backing_nodes(doc, kind, BACK)

    tracks, fills = [], []
    bevel = fill_outline(3.0)
    outline = fill_outline(0.0)
    for name, colour, shown, track_alpha, fill_alpha in colour_variants(kind, False, mp_variants):
        t = vec(f"{kind}-track-{name}", [svg_path("track", FILL, fill=ui.solid(rgb(colour, track_alpha)))],
                {"display": ui.keyword("none")})
        doc.show(t["id"], shown)
        tracks.append(t)
        paths = [svg_path("fill", FILL, fill=ui.solid(rgb(colour, fill_alpha))),
                 ring_path("bevel", FILL, bevel, rgb(colour, 0.45))]
        if kind == "armor":
            for i, x in enumerate(DIVIDERS):
                piece = clip_polygon(outline, [(x - 0.75, 13.8), (x + 0.75, 13.8), (x + 0.75, 44.7), (x - 0.75, 44.7)])
                if len(piece) >= 3:
                    paths.append(pts_path(f"cell{i}", scaled(piece), fill=ui.solid([0, 0, 0, 0.28])))
        f = vec(f"{kind}-fill-{name}", paths, {"display": ui.keyword("none")})
        doc.show(f["id"], shown)
        fills.append(f)
    plate += tracks
    if kind == "health":
        b = EKG_BASE
        beat = [(0, b), (11.2, b), (14.9, b - 3.46), (18.6, b), (22.7, b), (23.7, b + 2.5), (25.8, b - 17.5), (27.7, b + 6.2),
                (29.2, b), (37.4, b), (43.5, b - 6.22), (44.7, b - 6.22), (49.6, b), (51.8, b), (55.2, b - 3), (58.6, b),
                (62.5, b)]
        plate.append(ekg_layer(doc, "health", FILL, [(beat, 62.5, 2500)], MARINE["ekg_low"], MARINE["ekg"]))
    pct = {"ammo": "player_ammopct", "health": "player_healthpct", "armor": "player_armorpct"}[kind]
    plate.append(fill_window(doc, kind + "-fill", w, h, pct, fills, 5.4, 115, 120))
    if kind == "health":
        plate.append(low_rim(doc, "health", BACK))

    ink = rgb(MARINE["ink"])
    if kind == "ammo":
        clipped = AND(op(">=", S("player_clip_size"), 2), op(">=", S("player_ammo"), 0), NOT(mp))
        infinite = op("<", S("player_totalammo"), 0)
        clip = digit_readout(doc, "ammo-clip", S("player_ammo"), face="chain", size=36, right=u(80), baseline=u(32.7), colour=ink)
        reserve = digit_readout(doc, "ammo-reserve", S("player_totalammo"), face="chain", size=23, right=u(118), baseline=u(31),
                                colour=ink)
        total = digit_readout(doc, "ammo-total", S("player_totalammo"), face="chain", size=36, right=u(118), baseline=u(32.7),
                              colour=ink)
        doc.show(clip["id"], AND(clipped, NOT(infinite)))
        doc.show(reserve["id"], AND(clipped, NOT(infinite)))
        doc.show(total["id"], AND(NOT(clipped), NOT(infinite)))
        readouts = [clip, reserve, total]
    else:
        key = "player_health" if kind == "health" else "player_armor"
        readouts = [digit_readout(doc, f"{kind}-num", S(key), face="chain", size=36, right=u(118), baseline=u(32.7), colour=ink)]
    numerals = full(kind + "-numerals", readouts, transform=ui.transform(), color=ui.colour(ink))

    children = [shear_group(kind + "-plate", 0, 0, u(w), u(h), k, plate)]
    if kind == "ammo":
        # The weapon's render behind the clip count, upright, single player.
        renders = []
        for name in GUN_RENDERS:
            key = name.replace("gun_", "")
            gw = hud_svg.symbol_width(name, 26)
            x0 = GUN_X0.get(key)
            left = (-2 + x0 * 106 / 256) if x0 is not None else 105 - gw
            holder = full(f"gun-{key}-on", hud_svg.icon_nodes(name, f"gun-{key}", u(left), u(15), u(26), tint=MARINE["gun"]),
                          display=ui.keyword("none"))
            doc.show(holder["id"], AND(NOT(mp), op("==", S("weapicon"), name)))
            renders.append(holder)
        children.append(full("ammo-renders", renders))
    children.append(shear_group(kind + "-digits", 0, 0, u(w), u(h), k, [numerals]))

    # The notch icon, upright, with a white copy for the gain flash.
    if kind == "ammo":
        sp_icons = full("ammo-icon-sp", glyph_choice(doc, "ammo-icon", "ammoIcon", WEAPON_GLYPHS, u(100), u(38), u(16),
                                                     tint=MARINE["icon"]))
        doc.show(sp_icons["id"], NOT(mp))
        mp_icons = []
        for index, glyph in enumerate(MP_ROSTER):
            holder = full(f"ammo-icon-mp{index}", hud_svg.icon_nodes(glyph, f"ammo-icon-mpg{index}", u(100), u(38), u(16),
                                                                     tint=rgb(MP_AMMO[index])), display=ui.keyword("none"))
            doc.show(holder["id"], AND(mp, op("==", S("player_weapon"), index)))
            mp_icons.append(holder)
        flash = full("ammo-flash", glyph_choice(doc, "ammo-flash", "ammoIcon", WEAPON_GLYPHS, u(100), u(38), u(16),
                                                tint=[1, 1, 1, 1]), opacity=num(0))
        children += [sp_icons, full("ammo-icon-mp", mp_icons), flash]
    else:
        glyph = "health" if kind == "health" else "armor"
        icon = full(kind + "-icon", hud_svg.icon_nodes(glyph, kind + "-icon-g", u(100), u(38), u(16), tint=MARINE["icon"]))
        doc.show(icon["id"], NOT(mp))
        # Multiplayer: health yellow and armor red at 0.80 (hud.gui:1001, 1087).
        mp_tint = rgb("#F7C004", 0.8) if kind == "health" else rgb("#FA2B05", 0.8)
        icon_mp = full(kind + "-icon-mp", hud_svg.icon_nodes(glyph, kind + "-icon-mpg", u(100), u(38), u(16), tint=mp_tint))
        doc.show(icon_mp["id"], mp)
        flash = full(kind + "-flash", hud_svg.icon_nodes(glyph, kind + "-flash-g", u(100), u(38), u(16), tint=[1, 1, 1, 1]),
                     opacity=num(0))
        children += [icon, icon_mp, flash]
    gain_timeline(doc, kind + "-gain", numerals["id"], flash["id"], (u(118), u(9 + 26 * 0.7)), (u(w), u(h)), ink)
    return box(kind + "-gauge", u(gx), u(gy), u(w), u(h), children)


def strogg_gauge(doc: HudDocument, kind: str, gx: float, gy: float) -> dict:
    """One Strogg gauge at (gx, gy) u: 129x67 u, sheared +0.22 about its own
    centre, upright R_Strogg numerals, the icon at the upper-leading shoulder,
    a two-wave EKG and masked static (findings section 3)."""
    w, h, k = 129, 67, 0.22
    plate = []
    plate += backing_nodes(doc, kind, ST_BACK)
    outline = svg_polygon(ST_FILL)
    # The Strogg fill reads as a flat plate with a thin rim (s_valbar).
    bevel = polygon_offset(outline, 1.6)
    tracks, fills = [], []
    for name, colour, shown, track_alpha, fill_alpha in colour_variants(kind, True, False):
        t = vec(f"{kind}-track-{name}", [svg_path("track", ST_FILL, fill=ui.solid(rgb(colour, track_alpha)))],
                {"display": ui.keyword("none")})
        doc.show(t["id"], shown)
        tracks.append(t)
        paths = [svg_path("fill", ST_FILL, fill=ui.solid(rgb(colour, fill_alpha))),
                 ring_path("bevel", ST_FILL, bevel, rgb(colour, 0.45))]
        if kind == "armor":
            # The same texel columns as the Marine cells: a 12.1 u pitch.
            for i, x in enumerate(d * 129 / 125 for d in DIVIDERS):
                piece = clip_polygon(list(reversed(outline)) if False else outline,
                                     [(x - 0.75, 15.7), (x + 0.75, 15.7), (x + 0.75, 48.7), (x - 0.75, 48.7)])
                if len(piece) >= 3:
                    paths.append(pts_path(f"cell{i}", scaled(piece), fill=ui.solid([0, 0, 0, 0.28])))
        f = vec(f"{kind}-fill-{name}", paths, {"display": ui.keyword("none")})
        doc.show(f["id"], shown)
        fills.append(f)
    plate += tracks
    if kind == "health":
        # s_ekg: a sine-like wave (64-texel period, 0.2 widths/s) and a slow
        # wave (128-texel period, 0.38 widths/s), #1D1403.
        def wave(period, amplitude, base, samples=32):
            return [(period * i / samples, base - amplitude * math.sin(2 * math.pi * i / samples)) for i in range(samples + 1)]
        p1, p2 = 64 * w / 256, 128 * w / 256
        plate.append(ekg_layer(doc, "health", ST_FILL,
                               [(wave(p1, 5.5, 33.0), p1, int(p1 / (0.2 * w) * 1000)),
                                (wave(p2, 3.5, 33.0), p2, int(p2 / (0.38 * w) * 1000))], STROGG["ekg_low"], STROGG["ekg"]))
    pct = {"ammo": "player_ammopct", "health": "player_healthpct", "armor": "player_armorpct"}[kind]
    plate.append(fill_window(doc, kind + "-fill", w, h, pct, fills, 5.1, 119, 124))
    # s_hudstatic: masked additive grain scrolling over each gauge.
    grain = []
    for i in range(70):
        gxp = 4 + (i * 37) % 120
        gyp = 12 + (i * 19) % 38
        grain.append(pts_path(f"g{i}", scaled([(gxp, gyp), (gxp + 0.7, gyp), (gxp + 0.7, gyp + 0.7), (gxp, gyp + 0.7)]),
                              fill=ui.solid(rgb("#5A5A5A", 0.5)), blend="additive"))
    grain_node = vec(kind + "-static", grain, {"transform": ui.transform()})
    run = doc.timeline(kind + "-static-run", 2000, [tr(grain_node["id"], "transform",
                                                       [(0, transform_value(0, 0)), (2000, transform_value(u(-w), u(2 * h)))])],
                       iterations=0)
    doc.joins.append(play(run))
    plate.append(full(kind + "-static-mask", [grain_node],
                      mask={"paths": [svg_path("mask", ST_FILL, fill=ui.solid([1, 1, 1, 1]))]}))
    if kind == "health":
        plate.append(low_rim(doc, "health", ST_BACK))

    ink = rgb(STROGG["ink"])
    mp = S("mp")
    if kind == "ammo":
        clipped = AND(op(">=", S("player_clip_size"), 2), op(">=", S("player_ammo"), 0), NOT(mp))
        infinite = op("<", S("player_totalammo"), 0)
        clip = digit_readout(doc, "ammo-clip", S("player_ammo"), face="r_strogg", size=36, right=u(119), baseline=u(34.3),
                             colour=ink)
        reserve = digit_readout(doc, "ammo-reserve", S("player_totalammo"), face="r_strogg", size=19, right=u(121),
                                baseline=u(51), colour=rgb(STROGG["ink"], 0.8))
        total = digit_readout(doc, "ammo-total", S("player_totalammo"), face="r_strogg", size=36, right=u(119),
                              baseline=u(33), colour=ink)
        doc.show(clip["id"], AND(clipped, NOT(infinite)))
        doc.show(reserve["id"], AND(clipped, NOT(infinite)))
        doc.show(total["id"], AND(NOT(clipped), NOT(infinite)))
        readouts = [clip, reserve, total]
    else:
        key = "player_health" if kind == "health" else "player_armor"
        readouts = [digit_readout(doc, f"{kind}-num", S(key), face="r_strogg", size=36, right=u(119), baseline=u(34.3), colour=ink)]
    numerals = full(kind + "-numerals", readouts, transform=ui.transform(), color=ui.colour(ink))
    children = [shear_group(kind + "-plate", 0, 0, u(w), u(h), k, plate)]
    if kind == "ammo":
        # The weapon's render behind the numerals: the stock 104x23 u
        # p_current_gun window at local (25, 16), #C6BC77 at 0.40, upright.
        renders = []
        for name in GUN_RENDERS:
            key = name.replace("gun_", "")
            gw = hud_svg.symbol_width(name, 23)
            x0 = GUN_X0.get(key)
            left = (25 + x0 * 104 / 256) if x0 is not None else 129 - gw
            holder = full(f"gun-{key}-on", hud_svg.icon_nodes(name, f"gun-{key}", u(left), u(16), u(23), tint=STROGG["gun"]),
                          display=ui.keyword("none"))
            doc.show(holder["id"], AND(NOT(mp), op("==", S("weapicon"), name)))
            renders.append(holder)
        children.append(full("ammo-renders", renders))
    children.append(numerals)
    if kind == "ammo":
        icons = full("ammo-icon-sp", glyph_choice(doc, "ammo-icon", "ammoIcon", WEAPON_GLYPHS, u(12), u(14), u(16),
                                                  tint=STROGG["icon"]))
        flash = full("ammo-flash", glyph_choice(doc, "ammo-flash", "ammoIcon", WEAPON_GLYPHS, u(12), u(14), u(16),
                                                tint=[1, 1, 1, 1]), opacity=num(0))
        children += [icons, flash]
    else:
        glyph = "health" if kind == "health" else "armor"
        children.append(full(kind + "-icon", hud_svg.icon_nodes(glyph, kind + "-icon-g", u(12), u(14), u(16), tint=STROGG["icon"])))
        flash = full(kind + "-flash", hud_svg.icon_nodes(glyph, kind + "-flash-g", u(12), u(14), u(16), tint=[1, 1, 1, 1]),
                     opacity=num(0))
        children.append(flash)
    gain_timeline(doc, kind + "-gain", numerals["id"], flash["id"], (u(119), u(11 + 26 * 0.7)), (u(w), u(h)), ink)
    return box(kind + "-gauge", u(gx), u(gy), u(w), u(h), children)


def status_bar(doc: HudDocument, *, strogg: bool = False, mp_variants: bool = False) -> list:
    """The three gauges as one group centred on the bottom edge with the stock
    gaps: Marine 52 u and 11 u (438 u, ammo at 101 u), Strogg 51 u and 11 u
    (449 u). Positions never shift while armor comes and goes (14.4)."""
    for key in ("player_health", "player_healthpct", "player_armor", "player_armorpct", "player_ammo", "player_totalammo",
                "player_clip_size", "player_ammopct", "player_weapon", "player_healthDelta", "player_armorDelta"):
        doc.number(key, -1 if key in ("player_weapon",) else 0)
    doc.number("player_healthpct", 1)
    doc.string("ammoIcon")
    doc.string("weapicon")
    doc.boolean("mp")
    doc.boolean("oq4.low")
    doc.boolean("oq4.teamStrogg")
    doc.boolean("oq4.hidden")
    if strogg:
        gauges = [strogg_gauge(doc, "ammo", 95.5, 419), strogg_gauge(doc, "health", 275.5, 419), strogg_gauge(doc, "armor", 415.5, 419)]
    else:
        gauges = [marine_gauge(doc, "ammo", 101, 420, mp_variants=mp_variants),
                  marine_gauge(doc, "health", 278, 420, mp_variants=mp_variants),
                  marine_gauge(doc, "armor", 414, 420, mp_variants=mp_variants)]
    # A ride's silhouette gauge (the flatbed, the tram) replaces all three.
    doc.boolean("oq4.ride")
    riding = S("oq4.ride")
    doc.show("armor-gauge", AND(op(">", S("player_armor"), 0), NOT(S("oq4.hidden")), NOT(riding)))
    doc.show("ammo-gauge", AND(NOT(S("oq4.hidden")), NOT(riding)))
    doc.show("health-gauge", AND(NOT(S("oq4.hidden")), NOT(riding)))
    # Low health: below 26% of the maximum (25% on the Strogg HUD), set from
    # the same updateHealth event the stock scripts read.
    threshold = 0.25 if strogg else 0.26
    doc.on("updateHealth",
           {"op": "setState", "values": {"oq4.low": op("<", S("player_healthpct"), threshold)}},
           {"op": "if", "condition": op("<", S("player_healthDelta"), 0), "then": [play("health-gain")]})
    doc.on("updateArmor", {"op": "if", "condition": op("<", S("player_armorDelta"), 0), "then": [play("armor-gain")]})
    doc.on("ammoPulse", play("ammo-gain"))
    doc.on("setTeam_marine", {"op": "setState", "values": {"oq4.teamStrogg": False}})
    doc.on("setTeam_strogg", {"op": "setState", "values": {"oq4.teamStrogg": True}})
    doc.joins.append({"op": "setState", "values": {"oq4.low": op("<", S("player_healthpct"), threshold)}})
    return gauges

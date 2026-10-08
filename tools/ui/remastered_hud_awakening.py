"""The Awakening's Remastered vehicle displays (ui-visual-design.md 14.12).

The space cannon (guis/vehicles/airassault/turretplayerhud.gui), the m03
dropship guns: the on-foot HUD's radio, boss bar and notices without its
gauges, strip and powerups, and the ship-status panel top-trailing directly
below the transmission display. The same panel joins the expansion's on-foot
HUD, which receives the script's markers, damage and warnings but cannot
show them. Constructions are the design atlas's (awakening sheet), measured
from the expansion's GUIs; the stock coordinates are the 640x480 canvas.
"""
from __future__ import annotations

import math

import build_retained_screens as ui
from remastered_hud_base import (S, HudDocument, anchor, beyond_minimal, box, join_events, layout_state, num, op, play, pts_path,
                                 rgb, sel, stroke, tr, transform_value, u, vec)

# The dropship from above, nose up (hud_ship_01), in the panel's 128 u box.
SHIP = [(62.7, 17.0), (58.6, 20.5), (56.9, 28.0), (51.0, 33.5), (47.5, 38.5), (47.5, 40.0), (34.0, 49.5), (34.6, 61.5),
        (35.8, 61.5), (37.0, 63.5), (48.1, 65.5), (51.0, 69.5), (51.0, 76.5), (52.2, 78.0), (55.1, 79.0), (57.5, 94.0),
        (52.2, 102.0), (50.4, 103.5), (45.2, 105.0), (44.6, 109.0), (61.0, 109.0), (62.2, 110.5), (66.3, 110.5),
        (67.4, 109.0), (74.5, 108.5), (83.8, 109.0), (83.2, 105.0), (78.0, 103.5), (70.9, 94.5), (73.3, 79.0), (76.2, 78.0),
        (77.4, 76.5), (77.4, 69.0), (79.1, 66.5), (85.6, 64.0), (90.9, 63.5), (94.4, 61.0), (94.4, 49.5), (80.9, 40.0),
        (80.9, 38.5), (77.4, 33.5), (71.5, 28.0), (70.9, 23.5), (68.6, 19.0), (65.7, 17.0)]
# The damaged sections, lit by ship_health_01..05 (80, 60, 40, 20 and 10% of the
# dropship's health): left wing, right wing, nose, tail, mid body.
DAMAGE = [
    [(48.7, 41.0), (47.5, 41.0), (42.2, 45.0), (38.7, 46.5), (34.6, 50.0), (34.6, 60.5), (36.4, 61.0), (38.1, 63.0),
     (43.4, 63.5), (45.8, 65.0), (48.7, 65.0)],
    [(79.7, 42.0), (79.7, 65.0), (82.7, 65.0), (85.0, 63.5), (90.3, 63.0), (92.0, 61.0), (93.8, 60.5), (93.8, 50.0),
     (83.2, 42.5)],
    [(48.1, 39.0), (48.1, 40.0), (80.3, 40.0), (76.8, 34.0), (70.9, 28.5), (70.9, 26.0), (68.0, 19.5), (65.1, 17.5),
     (63.3, 17.5), (60.4, 19.5), (58.1, 24.0), (57.5, 28.5), (51.6, 34.0), (49.9, 37.5)],
    [(57.4, 91.5), (70.9, 91.5), (70.9, 94.5), (78.0, 103.5), (83.2, 105.0), (83.2, 110.5), (45.2, 110.5), (45.2, 105.0),
     (50.4, 103.5), (52.2, 102.0), (57.5, 94.0)],
    [(50.4, 67.0), (51.6, 68.5), (51.6, 76.0), (55.7, 79.0), (72.7, 79.0), (76.8, 76.0), (76.8, 68.5), (78.0, 67.0)],
]
# ship_target_01..05: the objective markers' centres (the stock 16 u rects).
MARKERS = [(56, 83), (64, 79), (55, 66), (54, 51), (65, 37)]
# warning_<dir>: the darts' centres (the stock 24 u rects) and their headings.
WARNINGS = [("N", 65, 9, 0), ("NE", 107, 25, 45), ("E", 114, 63, 90), ("SE", 105, 103, 135), ("S", 64, 122, 180),
            ("SW", 23, 104, 225), ("W", 13, 62, 270), ("NW", 21, 23, 315)]
PANEL_X, PANEL_Y = 512, 32
MARKER_TURN_MS = round(1000 / 0.35)        # 0.35 turn a second
DART_MS = 3000                             # a dart fades over 3 s, pulsing at 2 Hz


def circle_points(cx: float, cy: float, r: float, steps: int = 64) -> list:
    return [(cx + r * math.cos(2 * math.pi * k / steps), cy + r * math.sin(2 * math.pi * k / steps)) for k in range(steps)]


def arc_points(cx: float, cy: float, r: float, a0: float, a1: float, step: float = 4.0) -> list:
    """An arc in the atlas's convention: degrees clockwise from east (y down)."""
    count = max(2, int(math.ceil(abs(a1 - a0) / step)) + 1)
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * k / (count - 1))),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * k / (count - 1)))) for k in range(count)]


def scaled(points: list, dx: float = 0, dy: float = 0) -> list:
    return [(u(x + dx), u(y + dy)) for x, y in points]


def ring_path(ident: str, cx: float, cy: float, inner: float, outer: float, fill) -> dict:
    """An annulus as one even-odd path."""
    def contour(r, prefix):
        pts = circle_points(cx, cy, r)
        cmds = [{"id": f"{prefix}0", "op": "move", "points": [[u(pts[0][0]), u(pts[0][1])]]}]
        cmds += [{"id": f"{prefix}{k}", "op": "line", "points": [[u(x), u(y)]]} for k, (x, y) in enumerate(pts[1:], 1)]
        cmds.append({"id": f"{prefix}z", "op": "close"})
        return cmds
    return {"id": ident, "commands": contour(outer, "o") + contour(inner, "i"), "fillRule": "evenodd", "fill": fill}


def radial_paint(cx: float, cy: float, r: float, stops: list) -> dict:
    """A radial paint in node-local dp: stops are (radius u, rgba)."""
    return {"type": "radial", "from": [u(cx), u(cy)], "to": [u(cx + r), u(cy)],
            "stops": [{"at": round(min(1.0, rad / r), 5), "color": ui.colour(colour)} for rad, colour in stops]}


def arc_stroke(ident: str, cx: float, cy: float, r: float, a0: float, a1: float, colour: list, width: float, blend=None) -> dict:
    return pts_path(ident, scaled(arc_points(cx, cy, r, a0, a1)), stroke=stroke(colour, u(width)), closed=False, blend=blend)


def rotate_about(points: list, cx: float, cy: float, degrees: float) -> list:
    c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in points]


def ship_panel(doc: HudDocument, *, always: bool, cone: bool) -> dict:
    """The ship-status panel (14.12): an olive disc 58.75 u in radius, #1F2A13
    at 0.85 with a #4D682F to #64883D rim, holding the dropship from above in
    #475736. Its five sections turn #A13C2B as the script reports damage (and
    clear on the _off events). Gold markers, broken rings 16 u across, spin at
    0.35 turn a second over the place that needs the player; eight red darts
    on the rim point in from attackers, pulsing at 2 Hz while they fade over
    3 s. On the gun, an additive olive cone 52 degrees wide shows where it
    looks relative to the ship's nose. On foot the panel appears once the
    script sends it something."""
    doc.boolean("oq4.ship")
    doc.number("playerYaw")
    doc.number("oq4_shipYaw")
    olive = rgb("#585E29")
    layers = [
        vec("ship-disc", [pts_path("disc", scaled(circle_points(64, 64, 58.75)), fill=ui.solid(rgb("#1F2A13", 0.85)))]),
        vec("ship-rim", [ring_path("rim", 64, 64, 56.45, 58.75,
                                   radial_paint(64, 64, 58.75, [(0, rgb("#4D682F")), (56.4, rgb("#4D682F")), (58.75, rgb("#64883D"))]))]),
        vec("ship-shade", [pts_path("shade", scaled(circle_points(64, 64, 56)),
                                    fill=radial_paint(64, 64, 56, [(0, [0, 0, 0, 0.5]), (28, [0, 0, 0, 0.5]), (40, [0, 0, 0, 0.25]),
                                                                   (56, [0, 0, 0, 0])]))]),
    ]
    if cone:
        reach, half = 33.7, math.radians(26)
        left = (60 - reach * math.sin(half), 60 - reach * math.cos(half))
        right = (60 + reach * math.sin(half), 60 - reach * math.cos(half))
        wedge = [(60, 60), left] + arc_points(60, 60, reach, -90 - 26, -90 + 26)[1:-1] + [right]
        fill = {"type": "linear", "from": [u(60), u(60)], "to": [u(60), u(60 - reach)],
                "stops": [{"at": 0, "color": ui.colour([*olive[:3], 0.7])}, {"at": 1, "color": ui.colour([*olive[:3], 0.14])}]}
        cone_paths = [pts_path("wedge", scaled(wedge), fill=fill, blend="additive"),
                      pts_path("edge-l", scaled([(60, 60), left]), stroke=stroke([*olive[:3], 0.95], u(1.3)), closed=False, blend="additive"),
                      pts_path("edge-r", scaled([(60, 60), right]), stroke=stroke([*olive[:3], 0.95], u(1.3)), closed=False, blend="additive"),
                      pts_path("hub", scaled(circle_points(60, 60, 2.35, 24)), stroke=stroke([*olive[:3], 0.55], u(1.9)), blend="additive")]
        turning = box("ship-cone", u(4), u(4), u(120), u(120), [vec("ship-cone-v", cone_paths)], transform=ui.transform())
        # The gun's heading relative to the ship's nose, counter-clockwise for a left turn.
        doc.bind("ship-cone.turn", "ship-cone", "transform",
                 [0, 0, 1, 1, op("-", 0, op("-", S("playerYaw"), S("oq4_shipYaw")))])
        layers.append(turning)
    layers.append(vec("ship-hull", [pts_path("hull", scaled(SHIP), fill=ui.solid(rgb("#475736")),
                                              stroke=stroke(rgb("#63883C"), u(1.1), join="round"))]))
    for index, section in enumerate(DAMAGE):
        key = f"oq4.ship.damage{index + 1}"
        doc.boolean(key)
        ident = f"ship-damage{index + 1}"
        layers.append(vec(ident, [pts_path("d", scaled(section), fill=ui.solid(rgb("#A13C2B")),
                                           stroke=stroke(rgb("#76351D"), u(0.9), join="round"))], {"display": ui.keyword("none")}))
        doc.show(ident, S(key))
        number = f"{index + 1:02d}"
        doc.on(f"ship_health_{number}_on", {"op": "setState", "values": {key: True, "oq4.ship": True}})
        doc.on(f"ship_health_{number}_off", {"op": "setState", "values": {key: False, "oq4.ship": True}})
    spin_tracks = []
    for index, (mx, my) in enumerate(MARKERS):
        key = f"oq4.ship.target{index + 1}"
        doc.boolean(key)
        ident = f"ship-target{index + 1}"
        gold = rgb("#FFD800")
        paths = [pts_path("core", scaled(circle_points(8, 8, 3.75, 24)), fill=ui.solid(rgb("#695900")), blend="additive")]
        for k, a in enumerate((145, 325)):
            paths.append(arc_stroke(f"tick{k}", 8, 8, 3.3, 360 - a - 10, 360 - a + 10, gold, 0.9, "additive"))
        for k, (a0, a1) in enumerate(((20, 95), (200, 275), (110, 125), (140, 155), (170, 185), (290, 305), (320, 335), (350, 365))):
            paths.append(arc_stroke(f"ring{k}", 8, 8, 5.875, 360 - a1, 360 - a0, gold, 1.25, "additive"))
        layers.append(box(ident, u(mx - 8), u(my - 8), u(16), u(16), [vec(ident + "-v", paths)],
                          transform=ui.transform(), **{"display": ui.keyword("none")}))
        doc.show(ident, S(key))
        spin_tracks.append(tr(ident, "transform", [(0, transform_value(0, 0, 1, 1, 0)), (MARKER_TURN_MS, transform_value(0, 0, 1, 1, 360))]))
        number = f"{index + 1:02d}"
        doc.on(f"ship_target_{number}_on", {"op": "setState", "values": {key: True, "oq4.ship": True}}, play("ship-spin"))
        doc.on(f"ship_target_{number}_off", {"op": "setState", "values": {key: False}})
    doc.timeline("ship-spin", MARKER_TURN_MS, spin_tracks, iterations=0)
    for name, cx, cy, heading in WARNINGS:
        ident = f"ship-warn-{name}"
        # The dart points along +y before turning to its heading; its faint
        # arcs turn with it.
        dart = [(-4.1, -5.25), (0, 7.1), (3.75, -4.9), (1.5, -1.9), (-1.5, -1.9), (-3.75, -4.1)]
        paths = [arc_stroke(f"arc{k}", 12, 12, 6.6, a0 + heading, a1 + heading, rgb("#760000", 0.5), 2.6, "additive")
                 for k, (a0, a1) in enumerate(((200, 340), (20, 160)))]
        paths.append(pts_path("dart", scaled(rotate_about([(12 + x, 12 + y) for x, y in dart], 12, 12, heading)),
                              fill=ui.solid(rgb("#D50000")), blend="additive"))
        layers.append(box(ident, u(cx - 12), u(cy - 12), u(24), u(24), [vec(ident + "-v", paths)], opacity=num(0)))
        # 2 Hz pulse inside a linear fade from full to nothing over 3 s.
        keys = []
        for step in range(0, DART_MS // 250 + 1):
            t = step * 250
            envelope = 1 - t / DART_MS
            keys.append((t, num(envelope * (1 if step % 2 == 0 else 0.4))))
        doc.timeline(ident + "-fade", DART_MS, [tr(ident, "opacity", keys)], essential=True)
        doc.on(f"warning_{name}", {"op": "setState", "values": {"oq4.ship": True}}, play(ident + "-fade"))
    panel = box("ship", u(PANEL_X), u(PANEL_Y), u(128), u(128), layers, **{"display": ui.keyword("block" if always else "none")})
    if not always:
        doc.show("ship", S("oq4.ship"))
    # A joining peer finds the markers spinning.
    doc.joins.append(play("ship-spin"))
    return panel


def center_box(ident: str, children: list) -> dict:
    """The unscaled 4:3 canvas at the view centre (world-projected brackets)."""
    return ui.group(ident, {"position": ui.keyword("absolute"), "left": ui.length(50, "%"), "top": ui.length(50, "%"),
                            "margin-left": ui.length(-480), "margin-top": ui.length(-360), "width": ui.length(960),
                            "height": ui.length(720), "display": ui.keyword("block")}, children)


def turret_document() -> dict:
    """The space cannon (turretplayerhud.gui, 14.12): the on-foot HUD's radio,
    boss bar and notices without its gauges, strip, pickups or powerups, the
    damage direction, brackets and missile warning, the ship-status panel
    directly below the transmission display, and the HUD prompt's Exit,
    which the turret HUD never showed. The HUD exists only while the gun is
    crewed, so a joining peer shows the prompt at once."""
    from remastered_hud_groups import boss_bar, hit_indicator, interactive_brackets, missile_chevron, notices, transmission
    from remastered_hud_status import full
    from remastered_hud_vehicle import hud_prompt
    doc = HudDocument("openq4.hud.remastered.turret")
    layout_state(doc)
    radio = transmission(doc, strogg=False, emblem="squad_falcon")
    hit = hit_indicator(doc)
    brackets = interactive_brackets(doc)
    boss = boss_bar(doc)
    exit_notice, saved, missile_text = notices(doc)
    missile = missile_chevron(doc)
    panel = ship_panel(doc, always=True, cone=True)
    prompt = hud_prompt(doc)
    doc.on("showExitmessage", {"op": "setState", "values": {"oq4.vehicle": True}})
    doc.on("hideExitmessage", {"op": "setState", "values": {"oq4.vehicle": False}})
    doc.on("showLockmessage", {"op": "setState", "values": {"oq4.locked": True}})
    doc.on("hideLockmessage", {"op": "setState", "values": {"oq4.locked": False}})
    doc.joins.append({"op": "setState", "values": {"oq4.vehicle": True}})
    doc.on("hudJoin", *doc.joins)
    doc.on("onInit", *doc.joins)
    # The stock windows that hold an event's lasting effect.
    pairs = [("p_radio::visible", "radioChatterUp"), ("BossBar::visible", "showBossBar"),
             ("p_boss_shield_bar_left::visible", "showBossShieldBar"), ("brackets::visible", "showBrackets"),
             ("missile_indicator::visible", "missileThreatUp")]
    pairs += [(f"ship_core_{i + 2:02d}::visible", f"ship_health_{i + 1:02d}_on") for i in range(5)]
    pairs += [(f"ship_target_{i + 1:02d}::visible", f"ship_target_{i + 1:02d}_on") for i in range(5)]
    joins = join_events(doc, pairs)
    # Minimal (ui_hudLayout 2) keeps the ship panel, the prompt and the save notice.
    root = full("hud-root", [
        center_box("hud-world", [brackets]),
        anchor(doc, "tr", [beyond_minimal(doc, "tr-full", [radio]), panel]),
        anchor(doc, "tc", [beyond_minimal(doc, "tc-full", [boss, missile_text, exit_notice]), saved]),
        anchor(doc, "cc", [beyond_minimal(doc, "cc-full", [hit, missile])]),
        anchor(doc, "br", [prompt]),
    ])
    return doc.build(root, {"canvas": {"height": 720, "minWidth": 960},
                            "extensions": {"openq4": {"reference": "ui-visual-design.md 14.12",
                                                      "stock": "guis/vehicles/airassault/turretplayerhud.gui",
                                                      "joinEvents": joins}}})


# ------------------------------------------------------------ the MCC cannon

# The stock gauge plate (backbar) and track (valbar), 125x59 u, drawn 136x70 u.
MCC_BACK = "M1 10.4Q1 9.2 2.2 9.2H122.8Q124 9.2 124 10.4V54.7Q124 56.2 122.5 56.2H91.7L84.9 49.3H2.2Q1 49.3 1 48.1Z"
MCC_FILL = ("M5.4 14.8Q5.4 13.8 6.4 13.8H118.6Q119.6 13.8 119.6 14.8V34.6H105.4A16.6 16.6 0 0 0 93.6 44.7H6.4Q5.4 44.7 5.4 "
            "43.7Z")
HULL, HULL_WARN = "#B3D06E", "#FF3300"
USS_KEY, HANNIBAL_KEY = "#str_231242", "#str_231243"


def mcc_readout(doc: HudDocument, dy: float = -36.0) -> dict:
    """The U.S.S. Hannibal readout (cannonplayerhud.gui, group 469,64): the
    stock gauge plate scaled to 136x70 u under U.S.S. Hannibal in Marine 0.25,
    the hull in Marine 0.50 #F1F0C9 (the MCC's health divided by 10, 2000 when
    whole). Remastered stacks it 36 u higher, directly under the transmission
    display, fills the track in proportion (the stock track is static) and at
    or below 25% turns it #FF3300 at 0.60 with a 2 Hz pulse, as the vehicle
    gauges warn."""
    from remastered_hud_base import BOUND_TEXT, shear_group, svg_path, text_box
    from remastered_hud_vehicle import moving_window
    doc.number("mcchealth", 2000)
    width, height = 136.0, 70.0
    sx, sy = width / 125.0, height / 59.0
    m = (round(sx * ui.U, 5), 0, 0, round(sy * ui.U, 5), 0, 0)
    health = op("clamp", op("/", S("mcchealth"), 2000), 0, 1)
    warn = op("<=", health, 0.25)

    def part(ident, data, colour, alpha, **props):
        return vec(ident, [svg_path(ident, data, matrix=m, fill=ui.solid(rgb(colour, alpha)))], props or None)
    back = part("mcc-back", MCC_BACK, "#000000", 0.5)
    back_opaque = part("mcc-back-opaque", MCC_BACK, "#000000", 0.85, display=ui.keyword("none"))
    doc.show("mcc-back", op("!", S("oq4.opaque")))
    doc.show("mcc-back-opaque", S("oq4.opaque"))
    track = part("mcc-track", MCC_FILL, HULL, 0.2)
    doc.show("mcc-track", op("!", warn))
    track_warn = part("mcc-track-warn", MCC_FILL, HULL_WARN, 0.6, opacity=num(1), display=ui.keyword("none"))
    doc.show("mcc-track-warn", warn)
    doc.timeline("mcc-pulse", 500, [tr("mcc-track-warn", "opacity", [(0, num(1)), (250, num(0.5)), (500, num(1))])], iterations=0)
    doc.joins.append(play("mcc-pulse"))
    fill = part("mcc-fill", MCC_FILL, HULL, 0.35)
    doc.show("mcc-fill", op("!", warn))
    fill_warn = part("mcc-fill-warn", MCC_FILL, HULL_WARN, 0.6, display=ui.keyword("none"))
    doc.show("mcc-fill-warn", warn)
    # The fill grows from the leading end: the window's trailing edge follows the hull.
    window = moving_window(doc, "mcc-win", 0, 0, width, height, [fill, fill_warn],
                           op("-", op("*", op("+", 5.4, op("*", 114.2, health)), sx), width))
    gauge = box("mcc-gauge", u(494), u(80 + dy), u(width), u(height),
                [shear_group("mcc-plate", 0, 0, u(width), u(height), -0.22, [back, back_opaque, track, track_warn, window])])
    labels = [text_box("mcc-uss", USS_KEY, face="marine", size=u(8.375), colour=[1, 1, 1, 0.74], baseline=u(89 + dy), x=u(512),
                       width=u(60)),
              text_box("mcc-name", HANNIBAL_KEY, face="marine", size=u(8.375), colour=[1, 1, 1, 0.74], baseline=u(89 + dy),
                       x=u(560), width=u(80))]
    hull = text_box("mcc-hull", BOUND_TEXT, face="marine", size=u(16.75), colour=rgb("#F1F0C9", 0.733), baseline=u(128 + dy),
                    right=u(585), width=u(101))
    doc.bind("mcc-hull.text", "mcc-hull", "text", op("numberText", op("floor", S("mcchealth"))))
    doc.bind("mcc-hull.color", "mcc-hull", "color", [sel(warn, c, d) for c, d in zip(rgb(HULL_WARN, 0.733), rgb("#F1F0C9", 0.733))])
    return ui.group("mcc", {**ui.FULL, "display": ui.keyword("block")}, [gauge, *labels, hull])


def mcc_document() -> dict:
    """The MCC cannon (guis/vehicles/mcc/cannonplayerhud.gui, 14.12): the
    side gun cannot be damaged (it inherits noDamage, health 0 and god mode),
    so its HUD reports what the player defends, the MCC's hull. Remastered
    adds the transmission display the side gun's HUD lacks, stacks the readout
    under it and shows Exit in the HUD prompt."""
    from remastered_hud_groups import notices, transmission
    from remastered_hud_status import full
    from remastered_hud_vehicle import hud_prompt
    doc = HudDocument("openq4.hud.remastered.mcc")
    layout_state(doc)
    radio = transmission(doc, strogg=False, emblem="squad_falcon")
    exit_notice, saved, missile_text = notices(doc)
    readout = mcc_readout(doc)
    prompt = hud_prompt(doc)
    doc.on("showExitmessage", {"op": "setState", "values": {"oq4.vehicle": True}})
    doc.on("hideExitmessage", {"op": "setState", "values": {"oq4.vehicle": False}})
    doc.on("showLockmessage", {"op": "setState", "values": {"oq4.locked": True}})
    doc.on("hideLockmessage", {"op": "setState", "values": {"oq4.locked": False}})
    # The HUD exists only while the gun is crewed.
    doc.joins.append({"op": "setState", "values": {"oq4.vehicle": True}})
    doc.on("hudJoin", *doc.joins)
    doc.on("onInit", *doc.joins)
    # Minimal (ui_hudLayout 2) keeps the hull readout, the prompt and the save notice.
    root = full("hud-root", [
        anchor(doc, "tr", [beyond_minimal(doc, "tr-full", [radio]), readout]),
        anchor(doc, "tc", [beyond_minimal(doc, "tc-full", [missile_text, exit_notice]), saved]),
        anchor(doc, "br", [prompt]),
    ])
    return doc.build(root, {"canvas": {"height": 720, "minWidth": 960},
                            "extensions": {"openq4": {"reference": "ui-visual-design.md 14.12",
                                                      "stock": "guis/vehicles/mcc/cannonplayerhud.gui"}}})

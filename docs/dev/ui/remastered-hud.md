# Remastered HUD layout

With `ui_retained 1` and `ui_hudLayout 1` (the default), the stock HUD GUIs
draw the Remastered layout of [visual specification section 14](../ui-visual-design.md)
instead of their stock rectangles. Each covered GUI is paired with a retained
document that receives the same state keys and named events the game sends
the stock GUI, and lays its groups out on the section 14.4 anchors: the 4:3
center screen, the side screens capped by `ui_hudWidth`, the safe inset
`ui_hudSafeInset` and the HUD scale `ui_hudScale`. `ui_hudLayout 0` keeps the
stock layout, drawn through the [HUD presentation bridge](hud-coverage.md).
Settings > Game Options > HUD sets all four (HUD Layout, HUD Size, HUD Width
and Safe Area Inset); see the [settings structure](../settings-menu-structure.md).

## Minimal

`ui_hudLayout 2` is section 13.9's Minimal preset: the same documents, with
everything but health, armor, ammunition and the crosshair family left out.
What answers or asks for the player's action stays: the vehicle's Exit or
locked prompt, the save notice, chat, voice and votes, the respawn prompt,
the follow card, an objective failure and the held displays (objectives,
scoreboard, statistics). A vehicle keeps its own display: the cannon's optic,
the ship panel, the MCC's hull readout and the speeder's race display. It
leaves out the weapon strip, pickups,
transmissions, the boss bar, damage direction, missile warnings, objective
notices, powerups, the vehicle's hit flash, electrical warnings and the
speeder's speed streak, and in
multiplayer the standings, team panels, timer, center notices, kill feed, aim
text, the Tourney strip, the match card and the lag notice. Each document reads
the preference as `hud.layout` and wraps the groups Minimal drops in a
full-anchor container (`beyond_minimal`), so the remaining groups keep their
places.

## Covered GUIs

| Stock GUI | Document | Contents |
| --- | --- | --- |
| `guis/hud.gui`, `guis/q4xhud.gui` | `remastered_marine.q4ui`, `remastered_marine_awakening.q4ui` | Status bar, weapon strip, pickups, transmission display, boss bar, notices, interactive brackets, damage direction, missile warning; in multiplayer the powerup slots and developer readouts |
| `guis/hud_strogg.gui`, `guis/q4xhud_strogg.gui` | `remastered_strogg.q4ui`, `remastered_strogg_awakening.q4ui` | The Strogg family of the same groups |
| `guis/mphud.gui` | `remastered_mp.q4ui` | Kill feed, standing, team panels with flags and zone, timer, center column notices, aim text, vote, voice, chat lines, follow card, respawn prompt, Tourney strip, statistics card, Arena ceremony, lag notice |
| `guis/hud.gui` while riding the flatbed, `guis/hud_strogg.gui` on the tram | (the on-foot documents) | The ride's silhouette gauge replaces the three gauges (`enterFlatbed`/`enterTram`), traced from the stock truck and tram art |
| `guis/vehicles/airassault/turretplayerhud.gui` | `remastered_turret.q4ui` | The Awakening's space cannon (m03's dropship guns): the transmission display, boss bar, notices, damage direction, brackets and missile warning without gauges, strip or powerups; the ship-status panel under the transmission display (damage sections, spinning objective markers, attack darts, and the view cone relative to the ship's nose); the HUD prompt's Exit |
| `guis/vehicles/mcc/cannonplayerhud.gui` | `remastered_mcc.q4ui` | The Awakening's MCC cannon (m06): the gun takes no damage, so the readout is the MCC's hull, U.S.S. Hannibal over the stock gauge plate with the hull number (the MCC's health divided by 10), stacked under the transmission display the side gun's HUD lacked, its track filled in proportion and warning at 25%; the HUD prompt's Exit |
| `guis/vehicles/speederplayerhud.gui` | `remastered_speeder.q4ui` | The Awakening's speeder bike (m07's races): the vehicle status bar with the bike's own glyphs (its machinegun first, then the cannon with its recharge bar), the hull and shield warnings the expansion removed and the entry lines its HUD never played; the race display 18 u lower (the plate, ten checkpoint pips, the countdown with two-digit hundredths and its seconds #FF3333 in the last 10 s, shown only while the race runs it); the HUD prompt (Boost on the crouch control while a boost is ready, the hazard and Boosting while it pushes, then Exit or Vehicle locked); the speed streak while boosting; transmission display, boss bar, notices |
| `guis/vehicles/hud.gui` | `remastered_vehicle.q4ui` | The walker's and hover tank's status bar (ARMOR and SHIELDS gauges, weapon panel, rocket count, charge bar), entry lines, hit flash, electrical warning, HUD prompt (Exit or Vehicle locked), transmission display, boss bar, notices |
| `guis/vehicles/airdef_cannon.gui` | `remastered_airdef.q4ui` | The air-defence cannon: the retained nailgun lens held still (Gaussian focus beyond the aperture) with its two crescent load bars and amber reticle, the transmission display the stock lacks, the HUD prompt (Exit or Vehicle locked) in place of the centered exit line, and the save notice |
| `guis/wristcomm.gui`, `guis/wristcomm_strogg.gui` | `remastered_objectives.q4ui`, `remastered_objectives_strogg.q4ui` | Objectives display (every objective, three at a time, a COMPLETED section), new-objective and objective-complete notices, OBJECTIVE FAILED |
| `guis/scoreboard.gui` | `remastered_scoreboard.q4ui` | The held scoreboard: soft backing, server header, your place, your team's panel, the other team's and the spectators', rows with your marker, DEATHS, ping bars, dead players and bots, and the footer; in Tourney the bracket tree (closed outlines, elbow connectors, your arena in aqua, finished arenas in blue, EMPTY arenas) over the players and spectators lists |

All documents live in `content/baseoq4/pak0/guis/hud/` and are generated by
`tools/ui/build_remastered_hud.py` from `tools/ui/remastered_hud_*.py`; never
edit the generated JSON. `--check` fails when a document is stale.

## Engine peer

`idRemasteredHud` (`src/ui/RetainedHud.cpp`) pairs a stock GUI with its
document by source name. `idUserInterfaceLocal` forwards every state write,
deletion and named event to the peer; the stock windows keep evaluating unseen
so their scripts, timers and presentation aliases stay where the stock layout
expects them. On its first frame the peer joins the stock GUI's dictionary.
Named events sent before that frame (a vehicle's boarding events arrive before
its HUD is first drawn) are queued, up to 32, and replayed after the join.
State that only events carry survives a peer rebuilt mid-game (the layout
switched to Classic and back): a document's `extensions.openq4.joinEvents`
pairs a stock window variable with the event that set it (`exit_msg::visible`
with Exit, `p_radio::visible` with the transmission display, `desktop::hidden`
with `hideHud`), and the peer replays the event at its join when the variable
holds (`!` inverts the test). The generator refuses an event the document does
not answer.
Strings cross over with color escapes removed and `^ik` key escapes resolved
to key names. A mod that ships its own stock-named GUI keeps it.

Diagnostics: `ui_hudProfile N` prints per-document forward, event, state,
draw and unseen-evaluation timings averaged over N frames, and
`ui_hudTest <gui> state|event|get ...` writes, sends or reads a key without
input.

## Game fields

Where the stock GUIs lack what the Remastered layout shows, the game publishes
extra fields beside the stock ones; the stock GUIs ignore them.

- Multiplayer (`idMultiplayerGame::PublishRemasteredHud`): the kill feed and chat
  as structured lines (`oq4_feed*`, `oq4_chat*`, `rvHudLines`), ordinals, the
  lead gap, limits, the clock and its warnings, flags, zone, alive counts, the
  vote tally, the followed player and the control keys. `oq4_flags`,
  `oq4_clockWarn` and `oq4_state` are announced as events, so looping motion
  starts only when its state does.
- Respawn (`PublishRemasteredHud`): `oq4_respawn` is 1 while you lie dead
  and the attack control would bring you back, 2 while overtime holds the
  respawn with `oq4_respawn_in` seconds left, else 0; the `oq4_respawn` event
  fades the prompt in as it appears. It follows `CheckRespawns`: round modes
  keep the eliminated out until the next round, sudden death in deathmatch
  brings back only the leaders, and Tourney only its arena's two players. A
  client mirrors the server's hold (the 3 s fall, longer in overtime) when its
  snapshot reports the death.
- Scoreboard (`idMultiplayerGame::PublishRemasteredScoreboard`): per panel
  `oq4_sb{p}_kind`, `_score` and `_count`, per row `oq4_sb{p}_{r}_name`, `_clan`,
  `_score`, `_kills`, `_deaths`, `_time`, `_ping` and `_flags` (you, dead, bot,
  eliminated, ready, muted, flag carrier), the pitch `oq4_sb_pitch`,
  `oq4_sb_kills` and the rank line `oq4_sb_rank`. The header and footer read the
  stock keys. `testScoreboard <msec>` holds the board open without input.
- Vehicles (`rvVehicle::UpdateHUD`): `oq4_key_jump`, the control that leaves,
  and `oq4_shipYaw`, the heading of whatever carries the vehicle (m03's
  dropship), against which the ship panel's cone shows the gun's heading.
- The speeder bike's boost (`riVehiclePartBoost`): `oq4_boost` (0 ready, 1
  pushing, 2 recharging), `oq4_boost_envelope` (the push's strength, 0 to 1,
  which the speed streak follows) and `oq4_key_crouch`, the control that fires
  it. The race's timer, checkpoints and countdown come from m07's scripts
  through the vehicle's script GUI events.
- The Awakening's on-foot documents carry the ship panel too, shown once m03's
  script sends the player's HUD a marker, damage or warning.
- Objectives (`idPlayer`): `oq4_new_title`/`oq4_new_shot` and `oq4_done_title`
  latched per notice, `oq4_obj_count` with `oq4_obj{i}_title/_text/_shot`
  newest first, `oq4_done_count` with `oq4_done{i}`, `oq4_obj_scroll`, and
  `oq4_failed_title`. Each notice closes on its own timer
  (`oq4_closeNewObjective`, `oq4_closeDoneObjective`), and the completion click
  plays once. While the display is open and holds more than three items, the
  weapon-cycle controls scroll it. `oq4_obj_toggle` reports Toggle Objectives
  (`g_objectivesToggle`, Settings > Game Options > HUD), under which each press
  of the scores button opens or closes the display, as the Xbox 360 edition's
  did; the display then shows a HUD prompt naming `oq4_key_scores` and Close.

## Runtime cost

HUD documents are large (about 7,200 authored values in the Marine document)
and animate continuously, which the retained runtime now handles without
walking every value:

- A frame applies only the motion values written since the last complete
  pass, plus the properties state bindings own when the state changed
  (`Motion::TakeChanged`). Input-time refreshes of a constrained menu popup
  stay complete passes.
- Motion values are copy-on-write, and an event samples the live motion in
  place (`Motion::AdvanceUndoable`), rolling the sample back if the event is
  rejected; an event no longer copies the document's values.
- Vector geometry caches quarter-pixel phases for elements moving by
  translation; the vehicle, ride and MCC gauges drain through transformed clip
  windows, so a regenerating shield never lays the document out again. RmlUi
  clips only content that overflows a window toward its trailing or bottom
  side, so each window's content box reaches past its trailing edge; the
  native checks measure the drawn fill, not only the window's transform.

`openq4-ui-retained-screens-test --hud-bench <document>` reports event, state
and frame costs for a document.

## Validation

- `openq4-ui-remastered-hud` loads every document through the real compiler and
  runtime and checks each group's anchors at several aspects, its states and
  its events. `openq4-ui-remastered-hud-generated` checks the generated
  documents are current.
- `tools/ui/capture_remastered_hud.py --assets <Quake 4> --output .tmp/<dir>
  --scenario <name>` captures windowed gameplay without host input: `marine`,
  `strogg`, `objectives`, `tank` (convoy1, boarding through the actor's
  `enterVehicle` script event), `airdef` (mcc_landing's cannon, boarded the
  same way), `turret` (m03's top gun, with `--awakening <content root holding
  q4xbase>`, after the level's own hand-off from its AI gunner), `mcc` (m06's
  side gun, the same way), `objectives-toggle` (Toggle Objectives opening,
  keeping and closing the display with presses), `speeder` (m07_race1, whose
  script seats the player
  on the bike and runs the countdown; the race's checkpoint event, the
  countdown's expiry and the crouch control drive the rest), `mp` (with the
  scoreboard and the respawn
  prompt after `kill 0`) and `mp-ctf` (with the scoreboard), and the
  `mp-pace`/`sp-pace` profiles.

## Open work

- The match summary (section 14.14), the scoreboard's scrolling past 16
  rows, The Awakening's GEV display, the chat entry line (an
  interactive field, which belongs with the retained menu input) still draw
  their stock layout through the presentation bridge.
- The touch control that toggles the objectives display is not implemented.

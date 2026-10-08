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

## Competitive

`ui_hudLayout 3` is section 13.9's Competitive preset, built by
`tools/ui/remastered_hud_competitive.py` into the same documents. In single
player it is the Remastered layout. In multiplayer:

- **Match bar.** The standing or team panels, the timer and the Tourney strip
  (`tl-rem`, `tc-strip`) give way to one bar at the top center: three sheared
  plates from y 6, 26 u tall and 2 u apart, a 60 u timer at 0.72 between 122 u
  sides at 0.55. In team modes each side's outer 25 u block is the team color
  at 0.85 with its faction mark, then the team name; your team (or the followed
  player's) leads. In deathmatch and Tourney the block is the header band with
  the rank ordinal (the leader's in #FFBE23) and the name, cut at 52 u, with the
  marker on your side. The scores sit at the inner ends in Chain 21 u.
- **Timer.** The Remastered timer's states in Chain 15.5 u, with WARM-UP, the
  countdown and STARTS IN, SUDDEN DEATH and the infinity mark, and under it the
  limit (TO 30), the round, the arena or the warm-up's ready count.
- **Under the bar.** Two-flag modes hang each side's flag 6 u in from its inner
  end (AT BASE, TAKEN with the carrier, DROPPED); round modes show five alive
  pips; Dead Zone puts the holder, CONTESTED or EMPTY under the timer.
- **Network quality** follows the bar's trailing end at all times: the ping
  beside four bars (four to 60 ms, three to 120 ms, two to 200 ms, one above),
  each 2% of this client's incoming packet loss removing one, down to one.
- **Team status.** Team modes list your team's players (up to eight, best score
  first) at the leading edge from y 150: the weapon, the name, health in Chain
  (#E46D56 at 25 and below, #FFBE23 above 100) and armor; your row on the band
  plate with the marker; the dead at 0.40 under a cross. Spectators see the
  Marines leading and the Strogg trailing, the followed row lit at 0.22 with
  the marker; held statistics take the trailing list's place.
- **Readouts.** The status bar, the weapon strip and the pickups grow by 15%
  about the bottom edge (`status_scale`, still limited to the span on narrow
  views). The chat box keeps its 8 u clearance above the bar and never rises
  toward the team status: short of the 180 u it needs beside the bar, it is
  200 u wide, or as wide as the space beside the widest enlarged strip allows,
  down to 120 u at 4:3. The vote moves to the top-leading corner (36 u down,
  clear of the team status) and the free camera's name tags add a health line.
- **Narrow views.** Below about 16:9 at the HUD scale in use
  (`under_match_bar`), the kill feed, the managed-match card and the lag
  notice step 38 u down, so the feed's longest line clears network quality.

The game publishes the bar's sides (`oq4_mb0_*`, `oq4_mb1_*`), the line under
the timer (`oq4_mb_sub`, `oq4_mb_ready`), the team status rows
(`oq4_ts{team}_count`, `oq4_ts{team}_{row}_*`), the tags' health and
`oq4_loss` from `idMultiplayerGame::PublishRemasteredCompetitive` and
`PublishRemasteredHud`.

## Covered GUIs

| Stock GUI | Document | Contents |
| --- | --- | --- |
| `guis/hud.gui`, `guis/q4xhud.gui` | `remastered_marine.q4ui`, `remastered_marine_awakening.q4ui` | Status bar, weapon strip, pickups, transmission display, boss bar, notices, interactive brackets, damage direction, missile warning; in multiplayer the powerup slots and developer readouts |
| `guis/hud_strogg.gui`, `guis/q4xhud_strogg.gui` | `remastered_strogg.q4ui`, `remastered_strogg_awakening.q4ui` | The Strogg family of the same groups |
| `guis/mphud.gui` | `remastered_mp.q4ui` | Kill feed, standing, team panels with flags and zone, timer, center column notices, aim text, vote, voice, chat lines and, while message mode is open (`guis/mpmsgmode.gui`), the chat input, follow card (Next, Previous, Free camera), the free camera's name tags, respawn prompt, Tourney strip, statistics card, Arena ceremony, lag notice; in Competitive the match bar, network quality and team status |
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

A document can ask the peer to read its layout back into its state for the
next frame (`extensions.openq4.readbacks`), where bindings alone cannot know a
laid-out size. None of these readbacks feeds the layout it measures:

- `[state, node, view]`: whether the node lays out wider than the view (the
  chat entry's mark of text scrolled out of view).
- `[state, node, "@height"]`: the node's laid-out height in canvas dp while it
  has one (the chat prompts, one row or wrapped).
- `[state, view, "@firstInside", prefix, count]`: the first of the shown
  nodes `prefix0` to `prefix<count-1>` whose top lies inside the view (the
  first whole message of the chat history; the lines above it fade).

The retained GUI answers `node::@width`, `@height`, `@x` and `@y` presentation
reads with the node's laid-out box.

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
- Chat input (`PublishRemasteredChat`): while message mode is open,
  `oq4_chatting`, up to eight lines of untimed history in the chat slots
  (`oq4_chat_count`), and the entry from the message-mode GUI's edit control
  split at its caret (`oq4_chatin_before`, `_after`), the characters left of
  the stock 128 (`oq4_chatin_left`), the channel (`oq4_chatin_team`), whether
  team chat exists (`oq4_chatin_channels`) and the overstrike mode. The
  control (`idChatWindow`) publishes its caret and overstrike mode
  (`oq4_chat_caret`, `oq4_chat_overstrike`) and, in the Remastered layout,
  stops drawing its own panel; it keeps the typing, recall and channel
  switching. Opening the input, typing and moving the caret send
  `oq4_chatType`, which restarts the caret's blink. `chatInput <text>` and
  `chatCaret <n>` fill the open entry and move its caret without device
  input.
- Name tags (`PublishRemasteredNameTags`): for a free-flying spectator,
  `oq4_tags` players in view, nearest first and at most eight, each with
  `oq4_tag{i}_x`/`_y` (the point above its head as fractions of the view),
  `_name`, `_team` (-1 outside team modes) and `_health`. The game projects each
  player through the spectator's render view and traces it, so no tag shows
  through a wall. `oq4_key_prev` names the control a following spectator steps
  back with: the weapon-cycle controls now step through the followed players
  outside Tourney, as Tourney's do.
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

## Match summary

The match summary (`guis/summary.gui`, section 14.14) keeps the stock layout
the specification adopts, drawn through the presentation bridge. The bridge
draws the stock menu furniture as vector reconstructions authored in each
texture's own texels (`artwork.q4ui`, from `tools/ui/build_retained_hud.py`),
so each stretches and flips as the stock art does: the soft backing, the three
tab strips, the list bands (line, hover and greyed), the separators, the header
band, the light plate, the chat box, the card frame, the backdrop gradient and
the corner marker. Rails are filled one-texel geometry, never strokes, which
would thicken unevenly in a stretched tile. openQ4's
`content/baseoq4/pak0/guis/summary.gui` adds to the stock:

- **Draw.** A team match that ends level reads DRAW over neutral team panels
  (the managed result's `summary_result_*` presentation), where the stock named
  the Strogg the winners.
- **Exit prompt.** The engine publishes `oq4_summary_exit` (`#str_231257`)
  with the key of the device last used: Escape, or a controller's east button,
  which menus take as Escape.
- **Your row.** The corner marker in #E38900 stands 1 u before your row's band
  on the SUMMARY and SCOREBOARD tabs, beside the brighter band or a top-three
  tint. The game publishes each list's row as `<list>_you`, counted from 1 (0
  when you are not listed), apart from the selection that a click on the tab
  moves; each marker follows its list's `itemheight`. The top of your team,
  which the stock left unmarked, now gets the brighter band too.
- **Layouts.** Arena One Flag CTF (7) takes the flag modes' SCOREBOARD and
  STATISTICS layout, and Clan Arena, Freeze Tag and Red Rover (10–12) Team
  DM's, as `scoreboard.gui` does. They fell through to the Tourney layout,
  whose lists the team update never fills.

The engine's list window draws a row's `<list>_item_<i>_highlight` fill and
`<list>_item_<i>_greyed` band as Quake 4 1.4.2 does, in its order: the
selection, a greyed row, the hover band, the highlight, then the line band.
The summary's top three deathmatch rows take their blue, red and yellow tints
from it, and the classic scoreboard greys eliminated Tourney players.

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
  prompt after `kill 0`, then the chat input through `clientMessageMode`,
  `chatInput` and `chatCaret`), `mp-spectator` (in warm-up, so the match is not
  abandoned when the only human spectates: the follow card through
  `follownext`, then the free camera's name tags through `followfree`),
  `mp-ctf` (with the scoreboard), the Competitive `mp-competitive` (deathmatch),
  `mp-competitive-ctf`, `mp-competitive-spec` (both teams' lists while
  following and flying free) and `mp-competitive-matrix` (eight players a
  team, a long name and the chat input with a long entry; run it at each
  `--size`), `mp-summary`, `mp-summary-team` and `mp-summary-ca` (the match
  summary once bots reach a frag limit of one, or a Clan Arena duel's round
  ends, its SCOREBOARD and STATISTICS tabs opened through `openq4_guiAction`),
  and the `mp-pace`/`sp-pace` profiles. `--gate 0` runs a scenario with the
  stock GUIs for comparison.

## Open work

- The scoreboard's scrolling past 16 rows and The Awakening's GEV display
  still draw their stock layout through the presentation bridge.
- The chat input's prompts name the keyboard's keys; controller and touch
  players' platform or on-screen keyboard (section 13.5) is not implemented.
- The touch control that toggles the objectives display is not implemented.

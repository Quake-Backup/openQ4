# Multiplayer bots

openQ4 multiplayer ships with bots that navigate any map without a per-map
authoring or compile step. This describes how they work and how to drive them.

## Why not AAS

Quake 4's SDK routes AI over AAS, an offline navigation compile stored as a
`.aas48` / `.aas96` file next to the `.proc`. id shipped those files for the
single-player campaign only: **not one of the 49 stock multiplayer maps has an
AAS file**, and neither do community maps built with the retail tools, because
the multiplayer game never had anything to navigate. AAS-based bots therefore
have nothing to route over in multiplayer, which is exactly what the SDK's own
`idGameLocal::AddBot` reports when it refuses to add a bot.

So navigation is generated at runtime instead, from the collision world the
players themselves move through.

## The navmesh

`src/mpgame/bots/NavMesh.{h,cpp}` (in the openQ4-GameLibs repository) builds a
multi-level grid navmesh at map load.

**Generation** follows the same shape as a modern voxel navmesh generator -
rasterise the walkable surface, connect what a walking agent can traverse - but
the "rasteriser" is the engine's own collision system, so a link exists only if
the player's bounding box can actually make that move:

1. **Seeds.** Spawn points, items, jump pads and their landing targets, and any
   player already in the map. These are places the map guarantees are standable,
   and flooding out from them avoids probing the solid majority of the world
   bounds.
2. **Flood fill.** From each node, the eight neighbouring grid cells are probed
   with `TryStep`, which sweeps the player's bounding box - `pm_bboxwidth` by
   `pm_normalheight`, taken live from the cvars the server is running - flat,
   then lifted by `pm_stepsize`, then lifted by `pm_jumpheight`, and drops
   whatever it reaches onto the floor. The cheapest lift that works decides the
   link type. A node is keyed by grid cell *and* floor height, so a catwalk over
   a corridor is two nodes in the same column.
3. **Off-mesh links.** `rvJumpPad` and teleporter `idTrigger_Multi` volumes get
   a one-way link from the volume to their target. On a stock Quake 4
   multiplayer map these are what keep the graph a single component - `q4dm1`
   alone has 15 of them.
4. **Areas.** Connected components over the links treated as undirected. Two
   nodes in different components definitely cannot reach each other, which lets
   a bot reject an unreachable goal without paying for a search. The converse is
   not guaranteed (a one-way drop leaves both ends in one component), so this is
   only ever used to say no.

Traversal types are `WALK`, `DROP`, `JUMP`, `JUMPPAD` and `TELEPORT`.

**Routing** is A* with a euclidean heuristic over a binary heap, with the search
scratch tagged by a serial number rather than cleared, so a query costs only the
nodes it opens. The node chain is then string-pulled: a corner is dropped when a
box sweep proves the shortcut is clear *and* the ground under it stays close, so
a shortcut can never cut a corner across a pit. Smoothing never skips a link
that has to be entered deliberately - a jump pad or a teleporter always survives
as its own corner.

Typical cost, `q4dm1` at the default 24-unit cell: **8327 nodes, 60253 links,
one area, about 2.7 seconds** to build. Generation is lazy - it happens the
first time a bot is added, so a server that never uses bots never pays for it.

## The bot

`src/mpgame/bots/Bot.{h,cpp}`. A bot is not an entity. It takes a real client
slot through `idNetworkSystem::AllocateClientSlotForBot`, spawns a real
`idPlayer`, and each server frame writes the user command that a remote player's
packet would have delivered. Everything downstream - movement physics, weapons,
damage, scoring, the scoreboard, game type rules, team logic - therefore treats
it as an ordinary player with no special cases.

Per frame a bot picks a goal (visible enemy, else the nearest reachable item,
else a random reachable point), routes to it, steers through the corners, aims
with a skill-scaled turn rate and steady-state error, picks a weapon from a
range-dependent preference list, and fires once its aim is inside the cone.
Dead or spectating, it holds attack, which is how `idPlayer` takes a respawn.

Three things keep a bot from ever parking:

- **Stuck detection.** No progress while trying to move triggers a sidestep, a
  jump and a fresh route.
- **Goal give-up.** A goal that has not resolved in 12 seconds is abandoned, and
  arriving at an item that is *still sitting there* abandons it immediately -
  that is a bot standing on its own CTF flag, or on a weapon it already has with
  full ammo. Abandoned goals go on a short per-bot blacklist. The blacklist
  holds several entries on purpose: with one slot a bot ping-pongs between two
  items it cannot take.
- **Off-mesh recovery.** If routing fails repeatedly the bot walks toward the
  nearest node it does know about, and wanders if there is not one.

Repathing is time-throttled rather than "whenever there is no path": an enemy
standing somewhere genuinely unreachable would otherwise cost a full failed A*
search every single frame. The throttle counts *attempts*, not successes —
keying it off "am I currently chasing an enemy" leaves it dead on the failure
path, which is the only path it exists for. Goal selection, which walks every
spawned item, is throttled the same way.

Jumping cycles the button rather than holding it. `idPhysics_Player::CheckJump`
only fires on a fresh press — `PMF_JUMP_HELD` blocks a held button and is
cleared only on a frame where `upmove` is released — and the press only takes if
the bot happens to be on the ground that frame. A single tap on a long cooldown
mostly misses.

## Commands

| Command | Effect |
| --- | --- |
| `addbot [name]` | Add one bot. Picks an unused name if none is given. Builds the navmesh on first use. |
| `removebot [name]` | Remove one bot, by name or the last one added. |
| `kickbots` | Remove every bot. |
| `botlist` | List the bots and the state of the navmesh. |
| `navmesh build` | Rebuild the navmesh, picking up a changed `bot_navCellSize`. |
| `navmesh info` | Report node, link and build-time counts. |

## Cvars

| Cvar | Default | Effect |
| --- | --- | --- |
| `bot_enable` | `1` | Allow bots to be added at all. |
| `bot_minPlayers` | `0` | Top the match up to this many players with bots. `0` disables. |
| `bot_skill` | `3` | 1 (harmless) to 5 (unpleasant). Scales turn speed, aim error, reaction time, sight range and how much a bot dodges. |
| `bot_debug` | `0` | `1` logs navigation events, `2` adds a periodic per-bot status line. |
| `bot_debugNav` | `0` | `1` draws the navmesh near the local player, `2` adds each bot's current route. |
| `bot_navCellSize` | `24` | Sampling resolution in world units. Smaller finds more ground and costs more to build. |
| `bot_pause` | `0` | Freeze all bot input. |

Bots are server-side only. `bot_minPlayers` and `bot_skill` are archived, so a
dedicated server config can set them once.

## Known limits

- Bots fight; they do not play objectives. On CTF they will shoot each other
  competently and ignore the flag.
- A jump pad's link lands on its target entity, but `rvJumpPad` aims the player
  to *arrive* there with its vertical speed spent, so the real landing spot is a
  little past it. Bots re-route on arrival, so this costs a moment, not a route.
- The A* heuristic is not admissible across teleporters (see the comment on
  `rvNavMesh::FindPath`); paths can be slightly longer than optimal, never
  invalid.
- `bot_debugNav` is implemented but has not been visually confirmed in this
  build. The renderer's debug line pool is fixed at 16384 entries and the draw
  is budgeted to stay inside it, but nothing on the server path calls
  `DebugClear`, so lines accumulate rather than expire.

## Testing

`tools/tests/mp_bot_navigation.py` pins the agreements that make this work and
that the compiler cannot check: the engine handing back the allocated slot,
bots being re-begun after a map change, user info being restored on every
update, bots thinking before entities do, and the navmesh deriving its agent
from the movement cvars rather than hardcoding a size.

For a live check, run a dedicated server with `bot_debug 2` and read the status
lines: each one carries the bot's position, health, goal, path progress, current
enemy and whether it is firing.

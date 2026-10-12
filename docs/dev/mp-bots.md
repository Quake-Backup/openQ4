# Multiplayer bots

openQ4 multiplayer generates bot navigation at map load without a per-map
authoring or compile step. Bots arrive with a name, a way of playing and
something to say about it. This describes their behaviour, validation and
current limits.

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

`src/mpgame/bots/NavMesh.{h,cpp}` builds a multi-level grid navmesh at map
load.

**Generation** follows the same shape as a modern voxel navmesh generator -
rasterise the walkable surface, connect what a walking agent can traverse - but
the "rasteriser" is the engine's own collision system, so a link exists only if
the configured player hull clears the sampled move:

1. **Seeds.** Spawn points, items, jump pads and their landing targets, and any
   player already in the map. These are places the map guarantees are standable,
   and flooding out from them avoids probing the solid majority of the world
   bounds.
2. **Hazards.** `trigger_hurt` volumes are read before the flood. One that gibs,
   deals 100 or more per touch or 30 or more per second refuses the ground it
   covers outright, as lava and slime do; a survivable one, one that starts
   switched off or one riding a mover makes its ground cost 384 more to cross.
   A mover sitting just above a lethal volume (a trapdoor) or one that crushes
   is priced the same way.
3. **Flood fill.** From each node, the eight neighbouring grid cells are probed
   with `TryStep`, which sweeps the player's hull - `pm_bboxwidth` by
   `pm_normalheight`, with `pm_usecylinder` selecting the same side count and
   alignment as player physics - flat,
   then lifted by `pm_stepsize`, then lifted by `pm_jumpheight`, and drops
   whatever it reaches onto the floor. The cheapest lift that works decides the
   link type. A smaller central support probe keeps ordinary nodes away from
   floors touched only by the outside edge of the hull. Jump and drop landings
   also need support around that centre, so a decorative rim beside a pit is
   not treated as stable ground. The probes lift for legitimate slopes and
   require the returned floor to remain within a step of the expected height.
   A node is keyed by grid cell *and* floor height, so a catwalk over
   a corridor is two nodes in the same column. Drops reach 512 units and pay
   for the fall damage they would cost. A proven level walk links back at once,
   which halves the build's collision sweeps. Every jump-height lift also needs
   the full jump trajectory proof: lifting a box over a railing and dropping
   it vertically does not model momentum over a narrow lower ledge or raised
   decorative lip. Near-ground air steps preserve momentum as multiplayer
   physics does; a blocked ascent can slide up a riser, while partially
   completed air steps fail closed. Drops also simulate slow and full-speed
   walk-offs, including the permitted sideways approach velocity, through
   touchdown and supported floor along the braking distance. Grounded steps
   over low lips remain usable under ceilings too low for a jump. A vertical
   projection onto a small island is insufficient.
4. **Gap jumps.** From a ledge with a straight run-up behind it, each direction
   is searched for the first floor beyond a gap within a running jump's reach,
   and the jump is linked only when full hull sweeps through touchdown at both
   85% and full approach speed prove it clears and lands. The simulation uses
   the bot's direction-to-corner air input and multiplayer air acceleration;
   landing requires supported, nonlethal ground with a walkable connection to
   the destination. Passing above an island while still rising is insufficient.
5. **Off-mesh links.** `rvJumpPad` and teleporter `idTrigger_Multi` volumes get
   a one-way link from the volume to where they put the player. A pad's target
   is the apex of its arc, not its landing, so the flight is integrated from the
   pad's own launch velocity at the multiplayer frame rate and swept with the
   player's box until it comes down; q4dm1's pad 24 aims over a ledge and lands
   players on the floor below. A pad can put a player down in more than one
   place, as Quake III's AAS knows: besides its natural landing, the supported
   node nearest the apex on each floor level within 256 units is flown again
   with air control steering for it, and linked when the flight really comes
   down within 32 units of it from the pad's centre and from the middle of
   every side a standing box can touch the volume from - a bot is thrown where
   it first touches the pad, not from its centre - up to four landings a pad. That is the only way
   onto the ledge beside q4dm1's vertical pads, whose arc falls straight back
   down their shaft. A bot rides a natural landing without steering, as long as
   the ridden arc lands on or past the node and within a box of its line -
   braking onto the node slowed flyers under the bots behind them, and over
   q4ctf5's gap they met in the air. Steering is `NavAirSteer`, shared with the
   bot in flight:
   it aims for the horizontal velocity that arrives over the landing as the arc
   comes down to its height, rather than at the landing itself, which overshot
   and swung back. Nothing that comes down on the pad itself is a landing, and
   a pad with no other is a trampoline that gets no link. Each landing costs
   the ground a bot would have run while in the air, never less than 128, so a
   second and a half aloft beside a vertical pad is not a cheap way past it. On
   a stock Quake 4 multiplayer map these links are what keep the graph a single
   component.
6. **Transport sealing.** A pad or teleporter takes whoever touches it, so every
   node whose standing box touches a live volume - tested against the brush
   itself, not its bounding box - keeps the transport, to each of its landings,
   as its only exits. A route either takes the pad or goes round it; it can never walk across
   one by accident. Links into the volume stay - entering it is how a route
   takes the transport - and a route never starts or ends on one. Shortcuts are
   held to the same brush: a line is tested by sweeping the standing box along
   it, and 48 units under it, against the volume's own clip model, and only a
   line whose box starts touching the brush counts as leaving it. A diagonal
   pad's bounding box covers floor beside the pad that sealing leaves walkable,
   and a line from that floor used to skip the pad altogether.
7. **Compaction.** Each node's links are rewritten into one contiguous run, so
   every search walks memory in order instead of chasing a list through a
   quarter of a million entries.
8. **Reachability.** Strongly connected components and the transitive closure
   of their condensation answer "can A reach B" exactly in constant time, so a
   goal nobody can reach - an enemy standing in a one-way pit - is refused
   without a search.
   The build line reports the share of nodes in the largest component and the
   dead ends - nodes with no way off at all - that a map sweep reads for
   places a bot can be trapped.
9. **Landmarks.** Eight far-apart nodes of the largest component carry exact
   travel costs to and from every node (ALT). The triangle inequality turns
   them into a lower bound on any route, which guides A* around walls and stays
   tight across the cheap long-range transports that collapse a straight-line
   bound. Each node's eight pairs sit in one 64-byte row, so a bound costs one
   cache line.

Traversal types are `WALK`, `DROP`, `JUMP`, `JUMPPAD` and `TELEPORT`. Every
navmesh sweep tests the world and solid entities but never a body: multiplayer
players carry `CONTENTS_SOLID` for their combat box, and a sweep that collided
with them refused route snaps on the very floor a bot stood on.

**Routing** is A* over a binary heap with weighted travel costs, guided by the
larger of the landmark bound and the straight-line distance scaled by the
graph's cheapest cost-to-distance ratio; both are consistent, and a node the
landmarks prove cannot lead to the goal is never opened. Ordinary walking pays
distance, jumps and drops add a safety/time penalty, jump pads pay for flight
time as running distance, and teleporters retain their short transport cost. Search scratch is tagged by a
serial number rather than cleared, so a query costs only the nodes it opens. A
selected route is then swept against the solids that can move after the build -
movers, doors, plats and loose physics objects - and only those; a stale edge is
excluded and A* retries. Ordinary unlocked touch/proximity doors count as open,
because approaching them opens them; locked, no-touch, keyed or scripted doors
stay barriers.

An injured bot also qualifies selected jump, drop and pad landings against its
current health and armour. The limit comes from the player's fall thresholds
and resolved multiplayer damage declarations. Predicted impacts use the same
water reductions and no-damage surface flag as `CrashLand`. Unsafe edges are
excluded only from that search, so healthy bots and safe alternate routes keep
using the shared graph. A grounded pre-entry check catches damage after route
planning, brakes and abandons the old goal; its cached proof is reused while
the route and survivable tier stay unchanged. An already launched flight keeps
its landing controller.

Route endpoints in the air - a jumping bot, an enemy mid-jump - are projected
onto whatever surface is under them before they snap to a node. A snap or a
string-pulled shortcut must pass `WalkableLine`: a box sweep, floor samples
along the line, no lethal ground, no step onto a transport, and no rise or fall
steeper than a walk, so a bot under a balcony never routes from the balcony. A
sweep stopped within its last two units has arrived: a player flush against a
wall is a legal endpoint the clip epsilon stops a box just short of.

The node chain is then string-pulled: a corner is dropped when the shortcut is
proven, and smoothing never skips a link that has to be entered deliberately.
Every jump, drop, jump pad and teleporter retains both its entry and exit. A jump - up a
step too tall to walk or across a gap - lands only where floor reaches within
8 units of the box's middle: a standing box rests on any overlap, so flood fill
lays nodes that hang off a platform by all but a sliver (the corner of q4dm1's
turned crate held one by six tenths of a unit), and a jump aimed at one comes
down beside the platform every time. A gap jump whose first floor is such an
edge lands one cell further in. A
gap jump - or a jump up more than two steps, such as q4dm1's crates - gets a
straight run-up corner in front of its takeoff. A jump that comes back down on
its takeoff floor short of the middle of its link fell short; the bot goes back
to the run-up and takes it again at speed, twice, before the stuck detector
decides. Height alone cannot tell, since a level jump lands at its takeoff's
height whether it made it or not. A goal that
stands in a pad's or teleporter's reach - an item beside one, an enemy at its
edge - ends its route at the nearest node, because nobody can stand exactly
there without being carried off. Two other final-leg refusals are not real: the
goal's own snap already proved the walk the other way, which stands; and a goal
set against a wall, such as an item in an alcove, can sit where a player's box
does not fit, so the route ends where the box stops within touching distance.
A chase may also settle for the nearest node on the target's floor when the
target stands where no node is proven walkable, such as a tight diagonal
corridor; item routes stay exact.

**Goal pricing** answers "which of these is worth going for" without a field
per decision. Every candidate gets a landmark lower bound on its route cost,
so its weight over that bound is the most it can score; candidates are priced
exactly, best bound first, by a corner-free A* (`rvNavMesh::PathCost`), until
the best few exact scores beat everything still unpriced. The top few are then
exact and FindPath proves only the winner. On `q4dm5` this expands about 6% of
what a whole-map Dijkstra field per decision settled. Where the bounds are weak
- long detours around pads and drops - each exact search would re-expand the
same ground, so past a work budget the candidates still unpriced are priced
from one bounded field and a decision never costs much more than a field.

Typical output for `q4dm5` at the default 24-unit cell is about **27,800 nodes,
204,000 links, 316 gap jumps and 44 sealed transports**, built in under two
seconds in a debug build. Generation is lazy - it happens the first time a bot
is added, so a server that never uses bots never pays for it.

## The bot

`src/mpgame/bots/Bot.{h,cpp}`. A bot is not an entity. It takes a real client
slot through `idNetworkSystem::AllocateClientSlotForBot`, spawns a real
`idPlayer`, and each server frame writes the user command that a remote player's
packet would have delivered. Everything downstream - movement physics, weapons,
damage, scoring, the scoreboard, game type rules, team logic - therefore treats
it as an ordinary player with no special cases.

Bot slots are excluded from outgoing unreliable-message queues and recipient
PVS checks because they have no network connection or snapshots to drain those
queues. The message's subject may still be a bot: human spectators following
it receive their feedback, and demo/repeater recording remains intact. The
documented `clientNum == -1` broadcast also reaches remote human recipients.

In team games, a newly allocated bot reserves the least-populated side before
its player entity exists. Pending players who intend to join count toward the
server's ordinary auto-balance check, so several `addbot` commands issued in
one frame still alternate sides instead of all inheriting the same team.

Movement intent and combat intent are deliberately separate. On each decision
tick a bot compares the current mode objective, useful pickups, an enemy chase,
and directed roaming, commits to the strongest reachable movement goal, and
keeps aiming and firing at the best perceived opponent while it travels there.
A flag carrier therefore continues running home through a firefight, and a
wounded bot can withdraw toward health without becoming harmless. Short goal
commitments and a material switch margin prevent equivalent choices from
chattering every quarter-second; an urgent rule change, such as a dropped flag
or enemy carrier, can still replace the route immediately.

Replacement routes are built in scratch state and committed together with their
goal identity only after a successful search. An unreachable challenger leaves
the current valid commitment intact. Repairing the same goal retains its progress
history and give-up deadline; only actual progress extends the deadline. A new
roam gets its own distance baseline, and exhausting every route clears stale
movement before off-mesh recovery takes over. Pickup commitments are rechecked
against the current inventory on each decision tick. A bot collecting its own
item does not queue an item-denial reaction.

Perception skips visibility traces for opponents that cannot improve either the
nearest or best visible choice, while always checking the current target for
continuity. Item selection rejects out-of-range pickups before inventory work
and probes navigation only for candidates that can enter the ten-item shortlist.
These filters preserve target ordering, directed route proof and team claims.
Once an enemy leaves sight, chase prediction and weapon choice use the last
observed position; combat strafing no longer follows the enemy's hidden live
position. Projectile threat rejection includes the larger of direct-hit clearance
and explosion radius, so nearby resting explosives and distant splash impacts
remain eligible for the detailed trajectory check.

*How well* and *in what manner* a bot executes those decisions - how far it
notices you, how long it takes to react, how steadily it holds an aim, how close
it wants to fight and what it wants to fight with - comes from its resolved
personality, which is the next section. Dead, it holds attack, which is how
`idPlayer` takes a respawn. Spectating is deliberately not the same thing:
attack cycles the spectator's view target rather than rejoining, so a
spectating bot clears `wantSpectate` and sets `forceRespawn` instead - the same
request a human makes on leaving the spectate menu - and lets the game type
decide when it gets in.

### Mode objectives

Objective discovery runs only during a live match, rejects stale players and
entities from other multiplayer instances, and supplies rule urgency to the
ordinary path-aware goal selector. The currently supported mode families are:

- **CTF variants.** Standard CTF, One Flag, Arena CTF and Arena One Flag use a
  deterministic team role allocation informed by navigation route bounds.
  Carriers follow the
  correct base or their instance's ordered assault-point chain; the nearest
  useful teammates return a dropped flag or intercept an enemy carrier, escorts
  form around a friendly carrier (with extra help when that carrier is hurt), a
  majority attacks the active enemy or neutral flag, and deliberately unassigned
  bots defend the capture end instead of joining a team-wide dogpile.
- **Freeze Tag.** During a live round bots are uniquely matched to nearby frozen
  teammates and remain at the body long enough for the rescue rule to act.
  Rescue urgency rises as more of the team freezes, when only one rescuer is
  left, and when enemies pressure the body.
- **DeadZone.** Bots read zone ownership and deadlock state to decide how many
  teammates should intercept, escort or contest. Carriers hold a valid authored
  control zone, while available artifacts are uniquely distributed among
  collectors. Maps whose control zone explicitly does not require the artifact
  are respected, with a larger contesting group for hostile or deadlocked zones
  and a smaller anchor after control is secure.

These are movement objectives, not blinders. Enemy perception, aiming and safe
fire continue throughout the route. Escorts occupy stable trailing formation
lanes projected onto navigation rather than standing on their carrier, while
base defenders hold a useful perimeter instead of pinning themselves to the
flag pedestal.

### Pickups and route choice

Item choice reads the bot's live inventory. Routine health is ignored at full
health, major health remains useful for an overstack, armour scales with the
current deficit, missing weapons outrank duplicates, ammo matters only below
capacity, and major powerups remain worth a detour. Flags and the DeadZone
artifact are excluded because the mode objective layer owns them.

Every useful pickup in range is a candidate. Goal pricing (above) returns the
best few by utility over route cost, including the exact legs onto the mesh and
off it, and the bot proves a real route to the winner, falling through to the
next when live geometry blocks it. Existing claims from teammates reduce a
pickup's value rather than hard-locking it, spreading the team across resources
without preventing two bots from contesting something important. Critical
health can interrupt most tasks, while a carrier's immediate capture remains
the top movement priority except at the edge of death.

Bots time items. A bot that sees or hears a pickup - the taker, anyone within
earshot, or everyone for an announced powerup - learns when it returns, with an
error set by its `itemTiming` trait. A timed item is worth heading for when the
run arrives about as it respawns: the remaining wait discounts it, up to half,
and the bot stands on the spot for the return instead of wandering off. An
unarmed bot puts a real weapon ahead of nearly everything, a pickup a few steps
away is taken on the way even mid-chase, and a defender keeps its base stocked.
In a buying mode a bot spends its credit on the weapon or armour it lacks.

### Perception and combat

Bots honour `fl.notarget` like every other thing in the game that acquires a
target, so a developer can watch them navigate on a listen server without being
shot at.

Perception is bounded by the bot's range and field of view, then traces exposed
chest, head and pelvis points plus lateral body samples so a visible shoulder at
cover can be acquired without granting vision through the wall. Invisible
opponents stay hidden unless they recently damaged the bot or reveal themselves
by firing nearby.

Bots also hear. Each server frame records the loudest noise every player made -
weapon fire carries 1700 units, the gauntlet 700, a pickup 900 and running
footsteps 420 - scaled by the listener's `hearing` trait. A heard opponent is
located, never seen: it becomes a pursuit and patrol goal and a direction to
face, but hearing grants no shot, and a sound-only contact is worth less than a
visible one when goals are compared. Target selection weighs distance, health and armour, active
powerups, objective carriers, enemies threatening a friendly carrier, firing
opponents, the last attacker, grudges and personality, with stickiness
preventing pointless target swaps.

Combat movement blends each character's preferred range with the weapon in
hand, so a shotgun encourages a closer engagement while a railgun preserves
space. Each fight is weighed as a trade - health and armour, the best weapon
each side holds and any active powerup - and a bot that cannot win it breaks
off toward health and armour while still shooting; how bad the trade has to be
is the character's aggression. A bot carrying quad goes hunting, and only a
well-armed, healthy bot turns toward an enemy carrying a powerup rather than
feeding it. When sight is lost, a bounded pursuit point projects the enemy's cached
position and velocity; its confidence and lead fade with memory age instead of
making the bot either stop immediately or track an opponent forever. Dodges test
the local route and floor on both sides, trying the opposite direction when the
first choice is obstructed or unsafe.

Damage is also a perception event. The damage path reports the real player or
projectile owner and attack direction, schedules one bounded dodge that
sustained fire cannot postpone forever, and promotes the attacker only if line
of sight confirms it - being hit is not permission to see through a wall. A
short-horizon scan separately advances hostile explosives through segmented,
gravity-aware hull sweeps relative to the bot's own motion. It can dodge before
a direct intercept, a predicted world impact and splash, or a slow/resting
fused explosive becomes dangerous; world cover is considered rather than
treating every nearby explosive as unobstructed.

Projectile aim resolves the live multiplayer projectile definition, including
launch speed, gravity and collision bounds, and iterates target motion, the
bot's own motion and drop compensation to lead rockets, nails, grenades and
napalm consistently. Instant-hit weapons are not led. Splash weapons aim at a
grounded target's feet, where a near miss still lands. Weapons the bot would be
wasting at range stay holstered past their useful reach, and the nailgun,
lightning gun and gauntlet hold the trigger instead of bursting. The weapon selector
covers every stock and expansion weapon, blends close- and long-range
suitability with character bias, penalizes splash weapons at point-blank range,
and uses score hysteresis and a minimum dwell to avoid switch loops.

Before firing, projectile weapons sweep their real hull along segmented linear
or ballistic trajectories rather than checking only a ray to the nominal aim
point. Safety projects moving teammates into those future segments, rejects
their future splash exposure (including the shooter), and accounts for world
cover around a predicted impact; safe world impacts can still support doorway
suppression. Single-shot weapons also respect readiness and cadence. Splash safety uses the
game's own damage model - linear falloff from the explosion and half damage to
the shooter - so a bot refuses a shot that would cost it more than a token
amount of health, or more than it can spare, and team damage is only
considered when `si_teamDamage` is on.

That sweep runs down the axis the weapon actually fires along - the view - and
not toward the aim point. The trigger gate deliberately allows the two to differ
by up to the whole fire cone, which is 18 degrees at skill 1, so a proof run
against the aim ray would clear a trajectory the bot never takes. The
usefulness half of the same query is answered before the splash test rather than
through it: an unobstructed shot and one that stops on another hostile are
useful by construction, and only an impact on the world has to put the target
inside the resulting splash. Weapons with no splash carry a zero radius, and
routing those through a splash test would make every hitscan weapon hold fire
unless the trace happened to land on the target hull - which the deliberate aim
error is designed to prevent.

### Navigation and progress

Four things keep a bot from ever parking:

- **Off-route recovery.** A route is a statement about the floor the bot stands
  on. Once grounded, the bot compares its height with the height the current
  leg has where it stands; knocked off a ledge, carried by a pad or fallen past
  a landing, it is more than 80 units below (or, on a walk, 96 above) and the
  route is replaced at once, with the goal itself decided again. So is a bot
  more than 384 units to the side of its leg, as a teleporter or a long
  knockback leaves it, and a drop or a jump that came down more than a step
  below its takeoff without reaching its landing - momentum past the landing's
  wall, a pad beside the landing, a gap jump into its gap - since that landing
  may now be behind a wall.
- **Stuck detection.** Progress means advancing a path corner or materially
  shortening the horizontal distance to the current one, not merely moving in
  any direction. Two consecutive failed samples trigger a sidestep, a jump and
  a fresh route. Vertical motion alone cannot disguise a bot hopping in place,
  while a deliberate traversal gets a bounded grace period in which to finish.
- **Goal give-up.** The deadline scales with the route's expected travel time
  and is extended only when the complete remaining path gets shorter. A stale
  goal with no recent progress is abandoned, and arriving at an item that is
  *still sitting there* abandons it immediately - that is a weapon it already
  has with full ammo or another pickup it cannot use. Abandoned goals go on a
  short per-bot blacklist. The blacklist holds several entries on purpose:
  with one slot a bot ping-pongs between two items it cannot take.
- **Off-mesh recovery.** If routing fails repeatedly the bot walks toward the
  nearest node it does know about, and wanders if there is not one. Both paths
  use the grounded momentum/edge brake; abandoning a route cannot bypass it.
  A stationary bot supported only by its outer hull may crawl toward a known
  node at no more than 32 units/s if a separate, at-most-64-unit proof finds
  nearby strong support. Every four-unit sample must retain same-height hull
  contact and body clearance. Consecutive footprints must also share a known
  floor patch, so even a narrow loss of support between samples refuses the
  escape. Gaps, walls, transports and lethal floor are rejected. Ordinary
  walking and random wandering gain no such exception.
  A recovery turn updates the command's view yaw before projecting movement,
  so physics uses the same world direction that passed the floor check.

A foe last seen inside a lethal volume - falling into q4dm5's void - is not
chased: it is dying, and no route reaches the air it falls through.

Repathing is time-throttled rather than "whenever there is no path": an enemy
standing somewhere genuinely unreachable would otherwise cost a full failed A*
search every single frame. The throttle counts *attempts*, not successes —
keying it off "am I currently chasing an enemy" leaves it dead on the failure
path, which is the only path it exists for. Goal selection, which walks every
spawned item, is throttled the same way.

Grounded movement includes a void-edge guard. A route leg is proven, but
the box drifts off it - a string-pulled corner, a dodge, a team-mate's shove, a
landing at a pad's speed - and on q4ctf5's walkways over space momentum carried
bots over an edge ten times a minute. Each frame the bot looks 0.15 s ahead
along its velocity; with no floor within a survivable 512-unit fall there,
tested with the middle 16 units of the box so a crack is not a void, or a kill
volume, it brakes against its velocity instead. A jump takeoff and a pad or
teleporter entry leave an edge on purpose and are exempt.

World-space movement retains its intended strength while the bot turns to aim.
The projected forward/side command pair is scaled for the player's `CmdScale`,
so a 45-degree aim difference does not reduce speed or air braking. Deliberately
partial input and zero movement retain their strengths.

Combat strafing, teammate separation and unsticking are local hints layered on
top of the route. Each altered direction must pass a player-bounds sweep and a
floor check before it can replace the nav direction, preventing a useful dodge
from becoming a step into a wall or off a ledge.
Discretionary dodge and recovery hops additionally simulate their flight
through touchdown for both current momentum and a full-speed approach. A
72-unit walking probe does not cover a hop's roughly 220-unit travel. The
bounded proof runs only on fresh grounded jump pulses and refuses unsupported,
lethal or transport landings; deliberate route jumps retain their own proof.
An accepted hop holds that proved world-space input through touchdown while
aiming and firing remain independent. A changed goal, strafe or view angle
cannot bend the flight after qualification. A refused launch releases after
128 ms, the flight hold expires after 1,600 ms, and death/spawn clear it. Real
pad launches supersede the hold. A newly acquired speed boost cannot increase
the held command beyond its captured wish speed.

A drop's landing can lie under the lip of its own ledge, so near a drop or a jump
the bot walks the link's own direction until it actually leaves the ground.
A bot that has pressed along its route for a quarter second without closing in
on a walk corner more than a few units above it, or on the pad or teleporter
volume it is stepping into, hops, on any leg. The navmesh steps onto a rise in
one 24-unit stride and the player in five-unit frames, so a bevelled lip - the
sides of q4dm1's pad ramps - can pass the generator and still stop the feet, and
the ramp can stand between a pad's entry node and its trigger; the generic
unstick hop waits for the stuck detector and stays off on the approach to an
action, which is exactly where those ramps are. Entering a pad or a teleporter steers at the volume's own
centre rather than the node the route enters it by. In a pad's flight or a
drop's fall the bot steers with `NavAirSteer` for the velocity that lands it on
the route's node, coasting when no correction is needed, so it comes down where
the navmesh predicted instead of overshooting onto the pad beside a landing.
Pads that throw players at each other over a gap - q4ctf5's either side of
the floating large armour - meet head-on over the void, and solid players
stop each other dead there: within 80 units of touching a pad, a bot brakes
while anyone is flying along that finite corridor in either direction. A
same-direction leader can rebound from the far pad into a follower. Reserving the
expanded trigger boundary does not release that hold; an actual launch does.
A blockage lasting two seconds requests route reconsideration while continuing
to hold, and waiting cannot trigger a climb hop into the pad. A flight that
comes down on the next pad of a chain hands over to it on
touch, and a bot thrown away from its landing at a pad's speed - a short landing
on the pad opposite, a pad the route never planned - coasts rather than brake
the throw over the void, and replans where it sets down. A pad counts as taken only on real launch
speed - more than a jump climbs or a player runs - and its flight ends when the
bot is back on the ground, wherever that is; a flight still airborne after four
seconds is a bounce and releases the route. A
drop or a gap jump taken at speed may land far past its one-cell landing; down
on the landing's floor and ahead of it, the action counts as done only after
the route from the actual touchdown to the next walking corner is checked.
An unsafe continuation brakes and replans. Before a sharp turn into a jump or drop,
the bot sheds sideways momentum while it still has ground to stop on.
It also sheds overspeed before either action. A routed jump is checked again
at the actual takeoff position with its live velocity, rather than assuming
the bot is centred on the entry and already moving along the link. The first
frame uses the encoded movement command, including the jump axis in
`CmdScale`; the flight then uses the same direction-to-landing steering as
the proof. Refused launches brake, can recenter over proven supported ground,
and abandon an unusable approach after the traversal timeout.
Haste and Scout retain their benefit
on ordinary routes, while action entry and flight commands stay within the
base-speed trajectory model. Discretionary combat hops instead qualify their
actual boosted wish speed.

An unplanned pad touch also replaces an ordinary walking route with the pad's
proven natural landing. Detection requires the actual player hull to contact
the force-field brush and a launch impulse, including the first frame when
the grounded flag can still describe the preceding physics update. The
original goal remains available after landing. An overlapping bounding box
alone cannot activate this recovery, and an already planned pad keeps its
chosen landing.
An active pad with no accepted entry node or landing still registers its
activation volume. Its ground nodes have no ordinary exits, preventing a
walking shortcut through a launcher that the route planner cannot safely use.
No destination or recovery flight is invented for that volume. Objective
holding also waits for actual grounding, so crossing a scoring volume in the
air cannot replace an existing landing controller with stationary combat.

Every navigation event is in the `bot_debug 1` log: stuck and off-route
reports with the corner, its travel type and height, completed actions with
their entry, exit and observed landing, and - rate limited - each route snap or
final leg the navmesh refused, with the first test that failed and the entity
that blocked it.

Non-walk links are committed movements rather than loose proximity corners. The
bot first reaches the entry, suppresses combat strafing, generic unsticking and
periodic replans during the action, then advances only after it has left the
entry and reached the destination side. A jump specifically has to leave the
ground and land on the destination floor; being one nearby grid cell below it
does not count. Bounded forward overshoot is accepted so a natural jump need not
stop on an exact sample.

Jump input is pulsed on alternating grounded frames rather than held.
`idPhysics_Player::CheckJump` only fires on a fresh press —
`PMF_JUMP_HELD` blocks a held button and is cleared only on a frame where
`upmove` is released. Grounded pulses provide a fresh edge within one frame
without spraying jump input through the flight.

### Team orders

A human player can order the bots on their own team in team chat, Quake III
style. The line starts with who is addressed - one or more bot names joined by
"and", or a group word (`everyone`, `everybody`, `all`, `team`, `bots`, `guys`,
`squad`) - followed straight away by the order, optionally after `please`:

| Order | Phrases | Effect |
| --- | --- | --- |
| Follow | `follow`, `follow me`, `escort me`, `cover me`, `with me`, `on me`, `come with me`, `regroup` | Escort the speaker in formation, fighting on the way. In every team mode. |
| Defend | `defend`, `defend the base`, `guard`, `defense`, `hold the base`, `stay home`, ... | CTF: hold the team's base. Elsewhere, as below. |
| Defend here | `defend here`, `hold here`, `hold this position`, `guard here`, `stay here`, `camp here` | Hold the area around the nearest spawn point or pickup to where the speaker stands; with nothing fixed within 512 units, follow instead. |
| Attack | `attack`, `offense`, `get the flag`, `grab the flag`, `take the flag`, `capture the flag`, `push` | CTF: take the enemy flag, or escort a team-mate who has it. Elsewhere, free play. |
| Free | `free`, `freelance`, `dismissed`, `roam`, `carry on`, `as you were`, `stop following`, `do your thing` | Back to the bot's own judgement. |

Examples: `kane defend`, `everyone follow me`, `sorg and bagby hold here please`.
Anchoring the order to the names keeps ordinary team talk ("I'll defend", "kane
is following me") from turning into orders. Only humans give orders, only in
team chat, and only to their own team. Up to two of the bots ordered
acknowledge in team chat from their character's `order` lines, which skip the
chattiness roll but respect the flood throttle; orders themselves work with
`bot_chat 0`.

An order outranks the role the allocator would have handed out, but not the
rules' urgent steps: a carrier still takes the flag home, and anyone may still
grab health at the edge of death. Bots under orders are left out of role
allocation, so their team-mates share the remaining roles around them. An order
lasts until it is replaced or dismissed, the bot changes team, or the map
changes; a follow order lapses while its leader is dead and ends when the
leader leaves or switches team.

## Characters

`src/mpgame/bots/BotCharacter.{h,cpp}`. Difficulty used to be five numbers on a
straight line, which made every bot at a given `bot_skill` identical and every
match feel the same. It is now three layers of content that resolve, once per
spawn, into one flat `botTraits_t` — 50 floats that are the only thing the
per-frame combat code reads:

1. **Skill level, 1 to 5.** The baseline curve. Every trait has a value at every
   level, and the fractional levels `bot_skillVariance` produces are interpolated
   between the two bracketing rows. This layer on its own is a complete, playable
   bot, and it is exactly what `bot_characters 0` gives you.
2. **Play style.** A named archetype that biases the baseline. A style says what
   a bot *wants to do*, never *how well it does it*: a skill 1 sniper and a
   skill 5 sniper both hold range and both reach for the rail gun, and only one
   of them hits with it. Six ship — `rusher`, `sniper`, `roamer`, `hunter`,
   `ambusher`, `skirmisher`.
3. **Character.** Identity: display name, player model, the skill band it is
   picked for, and its own per-trait bias and per-skill-level overrides. Its
   voice is a separate chat bank joined to that identity by character name.

Layers 2 and 3 do not restate the trait list. They carry sparse statements —
`set`, `add`, `scale` — that name a trait by the same word the struct uses, and
that resolve through one static field table. Adding a new trait is one row in
that table and one number per skill level in the baseline curve; no parser and
no content file has to learn it exists.

Resolution order matters, because `scale` composes with whatever came before it:

```
baseline( skill level, after variance )
  -> style body
  -> style skill <n> { } block
  -> character body
  -> character skill <n> { } block
```

Each statement is applied in file order and its field is clamped to the table's
range immediately afterwards, so a typo in a content file cannot produce a bot
that turns at 90000 degrees a second.

Weapon preferences are the one thing not reachable through the field table.
They are named entries rather than scalars, and they *merge* rather than
overwrite, so a character with an opinion about one weapon keeps the rest of its
style's opinions intact.

### What each skill level feels like

The point of the curve is that the levels differ in kind, not only in degree.

- **1** is genuinely poor and should be beatable by someone who has never played
  the map. It notices you inside a 130 degree cone at 1200 units, takes
  460 ± 180 ms to react, and pays most of that again every time you break line
  of sight. Its aim trails the truth by 0.4 seconds, carries 4.2 degrees of
  wandering tremor, and swings at 170 deg/s on an underdamped slew that still
  overshoots and hunts. Its 0.80 initiative turns the raw 14 degree / 240 ms
  trigger values into an effective 18.2 degree cone and 96 ms settle instead
  of waiting so long for bad aim to settle that it barely fires;
  the extra attacks remain inaccurate. It barely leads a projectile and gets
  roughly one combat decision in two wrong.
- **2** is a distracted human. It reacts in about 0.39 s, tracks better, still
  overshoots, and still sprays.
- **3** is the default and is meant to read as a competent regular: 320 ms on a
  fresh sighting, 224 ms on a re-peek, 2.1 degrees of tremor, a 6.5 degree cone
  it settles into for 170 ms before shooting, and projectiles led at about 60 %
  correctness.
- **4** punishes standing still. It notices you at 2600 units across a 170
  degree cone, reacts in 220 ms, holds a 4 degree cone, and leads well enough
  that walking in a straight line at range is fatal.
- **5** is sharp but deliberately not an aimbot: 140 ± 45 ms reaction that it
  still re-pays at 40 % on a re-peek, a 0.08 s tracking lag, half a degree of
  tremor it can never switch off, a 2 degree cone it refuses to shoot outside
  of, and 97 % lead correctness with a 5 % random error — so a strafing target
  at range still survives the occasional rocket.

Awareness climbs with the same curve. `hearing` scales how far a bot hears
fire, footsteps and pickups - 45% of the nominal range at skill 1, all of it at
skill 5 - and `itemTiming` is how well it keeps a respawn timer it witnessed,
from barely at all at skill 1 (0.05) to within a few percent at skill 5 (0.92).

Note the trigger duty cycle runs the other way from intuition: skill 1 holds the
trigger down about 86 % of an engagement and skill 5 about 79 %. Low skill
*fires more* and hits far less. That is the intended read — spray and pray, not
a nerfed rate of fire.

`combatRange`, `aggression`, `patience`, seven of the tactical-personality
traits, and `chatiness` are deliberately flat across the whole curve. They are
taste, not competence, and a skill curve that moved them would make every
high-skill bot play identically, which is the thing this system exists to
prevent. `initiative` and `suppressionMsec` are the two exceptions: novices
take imperfect shots and waste rounds through recently crossed doorways, while
high-skill bots default to waiting for a visible, settled shot.

### The aim model

Skill is legible in play because it is spent on the *shape* of the aim rather
than on a hidden accuracy roll.

- **Belief.** The bot aims at where it *thinks* you are, a first-order lag
  behind where you actually are, with a time constant of 0.45 s at skill 1 and
  0.08 s at skill 5. On a fresh acquisition the belief snaps to the truth, so
  an engagement never opens with the bot aiming at the last fight.
- **Lead.** Only for weapons that actually launch something. The test is
  whether the weapon declares `def_projectile`, not the hitscan flag, because
  that flag is false for the gauntlet and the lightning gun even though both are
  instant-hit. Projectile speed and gravity are resolved through the entity def
  system so the multiplayer retune applies (rocket 935, not 900). Three short
  flight-time iterations combine target velocity, the bot's own velocity and
  gravity compensation, so rockets, nails, grenades and napalm receive the same
  physically consistent lead instead of treating ballistic weapons as linear.
- **Tracking error.** A lag *across* the view rather than noise: the faster you
  cross the bot's screen, the further behind its aim sits.
- **Tremor.** Two slow sine octaves, phase-seeded from the client number so no
  two bots shake in step. Continuous and smooth, so it reads as an unsteady hand
  rather than as white noise, which is what the old 400 ms step offset read as.
- **Turn.** A damped second-order slew driven by `turnSpeed`, `turnAccel` and
  `turnDamping`. Under-damping at low skill is what produces the visible
  overshoot-and-hunt; skill 5 settles cleanly.
- **Reaction.** Computed once per acquisition, never per frame, and it now
  includes a peripheral penalty scaled by how far off centre you were when the
  bot first saw you. A re-acquisition inside `reacquireMsec` still pays
  `reacquireFraction` of the full cost. That fraction is the fix for the old
  zero-reaction bug, in which a bot re-peeking a corner had already paid its
  reaction once and fired instantly forever after — and it is why the skill 5
  reaction time went *up*, from 70 ms to 140 ms.
- **Trigger.** The aim has to sit inside the fire cone for `aimSettleMsec` and
  the view has to be slewing slower than `holdFireTurnRate` before the trigger
  is released. The cone is measured against the same point the bot is aiming at,
  including the lead — a gate that tests the target centre while the aim sits on
  the lead point produces a bot that aims correctly and then refuses to shoot.
- **Mistakes.** Rolled on acquisition and periodically while engaged. One of:
  lose tracking, mistime a shot, keep the wrong weapon, dodge late. A mistake is
  a bounded, readable failure rather than a silent accuracy penalty.

`bot_debugAim 1` logs the reaction, effective cone, settle, lead and whether a
shot is suppression fire, which is how the curve is tuned.

### Tactical personality traits

The skill curve controls execution while these traits control intent. Seven
stay flat and styles or characters move them independently. `initiative` and
`suppressionMsec` also make the low-skill baseline more willing to shoot:

| Trait | Range | Behavior |
| --- | --- | --- |
| `initiative` | 0..1 | How readily the bot accepts an imperfect shot. Higher values shorten reaction and settle time, widen its effective fire cone, and tolerate a faster view slew without improving where the aim points. Baseline skill 1 is 0.80; skills 4-5 are 0.50. |
| `targetStickiness` | 0..1 | How much better a new opponent must be before the bot abandons its current target. |
| `opportunism` | 0..1 | How strongly wounded opponents are preferred during target selection. |
| `vengefulness` | 0..1 | How strongly the bot favors the opponent who last killed it. |
| `suppressionMsec` | 0..2000 | How long it keeps shooting at the last believed position after an opponent crosses cover. The plain curve falls from 1200 ms at skill 1 to zero at skills 4-5. |
| `strafeRhythmMsec` | 100..3000 | Base time between changes of combat strafe direction. |
| `strafeRhythmVarianceMsec` | 0..3000 | Random spread added to that strafe rhythm, from clockwork movement to irregular footwork. |
| `weaponSwitchMsec` | 100..3000 | How often the bot reconsiders its weapon for the current range. |
| `aimHeight` | -0.4..0.4 | Personal vertical bias within the target bounds: low body, centre mass, or high chest. Skill accuracy still applies around it. |

These make profiles tactically legible. Gunner and Rhodes suppress doorways,
Sorg changes direction and weapons rapidly, Cortez holds one target and aims
high, Bagby fires readily but is slow to correct a poor weapon choice, and
Makron develops a persistent grudge.

### Styles

| Style | Wants to |
| --- | --- |
| `rusher` | Close to 220 units and stay there. Aggressive, impatient, short-range weapons, a wider cone because it is always moving. |
| `sniper` | Hold 1400 units and trade with hitscan. Patient, still, rail gun first, and useless in a corridor - which is the point. |
| `roamer` | Play the map. Near baseline everywhere, `itemFocus` up 40 %, fights what it meets on the way to the next item. |
| `hunter` | Pick one opponent and keep going. Long pursuit and a long re-acquisition window, so it holds a target through cover; barely interested in items. |
| `ambusher` | Sit on a choke point and wait. Very patient, cheap peripheral reaction and a quick trigger because it is already pre-aimed, and it dodges little. |
| `skirmisher` | Trade and leave. High mobility, jumps often, breaks off early, short bursts. |

### The roster

Sixteen characters ship, reusing the existing bot name pool so the cast still
looks like Quake 4. Each has a skill band, and `PickCharacter` prefers an unused
character whose band contains the current skill — falling back to the band, then
to anyone at all, because a full roster must never stop a bot being added.

| Character | Style | Band | Combat identity | Voice |
| --- | --- | --- | --- | --- |
| Voss | roamer | 3-5 | controls useful ground, protects his health and chooses threats carefully | calm field authority; disciplined, protective orders |
| Cortez | sniper | 3-5 | holds long angles, relocates deliberately and favors economical rail shots | courteous, composed sharpshooter with quiet confidence |
| Bidwell | rusher | 3-5 | applies disciplined forward pressure without throwing away his life | gruff, profane sergeant with little patience |
| Rhodes | ambusher | 3-5 | controls weak points with predictive rockets and grenades | warm Texas demolition swagger and craftsman's pride |
| Sledge | rusher | 4-5 | advances under deliberate mid-close heavy-weapons pressure | thoughtful, formal and dryly understated |
| Morris | roamer | 3-5 | moves aggressively through the rotation and decides quickly | fast, crude commentary backed by real competence |
| Strauss | ambusher | 2-4 | solves prepared lanes, guards equipment and retreats early | precise, vain technician whose temper breaks through |
| Tetzlaff | sniper | 1-3 | fixates on static rail shots but reads moving fights poorly | self-promoting range ego with an excuse for everything |
| Marsh | roamer | 2-4 | flexible generalist who takes useful fights without tunnel vision | warm, observant competitor who respects good play |
| Hollenbeck | roamer | 2-4 | controls resources, consolidates gains and abandons wasteful chases | practical field officer who turns setbacks into orders |
| Sorg | skirmisher | 2-4 | reads peripheral movement and rotates through evasive routes | restless pathfinder always reading the next opening |
| Anderson | skirmisher | 1-3 | mobile, item-aware support fighter who breaks off to survive | reassuring young medic, proactive under pressure |
| Gunner | rusher | 3-5 | maintains nailgun and grenade suppression while pursuing targets | translated Strogg combat reports: suppress and advance |
| Makron | hunter | 5-5 | identifies the strongest threat and pursues it relentlessly | imperious ruler who issues threats, not conversation |
| Kane | skirmisher | 4-5 | controlled mid-range survivor with unusually steady movement | sparse, plain human tactical speech |
| Bagby | roamer | 1-2 | nervous logistics runner who fires readily, aims low and is slow to correct a poor weapon choice | chatty, candid novice who learns out loud |

Band coverage is intentionally broad rather than even: levels 1 through 5 have
3, 7, 12, 12 and 9 preferred characters respectively. Anderson keeps a canon
human in the level-1 pool, while Makron remains exclusive to level 5. The band
only controls roster preference; the resolved baseline still determines how
well every selected character sees, aims, reacts and decides.

## The file format

Styles, character mechanics and character voices are plain text in brace
blocks, read with `idLexer` under `DECL_LEXER_FLAGS`. They are deliberately
*not* decl types: this needed no engine change, and those flags carry
`LEXFL_NOFATALERRORS`, so a malformed content file warns and is skipped instead
of killing a server.

```
content/baseoq4/pak0/botfiles/styles/<style>.style
content/baseoq4/pak0/botfiles/characters/<name>.bot
content/baseoq4/pak0/botfiles/chats/<name>.chat
```

These subdirectories and extensions are independent of the pre-existing Quake 3
format prototypes in `botfiles/bots/*.c`, `botfiles/items.c` and
`botfiles/weapons.c`. Nothing has ever parsed those old files, and the new
readers can never pick them up. Files are enumerated through
`fileSystem->ListFiles`, so a mod or a pk4 can add or replace styles, characters
or voice banks without a build step; anything dropped under
`content/baseoq4/pak0` is packed into `pak0.pk4` automatically.

The grammar:

```
style "<name>" {
    description  "<text>"
    inherit      "<other style>"        // optional; resolved after every file is read

    set    <trait> <number>             // trait  = number
    add    <trait> <number>             // trait += number
    scale  <trait> <number>             // trait *= number

    weapon "<weapon class>" <bias>      // 1.0 neutral, 0.0 last resort

    skill <1..5> { <same statements> }  // layered after this file's body
}

character "<display name>" {
    description  "<text>"
    inherit      "<style>"
    skillBand    <min> <max>
    model        "<playerModel decl>"   // non-team games
    modelMarine  "<playerModel decl>"   // team games, marine side
    modelStrogg  "<playerModel decl>"   // team games, strogg side

    set/add/scale <trait> <number>
    weapon "<weapon class>" <bias>
    skill <n> { ... }
}

characterChat "<display name>" {
    chat <event> {
        "line"
        "line"
    }

    reply <rule name> {
        priority  <0..100>
        source    any | player | bot
        addressed either | required | forbidden

        trigger "<word or phrase>"
        trigger "<another phrase>"

        "response line"
        "another response"
    }
}
```

Nothing in a content file is allowed to be fatal. An unknown key warns and is
skipped, and so does an unknown trait name or an out-of-range `skill <n>`. An
unresolved `inherit` warns and leaves that layer empty. A `characterChat` owner
is matched to the loaded character name case-insensitively; an owner with no
matching character warns and is ignored rather than creating a half-character.
The shipped `.bot` files contain mechanics and models only, and every shipped
voice line lives under `botfiles/chats`.

Two chat errors are rejected at *load* rather than silently vanishing later: a
line longer than `BOT_CHAT_MAX_LEN` (160 characters, because the broadcast path
drops the whole line at 240 once the decorated name is added), and a line
beginning with `#`, because chat text is passed through
`common->GetLocalizedString` and `#str_` would be substituted out from under the
author.

A style, abbreviated from the shipped `sniper.style`:

```
style "sniper" {
	description	"holds range and trades with hitscan"

	set	combatRange		1400
	set	aggression		0.28
	set	patience		0.85

	// Takes its time and will not shoot at anything it is not sure of.
	scale	aimSettleMsec		1.30
	scale	fireConeDeg		0.70

	weapon	"weapon_railgun"	2.00
	weapon	"weapon_shotgun"	0.35

	// A bad sniper is worse than a bad anything else: it stands still at
	// range and cannot hit.  Give the low levels back some willingness to move.
	skill 1 {
		scale	strafeChance	1.50
		scale	combatRange	0.75
	}
}
```

Character mechanics, abbreviated from `voss.bot`, contain no dialogue:

```
character "Voss" {
	inherit		"roamer"
	description	"calm field leader who values discipline over heroics"

	skillBand	3 5

	model		"model_player_marine_voss"
	modelMarine	"model_player_marine_voss"
	modelStrogg	"model_player_kane_strogg"

	// Holds useful ground, protects his health and will not turn one kill into
	// an undisciplined chase.
	set	combatRange		720
	scale	aggression		0.84
	add	patience		0.15
	scale	pursuit			0.75
	add	retreatHealth		0.08

	// His edge is judgment, not superhuman aim.
	scale	targetSelection		1.06

	// Talks rarely, but an order arrives while it can still matter.
	set	chatiness		0.30
	scale	chatDelayScale		0.90
}
```

The corresponding voice bank is `botfiles/chats/voss.chat`:

```
characterChat "Voss" {
	chat kill {
		"$other overcommitted. Don't do the same."
		"I had the angle."
	}
	chat death {
		"Good shot. I gave you the angle."
	}
}
```

## Chat

Chat is event driven and entirely server-side. A line goes out through
`idMultiplayerGame::ProcessChatMessage` with the bot's own client number, which
is the same call the server makes for a human's `say`, so the result is
indistinguishable from a player typing. Routing it through the `say` command
instead would make every bot speak as the host, and hooking the obituary would
make bots silent on a dedicated server, because that path's local branch never
runs there.

Dialogue is character-owned without being mixed into the mechanics file. Each
`botfiles/chats/*.chat` file names its owner in a
`characterChat "<display name>"` header. Chat banks load after the character
definitions and merge by that name case-insensitively, so the filename is a
useful convention rather than the identity key.

The events are: `entergame`, `levelstart`, `kill`, `killGauntlet`,
`killStreak`, `revenge`, `death`, `deathAccident`, `itemDenied`, `leadTaken`,
`leadLost`, `matchWin`, `matchLose`, `farewell`, `order`. A character with no
lines for an event simply says nothing — silence is a valid answer and nothing
invents a fallback line. `order` is the acknowledgment of a team order (see
"Team orders"); it goes to team chat, skips the chattiness roll so the speaker
knows the order landed, and still respects the flood throttle.

Every shipped character carries eight alternatives for each event: 120 lines
per character and 1,920 across the roster. This is a content target, not a
parser requirement; add-on characters may use smaller banks or intentional
silence. The uniform depth keeps losses, lead changes and departures from
becoming more repetitive than kills.

### Triggered replies

The same voice file may also contain `reply` rules. They let a bot answer
recognisable ideas in typed player chat and ordinary event-driven bot chat
without putting dialogue or phrase lists in C++. The shipped rules cover
requests for help, post-match sportsmanship, challenges, greetings, thanks,
praise, apologies and farewells. A ninth low-priority rule lets each character
answer when addressed by name. These are deliberately broad conversational
signals, not a general language model.

Incoming text is stripped of Quake 4 colour escapes, folded to lower-case and
split at punctuation. Triggers are normalised the same way and match only
contiguous whole words: `hi` matches `Hi, Voss`, but never the `hi` inside
`this`. Rules first filter on `source` and `addressed`; the highest `priority`
then wins, followed by the longest matching phrase; file order breaks a
remaining tie. The selected response cannot immediately repeat. A directly
named bot is preferred as the responder. Otherwise at most one eligible bot
answers, so one greeting cannot make the whole server speak.

`source` accepts `any`, `player` or `bot`. `addressed` accepts `either`,
`required` or `forbidden`; a bot is addressed when its display name appears as
whole words in the message. Each character may contain at most 32 reply rules,
with up to 32 triggers and 32 responses in each. Triggers are limited to 64
characters after normalisation. Priorities are clamped to
0 through 100. Invalid values, oversized rules and unknown keys warn and remain
contained to the affected setting, trigger, line or rule rather than preventing
the server from starting.

Replies use only `$self`, `$other` and `$map`: the speaker and map are always
known, while `$other` is filled from the server's trusted player name rather
than from chat-packet display text. Unknown or unavailable reply tokens are
rejected when the bank loads.

Responses keep all normal chat safeguards. A bot with a line already queued
does not lose it to a reply. Replies use the character's typing delay and the
existing per-bot and global throttles, plus a per-speaker cooldown that prevents
one chatter from rotating through the whole roster. Unaddressed bot-to-bot
responses are intentionally much rarer than replies to people. A generated
reply is explicitly marked in the send path and cannot trigger another reply;
voice macros are excluded too, so acknowledgement loops cannot form.

Global chat may choose any bot. Team chat can choose only a bot on the
speaker's team and the response remains team-only. Spectator-only chat, server
console text and malformed or rejected messages never reach reply matching.

Each of the fifteen normally talkative characters ships four alternatives for
each of the nine reply rules. Kane retains two terse alternatives per rule.
That adds 558 reactive lines to the 1,920 event lines, for 2,478 authored lines
across the roster.

Lines may use `$self`, `$other`, `$weapon`, `$map` and `$item`. A line requiring
a token that is unavailable for its event is skipped, preventing a partial or
mangled sentence from reaching chat. `$map` is supplied for every event;
opponent and weapon names come from combat events, `$item` is specific to
`itemDenied`, and in `order` `$other` is the player who gave it. Match-end events do not identify the winner or loser as `$other`.
Selection is uniform over the usable lines excluding whichever was used last,
so a line never repeats back to back.

The send delay and throttles keep the chatter believable and safe:

- A bot never speaks on the frame of the event. It pauses for `bot_chatDelay`,
  then waits for the visible line length at `bot_chatCPM` characters per
  minute. Formatting escapes do not count as typed characters. The whole delay
  is scaled by the bot's `chatDelayScale`, which is worse at low skill, given a
  little jitter, and capped at five seconds so even a long line remains brief.
  During that interval Quake 4's stock chat icon appears above the bot just as
  it does above a human player composing a message; no replacement material is
  required.
- Throttles, per bot and server wide. This is not politeness: **the engine has
  no chat flood protection anywhere**, and `idAsyncServer::SendReliableMessage`
  drops any client whose reliable queue overflows, so unbounded bot chatter can
  kick real players off a server. `bot_chat 2` halves both throttles and makes
  every bot more talkative; `bot_chat 0` silences them completely.

Team chat is only ever used in a team game. `ProcessChatMessage` does not check
that itself, and `team` set in a deathmatch colours everyone as Marine and
restricts delivery by the team field.

## Adding a character

1. Write `content/baseoq4/pak0/botfiles/characters/<name>.bot`. Start from an
   existing one; the only required keys are the `character "<name>"` header and
   an `inherit` naming a style that exists. Keep this file to models, selection
   metadata and play-style modifiers.
2. Set `skillBand` to the range the character is meant to be picked for. A
   character out of band is only used as a last resort, so a band of `5 5` means
   "this one shows up when the server is set to hard".
3. If it names a model, the name must be a real `playerModel` decl, and in a
   team game its decl team has to match the side it spawns on — hence the
   separate `modelMarine` and `modelStrogg` keys. `model_player_tactical_elite`,
   `model_player_tactical_command` and `model_player_marine_tech` declare no
   team and are legal on either side. An unknown model name makes the player
   rewrite its own user info every update.
4. Bias only what makes the character distinct. The style already covers how it
   plays and the skill curve already covers how well; a `.bot` file that sets
   twenty traits is fighting both.
5. Write `content/baseoq4/pak0/botfiles/chats/<name>.chat` with a
   `characterChat "<name>"` header. The owner name is matched
   case-insensitively to the character header; matching filenames keep the pair
   easy to find but are not used for ownership.
6. Add lines for as many events as the voice supports. Keep them short, in
   character and inoffensive. The shipped roster uses eight alternatives for
   all fourteen events; add-ons may deliberately use fewer or stay silent. Add
   `reply` rules when the voice should recognise typed phrases. Prefer
   whole-word phrases over single common words, use `addressed required` for a
   name-only fallback, and make every response sensible without knowing more
   than the rule's conversational intent.
7. `ninja -C builddir pak0.pk4` to repack, or run from a loose `fs_devpath`
   tree, then `botreload` in the console. It rereads styles, character mechanics
   and chat banks without a restart or map change.

## Commands

| Command | Effect |
| --- | --- |
| `addbot [name] [skill] [exact]` | Add one bot. Picks an unused name and character if none is given. The optional skill is a per-bot override that bypasses `bot_skill` and `bot_skillVariance`; `exact` requires the named character and is intended for authored matches. Builds the navmesh on first use. |
| `removebot [name]` | Remove one bot, by name or the last one added. |
| `kickbots` | Remove every bot. |
| `botlist` | List the bots with their character, style and effective skill, and the state of the navmesh. |
| `botcharacters` | List every loaded character with its style, skill band and which bot currently has it. |
| `botreload` | Re-read style, character and chat files without a map change. Bots keep the character they are wearing where the name survives. |
| `navmesh build` | Rebuild the navmesh, picking up a changed `bot_navCellSize`. |
| `navmesh info` | Report node, link and build-time counts. |

## Cvars

| Cvar | Default | Effect |
| --- | --- | --- |
| `bot_enable` | `1` | Allow bots to be added at all. |
| `bot_minPlayers` | `0` | Top the match up to this many players with bots. `0` disables. |
| `bot_skill` | `3` | 1 (harmless) to 5 (unpleasant). Selects the baseline row every trait is read from - vision, reaction, aim steadiness, trigger discipline, decision quality. See "What each skill level feels like". |
| `bot_skillVariance` | `0` | Spread skill up to this many levels either side of `bot_skill`, per bot, so a match is not all one difficulty. The roll is seeded from the client number, so it is stable for the life of that bot. |
| `bot_characters` | `1` | Use the character files. `0` leaves every bot on the plain skill curve with a name from the pool and nothing to say. |
| `bot_forceCharacter` | `""` | Put every bot on this one character, by name. A tuning knob, deliberately not archived - a server that saved it would field a roster of clones. |
| `bot_chat` | `1` | `0` silent, `1` normal, `2` chatty and with chat throttles halved. Applies to event lines and triggered replies. |
| `bot_chatDelay` | `600` | Initial thinking pause before the length-based typing delay, in milliseconds. |
| `bot_chatCPM` | `900` | Base typing speed in visible characters per minute. Character delay scales and a five-second cap keep the result varied but brief. |
| `bot_debug` | `0` | `1` logs navigation events, `2` adds a periodic per-bot status line. |
| `bot_debugNav` | `0` | `1` draws the navmesh near the local player, `2` adds each bot's current route. |
| `bot_debugAim` | `0` | Log the reaction, settle and lead values behind every shot. |
| `bot_profile` | `0` | Every N seconds print one `botprofile:` line: bot think time (average and worst frame), path searches and their failures by cause (snap, search, final leg), A* expansions per search, goal-pricing searches, and navigation milliseconds per frame with the goal-pricing share. |
| `bot_navCellSize` | `24` | Sampling resolution in world units. Smaller finds more ground and costs more to build. |
| `bot_pause` | `0` | Freeze all bot input. |

Bots are server-side only. Everything a server operator is expected to set is
archived - `bot_enable`, `bot_minPlayers`, `bot_skill`, `bot_skillVariance`,
`bot_characters`, `bot_chat`, `bot_chatDelay` and `bot_chatCPM` - so a dedicated
server config can set them once. That is not cosmetic: a command-line `+set`
never reaches a `CVAR_GAME` cvar, because the game module registers it after
the engine has parsed the command line, so only archived values survive.

## The create-server menu

`bot_minPlayers` and `bot_skill` are the two settings a listen-server host is
expected to touch, so both are rows in **Advanced Server Options** on the
multiplayer create-server screen - `pop_createAdv_botFill_val` and
`pop_createAdv_botSkill_val` in `guis/mainmenu.gui`. Both are `choiceType 1`
widgets, which write the raw string from their `values` list rather than an
index, so the list has to stay inside the range the cvar declares.

That menu is reachable while `game_sp` is loaded - the module is not swapped
until `spawnServer` - and `idChoiceWindow::InitVars` resolves its cvar exactly
once, when the gui is parsed. A `CVAR_GAME` cvar the loaded module never
declared is simply absent, and the widget warns and then does nothing for the
rest of the session. `Session_DeclareMultiplayerMenuGameCVars` in
`src/framework/Session.cpp` therefore declares both, with `game_mp`'s own
defaults, immediately before the main menu gui is loaded. `idInternalCVar::Update`
keeps the current value when a static declaration arrives later and only adopts
its reset value and range, so the module swap at `spawnServer` inherits whatever
the host picked. Anything else added to that popup that lives in `game_mp` needs
the same treatment; `tools/tests/mp_bot_server_menu.py` pins it.

## Arena Campaign orchestration

The single-player Arena Campaign drives the multiplayer bot runtime through a
private local match. Its authored ladder is
`content/baseoq4/pak0/arena/openq4_campaign.cfg`: a versioned, lexer-friendly
manifest containing five tiers of four matches. Each match names a stock map,
one combat game type, its mode limits, a base skill, and an explicit character
roster with per-bot skill offsets.

The roster is intentionally explicit instead of using `bot_minPlayers`.
Campaign matches must be repeatable: the same card always produces the same
characters and effective skills, while the ordinary automatic-fill behavior
remains available for multiplayer servers. Effective skill is the tier's base
skill plus the campaign difficulty adjustment and character offset, clamped to
the normal 1-to-5 bot range. Team matches list five bots so the local player and
roster can form balanced three-player sides.

Progression follows the Quake 3-style ladder contract. Match indexes 0 through
2 begin unlocked inside an available tier. Winning all three unlocks index 3,
the tier boss. A boss win unlocks the next tier, and the final boss completes
the campaign. Losses never remove wins or locks; reset is a separate confirmed
menu action. Arena progress is independent of Mission savegames and ordinary
server configuration.

Only kill-driven modes appear in the shipped ladder: Duel, Deathmatch, Team
Deathmatch, Red Rover, and Clan Arena. All selected maps advertise DM or Team
DM in the stock map declarations, which also supplies the layout for openQ4's
derived modes. This is an authored campaign-scope choice, not a general bot
limitation: ordinary multiplayer bots now pursue Standard CTF, One Flag, Arena
CTF, Arena One Flag, Freeze Tag rescues and DeadZone play. Adding one of those
to Arena still requires a deliberate card, roster, limit, progression and
result-flow pass for that mode.

## Known limits

- Ladders and lifts are not modelled. No stock multiplayer map routes through
  either, but a custom map that needs one to connect its levels leaves those
  levels unreachable to bots.
- Role allocation compares landmark route-cost lower bounds rather than full
  routes, with a margin for bots already filling a role. It can mis-rank runs
  where those bounds are loose, and falls back to straight-line distance when
  a target or player has no usable navigation node. Teammate pickup claims
  count existing commitments and reduce utility; they do not rank travel times.
- `FindNearestNode` spends at most `NAV_MAX_WALKABLE_PROBES` collision probes
  when asked to prove walkability. A bot standing somewhere that genuinely needs
  more than that to escape falls back to wandering rather than stalling the
  server frame.
- Generation past `NAV_MAX_NODES` stops adding ground but still finishes linking
  what it has, so the graph is smaller rather than broken. A cell size fine
  enough to trip the limit still leaves part of the map unnavigable, and the
  warning says so.

- Deliberate objective play currently covers Standard CTF, One Flag, Arena CTF,
  Arena One Flag, Freeze Tag rescue and DeadZone. Attack & Defend, Overload,
  Harvester and Domination have descriptors or borrowed layouts but not the
  complete authoritative runtime state and scoring rules required for honest
  objective decisions. Their objective query therefore fails closed and bots
  deliberately fall back to combat, inventory pickups and directed roaming.
- A pad's landing is simulated with the same air steering the bot flies with,
  but at a fixed frame step and without other players in the way, so a flight
  can still come down somewhat off its node. It ends when the bot is back on
  the ground, and off-route recovery re-routes from wherever it landed.
- Enemy knockback and in-flight player collisions can still carry a bot into
  a void. Corridor occupancy reduces preventable launches, but does not
  arbitrate simultaneous grounded entries at opposite ends. The health guard
  prices each landing using current health and armour; damage received after
  takeoff can still make that committed landing fatal.
- Routed jump qualification conservatively refuses unsettled inward ground
  velocity instead of approximating the player's slope-slide adjustment.
  Command-byte rounding and changing speed powerups remain small differences
  from the fixed-step flight model. A failed live launch may require a new
  approach; the bot does not construct a new run-up while standing on the lip.
- The navmesh is rebuilt at every map load rather than cached on disk.
  Generation runs once per map and is lazy, so a server without bots never pays
  it.
- `bot_debugNav` uses the renderer's fixed 16384-entry debug-line pool. Its draw
  is budgeted to stay inside that pool, but nothing on the server path calls
  `DebugClear`, so lines accumulate rather than expire.
- A character's preferred team is only ever a hint. With `si_autoBalance` on,
  the server overrides the requested side whenever the teams are uneven, and a
  team switch kills and respawns the player.

## Testing

Everything below runs in-tree and in CI with no game assets; the compiled
fixtures need a C++17 compiler (`clang++` or `g++`, or `CXX`).

- `tools/tests/mp_bot_navigation.py` pins the agreements that make navigation
  work and that the compiler cannot check: the engine handing back the
  allocated slot, bots re-begun after a map change, user info restored on every
  update, bots thinking before entities, and the navmesh deriving its agent
  from the movement cvars. It guards the body-free agent sweep, walk-door
  classification, transport sealing and the walk slope bound, weighted route
  costs, the landmark heuristic and goal pricing, dynamic-only edge validation
  and repair, roaming retries, off-route recovery, the drop push-through, and
  the action entry/airborne/grounded-exit contract.
- `tools/tests/mp_bot_navmesh_search.py` splices the search core - components
  and their closure, landmark selection, the heuristic, both cost fields and
  pair pricing - into a native fixture and proves it on random two-floor graphs
  with one-way drops, cheap transports and sealed pits: the closure equals
  breadth-first reachability on 6,000 pairs, pair pricing equals the exact
  route cost, the heuristic never overestimates and is consistent on every
  edge, and ranked goal pricing returns exactly the exhaustive field's best four
  in 180 runs while expanding 6% of its nodes.
- `tools/tests/mp_bot_jump_trajectories.py` executes production hull construction,
  support, floor, step and jump proof against an independent convex sweep
  fixture using the engine's cylinder vertices. It rejects the
  narrow lower ledge and island cases that a vertical landing projection
  accepted, unsupported corner/rim nodes and landings, lethal floors, and
  raised lips whose running air step overshoots. Broad raised and lower floors,
  normal steps and gaps, a 24-unit platform, a floor seam and a 45-degree
  slope provide positive cases. Discretionary hop cases cover a platform where
  walking is safe but jumping overshoots, opposing initial momentum, and
  lethal/transport landings. The former proof fails the narrow 280-unit descent
  regression. Landing-impact cases cover descending jumps, drops and pads,
  with the same no-damage surface and water reductions as player physics.
  Running-drop cases reject small islands and preserve broad lower floors;
  landing continuation cases distinguish connected ground from a gap or rim,
  with low-ceiling, small-lip and boosted-hop regressions.
- `tools/tests/mp_bot_regressions.py` compiles the production decision
  functions against a deterministic world: goal replacement and stall
  recovery, inventory changes, hearing and item timing, fight-or-flight,
  arming and duty pickups, perception against exhaustive target selection
  over 240 cases, one goal-pricing search for 80 pickups, grenade bounce
  threats, bot message queues that never accumulate, and pad holds that brake
  through reserved entry, retain real flights and reconsider blocked routes,
  plus adoption of observed unplanned pad launches without replacing goals.
  Landing-health cases cover the live damage service and armour, immunity,
  alternate pickup routes, cached pre-entry checks and preserved flights.
  Production movement projection and player command scaling are checked
  together at 0, 45 and 90 degrees, including partial input and zero movement.
  Transition cases cover transverse jump/drop approaches, unsafe actual landing
  continuations, same-direction pad followers and pad completion only on
  grounding or an explicit chained handoff.
  Hop ownership cases change goals, view yaw and speed boosts during flight,
  then verify release on landing, failed launch, death, expiry and pad transfer.
  Recovery cases exercise both edge-guard branches, inland rescue from a
  marginal footprint, and actual command yaw with a nonzero view delta.
  Actual box/cylinder rim fixtures distinguish continuous inland rescue from
  unsupported tangency, a distant floor across a gap, walls and hazards.
- `tools/tests/mp_bot_team_orders.py` runs the team-order parser on phrasings
  that must and must not be orders and pins who may give them, when they
  lapse, and how every mode's planner reads them.
- `tools/tests/mp_bot_intelligence.py`, `mp_bot_goal_contracts.py`,
  `mp_bot_combat.py`, `mp_bot_objectives.py` and `mp_bot_team_balance.py` pin
  goal selection, weapon-aware spacing and pursuit, splash and trajectory
  safety, threat prediction, CTF/Freeze Tag/DeadZone roles with incumbency,
  and pre-spawn team reservations.
- `tools/tests/mp_bot_characters.py` covers the personality layer: the trait
  table and baseline curve against `botTraits_t`, monotonic skill curves, and
  every shipped `.style`, `.bot` and `.chat` file, including exactly eight
  unique lines per chat event per character and the tokens each event may use.

**Live audit.** `tools/tests/mp_bot_audit.py --basepath <retail-install>` runs
a hidden listen server with a fixed roster for a fixed time and reduces the log
to the numbers that describe a bot implementation: kills, deaths to the map,
navigation failures by kind (no route, stuck, off route, abandoned goals,
timed-out traversals), the `bot_profile` cost rows, how close the simulation
kept to real time, and process CPU. `--map`, `--gametype`, `--bots` and
`--skill` choose the match; `--team-order "everyone follow me"` has the
observer give an order and reports its team-mates' distance to it. Use the
optimized `builddir-perf` runtime for absolute costs; `builddir` is a debug
build and is several times slower.
The report records SHA-256 identities for the client, MP module and runtime
packs and refuses a stage older than `builddir`. `--debug 2` includes traversal entry positions
and velocities; `--max-world-deaths 0` turns any complete-run death to the map into
an audit failure. A default audit remains a measurement run, so its successful
exit alone does not establish safe navigation. A fatal engine-log error stops
the audit's own hidden client promptly, including a content-pack mismatch.
Timed rates retain their measurement window; warmup and complete-run world
deaths are separate report fields and both contribute to the safety gate.

On `mp/q4dm5` with eight skill-4 bots in a debug build, the October 2026 pass
took the server from 0.32 to 0.96 of real time, stuck reports from 29 to 3-4
per two minutes, failed path searches from about 10% to under 2%, and goal
pricing from about 7,600 settled nodes per decision to about 2,200 expanded,
with no deaths to the map.

That single-map result does not qualify every stock map. The subsequent
32-map sweep found world deaths on q4dm8, q4ctf3, q4ctf5 and q4dz3. Follow-up
six-bot, 45-second audits exposed hull/support mismatches, unsafe upward hops,
pad steering ownership, low-health landing risk and unsafe landing handoffs.
The landing candidate recorded one warmup and four measured world deaths;
the warmup fall revealed why qualification must include the complete run.

The subsequent control candidate completed all four affected maps for 45
seconds each, with one measured world death per map and no warmup deaths or
traversal timeouts. All engine processes exited normally; the strict safety
audit failed. The DM8 fall followed a documented enemy impulse. CTF3 exposed
a routed jump launched with large transverse momentum outside its graph
proof. CTF5's fall followed unclassified non-enemy damage; the log cannot
separate self/team impulse from steering. DZ3's low-health hard-fall death
strongly matches an accidental launch from an active pad omitted from the
transport registry. The exact-state jump gate and unroutable-pad sealing
corrections pass all 12 suites, including production registry/sealing fixtures;
their staged rerun and the final 32-map sweep remain pending. No cross-map
safety or optimized-performance qualification is claimed. These four debug
maps built navigation in 986–3,267 ms and ran at 0.963 of real time; these are
diagnostic observations, not controlled performance comparisons.

For a live look, run a listen server with `bot_debug 2` and read the status
lines: each carries the bot's position, health, armour, goal, path progress,
current enemy and whether it is firing. Damage and voluntary-jump lines include
position, velocity and game time so a fall can be traced to the preceding
input or hit. Positive self/team/world damage is included; zero-damage
impulses are not forwarded by the player damage callback. `bot_debugNav 2` draws the mesh and
every bot's route near the local player, and `bot_debugAim 1` logs the
reaction, settle and lead behind every shot.

`tools/tests/mp_bot_gameplay_smoke.py` remains the repeatable stock-gameplay
check for DM, Team DM and CTF: live-match admission, bot movement, firing,
damage/scoring and CTF objective decisions, with bot message-queue overflow a
failure.

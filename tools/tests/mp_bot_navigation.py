#!/usr/bin/env python3
"""Guards the multiplayer bot's navigation contract.

openQ4 multiplayer bots occupy real client slots and are driven by user commands
the game writes into the engine's own array.  That only works while a handful of
non-obvious agreements hold, none of which the compiler can check:

  * The engine hands back the client slot it allocated for a bot.  It used to
    return a bare 1, which is indistinguishable from client 1.
  * Bots are re-begun after a map change.  A remote client announces itself with
    CLIENT_RELIABLE_MESSAGE_INGAME; a bot has no remote end to do that.
  * A bot's user info is restored on every update.  The engine broadcasts a bot
    with nothing but a name, and an idPlayer without ui_autoJoin sits in the
    join menu as a spectator forever.
  * Bots think before any entity does, so their commands are in place when the
    players read them.
  * Navigation is generated from the collision world with the player's own
    bounding box and step height, which is what makes it work on maps that have
    no .aas file - which is every stock Quake 4 multiplayer map.
  * A non-walk link retains both its entry and exit and is completed from
    grounded traversal state, not merely because its adjacent grid corner is
    within the ordinary steering radius.
  * Item and roam goals are reachable in the requested direction; weak
    connected areas cannot make an upward-only ledge look reachable.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME_LIBS_ROOT = Path(os.environ.get("OPENQ4_GAMELIBS_REPO", ROOT)).resolve()


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def require_order(haystack: str, first: str, second: str, context: str) -> None:
    a = haystack.find(first)
    b = haystack.find(second)
    if a == -1:
        raise AssertionError(f"Missing {first!r} in {context}")
    if b == -1:
        raise AssertionError(f"Missing {second!r} in {context}")
    if a > b:
        raise AssertionError(f"{first!r} must appear before {second!r} in {context}")


def cvar_signature(source: str, name: str, context: str) -> tuple[str, set[str]]:
    match = re.search(
        rf'idCVar\s+\w+\(\s*"{re.escape(name)}"\s*,\s*"([^"]*)"\s*,\s*([^,]+),',
        source,
        re.IGNORECASE,
    )
    if match is None:
        raise AssertionError(f"Missing CVar declaration for {name!r} in {context}")
    return match.group(1), {flag.strip() for flag in match.group(2).split("|")}


def validate_engine() -> None:
    source = read(ROOT / "src" / "framework" / "async" / "AsyncServer.cpp")

    # The allocator has to report which slot it took.
    alloc = source[source.index("int idAsyncServer::AllocOpenClientSlotForAI") :]
    alloc = alloc[: alloc.index("\n}\n") + 3]
    require(alloc, "return botClientId;", "idAsyncServer::AllocOpenClientSlotForAI")
    if re.search(r"^\treturn 1;$", alloc, re.MULTILINE):
        raise AssertionError(
            "AllocOpenClientSlotForAI returns a bare 1 again; the game cannot tell which slot it got"
        )

    # Bots have to be put back into the game after a map change.
    change = source[source.index("void idAsyncServer::ExecuteMapChange") :]
    change = change[: change.index("\nint idAsyncServer::GetPort")]
    require(change, "botClient[MAX_ASYNC_CLIENTS]", "idAsyncServer::ExecuteMapChange")
    require(change, "NA_BOT", "idAsyncServer::ExecuteMapChange")
    require(change, "game->ServerClientBegin( i, true,", "idAsyncServer::ExecuteMapChange")
    # The name is captured before InitClient wipes the user info that holds it.
    require_order(
        change,
        "botClientName[i] = sessLocal.mapSpawnData.userInfo[i].GetString",
        "InitClient( i, clients[i].clientId",
        "idAsyncServer::ExecuteMapChange",
    )

    # And nothing may try to talk to a bot over the network.
    for guard in (
        "void idAsyncServer::SendReliableMessage",
        "void idAsyncServer::CheckClientTimeouts",
    ):
        block = source[source.index(guard) :][:2000]
        require(block, "NA_BOT", guard)


def validate_game_wiring() -> None:
    mp = GAME_LIBS_ROOT / "src" / "mpgame"
    if not mp.is_dir():
        print(f"mp_bot_navigation: skipped game checks (no GameLibs checkout at {GAME_LIBS_ROOT})")
        return

    game_local = read(mp / "Game_local.cpp")

    # Bots fill their user command slots before anything reads them.
    require_order(
        game_local,
        "usercmds = clientCmds;",
        "botManager.Think();",
        "idGameLocal::RunFrame",
    )
    run_frame = game_local[game_local.index("gameReturn_t idGameLocal::RunFrame") :][:6000]
    require_order(run_frame, "botManager.Think();", "SetupPlayerPVS();", "idGameLocal::RunFrame")

    # The slot is claimed before the player entity is created, because
    # idPlayer::Spawn reads the user info.
    spawn = game_local[game_local.index("void idGameLocal::SpawnPlayer") :][:3000]
    require_order(
        spawn,
        "botManager.OnSpawnPlayer( clientNum, isBot, botName );",
        "SpawnEntityDef( args, &ent )",
        "idGameLocal::SpawnPlayer",
    )

    # Every user info update restores the bot's identity.
    set_user_info = game_local[game_local.index("const idDict* idGameLocal::SetUserInfo") :][:3000]
    require(set_user_info, "botManager.IsBot( clientNum )", "idGameLocal::SetUserInfo")
    require(set_user_info, "botManager.FillUserInfo(", "idGameLocal::SetUserInfo")

    require(game_local, "botManager.OnMapShutdown();", "idGameLocal::MapShutdown")
    require(
        read(mp / "Game_network.cpp"),
        "botManager.OnClientDisconnect( clientNum );",
        "idGameLocal::ServerClientDisconnect",
    )

    # The manager's own lifecycle. rvBotManager::Shutdown is what hands every
    # bot's character back, so it has to run before the roster is freed or each
    # live bot is left pointing at released memory.
    require(game_local, "botManager.Init();", "idGameLocal::Init")
    require(game_local, "botManager.Shutdown();", "idGameLocal::Shutdown")
    require_order(
        game_local,
        "botManager.Shutdown();",
        "botCharacterManager.Shutdown();",
        "idGameLocal::Shutdown character release ordering",
    )

    multiplayer_game = read(mp / "MultiplayerGame.cpp")

    # Kill and death reactions - chat, kill streaks, grudges - hang off this one
    # call, and it went unwired once already. Scores are committed by the time
    # statManager->Kill returns, so a bot reacting here sees the post-frag board.
    require(
        multiplayer_game,
        "botManager.OnPlayerDeath( dead, killer, methodOfDeath );",
        "idMultiplayerGame::PlayerDeath",
    )
    require_order(
        multiplayer_game,
        "statManager->Kill( dead, killer, methodOfDeath );",
        "botManager.OnPlayerDeath( dead, killer, methodOfDeath );",
        "bot death reaction reads a committed scoreboard",
    )

    # idGameLocal::RunFrame keeps advancing gameLocal.time through a competitive
    # pause while it skips rvBotManager::Think, so every absolute bot deadline
    # has to be carried forward with the clock exactly as the players' are.
    require(
        multiplayer_game,
        "botManager.ShiftMatchTime( deltaMsec );",
        "idMultiplayerGame::RebaseCompetitivePauseFrame",
    )

    bot = read(mp / "bots" / "Bot.cpp")

    # Without ui_autoJoin a spawning idPlayer parks itself in the join menu.
    for key in ('"ui_autoJoin"', '"ui_spectate"', '"ui_ready"', '"ui_name"'):
        require(bot, key, "rvBot::FillUserInfo")

    # Full movement speed needs the run button; idPlayer::AdjustSpeed otherwise
    # drops the bot to pm_walkspeed.
    require(bot, "cmd.buttons |= BUTTON_RUN;", "rvBot::ApplyMove")

    # The player rebuilds its view from the command plus its delta angles.
    require(bot, "ANGLE2SHORT( aimAngles[i] - self->GetDeltaViewAngles()[i] )", "rvBot::UpdateAim")

    # An unreachable moving goal must not cost a failed search every frame, and
    # rule objectives must be considered only after a committed traversal has
    # been allowed to finish.
    # Sliced to the function's closing brace rather than a fixed character
    # count, so the pin keeps covering the whole of UpdateGoal as it grows.
    goal = bot[bot.index("void rvBot::UpdateGoal") :]
    goal = goal[: goal.index("\n}\n") + 3]
    require(goal, "gameLocal.time - enemyPathTime > BOT_REPATH_COMBAT_MSEC", "rvBot::UpdateGoal")
    require(goal, "gameLocal.time < nextGoalSelectTime", "rvBot::UpdateGoal")
    require_order(
        goal,
        "traversalCorner == pathCorner",
        "BotFindObjective( self, objective )",
        "rvBot::UpdateGoal traversal commitment",
    )
    require(goal, "BOT_GOAL_SWITCH_MARGIN", "rvBot::UpdateGoal hysteresis")
    require(goal, "criticalHealth", "rvBot::UpdateGoal survival priority")
    require(goal, "BOTGOAL_OBJECTIVE", "rvBot::UpdateGoal mode objective")

    # Goal selection must return the successful directed route along with the
    # entity.  A weak connected-area match is only a rejection shortcut:
    # one-way ledges are not proof that the item can be approached from below.
    item_goal = bot[bot.index("idEntity *rvBot::PickItemGoal") :]
    item_goal = item_goal[: item_goal.index("\n}\n") + 3]
    require(item_goal, "navMesh.FindPath(", "rvBot::PickItemGoal")
    require(item_goal, "candidates[i].item->GetPhysics()->GetOrigin(), goalPath )", "rvBot::PickItemGoal")

    # A special link is an action.  q4dm1's crate links are only one 24-unit
    # cell apart but rise 32-48 units; the ordinary 28x72 proximity test used
    # to consume each landing before UpdateMovement could press jump.
    advance = bot[bot.index("bool rvBot::AdvancePath") :]
    advance = advance[: advance.index("\n}\n") + 3]
    require(advance, "actionEntry ? BOT_TRAVEL_ENTRY_REACH", "rvBot::AdvancePath")
    require(advance, "traversalStarted && self->pfl.onGround", "rvBot::AdvancePath")
    require(advance, "BotReachedTravelEnd(", "rvBot::AdvancePath")
    for travel in (
        "NAVTRAVEL_JUMP",
        "NAVTRAVEL_DROP",
        "NAVTRAVEL_JUMPPAD",
        "NAVTRAVEL_TELEPORT",
    ):
        require(advance, travel, "rvBot::AdvancePath")

    travel_end = bot[bot.index("static bool BotReachedTravelEnd") :]
    travel_end = travel_end[: travel_end.index("\n}\n") + 3]
    require(
        travel_end,
        "Min( travelDistance * 0.5f",
        "BotReachedTravelEnd destination-side progress",
    )
    require(
        travel_end,
        "idMath::Fabs( end.z - start.z ) <= zTolerance",
        "BotReachedTravelEnd same-floor progress guard",
    )
    require(advance, "BotTouchesTravelVolume(", "rvBot::AdvancePath volume entry")
    require(advance, "self->IsInTeleport()", "rvBot::AdvancePath teleporter activation")

    movement = bot[bot.index("void rvBot::UpdateMovement") :]
    movement = movement[: movement.index("\n}\n") + 3]
    require(movement, "corner.travelType != NAVTRAVEL_WALK", "rvBot::UpdateMovement")
    require(movement, "!committedTravel && foe", "rvBot::UpdateMovement")
    require(movement, "!committedTravel && gameLocal.time < unstickUntil", "rvBot::UpdateMovement")
    require(movement, "const bool routeProgress", "rvBot::UpdateMovement route progress")
    require(movement, "stuckPathCorner", "rvBot::UpdateMovement corner progress")
    require(movement, "stuckChecks >= 2", "rvBot::UpdateMovement")
    require(movement, "const bool hasMoveDir", "rvBot::UpdateMovement vertical action")
    require(movement, "transportInProgress", "rvBot::UpdateMovement authored transport progress")
    require(movement, "BOT_TRAVEL_TIMEOUT_MSEC", "rvBot::UpdateMovement traversal timeout")
    require(movement, "BotMoveDirectionSafe", "rvBot::UpdateMovement safe combat steering")
    require(movement, "gameLocal.time - noRouteSince", "rvBot::UpdateMovement no-route recovery")
    # A bot knocked or carried onto another floor replaces its route at once
    # instead of pressing toward a corner overhead until the stuck detector
    # fires, and the goal is decided again from where it now stands.
    require_order(
        movement,
        "bool routing = AdvancePath( self );",
        "routing && OffRoute( self, offHeight )",
        "rvBot::UpdateMovement off-route check follows route advance",
    )
    require(movement, "nextGoalSelectTime = Min( nextGoalSelectTime, gameLocal.time );",
            "rvBot::UpdateMovement off-route goal re-decision")
    # A drop landing under its own ledge lip must not collapse the steering.
    require(movement, "moveDir * across < BOT_CORNER_REACH", "rvBot::UpdateMovement drop push-through")
    off_route = bot[bot.index("bool rvBot::OffRoute") :]
    off_route = off_route[: off_route.index("\n}\n") + 3]
    require(off_route, "self->pfl.onGround", "rvBot::OffRoute grounded only")
    require(off_route, "path.RouteStart()", "rvBot::OffRoute first-leg start")
    require(off_route, "-BOT_OFF_ROUTE_BELOW", "rvBot::OffRoute fallen below the leg")
    require(off_route, "corner.travelType == NAVTRAVEL_WALK && offHeight > BOT_OFF_ROUTE_ABOVE",
            "rvBot::OffRoute above only on walks")
    require(advance, "corner.travelType == NAVTRAVEL_DROP &&", "rvBot::AdvancePath long-drop overshoot")
    safe = bot[bot.index("static bool BotMoveDirectionSafe") :]
    safe = safe[: safe.index("\n}\n") + 3]
    require(safe, "navMesh.TransportAt( swept ) >= 0", "BotMoveDirectionSafe refuses jump pads and teleporters")
    if "climb > pm_stepsize.GetFloat()" in movement:
        raise AssertionError(
            "UpdateMovement guesses jumps from corner height again; jump input must follow "
            "the route's explicit NAVTRAVEL_JUMP action"
        )

    jump = bot[bot.index("void rvBot::PressJump") :]
    jump = jump[: jump.index("\n}\n") + 3]
    require(jump, "self->pfl.onGround", "rvBot::PressJump")
    require(jump, "gameLocal.framenum + clientNum", "rvBot::PressJump")

    # Bots outlive a map change, the navmesh does not.
    require(bot, "!navMesh.IsValid() && NumBots() > 0", "rvBotManager::Think")

    # A kicked bot's slot has to come back: it never sends a packet, so the
    # engine's zombie timeout can never reap it on its own.
    server = read(ROOT / "src" / "framework" / "async" / "AsyncServer.cpp")
    timeouts = server[server.index("void idAsyncServer::CheckClientTimeouts") :][:2000]
    require(timeouts, "clientState == SCS_ZOMBIE", "idAsyncServer::CheckClientTimeouts bot reaping")
    require(timeouts, "clientState = SCS_FREE", "idAsyncServer::CheckClientTimeouts bot reaping")


def validate_navmesh() -> None:
    mp = GAME_LIBS_ROOT / "src" / "mpgame"
    if not mp.is_dir():
        return

    nav = read(mp / "bots" / "NavMesh.cpp")

    # The agent is the player, taken from the cvars the server is running.
    agent = nav[nav.index("const idBounds &rvNavMesh::GetAgentBounds") :][:800]
    require(agent, "pm_bboxwidth.GetFloat()", "rvNavMesh::GetAgentBounds")
    require(agent, "pm_normalheight.GetFloat()", "rvNavMesh::GetAgentBounds")

    # Generation must not see other players as walls.
    mask = re.search(r"#define\s+NAV_CLIP_MASK\s+\(([^)]*)\)", nav)
    if mask is None:
        raise AssertionError("NAV_CLIP_MASK is not defined in NavMesh.cpp")
    if "CONTENTS_BODY" in mask.group(1):
        raise AssertionError(
            "NAV_CLIP_MASK includes CONTENTS_BODY; a player standing in a doorway would "
            "erase the doorway from the navmesh"
        )
    for flag in ("CONTENTS_SOLID", "CONTENTS_PLAYERCLIP"):
        require(mask.group(1), flag, "NAV_CLIP_MASK")

    # Traversal mirrors what idPhysics_Player will actually allow.
    step = nav[nav.index("bool rvNavMesh::TryStep") :][:3000]
    require(step, "pm_stepsize.GetFloat()", "rvNavMesh::TryStep")
    require(step, "pm_jumpheight.GetFloat()", "rvNavMesh::TryStep")

    # Closed proximity doors are usable traversal, not permanent holes in a
    # graph that happens to be generated before the first player approaches.
    # Every probe sweeps the world and solid entities but never a body: MP
    # players carry CONTENTS_SOLID for their combat box, and a bot's own box
    # overlaps every sweep that starts where it stands.
    door_trace = nav[nav.index("static bool NavAgentTranslation") :]
    door_trace = door_trace[: door_trace.index("\n}\n") + 3]
    require(door_trace, "CONTENTS_BODY | CONTENTS_CORPSE", "NavAgentTranslation ignores bodies")
    require(door_trace, "idActor::GetClassType()", "NavAgentTranslation ignores actors")
    require(door_trace, "GetWorldCollisionModel()", "NavAgentTranslation world sweep")
    require(door_trace, "NavDoorIsWalkActivated", "NavAgentTranslation usable door filter")
    door_filter = nav[nav.index("static bool NavDoorIsWalkActivated") :]
    door_filter = door_filter[: door_filter.index("\n}\n") + 3]
    require(door_filter, "idDoor::GetClassType()", "NavDoorIsWalkActivated")
    require(door_filter, "IsLocked()", "NavDoorIsWalkActivated locked door")
    require(door_filter, "IsNoTouch()", "NavDoorIsWalkActivated scripted door")
    require(door_filter, 'GetString( "requires"', "NavDoorIsWalkActivated required door")
    require(step, "NavAgentTranslation", "rvNavMesh::TryStep door traversal")

    # Off-mesh links are what keep a Quake 4 multiplayer map one component.
    links = nav[nav.index("void rvNavMesh::AddOffMeshLinks") :][:2500]
    require(links, "rvJumpPad::GetClassType()", "rvNavMesh::AddOffMeshLinks")
    require(links, "idTrigger_Multi::GetClassType()", "rvNavMesh::AddOffMeshLinks")
    require(links, "idPlayerStart::GetClassType()", "rvNavMesh::AddOffMeshLinks")

    # Smoothing may never skip a link that has to be entered deliberately.
    pull = nav[nav.index("void rvNavMesh::StringPull") :]
    pull = pull[: pull.index("\n}\n") + 3]
    require(pull, "!= NAVTRAVEL_WALK", "rvNavMesh::StringPull")
    require_order(
        pull,
        "path.Append( source, NAVTRAVEL_WALK );",
        "path.Append( nodes[nodePath[i + 1]].origin, immediateTravel,",
        "rvNavMesh::StringPull action entry and exit",
    )
    require(pull, "LinkActionEntity(", "rvNavMesh::StringPull volume identity")

    # Pads and teleporters carry whoever touches them, so the ground under a
    # live volume keeps the transport as its only exit; no route walks across
    # one, starts or ends on one, or shortcuts into one.
    build = nav[nav.index("bool rvNavMesh::Build") :]
    build = build[: build.index("\n}\n") + 3]
    require_order(build, "AddOffMeshLinks();", "SealTransportVolumes();", "rvNavMesh::Build transport sealing")
    require_order(build, "SealTransportVolumes();", "BuildComponents();", "rvNavMesh::Build sealing before closure")
    seal = nav[nav.index("void rvNavMesh::SealTransportVolumes") :]
    seal = seal[: seal.index("\n}\n") + 3]
    require(seal, "standing.IntersectsBounds( volume )", "rvNavMesh::SealTransportVolumes standing box test")
    require(seal, "nodes[n].flags |= NAVNODE_TRANSPORT;", "rvNavMesh::SealTransportVolumes flag")
    require(seal, "nodes[n].firstLink = -1;", "rvNavMesh::SealTransportVolumes exits")
    require(seal, "transport.destination", "rvNavMesh::SealTransportVolumes transport exit")
    require(links, "IsActive( TH_THINK )", "rvNavMesh::AddOffMeshLinks only live pads seal")
    nearest = nav[nav.index("int rvNavMesh::FindNearestNode") :]
    nearest = nearest[: nearest.index("\n}\n") + 3]
    require(nearest, "nodes[i].flags & NAVNODE_TRANSPORT", "rvNavMesh::FindNearestNode never snaps onto a pad")
    walkable = nav[nav.index("bool rvNavMesh::WalkableLine") :]
    walkable = walkable[: walkable.index("\n}\n") + 3]
    require(walkable, "LineEntersTransport( from, to )", "rvNavMesh::WalkableLine refuses stepping onto a pad")
    # A player flush against a wall is a legal endpoint the clip epsilon stops
    # a sweep just short of.
    require(walkable, "NavSweepNearlyClear( trace, start, end )", "rvNavMesh::WalkableLine flush-wall endpoints")
    # A shortcut steeper than a walk is another floor, however clear the sweep.
    require(walkable, "stepSize + flat.Length()", "rvNavMesh::WalkableLine walk slope bound")
    require(walkable, "Max( 2,", "rvNavMesh::WalkableLine interior ground sample")
    require_order(
        walkable,
        "NAV_GROUND_OFFSET",
        "probeLift = stepSize",
        "rvNavMesh::WalkableLine low-clearance flat sweep",
    )
    require(
        walkable,
        "NavAgentTranslation",
        "rvNavMesh::WalkableLine usable closed doors",
    )

    # Areas are weak/undirected by design, so all public reachability decisions
    # must still respect outgoing links.
    reachable = nav[nav.index("bool rvNavMesh::IsReachable") :]
    reachable = reachable[: reachable.index("\n}\n") + 3]
    require(reachable, "FindPath( start, goal, path )", "rvNavMesh::IsReachable")

    roam = nav[nav.index("bool rvNavMesh::RandomReachablePoint") :]
    roam = roam[: roam.index("\n}\n") + 3]
    # Draws are filtered by the exact component closure, which is constant
    # time, and each accepted draw is proven by a real route.  The draw is
    # widened past idRandom's 15 bits so large maps can reach every node.
    require(roam, "NodeCanReach( startNode, candidate )", "rvNavMesh::RandomReachablePoint")
    require(roam, "RandomInt() << 15", "rvNavMesh::RandomReachablePoint wide draw")
    require(roam, "FindPath( origin, nodes[candidate].origin", "rvNavMesh::RandomReachablePoint")
    require(roam, "attempts < NAV_MAX_ROUTE_REPAIRS", "rvNavMesh::RandomReachablePoint retry")
    require(roam, "nodes[candidate].flags & NAVNODE_TRANSPORT", "rvNavMesh::RandomReachablePoint pads are no stop")

    same_node = nav[nav.index("if ( startNode == goalNode )", nav.index("bool rvNavMesh::FindPathInternal")) :]
    same_node = same_node[: same_node.index("\n\t}\n") + 4]
    require(
        same_node,
        "if ( !WalkableLine( routeStart, routeGoal ) )",
        "rvNavMesh::FindPath same node",
    )
    require(
        same_node,
        "WalkableLine( routeStart, via )",
        "rvNavMesh::FindPath same-node inbound leg",
    )
    require(
        same_node,
        "WalkableLine( via, routeGoal )",
        "rvNavMesh::FindPath same-node outbound leg",
    )
    require(same_node, "path.Append( via, NAVTRAVEL_WALK )", "rvNavMesh::FindPath same-node via route")

    # FindPath is a thin wrapper that counts the query for bot_profile; the
    # search itself lives in FindPathInternal.
    wrapper = nav[nav.index("bool rvNavMesh::FindPath(") :]
    wrapper = wrapper[: wrapper.index("\n}\n") + 3]
    require(wrapper, "FindPathInternal( start, goal, path )", "rvNavMesh::FindPath wrapper")
    require(wrapper, "stats.pathSearches++", "rvNavMesh::FindPath profiling")

    find_path = nav[nav.index("bool rvNavMesh::FindPathInternal") :]
    find_path = find_path[: find_path.index("\n}\n") + 3]
    require(
        find_path,
        "path[path.Num() - 1].origin - routeGoal",
        "rvNavMesh::FindPath exact-goal completion",
    )
    require(
        find_path,
        "FindNearestNode( routeStart, 256.0f, true )",
        "rvNavMesh::FindPath reachable start snap",
    )
    require(
        find_path,
        "!WalkableLine( routeStart, path[0].origin )",
        "rvNavMesh::FindPath safe first shortcut",
    )
    require(find_path, "path.Clear();", "rvNavMesh::FindPath rejected partial path")
    # Exact directed reachability rejects an unreachable goal before any
    # search, and A* is guided by ALT landmarks on top of the transport-safe
    # straight-line bound.  A node a landmark proves cannot lead to the goal
    # is never opened.
    require(find_path, "NodeCanReach( startNode, goalNode )", "rvNavMesh::FindPath component rejection")
    require(find_path, "Heuristic( next, goalNode )", "rvNavMesh::FindPath landmark heuristic")
    require(find_path, "h >= NAV_UNREACHED", "rvNavMesh::FindPath dead-end pruning")
    # Goal choice prices only the targets that can still win: best landmark
    # bound first, each by an exact corner-free A*, stopping once the best few
    # are certain.  A whole-map field per decision was most of the bots' CPU.
    ranked = nav[nav.index("int rvNavMesh::BuildCostField( const idVec3 &origin, float maxCost, const navFieldTarget_t") :]
    ranked = ranked[: ranked.index("\n}\n") + 3]
    require(ranked, "Heuristic( startNode, node )", "rvNavMesh::BuildCostField landmark bound per target")
    require(ranked, "PathCost( startNode, node, maxCost )", "rvNavMesh::BuildCostField exact pricing")
    require(ranked, "best[keepBest - 1] >= bound[i]", "rvNavMesh::BuildCostField branch and bound stop")
    heuristic = nav[nav.index("float rvNavMesh::Heuristic") :]
    heuristic = heuristic[: heuristic.index("\n}\n") + 3]
    require(heuristic, "heuristicScale", "rvNavMesh::Heuristic transport-safe straight-line bound")
    require(heuristic, "landmarkGoal - landmarkNode", "rvNavMesh::Heuristic forward landmark bound")
    require(heuristic, "nodeLandmark - goalLandmark", "rvNavMesh::Heuristic backward landmark bound")
    require(heuristic, "node * NAV_LANDMARK_STRIDE", "rvNavMesh::Heuristic node-major landmark rows")
    require(heuristic, "return NAV_INFINITE_COST;", "rvNavMesh::Heuristic unreachable proof")
    require(find_path, "rejectedLinks", "rvNavMesh::FindPath blocked-edge repair")
    require(find_path, "LinkTraversable(", "rvNavMesh::FindPath live route validation")
    require(find_path, "SetTravelCost(", "rvNavMesh::FindPath weighted route cost")
    # A goal beside a pad cannot be stood on; its nearest node is the arrival.
    require(find_path, "TransportAt( standing ) >= 0", "rvNavMesh::FindPath transport-side goals")
    if find_path.count("path.routeStart = routeStart;") != find_path.count("return true;") + 1:
        raise AssertionError(
            "FindPathInternal must record the grounded route start on every success path; "
            "rvBot::OffRoute measures the first leg from it"
        )
    require(find_path, "routeStart", "rvNavMesh::FindPath airborne start projection")
    require(find_path, "routeGoal", "rvNavMesh::FindPath airborne goal projection")
    require(find_path, "GroundUnder( start", "rvNavMesh::FindPath safe landing projection")
    require(find_path, "start.z - projected.z > pm_stepsize", "rvNavMesh::FindPath airborne-only fallback")
    if find_path.index("GroundUnder( start") > find_path.index("FindNearestNode( routeStart"):
        raise AssertionError(
            "FindPath snaps before projecting an airborne endpoint; a nearby in-air node can "
            "then preserve an unreachable exact goal instead of routing to the landing floor"
        )

    link_nodes = nav[nav.index("void rvNavMesh::LinkNodes") :]
    link_nodes = link_nodes[: link_nodes.index("\n}\n") + 3]
    require(link_nodes, "cost / geometricCost", "rvNavMesh admissible heuristic scale")
    require(link_nodes, "Min( heuristicScale, edgeScale )", "rvNavMesh heuristic lower bound")

    length_from = nav[nav.index("float rvNavPath::LengthFrom") :]
    length_from = length_from[: length_from.index("\n}\n") + 3]
    require(length_from, "hasTravelCost", "rvNavPath retained graph cost")
    require(length_from, "return travelCost;", "rvNavPath weighted goal comparison")

    begin_search = nav[nav.index("void rvNavMesh::BeginSearch") :]
    begin_search = begin_search[: begin_search.index("\n}\n") + 3]
    require(begin_search, "0x7ffffffe", "rvNavMesh search serial rollover")
    require(begin_search, "openTag[i] = 0", "rvNavMesh search tag reset")

    traversable = nav[nav.index("bool rvNavMesh::LinkTraversable") :]
    traversable = traversable[: traversable.index("\n}\n") + 3]
    for travel in (
        "NAVTRAVEL_WALK",
        "NAVTRAVEL_DROP",
        "NAVTRAVEL_JUMP",
        "NAVTRAVEL_JUMPPAD",
        "NAVTRAVEL_TELEPORT",
    ):
        require(traversable, travel, "rvNavMesh::LinkTraversable")
    grid_validation = traversable[: traversable.index("case NAVTRAVEL_JUMPPAD")]
    # Only solids that can move after the build may invalidate a proven link.
    # Stock maps are mostly func_static brush entities, which generation
    # already proved; sweeping them again rejected working links and cost two
    # repair searches per route.
    require(
        grid_validation,
        "DynamicTranslation(",
        "rvNavMesh::LinkTraversable dynamic-entity validation",
    )
    require(grid_validation, "dynamicSolids.Num() == 0", "rvNavMesh::LinkTraversable mover-free maps")
    require(grid_validation, "DynamicSolidNear( swept )", "rvNavMesh::LinkTraversable broad phase")
    dynamic = nav[nav.index("bool rvNavMesh::DynamicTranslation") :]
    dynamic = dynamic[: dynamic.index("\n}\n") + 3]
    require(dynamic, "NavIsDynamicSolid( blocker ) && !NavDoorIsWalkActivated( blocker )",
            "rvNavMesh::DynamicTranslation blocks only on movers")
    if "TryStep(" in grid_validation:
        raise AssertionError(
            "LinkTraversable re-samples generated grid topology; live repair must only "
            "invalidate links blocked by newly present solid entities"
        )
    if "WalkableLine(" in grid_validation:
        raise AssertionError(
            "LinkTraversable uses the stricter string-pull proof on adjacent graph edges; "
            "stair-riser midpoint probes can reject links that TryStep legally generated"
        )
    require(
        nav,
        "gameLocal.TranslationEntities(",
        "rvNavMesh dynamic route-edge entity sweep",
    )
    require(traversable, "IsActive( TH_THINK )", "rvNavMesh::LinkTraversable live jump pad")

    header = read(mp / "bots" / "NavMesh.h")
    require(header, "LengthFrom", "rvNavPath complete length")
    require(header, "travelCost", "rvNavPath weighted route cost")
    require(header, "heuristicScale", "rvNavMesh scaled admissible heuristic")
    for travel in (
        "NAVTRAVEL_WALK",
        "NAVTRAVEL_DROP",
        "NAVTRAVEL_JUMP",
        "NAVTRAVEL_JUMPPAD",
        "NAVTRAVEL_TELEPORT",
    ):
        require(header, travel, "navTravelType_t")

    # The debug colour table is indexed by travel type, so it has to keep pace.
    enum_body = header[header.index("typedef enum {") : header.index("} navTravelType_t;")]
    travel_types = [
        name for name in re.findall(r"\b(NAVTRAVEL_\w+)\b", enum_body) if name != "NAVTRAVEL_NUM"
    ]
    table_start = nav.index("travelColors[NAVTRAVEL_NUM]")
    table = nav[table_start : nav.index("};", table_start)]
    if table.count("&color") != len(travel_types):
        raise AssertionError(
            "rvNavMesh::DebugDraw travelColors has "
            f"{table.count('&color')} entries for {len(travel_types)} travel types"
        )


def validate_cvars() -> None:
    mp = GAME_LIBS_ROOT / "src" / "mpgame"
    if not mp.is_dir():
        return

    declared = read(mp / "gamesys" / "SysCvar.cpp")
    exported = read(mp / "gamesys" / "SysCvar.h")
    sp_bots = read(GAME_LIBS_ROOT / "src" / "game" / "bots" / "Bot.cpp")

    for name in (
        "bot_enable",
        "bot_minPlayers",
        "bot_skill",
        "bot_debug",
        "bot_debugNav",
        "bot_navCellSize",
        "bot_pause",
    ):
        require(declared, f'idCVar {name}(', "mpgame SysCvar.cpp")
        require(exported, f"extern idCVar {name};", "mpgame SysCvar.h")

    # These legacy SP bot diagnostics share global names with the richer MP
    # diagnostics. Keep their declaration signatures aligned so loading SP
    # before MP cannot leave the CVar system with conflicting types or policy.
    for name in ("bot_debug", "bot_debugnav"):
        sp_signature = cvar_signature(sp_bots, name, "game bots/Bot.cpp")
        mp_signature = cvar_signature(declared, name, "mpgame SysCvar.cpp")
        if sp_signature != mp_signature:
            raise AssertionError(f"SP/MP bot CVar signatures differ for {name!r}")
        if "CVAR_INTEGER" not in sp_signature[1] or "CVAR_BOOL" in sp_signature[1]:
            raise AssertionError(f"{name} must retain the shared integer diagnostic type")

    commands = read(mp / "gamesys" / "SysCmds.cpp")
    for command in ("addbot", "removebot", "kickbots", "botlist", "navmesh"):
        require(commands, f'"{command}"', "mpgame console commands")


def main() -> int:
    try:
        validate_engine()
        validate_game_wiring()
        validate_navmesh()
        validate_cvars()
    except AssertionError as error:
        print(f"mp_bot_navigation: FAILED - {error}")
        return 1

    print("mp_bot_navigation: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

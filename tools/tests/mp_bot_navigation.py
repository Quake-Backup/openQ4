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
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME_LIBS_ROOT = Path(os.environ.get("OPENQ4_GAMELIBS_REPO", ROOT.parent / "openQ4-game")).resolve()


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

    bot = read(mp / "bots" / "Bot.cpp")

    # Without ui_autoJoin a spawning idPlayer parks itself in the join menu.
    for key in ('"ui_autoJoin"', '"ui_spectate"', '"ui_ready"', '"ui_name"'):
        require(bot, key, "rvBot::FillUserInfo")

    # Full movement speed needs the run button; idPlayer::AdjustSpeed otherwise
    # drops the bot to pm_walkspeed.
    require(bot, "cmd.buttons |= BUTTON_RUN;", "rvBot::ApplyMove")

    # The player rebuilds its view from the command plus its delta angles.
    require(bot, "ANGLE2SHORT( aimAngles[i] - self->GetDeltaViewAngles()[i] )", "rvBot::UpdateAim")

    # An unreachable goal must not cost a failed search every frame.  The
    # throttle counts attempts, not successes: keying it off goalType leaves it
    # dead on the failure path, which is the only path it exists for.
    goal = bot[bot.index("void rvBot::UpdateGoal") :][:3000]
    require(goal, "gameLocal.time - enemyPathTime <= BOT_REPATH_COMBAT_MSEC", "rvBot::UpdateGoal")
    require(goal, "gameLocal.time < nextGoalSelectTime", "rvBot::UpdateGoal")

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

    # Off-mesh links are what keep a Quake 4 multiplayer map one component.
    links = nav[nav.index("void rvNavMesh::AddOffMeshLinks") :][:2500]
    require(links, "rvJumpPad::GetClassType()", "rvNavMesh::AddOffMeshLinks")
    require(links, "idTrigger_Multi::GetClassType()", "rvNavMesh::AddOffMeshLinks")
    require(links, "idPlayerStart::GetClassType()", "rvNavMesh::AddOffMeshLinks")

    # Smoothing may never skip a link that has to be entered deliberately.
    pull = nav[nav.index("void rvNavMesh::StringPull") :][:2000]
    require(pull, "!= NAVTRAVEL_WALK", "rvNavMesh::StringPull")

    header = read(mp / "bots" / "NavMesh.h")
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

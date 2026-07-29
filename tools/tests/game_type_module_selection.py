#!/usr/bin/env python3
"""Guards how the engine picks a game module from si_gameType.

The engine has to choose between game_sp and game_mp before any game module is
loaded, so it cannot ask the game for its own gametype table. It keeps a mirror
of si_gameTypeArgs instead. This test pins that mirror against the GameLibs
table, and pins the shipped default.cfg value that decides which module a plain
client boots (issue #73: booting game_mp meant every New Game tore the renderer
down for a module swap).
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


def parse_string_array(source: str, declaration: str) -> list[str]:
    start = source.find(declaration)
    if start == -1:
        raise AssertionError(f"Missing array declaration {declaration!r}")
    open_brace = source.index("{", start)
    close_brace = source.index("}", open_brace)
    body = source[open_brace + 1 : close_brace]
    return re.findall(r'"([^"]*)"', body)


def validate_engine_mirror() -> None:
    common = read(ROOT / "src" / "framework" / "Common.cpp")
    engine_types = parse_string_array(common, "static const char *openQ4_multiplayerGameTypes[] = {")
    if not engine_types:
        raise AssertionError("engine multiplayer gametype allowlist is empty")

    game_types_source = GAME_LIBS_ROOT / "src" / "mpgame" / "mp" / "GameTypes.cpp"
    if not game_types_source.is_file():
        print(
            f"game_type_module_selection: skipped table cross-check "
            f"(no GameLibs checkout at {GAME_LIBS_ROOT})"
        )
    else:
        game_types = parse_string_array(read(game_types_source), "const char *si_gameTypeArgs[] = {")
        if not game_types:
            raise AssertionError("GameLibs si_gameTypeArgs is empty")
        if game_types[0] != "singleplayer":
            raise AssertionError(
                f"si_gameTypeArgs[0] is {game_types[0]!r}, expected 'singleplayer'"
            )
        expected = game_types[1:]
        if engine_types != expected:
            raise AssertionError(
                "openQ4_multiplayerGameTypes has drifted from si_gameTypeArgs:\n"
                f"  engine:   {engine_types}\n"
                f"  gamelibs: {expected}"
            )

    # An allowlist, not "anything that is not singleplayer".
    for token in (
        "static bool openQ4_IsMultiplayerGameType( const char *gameType ) {",
        "for ( int i = 0; openQ4_multiplayerGameTypes[i] != NULL; i++ ) {",
        'idStr::Icmp( gameType, openQ4_multiplayerGameTypes[i] ) == 0',
    ):
        require(common, token, "engine multiplayer gametype allowlist")

    # A dedicated server has no single-player mode; it must not be routed to
    # game_sp by an unrecognised gametype.
    require(common, "#ifdef ID_DEDICATED", "dedicated game module selection")
    require(
        common,
        'return ( gameType != NULL && idStr::Icmp( gameType, "singleplayer" ) == 0 ) ? "game_sp" : "game_mp";',
        "dedicated game module selection",
    )


def validate_default_cfg() -> None:
    cfg = read(ROOT / "content" / "baseoq4" / "pak0" / "default.cfg")
    require(cfg, "sets\tsi_gameType\t\tsingleplayer", "shipped default gametype")
    if re.search(r"^sets\s+si_gameType\s+dm\s*$", cfg, re.MULTILINE | re.IGNORECASE):
        raise AssertionError("default.cfg still selects a multiplayer gametype")


def validate_swap_guard() -> None:
    common = read(ROOT / "src" / "framework" / "Common.cpp")
    start = common.index("void Com_ReloadGameModule_f( const idCmdArgs &args ) {")
    end = common.index("idCommonLocal::GetLanguageDict", start)
    body = common[start:end]
    for token in (
        "try {",
        "commonLocal.ShutdownGame( true );",
        "commonLocal.InitGame();",
        "catch( idException &ex ) {",
        "swapFailed = true;",
        "Com_GameModuleLoadPhaseName( Com_GetGameModuleLoadPhase() )",
        "============= ReloadGameModule failed ============",
        "session->StartMenu();",
    ):
        require(body, token, "game module swap exception guard")


def main() -> int:
    validate_engine_mirror()
    validate_default_cfg()
    validate_swap_guard()
    print("game_type_module_selection: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Keep SP/MP zoom-scope yaw aligned with the camera actually presented."""

from __future__ import annotations

import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GAME_ROOT = Path(os.environ.get("OPENQ4_GAMELIBS_REPO", ROOT.parent / "openQ4-game")).resolve()


def read(path: Path) -> str:
    if not path.is_file():
        raise AssertionError(f"required GameLib source is missing: {path}")
    return path.read_text(encoding="utf-8")


def require(source: str, token: str, label: str) -> None:
    if token not in source:
        raise AssertionError(f"{label} is missing {token!r}")


def validate_mode(directory: str) -> None:
    player_cpp = read(GAME_ROOT / "src" / directory / "Player.cpp")
    player_h = read(GAME_ROOT / "src" / directory / "Player.h")
    machinegun = read(GAME_ROOT / "src" / directory / "weapon" / "WeaponMachinegun.cpp")
    label = f"{directory} zoom presentation"

    require(player_h, "void\t\t\t\t\tUpdateZoomGuiViewState( void );", label)
    for token in (
        "void idPlayer::UpdateZoomGuiViewState( void )",
        "weapon == NULL || weapon->GetZoomGui() == NULL",
        "renderView != NULL ? renderView->viewaxis : firstPersonViewAxis",
        'weapon->GetZoomGui()->SetStateFloat( "playerYaw", presentedViewAxis.ToAngles().yaw );',
        "UpdateZoomGuiViewState();",
        "Redraw( gameLocal.time );",
    ):
        require(player_cpp, token, label)

    draw_hud = player_cpp.index("void idPlayer::DrawHUD( idUserInterface *_hud )")
    update_call = player_cpp.index("UpdateZoomGuiViewState();", draw_hud)
    debug_read = player_cpp.index('State().GetFloat( "playerYaw" )', draw_hud)
    first_redraw = player_cpp.index("Redraw( gameLocal.time );", debug_read)
    if update_call > debug_read or update_call > first_redraw:
        raise AssertionError(f"{label} updates scope yaw after it is inspected or drawn")

    if "rvWeaponMachinegun::Think" in machinegun or "playerViewAxis.ToAngles().yaw" in machinegun:
        raise AssertionError(f"{label} still writes scope yaw from simulation-time weapon state")


def main() -> None:
    validate_mode("game")
    validate_mode("mpgame")
    validation = (ROOT / "tools" / "validation" / "openq4_validate.py").read_text(encoding="utf-8")
    require(validation, "weapon_zoom_view_alignment.py", "zoom presentation regression wiring")
    print("weapon zoom view alignment: ok")


if __name__ == "__main__":
    main()

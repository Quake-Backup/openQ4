#!/usr/bin/env python3
"""Regression checks for openQ4-game source staging."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
STAGE_SCRIPT = ROOT / "tools" / "build" / "stage_gamelibs.py"
MANIFEST_NAME = "openq4_gamelibs_stage_manifest.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def run_stage(project_root: Path, gamelibs_root: Path, stage_root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(STAGE_SCRIPT), str(project_root), str(gamelibs_root), str(stage_root)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def write_file(path: Path, data: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(data, encoding="utf-8")


def make_minimal_workspace(work: Path) -> tuple[Path, Path, Path]:
    project_root = work / "openQ4"
    gamelibs_root = work / "openQ4-game"
    stage_root = project_root / ".tmp" / "gamelibs_stage"

    write_file(project_root / "src" / "idlib" / "idlib_public.h", "// idlib\n")
    write_file(project_root / "src" / "renderer" / "RenderWorld.h", "// renderer\n")
    write_file(gamelibs_root / "src" / "game" / "Game_local.cpp", "// game\n")
    write_file(gamelibs_root / "src" / "game" / "gamesys" / "SysCvar.cpp", "// cvar\n")
    return project_root, gamelibs_root, stage_root


def validate_manifest(stage_root: Path) -> None:
    manifest_path = stage_root / MANIFEST_NAME
    if not manifest_path.is_file():
        raise AssertionError("stage manifest was not written")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    files = manifest.get("files")
    if manifest.get("format") != 1:
        raise AssertionError("unexpected stage manifest format")
    if not isinstance(files, list) or manifest.get("fileCount") != len(files):
        raise AssertionError("stage manifest file count mismatch")

    paths = {entry["path"]: entry["sha256"] for entry in files}
    for rel in (
        "src/game/Game_local.cpp",
        "src/game/gamesys/SysCvar.cpp",
        "src/idlib/idlib_public.h",
        "src/renderer/RenderWorld.h",
    ):
        staged = stage_root / rel
        if not staged.is_file():
            raise AssertionError(f"missing staged file: {rel}")
        if paths.get(rel) != sha256(staged):
            raise AssertionError(f"manifest hash mismatch for {rel}")


def validate_successful_stage(work: Path) -> None:
    project_root, gamelibs_root, stage_root = make_minimal_workspace(work)
    result = run_stage(project_root, gamelibs_root, stage_root)
    if result.returncode != 0:
        raise AssertionError(f"stage_gamelibs.py failed unexpectedly: {result.stderr}")
    if result.stdout.strip() != stage_root.resolve().as_posix():
        raise AssertionError("stage_gamelibs.py stdout should be the resolved stage root only")
    validate_manifest(stage_root)


def validate_symlink_rejection(work: Path) -> None:
    project_root, gamelibs_root, stage_root = make_minimal_workspace(work)
    target = gamelibs_root / "src" / "game" / "Game_local.cpp"
    link = gamelibs_root / "src" / "game" / "linked.cpp"
    try:
        os.symlink(target, link)
    except (OSError, NotImplementedError):
        return

    result = run_stage(project_root, gamelibs_root, stage_root)
    if result.returncode == 0:
        raise AssertionError("stage_gamelibs.py accepted a symlink source")
    if "refusing to stage symlink" not in result.stderr:
        raise AssertionError(f"unexpected symlink rejection message: {result.stderr}")


def validate_stage_root_guard(work: Path) -> None:
    project_root, gamelibs_root, _stage_root = make_minimal_workspace(work)
    result = run_stage(project_root, gamelibs_root, work / "outside-stage")
    if result.returncode == 0:
        raise AssertionError("stage_gamelibs.py accepted a stage root outside openQ4/.tmp")
    if "stage root must be under openQ4 .tmp" not in result.stderr:
        raise AssertionError(f"unexpected stage-root guard message: {result.stderr}")


def validate_source_contracts() -> None:
    script = STAGE_SCRIPT.read_text(encoding="utf-8")
    meson = (ROOT / "meson.build").read_text(encoding="utf-8")
    validator = (ROOT / "tools" / "validation" / "openq4_validate.py").read_text(encoding="utf-8")
    building = (ROOT / "BUILDING.md").read_text(encoding="utf-8")

    required_script_tokens = (
        "MANIFEST_NAME",
        "openq4_gamelibs_stage_manifest.json",
        "refusing to stage symlink",
        "refusing to stage non-regular file",
        "stage root must be under openQ4 .tmp",
        "sha256",
        "gameLibsGitCommit",
        "gameLibsGitDirty",
        "validate_stage_manifest",
    )
    for token in required_script_tokens:
        if token not in script:
            raise AssertionError(f"missing staging script token: {token}")

    for token in (
        "openq4_gamelibs_stage_manifest.json",
        "Staged openQ4-game source manifest not found",
    ):
        if token not in meson:
            raise AssertionError(f"missing Meson staging contract token: {token}")

    if "gamelibs_staging.py" not in validator:
        raise AssertionError("validation runner does not include gamelibs_staging.py")
    if "source-input repository" not in building:
        raise AssertionError("BUILDING.md does not document the GameLibs source-input role")


def main() -> None:
    work = ROOT / ".tmp" / "gamelibs-staging-test"
    shutil.rmtree(work, ignore_errors=True)
    try:
        validate_successful_stage(work / "success")
        validate_symlink_rejection(work / "symlink")
        validate_stage_root_guard(work / "stage-root")
        validate_source_contracts()
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("gamelibs_staging: ok")


if __name__ == "__main__":
    main()

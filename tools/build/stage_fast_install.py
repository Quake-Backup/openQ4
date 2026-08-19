#!/usr/bin/env python3
"""Incrementally stage local runtime files from builddir into .install."""

from __future__ import annotations

import argparse
import stat
import sys
from pathlib import Path

from openq4_pak import copy_file_if_changed, is_relative_to
from windows_runtime import cleanup_windows_stage_target, is_windows_host


ROOT_RUNTIME_PATTERNS = (
    "openQ4-client_*.exe",
    "openQ4-client_*.pdb",
    "openQ4-ded_*.exe",
    "openQ4-ded_*.pdb",
    "renderer-gl_*.dll",
    "renderer-gl_*.pdb",
    "renderer-vk_*.dll",
    "renderer-vk_*.pdb",
    "OpenAL32.dll",
)
GAME_RUNTIME_PATTERNS = (
    "game-sp_*.dll",
    "game-sp_*.pdb",
    "game-mp_*.dll",
    "game-mp_*.pdb",
    "mod.json",
    "pak0.pk4",
    "pak1.pk4",
)
NON_RUNTIME_PATTERNS = (
    "*.exp",
    "*.ilk",
    "*.lib",
    "*.map",
    "*.zip",
)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stage changed local build outputs into .install without running meson install."
    )
    parser.add_argument("--build-dir", required=True, help="Meson build directory.")
    parser.add_argument("--install-dir", required=True, help="Runtime staging root, usually .install.")
    parser.add_argument(
        "--source-root",
        default=str(Path(__file__).resolve().parents[2]),
        help="openQ4 source root used to validate the fast staging target.",
    )
    parser.add_argument(
        "--temporary-runtime",
        action="store_true",
        help=(
            "Allow one fresh alternate runtime below <source-root>/.tmp/stock-runtime/. "
            "This is for isolated compatibility captures and never replaces canonical .install staging."
        ),
    )
    return parser.parse_args(argv[1:])


def copy_if_changed(source: Path, destination: Path) -> bool:
    return copy_file_if_changed(source, destination)


def is_link_or_junction(path: Path) -> bool:
    is_junction = getattr(path, "is_junction", None)
    if path.is_symlink() or bool(is_junction and is_junction()):
        return True
    try:
        attributes = getattr(path.lstat(), "st_file_attributes", 0)
        return bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0))
    except OSError:
        return False


def validate_stage_roots(
    source_root: Path,
    build_dir: Path,
    install_dir: Path,
    temporary_runtime: bool = False,
) -> None:
    for label, path in (
        ("source root", source_root),
        ("build directory", build_dir),
        ("install directory", install_dir),
    ):
        if is_link_or_junction(path):
            raise RuntimeError(f"fast staging {label} must not be a link or junction: {path}")

    source_root = source_root.resolve()
    build_dir = build_dir.resolve()
    install_dir = install_dir.resolve()
    expected_install_dir = source_root / ".install"
    temporary_parent = source_root / ".tmp" / "stock-runtime"

    if install_dir != expected_install_dir:
        if not temporary_runtime:
            raise RuntimeError(f"fast staging install directory must be {expected_install_dir}: {install_dir}")
        if install_dir == temporary_parent or not is_relative_to(install_dir, temporary_parent):
            raise RuntimeError(
                f"temporary runtime directory must stay below {temporary_parent}: {install_dir}"
            )
        if install_dir.exists():
            raise RuntimeError(f"temporary runtime directory must be new: {install_dir}")
        current = source_root
        for part in install_dir.relative_to(source_root).parts[:-1]:
            current /= part
            if current.exists() and is_link_or_junction(current):
                raise RuntimeError(
                    f"temporary runtime parent must not be a link or junction: {current}"
                )
    elif temporary_runtime:
        raise RuntimeError("--temporary-runtime is only valid for an alternate runtime directory")
    if not is_relative_to(build_dir, source_root):
        raise RuntimeError(f"fast staging build directory must stay under {source_root}: {build_dir}")
    if build_dir == install_dir or is_relative_to(install_dir, build_dir) or is_relative_to(build_dir, install_dir):
        raise RuntimeError(f"fast staging build and install directories must not overlap: {build_dir}")


def remove_matches(root: Path, patterns: tuple[str, ...]) -> list[Path]:
    removed: list[Path] = []
    if not root.is_dir():
        return removed
    for pattern in patterns:
        for path in sorted(root.glob(pattern)):
            if path.is_file():
                path.unlink()
                removed.append(path)
    return removed


def copy_matches(source_root: Path, destination_root: Path, patterns: tuple[str, ...]) -> list[Path]:
    copied: list[Path] = []
    if not source_root.is_dir():
        return copied
    for pattern in patterns:
        for source in sorted(source_root.glob(pattern)):
            if not source.is_file():
                continue
            destination = destination_root / source.name
            if copy_if_changed(source, destination):
                copied.append(destination)
    return copied


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    source_root = Path(args.source_root)
    build_dir = Path(args.build_dir)
    install_dir = Path(args.install_dir)

    try:
        validate_stage_roots(
            source_root, build_dir, install_dir, args.temporary_runtime
        )
        source_root = source_root.resolve()
        build_dir = build_dir.resolve()
        install_dir = install_dir.resolve()
        build_game_dir = build_dir / "baseoq4"
        install_game_dir = install_dir / "baseoq4"
        install_dir.mkdir(parents=True, exist_ok=True)
        install_game_dir.mkdir(parents=True, exist_ok=True)

        stage_cleanup = (
            cleanup_windows_stage_target(install_dir)
            if is_windows_host()
            else {"removed_stale_files": [], "removed_empty_directories": []}
        )
        removed = remove_matches(install_dir, NON_RUNTIME_PATTERNS)
        removed += remove_matches(install_game_dir, NON_RUNTIME_PATTERNS + ("*.so", "*.dylib"))
        copied = copy_matches(build_dir, install_dir, ROOT_RUNTIME_PATTERNS)
        copied += copy_matches(build_game_dir, install_game_dir, GAME_RUNTIME_PATTERNS)
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(
        f"fast-staged .install: copied={len(copied)} "
        f"removed_non_runtime={len(removed)} "
        f"removed_stale={len(stage_cleanup['removed_stale_files'])} "
        f"removed_empty_dirs={len(stage_cleanup['removed_empty_directories'])}"
    )
    for path in copied[:20]:
        print(f"  copied {path}")
    if len(copied) > 20:
        print(f"  ... {len(copied) - 20} more")
    return 0


if __name__ == "__main__":
    import sys

    raise SystemExit(main(sys.argv))

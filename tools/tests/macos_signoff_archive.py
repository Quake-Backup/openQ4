#!/usr/bin/env python3
"""Regression checks for macOS signoff archive validation."""

from __future__ import annotations

import importlib.util
import io
import sys
import tarfile
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True


def load_validator():
    validator_path = ROOT / "tools" / "macos" / "validate_signoff_archive.py"
    spec = importlib.util.spec_from_file_location("validate_signoff_archive_for_test", validator_path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"Unable to import signoff archive validator: {validator_path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def report_text(bridge: str, *, completed: bool) -> str:
    checkbox = "x" if completed else " "
    return f"""# macOS Runtime Signoff

- Date (UTC): 2026-06-20T12:00:00Z
- Host: test-mac
- Architecture: arm64
- Graphics bridge: {bridge}
- OpenAL provider: apple_framework
- Build directory: builddir-{bridge}
- Asset basepath: /Users/test/openq4-work/Quake4
- Client: /Users/test/openq4-work/openQ4/.install/openQ4-client_arm64
- Results: /Users/test/openq4-work/results/testrun-signoff-{bridge}

## Automated Evidence
- [x] Bridge-specific build and staged install completed.
- [x] Renderer smoke profile completed with retail Quake 4 assets.
- [x] macOS-facing renderer validation matrix completed.
- [x] Desktop launcher was written for Finder/Terminal launch checks.
- Renderer smoke output: /Users/test/openq4-work/results/testrun-signoff-{bridge}/renderer-smoke
- Renderer matrix output: /Users/test/openq4-work/results/testrun-signoff-{bridge}/renderer-matrix
- Workflow log: /Users/test/openq4-work/results/testrun-signoff-{bridge}/openq4-macos-workflow.log

## Manual Hardware Checklist
- [{checkbox}] Launch openQ4 from Finder or the Desktop launcher and enter a single-player map.
- [{checkbox}] Verify keyboard text entry, console toggle, mouse-look, clicks, and wheel input.
- [{checkbox}] Verify at least one SDL game controller, including hotplug and rumble when hardware supports it.
- [{checkbox}] Verify audio output, volume changes, and at least one device switch or reconnect.
- [{checkbox}] Verify windowed, fullscreen, selected-display, and HiDPI/Retina behavior on attached displays.
- [{checkbox}] Verify the matching OpenGL or Metal bridge package path in actual gameplay, not only at the main menu.
"""


def log_text(bridge: str) -> str:
    return f"""Configuring openQ4 (debug, backend=sdl3, macos_graphics_bridge={bridge}, macos_openal_provider=apple_framework)
Compiling openQ4
Staging openQ4 into .install
Running openQ4 macOS renderer smoke
Running macOS-facing renderer validation matrix
Installed macOS launcher: /Users/test/Desktop/openQ4.command
macOS runtime signoff report: /Users/test/openq4-work/results/testrun-signoff-{bridge}/macos-runtime-signoff.md
"""


def add_file(archive: tarfile.TarFile, name: str, text: str) -> None:
    data = text.encode("utf-8")
    member = tarfile.TarInfo(name)
    member.mode = 0o644
    member.size = len(data)
    archive.addfile(member, io.BytesIO(data))


def write_archive(path: Path, *, bridges: tuple[str, ...], completed: bool = False) -> None:
    with tarfile.open(path, "w:gz") as archive:
        for bridge in bridges:
            root = f"testrun-signoff-{bridge}"
            directory = tarfile.TarInfo(root)
            directory.type = tarfile.DIRTYPE
            directory.mode = 0o755
            archive.addfile(directory)
            add_file(archive, f"{root}/macos-runtime-signoff.md", report_text(bridge, completed=completed))
            add_file(archive, f"{root}/openq4-macos-workflow.log", log_text(bridge))
            add_file(archive, f"{root}/renderer-smoke/report.json", "{}\n")
            add_file(archive, f"{root}/renderer-matrix/report.md", "# ok\n")


def expect_error(fragment: str, callback) -> None:
    try:
        callback()
    except Exception as exc:
        if exc.__class__.__name__ != "SignoffArchiveError":
            raise
        if fragment not in str(exc):
            raise AssertionError(f"Unexpected validation error: {exc}") from exc
        return
    raise AssertionError(f"Expected validation error containing {fragment!r}")


def main() -> int:
    validator = load_validator()
    with tempfile.TemporaryDirectory(prefix="openq4-macos-signoff-") as temp_root:
        temp = Path(temp_root)
        good = temp / "openq4-macos-results-testrun.tar.gz"
        write_archive(good, bridges=("opengl", "metal"), completed=False)
        run_id = validator.validate_signoff_archive(
            good,
            run_id=None,
            action="signoff",
            bridges=("opengl", "metal"),
            require_completed_checklist=False,
        )
        if run_id != "testrun":
            raise AssertionError(f"Unexpected inferred run ID: {run_id}")

        completed = temp / "openq4-macos-results-completed.tar.gz"
        write_archive(completed, bridges=("opengl", "metal"), completed=True)
        validator.validate_signoff_archive(
            completed,
            run_id="testrun",
            action="signoff",
            bridges=("opengl", "metal"),
            require_completed_checklist=True,
        )

        expect_error(
            "still has open manual checklist",
            lambda: validator.validate_signoff_archive(
                good,
                run_id="testrun",
                action="signoff",
                bridges=("opengl", "metal"),
                require_completed_checklist=True,
            ),
        )

        missing_metal = temp / "openq4-macos-results-missing.tar.gz"
        write_archive(missing_metal, bridges=("opengl",), completed=True)
        expect_error(
            "Expected exactly one *-signoff-metal directory",
            lambda: validator.validate_signoff_archive(
                missing_metal,
                run_id=None,
                action="signoff",
                bridges=("opengl", "metal"),
                require_completed_checklist=False,
            ),
        )

    print("macos_signoff_archive: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

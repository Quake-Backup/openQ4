#!/usr/bin/env python3
"""Deterministic contract tests for per-map renderer CPU/GPU budgets."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import struct
import sys
import tempfile
from pathlib import Path
from types import ModuleType


ROOT = Path(__file__).resolve().parents[2]
VALIDATION_DIR = ROOT / "tools" / "validation"
if str(VALIDATION_DIR) not in sys.path:
    sys.path.insert(0, str(VALIDATION_DIR))

import renderer_budget_contract as budget


def load_benchmark() -> ModuleType:
    path = ROOT / "tools" / "tests" / "renderer_gameplay_benchmark.py"
    spec = importlib.util.spec_from_file_location("openq4_renderer_gameplay_benchmark", path)
    if spec is None or spec.loader is None:
        raise AssertionError("could not load renderer_gameplay_benchmark.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def timing_marker(
    *,
    map_name: str = "game/storage1",
    backend: str = "opengl",
    profile: str = "baseline",
    cpu_samples: int = 256,
    cpu: tuple[int, int, int] = (7000, 11000, 14000),
    gpu_available: bool = True,
    gpu_samples: int = 252,
    gpu: tuple[int, int, int] = (5000, 8000, 10000),
) -> str:
    return (
        f"{budget.MARKER_PREFIX} map={map_name} backend={backend} profile={profile} "
        f"cpuSamples={cpu_samples} cpuP50Us={cpu[0]} cpuP95Us={cpu[1]} cpuP99Us={cpu[2]} "
        f"gpuAvailable={int(gpu_available)} gpuSamples={gpu_samples} "
        f"gpuP50Us={gpu[0]} gpuP95Us={gpu[1]} gpuP99Us={gpu[2]}"
    )


def write_tga(path: Path, width: int = 1280, height: int = 720) -> None:
    header = bytearray(18)
    header[2] = 2
    struct.pack_into("<H", header, 12, width)
    struct.pack_into("<H", header, 14, height)
    header[16] = 24
    pixels = bytes((index * 17) % 256 for index in range(width * height * 3))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(header) + pixels)


def test_contract_and_marker() -> None:
    contract, binding = budget.load_contract(budget.DEFAULT_CONTRACT_PATH)
    assert contract["contractId"] == "stock-baseline-targets-v1"
    assert len(contract["budgets"]) == 16
    assert binding["path"] == "tools/validation/renderer_per_map_budgets.json"
    assert not Path(binding["path"]).is_absolute()

    marker = timing_marker()
    evidence, failures = budget.evaluate_timing_evidence(
        (marker,), contract, "game/storage1", "opengl", "baseline"
    )
    assert failures == [] and evidence["status"] == "pass"
    assert evidence["budgetId"] == "storage1-opengl-baseline"
    assert evidence["measurement"]["cpu"]["p95Us"] == 11000
    assert evidence["measurement"]["gpu"]["p99Us"] == 10000

    # A single console line mirrored once into stdout is accepted, but a
    # command that emits the same marker twice in one authoritative stream is
    # rejected as an ambiguous capture window.
    _, mirrored_failures = budget.parse_timing_evidence((marker, marker))
    assert mirrored_failures == []
    _, duplicate_failures = budget.parse_timing_evidence((marker + "\n" + marker,))
    assert any("contains 2" in failure for failure in duplicate_failures)
    _, conflicting_failures = budget.parse_timing_evidence(
        (marker, timing_marker(cpu=(7000, 12000, 14000)))
    )
    assert any("conflicting" in failure for failure in conflicting_failures)

    for changed, fragment in (
        ({"expected_map": "game/storage2"}, "does not match expected"),
        ({"expected_backend": "vulkan"}, "timing backend"),
        ({"expected_profile": "modern"}, "timing profile"),
    ):
        _, identity_failures = budget.evaluate_timing_evidence(
            (marker,),
            contract,
            changed.get("expected_map", "game/storage1"),
            changed.get("expected_backend", "opengl"),
            changed.get("expected_profile", "baseline"),
        )
        assert any(fragment in failure for failure in identity_failures)

    unavailable = timing_marker(
        gpu_available=False, gpu_samples=0, gpu=(-1, -1, -1)
    )
    _, unavailable_failures = budget.evaluate_timing_evidence(
        (unavailable,), contract, "game/storage1", "opengl", "baseline"
    )
    assert "required GPU frame timing is unavailable" in unavailable_failures
    _, malformed_unavailable = budget.parse_timing_evidence(
        (timing_marker(gpu_available=False, gpu_samples=1, gpu=(-1, -1, -1)),)
    )
    assert any("zero samples" in failure for failure in malformed_unavailable)

    _, sample_failures = budget.evaluate_timing_evidence(
        (timing_marker(cpu_samples=119, gpu_samples=63),),
        contract,
        "game/storage1",
        "opengl",
        "baseline",
    )
    assert any("CPU samples" in failure for failure in sample_failures)
    assert any("GPU samples" in failure for failure in sample_failures)
    _, threshold_failures = budget.evaluate_timing_evidence(
        (timing_marker(cpu=(7000, 20001, 28001), gpu=(5000, 20001, 28001)),),
        contract,
        "game/storage1",
        "opengl",
        "baseline",
    )
    assert sum("exceeds" in failure for failure in threshold_failures) == 4


def test_schema_rejection(base: Path) -> None:
    payload = json.loads(budget.DEFAULT_CONTRACT_PATH.read_text(encoding="utf-8"))
    payload["budgets"].append(dict(payload["budgets"][0]))
    duplicate = base / "duplicate.json"
    duplicate.write_text(json.dumps(payload), encoding="utf-8")
    try:
        budget.load_contract(duplicate)
    except ValueError as exc:
        assert "duplicate budget id" in str(exc)
    else:
        raise AssertionError("duplicate contract identity must fail")

    payload = json.loads(budget.DEFAULT_CONTRACT_PATH.read_text(encoding="utf-8"))
    payload["budgets"][0]["gpu"]["required"] = False
    optional_gpu = base / "optional-gpu.json"
    optional_gpu.write_text(json.dumps(payload), encoding="utf-8")
    try:
        budget.load_contract(optional_gpu)
    except ValueError as exc:
        assert "gpu.required must be true" in str(exc)
    else:
        raise AssertionError("promotion budget must fail closed on unavailable GPU timing")


def test_benchmark_replay(base: Path) -> None:
    harness = load_benchmark()
    relative_base = os.path.relpath(base, ROOT)
    assert harness.resolve_basepath(relative_base) == str(base.resolve())
    assert harness.resolve_basepath(str(base / "missing")) == ""
    contract, binding = budget.load_contract(budget.DEFAULT_CONTRACT_PATH)
    runtime_dir = base / "runtime"
    runtime_dir.mkdir()
    try:
        harness.validate_runtime_dir(runtime_dir, ROOT)
    except ValueError as exc:
        assert ".tmp\\stock-runtime" in str(exc) or ".tmp/stock-runtime" in str(exc)
    else:
        raise AssertionError("alternate benchmark runtime outside .tmp/stock-runtime must fail")
    suffix = ".exe" if os.name == "nt" else ""
    executable = runtime_dir / f"openQ4-client_{harness.host_arch()}{suffix}"
    executable.write_bytes(b"deterministic-client")
    launch_args = harness.parse_args(
        [
            "--cases",
            "mp-q4dm1-listen",
            "--render-api",
            "vk",
            "--set-cvar",
            "r_mode=5",
            "--dry-run",
        ]
    )
    launch_args.runtime_dir_path = runtime_dir
    launch_args.budget_contract = contract
    mp_spec = harness.build_specs(launch_args)[0]
    dry_mp = harness.run_mp_spec(
        ROOT,
        executable,
        base / "mp-plan",
        "",
        "test-run",
        mp_spec,
        0,
        launch_args,
    )

    def cvar_values(arguments: list[str], name: str) -> list[str]:
        return [
            arguments[index + 2]
            for index in range(len(arguments) - 2)
            if arguments[index] in ("+set", "+seta") and arguments[index + 1] == name
        ]

    assert cvar_values(dry_mp["serverArgs"], "ui_autoJoin") == ["1"]
    assert cvar_values(dry_mp["clientArgs"], "ui_autoJoin") == ["1"]
    assert cvar_values(dry_mp["serverArgs"], "si_pure") == ["1"]
    assert cvar_values(dry_mp["serverArgs"], "net_serverAllowServerMod") == ["0"]
    assert cvar_values(dry_mp["serverArgs"], "r_renderApi") == ["vulkan"]
    assert cvar_values(dry_mp["clientArgs"], "r_renderApi") == ["vulkan"]
    assert cvar_values(dry_mp["serverArgs"], "logFileName") == [
        f"logs/{harness.ROLE_LOG_NAME}"
    ]
    assert cvar_values(dry_mp["clientArgs"], "logFileName") == [
        f"logs/{harness.ROLE_LOG_NAME}"
    ]
    server_cfg = (
        base
        / "mp-plan"
        / "savepaths"
        / f"{mp_spec.id}_server"
        / "baseoq4"
        / dry_mp["serverAutoexecCfg"]
    ).read_text(encoding="utf-8")
    client_cfg = (
        base
        / "mp-plan"
        / "savepaths"
        / f"{mp_spec.id}_client"
        / "baseoq4"
        / dry_mp["clientAutoexecCfg"]
    ).read_text(encoding="utf-8")
    assert f"waitMsec {harness.MP_SERVER_CLIENT_GRACE_MSEC}" in server_cfg
    assert f"waitMsec {harness.MP_SERVER_CLIENT_GRACE_MSEC}" not in client_cfg
    for payload in (server_cfg, client_cfg):
        lines = payload.splitlines()
        reset_index = lines.index("framePacingReset")
        for name, value in harness.budget_display_contract()["cvars"].items():
            matches = [line for line in lines[:reset_index] if line.startswith(f"{name} ")]
            assert matches
            assert matches[-1] == f"{name} {value}"
        assert [line for line in lines[:reset_index] if line.startswith("r_mode ")] == [
            "r_mode 5",
            "r_mode -1",
        ]
    for arguments in (dry_mp["serverArgs"], dry_mp["clientArgs"]):
        for name, value in {
            "r_fullscreen": "0",
            "r_borderless": "0",
            "r_borderlessDefaultMigrated": "1",
            "r_fullscreenDesktop": "0",
            "r_windowWidth": "1280",
            "r_windowHeight": "720",
            "r_mode": "-1",
            "r_customWidth": "1280",
            "r_customHeight": "720",
        }.items():
            assert cvar_values(arguments, name) == [value]
    assert dry_mp["displayContract"] == harness.budget_display_contract()
    assert cvar_values(dry_mp["serverArgs"], "fs_devpath") == [
        str(base / "mp-plan" / "savepaths" / f"{mp_spec.id}_server")
    ]
    savepath = base / "evidence" / "savepaths" / "case"
    log = savepath / "baseoq4" / "logs" / "case.log"
    stdout = base / "evidence" / "case.out.txt"
    stderr = base / "evidence" / "case.err.txt"
    screenshot = savepath / "baseoq4" / "screenshots" / "renderer-bench" / "sp_0.tga"
    log.parent.mkdir(parents=True)
    stdout.parent.mkdir(parents=True, exist_ok=True)
    marker = timing_marker()
    log.write_text(
        marker
        + "\nrendererBenchmark capture(preset=baseline samples=256 frame(avg=10 p50=8 p95=11 p99=14 max=15 latest=10 thresholds=20/28 pass=1)\n"
        + "Renderer benchmark: preset=baseline samples=256 frame(avg=10 p50=8 p95=11 p99=14 max=15) thresholds(p95=20 p99=28 pass=1)\n"
        + "Selected renderer tier: gl45\n"
        + "MODE: -1, 1280 x 720 windowed hz:N/A\n",
        encoding="utf-8",
    )
    stdout.write_text(log.read_text(encoding="utf-8"), encoding="utf-8")
    stderr.write_text("", encoding="utf-8")
    write_tga(screenshot)
    spec = harness.RunSpec(
        case_id="sp-storage1",
        mode="SP",
        map_name="game/storage1",
        budget_map_name="game/storage1",
        purpose="test",
        path_name="spawn-static",
        tier="auto",
        maxfps="240",
        swap_interval="0",
        display_mode="windowed",
        shadow_preset="default",
        renderer="best",
        render_api="gl",
    )
    role = harness.evaluate_role_result(
        spec,
        "sp",
        0,
        False,
        1.0,
        savepath,
        "case.log",
        stdout,
        stderr,
        "screenshots/renderer-bench/sp_0.tga",
        None,
        2.0,
        24,
        False,
        True,
        0.0,
        0.0,
        0.0,
        contract,
        "baseline",
    )
    assert role["status"] == "pass", role["missing"]
    result = {
        "id": spec.id,
        "mode": "SP",
        "map": spec.map_name,
        "budgetMap": spec.budget_map_name,
        "expectedBackend": spec.expected_backend,
        "renderApi": spec.render_api,
        "displayContract": harness.budget_display_contract(),
        "purpose": "test",
        "tier": "auto",
        "maxfps": "240",
        "swapInterval": "0",
        "display": "windowed",
        "shadowPreset": "default",
        "renderer": "best",
        "status": "pass",
        "roles": [role],
    }
    harness.attach_result_artifacts(base / "evidence", [result])
    runtime_files = harness.collect_runtime_files(runtime_dir)
    metadata = {
        "generated": "2026-08-19 00:00:00 +0000",
        "host": "test",
        "executable": str(executable),
        "runtime": {
            "path": harness.path_hint(runtime_dir, ROOT),
            "executable": executable.relative_to(runtime_dir).as_posix(),
            "files": runtime_files,
        },
        "git": harness.git_state(ROOT),
        "budgetContract": binding,
        "budgetEnforced": True,
        "budgetDisplayContract": harness.budget_display_contract(),
        "basepath": "",
        "profile": "smoke",
        "benchmarkPreset": "baseline",
        "renderApi": "gl",
        "dryRun": False,
        "autoexecDelayMs": 1000,
        "settleFrames": 360,
        "sampleFrames": 600,
        "sampleMsec": 0,
        "minPacingHz": 0.0,
        "maxP95Ms": 0.0,
        "maxP99Ms": 0.0,
        "profileCvars": {},
        "profileExecCommands": [],
        "launchCvars": {},
        "execCommands": [],
    }
    report_path, _ = harness.write_reports(base / "evidence", [result], metadata)
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert harness.verify_benchmark_report(
        report, report_path.parent, ROOT, runtime_dir, executable, contract, binding
    ) == []

    original_executable = executable.read_bytes()
    executable.write_bytes(b"mutated-runtime-client")
    assert any(
        "runtime file differs" in failure
        for failure in harness.verify_benchmark_report(
            report, report_path.parent, ROOT, runtime_dir, executable, contract, binding
        )
    )
    executable.write_bytes(original_executable)

    original_log = log.read_text(encoding="utf-8")
    log.write_text(original_log.replace("cpuP95Us=11000", "cpuP95Us=12000"), encoding="utf-8")
    changed_log_report = json.loads(json.dumps(report))
    changed_log_role = changed_log_report["results"][0]["roles"][0]
    changed_log_artifact = next(
        item for item in changed_log_role["artifacts"] if item["kind"] == "engineLog"
    )
    changed_log_artifact.update(harness.file_record(log, report_path.parent))
    assert any(
        "recorded renderer budget evidence differs" in failure
        for failure in harness.verify_benchmark_report(
            changed_log_report,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            contract,
            binding,
        )
    )
    log.write_text(original_log, encoding="utf-8")

    log.write_text(
        original_log.replace("MODE: -1, 1280 x 720 windowed", "MODE: -1, 1279 x 720 windowed"),
        encoding="utf-8",
    )
    changed_mode_report = json.loads(json.dumps(report))
    changed_mode_role = changed_mode_report["results"][0]["roles"][0]
    changed_mode_artifact = next(
        item for item in changed_mode_role["artifacts"] if item["kind"] == "engineLog"
    )
    changed_mode_artifact.update(harness.file_record(log, report_path.parent))
    assert any(
        "runtime MODE" in failure or "recorded display evidence differs" in failure
        for failure in harness.verify_benchmark_report(
            changed_mode_report,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            contract,
            binding,
        )
    )
    log.write_text(original_log, encoding="utf-8")

    original_screenshot = screenshot.read_bytes()
    changed_screenshot = bytearray(original_screenshot)
    struct.pack_into("<H", changed_screenshot, 12, 1279)
    screenshot.write_bytes(changed_screenshot)
    changed_screenshot_report = json.loads(json.dumps(report))
    changed_screenshot_role = changed_screenshot_report["results"][0]["roles"][0]
    changed_screenshot_artifact = next(
        item for item in changed_screenshot_role["artifacts"] if item["kind"] == "screenshot"
    )
    changed_screenshot_artifact.update(harness.file_record(screenshot, report_path.parent))
    assert any(
        "engine screenshot is 1279x720" in failure
        or "recorded display evidence differs" in failure
        for failure in harness.verify_benchmark_report(
            changed_screenshot_report,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            contract,
            binding,
        )
    )
    screenshot.write_bytes(original_screenshot)

    for field, value in (
        ("contractId", "wrong-contract-v1"),
        ("path", "tools/validation/wrong.json"),
        ("sha256", "0" * 64),
    ):
        changed_binding_report = json.loads(json.dumps(report))
        changed_binding_report["budgetContract"][field] = value
        assert any(
            "budget contract binding differs" in failure
            for failure in harness.verify_benchmark_report(
                changed_binding_report,
                report_path.parent,
                ROOT,
                runtime_dir,
                executable,
                contract,
                binding,
            )
        )

    changed_contract_payload = json.loads(
        budget.DEFAULT_CONTRACT_PATH.read_text(encoding="utf-8")
    )
    changed_contract_payload["budgets"][0]["cpu"]["p95Us"] = 19000
    changed_contract_path = base / "changed-contract.json"
    changed_contract_path.write_text(json.dumps(changed_contract_payload), encoding="utf-8")
    changed_contract, changed_binding = budget.load_contract(changed_contract_path)
    assert any(
        "budget contract binding differs" in failure
        for failure in harness.verify_benchmark_report(
            report,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            changed_contract,
            changed_binding,
        )
    )

    wrong_backend = json.loads(json.dumps(report))
    wrong_backend["results"][0]["expectedBackend"] = "vulkan"
    assert any(
        "backend/launch render API binding differs" in failure
        for failure in harness.verify_benchmark_report(
            wrong_backend, report_path.parent, ROOT, runtime_dir, executable, contract, binding
        )
    )
    wrong_measurement = json.loads(json.dumps(report))
    wrong_measurement["results"][0]["roles"][0]["budgetEvidence"]["measurement"]["cpu"]["p95Us"] = 1
    assert any(
        "recorded renderer budget evidence differs" in failure
        for failure in harness.verify_benchmark_report(
            wrong_measurement, report_path.parent, ROOT, runtime_dir, executable, contract, binding
        )
    )

    wrong_display = json.loads(json.dumps(report))
    wrong_display["metadata"]["budgetDisplayContract"]["width"] = 1920
    assert any(
        "budget display contract differs" in failure
        for failure in harness.verify_benchmark_report(
            wrong_display, report_path.parent, ROOT, runtime_dir, executable, contract, binding
        )
    )
    wrong_result_display = json.loads(json.dumps(report))
    wrong_result_display["results"][0]["displayContract"]["cvars"]["r_borderless"] = "1"
    assert any(
        "launch display contract differs" in failure
        for failure in harness.verify_benchmark_report(
            wrong_result_display,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            contract,
            binding,
        )
    )
    wrong_runtime_evidence = json.loads(json.dumps(report))
    wrong_runtime_evidence["results"][0]["roles"][0]["displayEvidence"]["runtime"]["width"] = 1279
    assert any(
        "recorded display evidence differs" in failure
        for failure in harness.verify_benchmark_report(
            wrong_runtime_evidence,
            report_path.parent,
            ROOT,
            runtime_dir,
            executable,
            contract,
            binding,
        )
    )

    for invalid_args in (
        ["--width", "1920"],
        ["--height", "1080"],
        ["--display-modes", "fullscreen"],
        ["--set-launch-cvar", "r_windowWidth=1920"],
        ["--set-launch-cvar", "r_renderApi=vk"],
    ):
        try:
            with contextlib.redirect_stderr(io.StringIO()):
                harness.parse_args(invalid_args)
        except SystemExit as exc:
            assert exc.code == 2
        else:
            raise AssertionError(f"noncanonical budget display must fail: {invalid_args}")


def main() -> None:
    test_contract_and_marker()
    harness = load_benchmark()
    runtime_parent = ROOT / ".tmp" / "stock-runtime"
    runtime_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="renderer-runtime-contract-", dir=runtime_parent) as raw:
        runtime = Path(raw)
        assert harness.validate_runtime_dir(runtime, ROOT) == runtime.resolve()
    temp_parent = ROOT / ".tmp" / "renderer-budget-tests"
    temp_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="contract-", dir=temp_parent) as raw:
        base = Path(raw)
        test_schema_rejection(base)
        test_benchmark_replay(base)
    print("renderer_budget_contract: ok")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Prove player-hull support and jump/drop landings on synthetic geometry."""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from mp_bot_navmesh_search import function

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    nav = (ROOT / "src/mpgame/bots/NavMesh.cpp").read_text(encoding="utf-8")
    constants = "\n".join(re.findall(r"^static const (?:int|float)\s+NAV_\w+\s*=\s*[0-9][^\n]+", nav, re.M))
    bodies = "\n".join(function(nav, signature) for signature in (
        "static float NavLandingImpactDelta(",
        "static bool NavFloorSupported(",
        "static bool NavLandingSupported(",
        "bool rvNavMesh::SampleFloor(",
        "bool rvNavMesh::TryStep(",
        "bool rvNavMesh::RunningDropClear(",
        "bool rvNavMesh::LandingContinuationClear(",
        "static bool NavRecoveryFootprintContains(",
        "static bool NavRecoverySharedSupport(",
        "bool rvNavMesh::RecoveryStepClear(",
        "bool rvNavMesh::GapJumpClear(",
        "bool rvNavMesh::JumpTakeoffClear(",
        "bool rvNavMesh::DodgeJumpClear(",
        "bool rvNavMesh::PadLanding(",
        "bool rvNavMesh::LandingSurvivable(",
    ))
    header = (ROOT / "src/mpgame/bots/NavMesh.h").read_text(encoding="utf-8")
    bodies = function(header, "ID_INLINE bool NavAirSteer(").replace("ID_INLINE ", "") + "\n" + bodies
    constants += "\n" + re.search(r"^static const float\s+NAV_AIR_STEER_DEADBAND[^\n]+", header, re.M).group()
    hull = function(nav, "static idTraceModel NavCreateAgentTrace(")
    trace_model = (ROOT / "src/idlib/geometry/TraceModel.cpp").read_text(encoding="utf-8")
    cylinder = function(trace_model, "void idTraceModel::SetupCylinder( const idBounds &")
    fixture = (ROOT / "tools/tests/native/MpBotJumpTrajectoriesFixture.cpp").read_text(encoding="utf-8")
    fixture = fixture.replace("// PRODUCTION_CONSTANTS", constants).replace("// PRODUCTION_FUNCTIONS", bodies)
    fixture = fixture.replace("// PRODUCTION_HULL", cylinder + "\n" + hull)
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("A C++ compiler is required for the jump trajectory proofs")
    output = ROOT / ".tmp"
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mp-bot-jumps-", dir=output) as directory:
        path = Path(directory)
        cpp, exe = path / "jumps.cpp", path / ("jumps.exe" if os.name == "nt" else "jumps")
        cpp.write_text(fixture, encoding="utf-8")
        command = [compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wno-unused-const-variable",
                   str(cpp), "-o", str(exe)]
        if os.environ.get("MP_MATCH_TEST_SANITIZERS") == "1":
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command, cwd=path, check=True)
        subprocess.run([str(exe)], cwd=path, check=True)
        registry = (ROOT / "tools/tests/native/MpBotTransportFixture.cpp").read_text(encoding="utf-8")
        registry_bodies = "\n".join(function(nav, signature) for signature in (
            "void rvNavMesh::AddOffMeshLinks(",
            "void rvNavMesh::SealTransportVolumes(",
            "int rvNavMesh::TransportAt(",
            "bool rvNavMesh::NaturalPadLanding(",
            "bool rvNavMesh::IsSteeredPadLanding(",
        ))
        cpp.write_text(registry.replace("// PRODUCTION_FUNCTIONS", registry_bodies), encoding="utf-8")
        subprocess.run(command, cwd=path, check=True)
        subprocess.run([str(exe)], cwd=path, check=True)


if __name__ == "__main__":
    main()

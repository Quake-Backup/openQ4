#!/usr/bin/env python3
"""Execute production bot decisions with deterministic navigation/perception fixtures.

The fixture in tools/tests/native/MpBotRegressionsFixture.cpp supplies a small
mock world - players, items, projectiles, a cost-field navmesh, item timers and
noises - and the decision functions are spliced in verbatim from production, so
there is no second implementation of goal policy to drift.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    """Return one function, from its signature to the matching closing brace."""
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main() -> None:
    bot = (ROOT / "src/mpgame/bots/Bot.cpp").read_text(encoding="utf-8")
    combat = (ROOT / "src/mpgame/bots/BotCombat.cpp").read_text(encoding="utf-8")
    network = (ROOT / "src/mpgame/Game_network.cpp").read_text(encoding="utf-8")
    constants = "\n".join(re.findall(r"^static const (?:int|float)\s+BOT_\w+\s*=\s*[0-9][^\n]+", bot, re.M))
    constants += "\n" + "\n".join(re.findall(
        r"^static const (?:int|float)\s+BOTCOMBAT_\w+[^\n]+", combat, re.M))
    constants += "\n" + "\n".join(re.findall(
        r"^static (?:const int|int)\s+(?:MAX_UNRELIABLE_BATCH_BYTES|UNRELIABLE_OVERFLOW_WARN_INTERVAL|unreliableOverflowWarnTime)[^\n]+",
        network, re.M))
    bodies = "\n".join(function(bot, signature) for signature in (
        "void rvBot::UpdateEnemy(",
        "int rvBot::EnemyLastKnownTime(",
        "void rvBot::UpdateHearing(",
        "bool rvBot::Repath(",
        "float rvBot::PathDistanceRemaining(",
        "void rvBot::AbandonGoal(",
        "int rvBot::ItemPendingMsec(",
        "idEntity *rvBot::PickItemGoal(",
        "void rvBot::UpdateGoal(",
    ))
    bodies += function(combat, "bool BotCombatFindIncomingProjectileThreat(")
    bodies += "\n".join(function(network, signature) for signature in (
        "void idGameLocal::QueueUnreliableMessage(",
        "void idGameLocal::SendUnreliableMessage(",
        "void idGameLocal::SendUnreliableMessagePVS(",
    ))
    harness = (ROOT / "tools/tests/native/MpBotRegressionsFixture.cpp").read_text(encoding="utf-8")
    harness = harness.replace("// PRODUCTION_CONSTANTS", constants).replace("// PRODUCTION_FUNCTIONS", bodies)
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("A C++ compiler is required for bot regressions")
    output = ROOT / ".tmp"
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mp-bot-regressions-", dir=output) as directory:
        path = Path(directory)
        cpp, exe = path / "bots.cpp", path / ("bots.exe" if os.name == "nt" else "bots")
        cpp.write_text(harness, encoding="utf-8")
        command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-const-variable",
                   "-Wno-unused-parameter", str(cpp), "-o", str(exe)]
        if os.environ.get("MP_MATCH_TEST_SANITIZERS") == "1":
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command, cwd=path, check=True)
        subprocess.run([str(exe)], cwd=path, check=True)


if __name__ == "__main__":
    main()

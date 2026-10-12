#!/usr/bin/env python3
"""Measure multiplayer bot quality and cost on a stock map with no input.

A hidden, windowed listen server hosts a fixed roster of bots for a fixed
time and the engine log is reduced to the numbers that describe a bot
implementation: how often bots died to the map rather than to a player,
how often navigation failed (no route, stuck, abandoned goals, timed-out
traversals), how much fighting happened, and what the server frame cost.
`bot_profile` adds the bots' own CPU time and navigation-query counts when
the build has it; `com_speeds` supplies the game-frame time either way, so
two builds can be compared on the same map.

The local client is an untargetable, invulnerable observer: public matches
need one active human, and this one never fights or blocks anything.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import statistics
import subprocess
import time
from pathlib import Path

from mp_round_remote_smoke import verify_runtime_is_current

try:
    import psutil
except ImportError:  # CPU sampling is optional; the log metrics are not
    psutil = None

ROOT = Path(__file__).resolve().parents[2]
NAMES = ("Cortez", "Rhodes", "Sledge", "Voss", "Strauss", "Kane", "Morris", "Tetzlaff",
         "Marsh", "Hollenbeck", "Sorg", "Gunner")

# player.def's obituary texts and openQ4's liquid deaths (strings/*_openq4.lang).
OBITUARY_KILLED_BY = (r"^(\S+) was (?:pummeled|machinegunned|hyperblasted|nailed|railed|toasted|killed|"
                      r"telefragged|compressed into a singularity) by (\S+)$")
OBITUARY_POSSESSIVE = r"^(\S+) (?:ate|rode|accepted) (\S+?)'s (?:boomstick|pineapple|rocket|discharge)$"
OBITUARY_WORLD = r"^(\S+) (?:died|drowned|dissolved in slime|burned in lava)$"

NAV_EVENTS = {
    "no_route": re.compile(r"^bot [^:]+: no route from", re.M),
    "stuck": re.compile(r"^bot [^:]+: stuck at", re.M),
    "traversal_timeout": re.compile(r"^bot [^:]+: (?:[a-z ]+ )?traversal timed out", re.M),
    "goal_give_up": re.compile(r"^bot [^:]+: giving up on goal", re.M),
    "pickup_refused": re.compile(r"^bot [^:]+: '[^']*' did not pick up", re.M),
    "traversal_done": re.compile(r"^bot [^:]+: completed (?:jump|drop|jump pad|teleport)", re.M),
    "ordered": re.compile(r"^bot [^:]+: ordered to", re.M),
    # A route replaced because the bot was knocked or carried onto another
    # floor.  Each one is a stuck report that no longer has to happen.
    "off_route": re.compile(r"^bot [^:]+: off route at", re.M),
    # Rate-limited to one a second between them, so floors, not counts.
    "snap_report": re.compile(r"^rvNavMesh: no node near route", re.M),
    "leg_report": re.compile(r"^rvNavMesh: final leg .* refused", re.M),
}


def strip_colours(text: str) -> str:
    return re.sub(r"\^[0-9a-zA-Z]", "", text)


def world_deaths(text: str, bot_names: set[str]) -> list[str]:
    direct = [name for name in re.findall(OBITUARY_WORLD, text, re.M) if name in bot_names]
    killed = re.findall(OBITUARY_KILLED_BY, text, re.M)
    return direct + [victim for victim, killer in killed if killer == "world" and victim in bot_names]


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * len(ordered)))]


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def log_has_fatal(log_path: Path) -> bool:
    try:
        with log_path.open("rb") as source:
            source.seek(0, os.SEEK_END)
            source.seek(max(0, source.tell() - 16384))
            tail = source.read().decode("utf-8", errors="replace")
    except OSError:
        return False
    return re.search(r"^FATAL:", strip_colours(tail), re.M) is not None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-dir", type=Path, default=ROOT / ".install")
    parser.add_argument("--executable-name", default="openQ4-client_x64.exe" if os.name == "nt" else "openQ4-client_x64")
    parser.add_argument("--basepath", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / ".tmp/mp-bot-audit")
    parser.add_argument("--gametype", default="DM")
    parser.add_argument("--map", dest="map_name", default="mp/q4dm1")
    parser.add_argument("--bots", type=int, default=8)
    parser.add_argument("--skill", type=int, choices=range(1, 6), default=4)
    parser.add_argument("--debug", type=int, choices=range(1, 3), default=1,
                        help="2 includes bot status, damage and movement diagnostics")
    parser.add_argument("--max-world-deaths", type=int, default=None,
                        help="fail the audit if deaths to the map exceed this limit")
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--port", type=int, default=28972)
    parser.add_argument("--timeout", type=int, default=0, help="defaults to seconds + 150")
    parser.add_argument("--profile-period", type=int, default=10)
    parser.add_argument("--speeds", action="store_true", help="log com_speeds frame timings")
    parser.add_argument("--set", action="append", default=[], metavar="CVAR=VALUE",
                        help="extra server cvar, e.g. si_warmup=1 for Tourney's bracket; repeatable")
    parser.add_argument("--team-order", default="",
                        help="team chat the observer says once the window opens, e.g. 'everyone follow me'; "
                             "the report then tracks how far its bot team-mates keep from it")
    args = parser.parse_args()
    if not 2 <= args.bots <= len(NAMES):
        parser.error(f"use 2 to {len(NAMES)} bots")
    timeout = args.timeout or args.seconds + 150
    runtime, output = args.runtime_dir.resolve(), args.output_dir.resolve()
    exe = runtime / args.executable_name
    if not exe.is_file() or not (args.basepath / "q4base").is_dir():
        parser.error("the staged client and retail q4base assets are required")
    verify_runtime_is_current(runtime, args.executable_name)
    identity_paths = [exe, *sorted((runtime / "baseoq4").glob("game-mp*")),
                      runtime / "baseoq4/pak0.pk4", runtime / "baseoq4/pak1.pk4"]
    runtime_identity = {str(path): file_sha256(path)
                        for path in identity_paths
                        if path.is_file() and path.suffix.lower() not in (".pdb", ".lib", ".exp")}

    save = output / "save"
    game = save / "baseoq4"
    game.mkdir(parents=True, exist_ok=True)
    log_path = game / "logs/openq4.log"
    if log_path.exists():
        log_path.unlink()

    script = ["notarget", "god", f"set bot_debug {args.debug}", f"set bot_profile {args.profile_period}",
              *[f"addbot {name} {args.skill} exact" for name in NAMES[:args.bots]],
              "waitMsec 5000", "botlist", "navmesh info", "echo MP_BOT_AUDIT_BEGIN"]
    if args.team_order:
        script.append(f'sayTeam "{args.team_order}"')
    if args.speeds:
        script.append("set com_speeds 1")
    for _ in range(args.seconds // 5):
        script += ["waitMsec 5000", "openq4_reportMPState"]
    if args.speeds:
        script.append("set com_speeds 0")
    script += ["echo MP_BOT_AUDIT_END", "quit"]
    (game / "bot_audit.cfg").write_text("\n".join(script) + "\n", encoding="utf-8")

    cvars = {
        "win_allowMultipleInstances" if os.name == "nt" else "sys_allowMultipleInstances": "1",
        "sys_consoleWindow": "0", "in_tty": "0", "in_mouse": "0", "in_joystick": "0",
        "r_fullscreen": "0", "r_borderless": "0", "r_fullscreenDesktop": "0",
        "r_borderlessDefaultMigrated": "1", "r_hiddenWindow": "1", "s_noSound": "1",
        "r_mode": "-1", "r_customWidth": "640", "r_customHeight": "480", "r_renderApi": "gl",
        "r_swapInterval": "0", "com_maxfps": "60", "ui_retained": "0",
        "fs_basepath": str(args.basepath.resolve()), "fs_savepath": str(save), "fs_devpath": str(save),
        "fs_game": "baseoq4", "com_gameMode": "MP", "logFile": "2", "logFileName": "logs/openq4.log",
        "developer": "1", "ui_autoJoin": "1", "ui_spectate": "Play", "ui_name": "BotAuditObserver",
        "net_serverDedicated": "0", "net_LANServer": "1", "net_ip": "127.0.0.1", "net_enableIPv6": "0",
        "net_allowCheats": "1", "sv_cheats": "1",
        "net_port": str(args.port), "si_pure": "0", "si_gameType": args.gametype,
        "si_maxPlayers": str(args.bots + 2), "si_minPlayers": "2", "si_warmup": "0", "si_useReady": "0",
        "si_fragLimit": "999", "si_captureLimit": "999", "si_timeLimit": "0",
        "g_matchProfile": "casual", "bot_enable": "1", "bot_minPlayers": "0", "bot_pause": "0",
        "bot_characters": "1", "bot_skillVariance": "0", "bot_chat": "0",
        "com_skipLoadingContinue": "1", "g_autoExecAfterMapLoad": "bot_audit.cfg",
        "g_autoExecAfterMapLoadDelayMs": "500",
    }
    for extra in args.set:
        name, _, value = extra.partition("=")
        if not name or not _:
            parser.error(f"--set expects CVAR=VALUE, not {extra!r}")
        cvars[name] = value
    command = [str(exe)]
    for name, value in cvars.items():
        command += ["+set", name, value]
    command += ["+spawnServer", args.map_name]
    (output / "launch.json").write_text(json.dumps(command, indent=2), encoding="utf-8")

    started = time.time()
    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    # (wall seconds, process CPU seconds) samples, so the audit window's CPU
    # cost can be separated from loading and navmesh generation.
    cpu_samples: list[tuple[float, float]] = []
    with (output / "console.log").open("w", encoding="utf-8") as console:
        process = subprocess.Popen(command, cwd=runtime, stdout=console, stderr=subprocess.STDOUT,
                                   creationflags=creationflags)
        watched = psutil.Process(process.pid) if psutil else None
        code = None
        deadline = started + timeout
        while code is None:
            try:
                code = process.wait(timeout=0.5)
            except subprocess.TimeoutExpired:
                # The fatal-error UI can keep a hidden process alive forever.
                # Its own engine log is sufficient evidence; stop only this
                # audit's child instead of waiting out the startup timeout.
                if log_has_fatal(log_path):
                    process.kill()
                    code = process.wait()
                    break
                if time.time() > deadline:
                    process.kill()
                    process.wait()
                    code = -1
                    break
                if watched:
                    try:
                        times = watched.cpu_times()
                        cpu_samples.append((time.time(), times.user + times.system))
                    except psutil.Error:
                        pass

    log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
    clean = strip_colours(log)
    warmup, _, measured = clean.partition("\nMP_BOT_AUDIT_BEGIN")
    sample = measured.partition("\nMP_BOT_AUDIT_END")[0]

    failures = []
    if code != 0 or "\nMP_BOT_AUDIT_END" not in clean:
        failures.append(f"server failed to complete (exit {code})")
    errors = re.findall(r"^.*(?:ERROR:|FATAL:|Assertion failed).*$", clean, re.M)
    failures.extend(errors)

    bot_names = set(NAMES[:args.bots])
    # Console obituaries come from player.def's per-weapon texts ("was railed
    # by", "rode X's rocket", ...), its generic "was killed by" - which names
    # "world" as the killer of a fall or a kill volume - and openQ4's liquid
    # texts.  Matching the generic text alone counted every death to the map
    # as a kill and missed every weapon kill.
    obituaries = re.findall(OBITUARY_KILLED_BY, sample, re.M) + re.findall(OBITUARY_POSSESSIVE, sample, re.M)
    died = world_deaths(sample, bot_names)
    warmup_died = world_deaths(warmup, bot_names)
    all_died = world_deaths(clean, bot_names)
    # Rates remain tied to the measured window. Safety qualification includes
    # startup too: a lethal route is no safer because a bot took it before BEGIN.
    if args.max_world_deaths is not None and len(all_died) > args.max_world_deaths:
        failures.append(f"{len(all_died)} complete-run world deaths "
                        f"({len(warmup_died)} during warmup, {len(died)} measured) "
                        f"exceed the limit of {args.max_world_deaths}")
    suicides = [m for m in re.findall(r"^(\S+) committed suicide$", sample, re.M) if m in bot_names]
    suicides += [victim for victim, killer in obituaries if killer == victim and victim in bot_names]
    kills = [(victim, killer) for victim, killer in obituaries if killer not in ("world", victim)]

    nav = {name: len(pattern.findall(sample)) for name, pattern in NAV_EVENTS.items()}

    frames = [float(v) for v in re.findall(r"^frame:\s*\d+\s+all:\s*\d+\s+gfr:\s*(\d+)", sample, re.M)]
    profile = re.findall(
        r"^botprofile: (\d+) bots (\d+) frames think avg ([\d.]+) ms max ([\d.]+) ms \| "
        r"paths (\d+) fail (\d+) quick (\d+) repair (\d+) expand ([\d.]+)/search \| "
        r"fields (\d+) settle ([\d.]+)/field \| nav ([\d.]+) ms/frame(?: \(fields ([\d.]+)\))?"
        r"(?: \| fail snap (\d+) search (\d+) leg (\d+))?", sample, re.M)
    profile_rows = [dict(zip(("bots", "frames", "think_avg_ms", "think_max_ms", "paths", "path_fail",
                              "quick_reject", "repair", "expand_per_search", "fields",
                              "settle_per_field", "nav_ms_per_frame", "field_ms_per_frame", "fail_snap",
                              "fail_search", "fail_leg"), (float(v) if v else 0.0 for v in row)))
                    for row in profile]

    poses: dict[str, list[tuple[float, ...]]] = {}
    for slot, coordinates in re.findall(r"MP_POSE slot=(\d+) entity=\S+ origin=([^\r\n]+?) hidden=", sample):
        poses.setdefault(slot, []).append(tuple(map(float, coordinates.split())))
    live = set(re.findall(r"MP_PLAYER slot=(\d+) bot=1 ", sample))
    path_length = {}
    for slot in live:
        points = poses.get(slot, [])
        path_length[slot] = round(sum(sum((a - b) ** 2 for a, b in zip(p, q)) ** 0.5
                                      for p, q in zip(points, points[1:])), 1)

    # Mean distance from the observer to its bot team-mates at each report: an
    # order to follow should pull it down, one to defend should hold it.
    observer_distance = []
    for block in sample.split("MP_STATE time=")[1:]:
        local = re.search(r"MP_LOCAL client=(\d+) ", block)
        teams = dict(re.findall(r"MP_PLAYER slot=(\d+) bot=\d team=(\d+) ", block))
        bots_in_block = set(re.findall(r"MP_PLAYER slot=(\d+) bot=1 ", block))
        where = {slot: tuple(map(float, c.split())) for slot, c in
                 re.findall(r"MP_POSE slot=(\d+) entity=\S+ origin=([^\r\n]+?) hidden=", block)}
        if not local or local.group(1) not in where:
            continue
        me = local.group(1)
        mates = [slot for slot in bots_in_block if teams.get(slot) == teams.get(me) and slot in where]
        if mates:
            observer_distance.append(round(statistics.fmean(
                sum((a - b) ** 2 for a, b in zip(where[slot], where[me])) ** 0.5 for slot in mates), 1))

    navmesh = re.search(r"^rvNavMesh: (\d+) nodes, (\d+) links[^\n]*in (\d+) ms", clean, re.M)

    # The server's own clock against the real-time report interval: below 1
    # the simulation could not keep up with real time.
    game_times = [int(v) for v in re.findall(r"^MP_STATE time=(\d+) ", sample, re.M)]
    sim_speed = None
    if len(game_times) >= 2:
        sim_speed = round((game_times[-1] - game_times[0]) / (5000.0 * (len(game_times) - 1)), 3)

    # CPU over the audit window: the last `seconds` before the run ended.
    window_cpu = None
    if len(cpu_samples) >= 2:
        end_wall, end_cpu = cpu_samples[-1]
        start = next((s for s in cpu_samples if s[0] >= end_wall - args.seconds), cpu_samples[0])
        if end_wall > start[0]:
            window_cpu = round((end_cpu - start[1]) / (end_wall - start[0]), 3)

    loop_frames = [float(v) for v in re.findall(r"^frame:\s*\d+\s+all:\s*(\d+)", sample, re.M)][1:]
    minutes = max(args.seconds / 60.0, 1e-6)
    bot_minutes = minutes * args.bots
    report = {
        "map": args.map_name, "gametype": args.gametype, "bots": args.bots, "skill": args.skill,
        "seconds": args.seconds, "exit_code": code, "elapsed_seconds": round(time.time() - started, 1),
        "runtime_identity": runtime_identity,
        "navmesh": {"nodes": int(navmesh.group(1)), "links": int(navmesh.group(2)),
                    "build_ms": int(navmesh.group(3))} if navmesh else None,
        "navmesh_line": next(iter(re.findall(r"^rvNavMesh: [^\n]*nodes[^\n]*$", clean, re.M)), ""),
        "kills": len(kills),
        "world_deaths": len(died), "suicides": len(suicides),
        "warmup_world_deaths": len(warmup_died),
        "complete_run_world_deaths": len(all_died),
        "world_deaths_per_bot_minute": round(len(died) / bot_minutes, 3),
        "suicides_per_bot_minute": round(len(suicides) / bot_minutes, 3),
        "kills_per_bot_minute": round(len(kills) / bot_minutes, 3),
        "nav_events": nav,
        "nav_failures_per_bot_minute": round((nav["no_route"] + nav["stuck"] + nav["traversal_timeout"] +
                                              nav["goal_give_up"]) / bot_minutes, 3),
        "sim_speed": sim_speed,
        "cpu_cores_busy": window_cpu,
        "main_loop_frame_ms": {"frames": len(loop_frames),
                               "mean": round(statistics.fmean(loop_frames), 2) if loop_frames else None,
                               "p95": percentile(loop_frames, 0.95), "max": max(loop_frames, default=0)},
        "game_frame_ms": {"frames": len(frames),
                          "mean": round(statistics.fmean(frames), 3) if frames else None,
                          "p95": percentile(frames, 0.95), "max": max(frames, default=0)},
        "bot_profile": profile_rows,
        "bot_path_length": path_length,
        "team_order": args.team_order,
        "teammate_distance_to_observer": observer_distance,
        "failures": failures,
        "log": str(log_path),
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    summary = {key: report[key] for key in ("map", "gametype", "bots", "navmesh_line", "kills",
                                             "world_deaths", "warmup_world_deaths", "complete_run_world_deaths",
                                             "suicides", "nav_events",
                                             "nav_failures_per_bot_minute", "sim_speed", "cpu_cores_busy",
                                             "main_loop_frame_ms", "failures")}
    summary["think_avg_ms"] = [row["think_avg_ms"] for row in profile_rows]
    print(json.dumps(summary, indent=2))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())

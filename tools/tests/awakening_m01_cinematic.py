#!/usr/bin/env python3
"""Bounded, input-free m01 playback using the engine's screenshot command.

Requires a matching staged runtime and user-supplied retail/Awakening content.
Never changes installed content, captures the desktop or sends input events.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--retail', type=Path, required=True)
    parser.add_argument('--awakening', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path, help='Reuse a task-owned generated asset cache')
    parser.add_argument('--duration', type=int, default=330)
    parser.add_argument('--capture-start', type=int, default=140)
    parser.add_argument('--interval', type=int, default=2)
    parser.add_argument('--overlay', type=Path, help='Candidate pak0 source tree, copied into this test profile')
    parser.add_argument('--audio', action='store_true', help='Record via the OpenAL Soft wave writer; no speaker output')
    parser.add_argument('--reduced-flashing', action='store_true')
    parser.add_argument('--skip', action='store_true')
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--focus', action='store_true', help='Test-only map script enters the cabin directly')
    parser.add_argument('--save-roundtrip', action='store_true')
    args = parser.parse_args()
    if args.duration <= 0 or args.interval <= 0:
        parser.error('Duration and interval must be positive.')
    runtime, output = args.runtime.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    game = output / 'profile' / 'q4xbase'
    game.mkdir(parents=True, exist_ok=True)
    overlay_identity = {}
    if args.overlay:
        for source in sorted(args.overlay.resolve().rglob('*')):
            if source.is_file():
                relative = source.relative_to(args.overlay.resolve())
                destination = game / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
                overlay_identity[str(relative)] = hashlib.sha256(source.read_bytes()).hexdigest()
    sidecar = game / 'maps/game/m01_stranarus_trench1.script'
    if not args.focus and sidecar.exists():
        parser.error('Use a fresh output directory for a full run; this profile contains the focus sidecar.')
    if args.focus:
        # A test-only sidecar is compiled by worldspawn. Production runs exercise
        # the original hangar/flyout and do not install this sidecar.
        setup = ['void main() {', 'sys.wait(0.5);',
                 'sys.killthread("m04_stranarus_trench::intro_unit1_seq_1_main");',
                 'sys.killthread("intro_unit1_seq_1_main");',
                 '$intro_unit1_seq_1_trigger_seq_2.remove();']
        for name in ['tak', 'navajas', 'mitchell', 'darnell', 'townsend', 'atkins', 'dekard']:
            actor = '$intro_unit1_seq_6_' + name
            setup += [f'sys.trigger({actor});',
                      f'{actor}.hideSurface("models/weapons/mp_guns/w_mg_lo");',
                      f'aiScriptedAnimLoop({actor}, "st_dropship_chair", 8);']
        for name in ['markswell', 'markswell_sitting']:
            setup += [f'$intro_unit1_seq_6_{name}.hideSurface("models/weapons/mp_guns/w_mg_lo");']
        setup += ['sys.trigger($intro_unit1_seq_5_trigger_seq_6);', '}']
        sidecar.parent.mkdir(parents=True, exist_ok=True)
        sidecar.write_text('\n'.join(setup) + '\n', encoding='ascii')
    exe = runtime / 'openQ4-client_x64.exe'
    settings = {
        'win_allowMultipleInstances': '1', 'r_hiddenWindow': '1',
        'r_fullscreen': '0', 'r_fullscreenDesktop': '0',
        'r_borderless': '0', 'r_borderlessDefaultMigrated': '1',
        'r_mode': '-1', 'r_customWidth': '960', 'r_customHeight': '540',
        'r_windowWidth': '960', 'r_windowHeight': '540', 'r_renderApi': 'gl',
        'in_mouse': '0', 'in_joystick': '0', 'in_joystickRumble': '0',
        's_volume': '1' if args.audio else '0', 's_noSound': '0',
        's_muteUnfocused': '0', 'ui_reducedFlashing': str(int(args.reduced_flashing)),
        'logFile': '2', 'logFileName': 'logs/m01.log',
        'fs_basepath': str(args.retail.resolve()),
        'fs_awakeningpath': str(args.awakening.resolve()),
        'fs_savepath': str(game.parent), 'fs_devpath': str(game.parent),
        'fs_cachepath': str(args.cache.resolve() if args.cache else output / 'cache'),
        'fs_game': 'q4xbase', 'si_gameType': 'singleplayer',
        'com_skipLoadingContinue': '1', 'com_loadingContinueAutoAdvance': '1',
        'com_maxfps': '60', 'g_autoSkipCinematics': str(int(args.skip)),
        'g_awakeningM01Cinematic': str(int(not args.baseline)),
        'developer': '1', 'g_autoExecAfterMapLoad': 'm01-check.cfg',
        'g_autoExecAfterMapLoadDelayMs': '500',
    }
    commands = ['echo M01_CHECK_BEGIN']
    for second in range(0, args.duration, args.interval):
        if second >= args.capture_start:
            if args.audio:
                commands += ['listPlayingSounds']
            commands += [f'echo M01_CAPTURE_{second:03d}',
                         f'screenshot "screenshots/m01-{second:03d}.tga"']
        commands += [f'waitMsec {args.interval * 1000}']
    commands += ['listThreads', 'echo M01_CHECK_PLAYBACK_END']
    if args.save_roundtrip:
        commands += ['set g_autoExecAfterMapLoad ""', 'saveGame m01_direction',
                     'waitMsec 500', 'loadGame m01_direction', 'waitMsec 2000',
                     'screenshot "screenshots/m01-restored.tga"', 'listThreads',
                     'echo M01_CHECK_RESTORED']
    commands += ['echo M01_CHECK_COMPLETE', 'quit']
    (game / 'm01-check.cfg').write_text('\n'.join(commands) + '\n', encoding='ascii')
    command = [str(exe)]
    for key, value in settings.items():
        command += ['+set', key, value]
    command += ['+map', 'game/m01_stranarus_trench1']
    identity = {}
    for path in [exe, runtime/'baseoq4/game-sp_x64.dll', runtime/'baseoq4/pak0.pk4']:
        identity[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    environment = os.environ.copy()
    audio_path = output / 'm01-mix.wav'
    if args.audio:
        if audio_path.exists():
            parser.error('Use a fresh output directory for audio; never overwrite a recording.')
        # OpenAL Soft's documented wave backend renders directly to a file.
        # https://github.com/kcat/openal-soft/blob/master/alsoftrc.sample
        config = output / 'alsoft.ini'
        config.write_text('[general]\nchannels=stereo\nsample-type=int16\nfrequency=48000\n'
                          + '[wave]\nfile=' + str(audio_path) + '\n', encoding='utf-8')
        environment.update(ALSOFT_DRIVERS='wave', ALSOFT_CONF=str(config),
                           ALSOFT_LOGFILE=str(output / 'openal.log'), ALSOFT_LOGLEVEL='3')
    beat_observations = {}
    started = time.monotonic()
    with (output / 'console.log').open('w', encoding='utf-8') as console:
        process = subprocess.Popen(command, cwd=runtime, stdin=subprocess.DEVNULL,
                                   stdout=console, stderr=subprocess.STDOUT,
                                   startupinfo=startup, env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
        print(f'Hidden playback PID {process.pid}; output {output}', flush=True)
        (output/'launch.json').write_text(json.dumps({'pid': process.pid, 'command': command,
                                                    'identity': identity, 'overlay_identity': overlay_identity}, indent=2))
        while process.poll() is None:
            log_path = game / 'logs/m01.log'
            current_log = log_path.read_text(errors='replace') if log_path.exists() else ''
            for beat, game_time in re.findall(r'oq4_m01: (\w+) @ ([\d.]+)', current_log):
                if beat not in beat_observations:
                    beat_observations[beat] = {'game_time': float(game_time),
                        'elapsed_seconds': round(time.monotonic()-started, 2),
                        'audio_bytes': audio_path.stat().st_size if args.audio and audio_path.exists() else None}
            if 'ERROR:' in current_log or time.monotonic() - started > args.duration + 130:
                process.kill()  # only the process this harness created
                break
            time.sleep(1)
        code = process.wait()
    log_path = game / 'logs/m01.log'
    log = log_path.read_text(errors='replace') if log_path.exists() else ''
    beats = re.findall(r'oq4_m01: (\w+) @ ([\d.]+)', log)
    failures = re.findall(r'^.*(?:ERROR:|Compile failed|script runtime error).*$',
                          log, re.M | re.I)
    complete = 'M01_CHECK_COMPLETE' in log
    handoff = args.baseline or any(beat == 'handoff_complete' for beat, _ in beats)
    saved = not args.save_roundtrip or (
        (game/'savegames/m01_direction.save').exists()
        and 'loadingSaveGame=1' in log and 'M01_CHECK_RESTORED' in log)
    audio_ok = not args.audio or (audio_path.exists() and audio_path.stat().st_size > 48000
        and 'OpenAL Soft on Wave File Writer' in (output/'openal.log').read_text(errors='replace'))
    result = {'exit_code': code, 'elapsed_seconds': round(time.monotonic()-started, 2),
              'focus': args.focus, 'skip': args.skip, 'baseline': args.baseline,
              'complete': complete, 'handoff': handoff, 'saved': saved,
              'audio': str(audio_path) if args.audio else None, 'audio_ok': audio_ok, 'overlay_identity': overlay_identity,
              'beats': beats, 'beat_observations': beat_observations, 'errors': failures, 'identity': identity}
    (output/'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2), flush=True)
    return 0 if code == 0 and complete and handoff and saved and audio_ok and not failures else 1


if __name__ == '__main__':
    raise SystemExit(main())

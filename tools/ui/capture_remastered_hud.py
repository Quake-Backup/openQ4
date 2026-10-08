#!/usr/bin/env python3
"""Capture the Remastered HUD in windowed gameplay without host input.

Drives the stock HUD GUIs through semantic developer commands (ui_hudTest
state/event, testStatusBar, testWeaponWheel) - the Remastered peer receives
the same state and events - and takes engine screenshots in a hidden window
with mouse and controller input disabled.

    python tools/ui/capture_remastered_hud.py --assets <Quake 4> --output .tmp/<new dir> [--size 1280x720]
"""
import argparse
import json
import re
from pathlib import Path

import capture_retained_screens as capture

ROOT = Path(__file__).resolve().parents[2]


def shot(name):
    return f'screenshot "screenshots/{name}.tga"'


def hud(gui, *ops):
    return [f'ui_hudTest {gui} {op}' for op in ops]


def equip(slot):
    return ['testWeaponWheel open', f'testWeaponWheel slot {slot}', 'testWeaponWheel release']


def on_foot(gui):
    """The single-player on-foot stages, shared by the Marine and Strogg HUDs."""
    return [
        'set g_autoExecAfterMapLoad ""', 'notarget', 'god', 'give all', 'waitMsec 1500',
        'testStatusBar stats 100 57', 'waitMsec 500', shot('base'),
        *equip(5), 'waitMsec 1600', *hud(gui, 'event weaponChange'), 'waitMsec 150', shot('strip'),
        *hud(gui, 'state itemtext "Armor Shard"', 'state itemicon gfx/guis/hud/icons/item_armorshard.tga', 'event itemPickup'),
        'waitMsec 120',
        *hud(gui, 'event itemPickup'), 'waitMsec 120',
        *hud(gui, 'state itemtext "Large Health"', 'state itemicon gfx/guis/hud/icons/item_health.tga', 'event itemPickup'),
        'waitMsec 200', shot('pickups'),
        *hud(gui, 'state rhinochatter 1', 'event radioChatterUp'), 'waitMsec 120', shot('radio-shrinking'),
        'waitMsec 600', shot('radio'),
        'testStatusBar stats 20 0', 'waitMsec 300', shot('low'), 'waitMsec 250', shot('low-pulse'),
        'testStatusBar stats 125 100', 'waitMsec 60', shot('gain'), 'waitMsec 500',
        *hud(gui, 'event playerHit'), 'waitMsec 40', shot('hit'),
        *hud(gui, 'state bracket_left 230', 'state bracket_top 170', 'state bracket_width 180', 'state bracket_height 140',
             'event showBrackets'), 'waitMsec 100', shot('brackets'),
        *hud(gui, 'event hideBrackets', 'event showExit', 'event saveMessage'), 'waitMsec 300', shot('notices'),
        *hud(gui, 'event radioChatterDown', 'event hideExit'),
        'testStatusBar stats 100 100', 'testStatusBar boss 70', 'waitMsec 100', 'testStatusBar stats 55 100', 'waitMsec 300',
        shot('boss'), 'testStatusBar boss 20 warn', 'waitMsec 300', shot('boss-warn'),
        'testStatusBar stats 125 100',
        'set ui_hudScale 1.25', 'set ui_hudSafeInset 4', 'waitMsec 300', shot('scaled'),
        'set ui_hudScale 1', 'set ui_hudSafeInset 0',
        *equip(0), 'waitMsec 1300', shot('unlimited'),
        'set ui_hudLayout 0', 'waitMsec 400', shot('classic'), 'set ui_hudLayout 1', 'waitMsec 400', shot('restored'),
    ]


def in_vehicle(vehicle):
    """A vehicle HUD (guis/vehicles/hud.gui): the player boards through the
    actor's enterVehicle script event, then the stock events drive the rest."""
    gui = 'guis/vehicles/hud.gui'
    return [
        # A map's opening objective notice hides vehicle HUDs for 5 s.
        'set g_autoExecAfterMapLoad ""', 'notarget', 'god', 'waitMsec 9000',
        # Driving off closes a notice the player has not moved away from (64 u).
        f'script "$player1.enterVehicle(${vehicle})"', 'openq4_testInput 1500 forward 127', 'waitMsec 700', shot('veh-enter'),
        'waitMsec 1700', shot('veh-line1'), 'waitMsec 700', shot('veh-line2'),
        *hud(gui, 'event vehicleHit'), 'waitMsec 60', shot('veh-hit'),
        *hud(gui, 'event electricWarningOn'), 'waitMsec 150', shot('veh-elec'), *hud(gui, 'event electricWarningOff'),
        *hud(gui, 'state rhinochatter 1', 'event radioChatterUp'), 'waitMsec 700', shot('veh-radio'),
        *hud(gui, 'event radioChatterDown'), 'testStatusBar boss 60', 'testStatusBar stats 55 0', 'waitMsec 300', shot('veh-boss'),
        'testStatusBar stats 100 0', *hud(gui, 'event hideBossBar', 'event hideBossShieldBar', 'event showLockmessage'), 'waitMsec 200',
        shot('veh-locked'), *hud(gui, 'event hideLockmessage'),
        'set ui_hudScale 1.25', 'waitMsec 300', shot('veh-scaled'), 'set ui_hudScale 1',
        'set ui_hudLayout 0', 'waitMsec 400', shot('veh-classic'), 'set ui_hudLayout 1', 'waitMsec 400', shot('veh-restored'),
    ]


def objectives(gui):
    """The objectives display through the held scores button (the game
    publishes the list), then the notices and a failure through the stock
    GUI's own events."""
    return [
        'set g_autoExecAfterMapLoad ""', 'notarget', 'god', 'waitMsec 7000', shot('obj-start'),
        'waitMsec 5000', 'openq4_testInput 2000 scores', 'waitMsec 700', shot('obj-display'), 'waitMsec 1600',
        *hud(gui, 'state oq4_new_title "Destroy the three Strogg communication relays before the convoy leaves"',
             'state oq4_new_shot gfx/objectives/none', 'event newObjective'), 'waitMsec 400', shot('obj-new'),
        *hud(gui, 'state oq4_done_title "Reach the hangar control room"', 'event newObjectiveComplete'), 'waitMsec 400',
        shot('obj-both'),
        *hud(gui, 'event oq4_closeNewObjective'), 'waitMsec 300', shot('obj-complete'),
        *hud(gui, 'event oq4_closeDoneObjective', 'state oq4_failed_title "Protect the convoy"', 'event objectiveFailed'),
        'waitMsec 300', shot('obj-failed'),
        'set ui_hudLayout 0', *hud(gui, 'event newObjective'), 'waitMsec 400', shot('obj-classic'),
    ]


SCENARIOS = {
    'marine': ('game/airdefense1', on_foot('guis/hud.gui')),
    'objectives': ('game/airdefense1', objectives('guis/wristcomm.gui')),
    'tank': ('game/convoy1', in_vehicle('tank_1')),
    'strogg': ('game/recomp', on_foot('guis/hud_strogg.gui')),
    'mp': ('mp:mp/q4dm1', [
        'set g_autoExecAfterMapLoad ""', 'waitMsec 2500', shot('mp-base'),
        'addbot', 'addbot', 'waitMsec 2500', shot('mp-bots'),
        'testScoreboard 1200', 'waitMsec 400', shot('mp-scoreboard'), 'waitMsec 1000',
        'say Remastered chat line from the capture', 'waitMsec 300', shot('mp-chat'),
        'kill 1', 'waitMsec 400', shot('mp-feed'), 'kill 0', 'waitMsec 400', shot('mp-feed-you'), 'waitMsec 6000', shot('mp-later'),
        *equip(2), 'waitMsec 250', shot('mp-strip'),
        'set ui_hudScale 1.25', 'waitMsec 300', shot('mp-scaled'), 'set ui_hudScale 1',
        'set ui_hudLayout 0', 'waitMsec 400', shot('mp-classic'), 'set ui_hudLayout 1', 'waitMsec 400', shot('mp-restored'),
    ]),
    'mp-pace': ('mp:mp/q4dm1', [
        'set g_autoExecAfterMapLoad ""', 'waitMsec 2500', 'set ui_hudProfile 2', 'waitMsec 8000', 'set ui_hudProfile 0',
        'echo PACE remastered', 'com_speeds 1', 'waitMsec 1500', 'com_speeds 0',
        'set ui_hudLayout 0', 'waitMsec 1000', 'echo PACE classic', 'com_speeds 1', 'waitMsec 1500', 'com_speeds 0',
    ]),
    'sp-pace': ('game/airdefense1', [
        'set g_autoExecAfterMapLoad ""', 'waitMsec 2500', 'set ui_hudProfile 30', 'waitMsec 4000', 'set ui_hudProfile 0',
        'echo PACE remastered', 'com_speeds 1', 'waitMsec 1500', 'com_speeds 0',
        'set ui_hudLayout 0', 'waitMsec 1000', 'echo PACE classic', 'com_speeds 1', 'waitMsec 1500', 'com_speeds 0',
    ]),
    'mp-tourney': ('mp:mp/q4dm1', [
        'set g_autoExecAfterMapLoad ""', 'waitMsec 2500', 'addbot', 'addbot', 'addbot', 'waitMsec 4000', shot('tourney-hud'),
        'testScoreboard 1500', 'waitMsec 500', shot('tourney-scoreboard'), 'waitMsec 1200',
    ]),
    'mp-ctf': ('mp:mp/q4ctf1', [
        'set g_autoExecAfterMapLoad ""', 'waitMsec 2500', shot('ctf-base'),
        'addbot', 'addbot', 'addbot', 'waitMsec 3000', shot('ctf-bots'),
        'testScoreboard 1200', 'waitMsec 400', shot('ctf-scoreboard'), 'waitMsec 1000',
        'sayTeam team line from the capture', 'waitMsec 300', shot('ctf-chat'),
        'set ui_hudLayout 0', 'waitMsec 400', shot('ctf-classic'), 'set ui_hudLayout 1', 'waitMsec 400',
    ]),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, default=ROOT / '.install')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderer', choices=('gl', 'vulkan'), default='gl')
    parser.add_argument('--size', default='1280x720')
    parser.add_argument('--scenario', action='append', choices=sorted(SCENARIOS))
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    args.output = args.output.resolve()
    if args.output.exists() or not args.output.is_relative_to((ROOT / '.tmp').resolve()):
        parser.error('use a fresh output directory below .tmp')
    args.language = 'english'
    args.gate = '1'
    results = []
    for scenario in args.scenario or ['marine']:
        capture.SCENARIOS[scenario] = SCENARIOS[scenario]
        capture.SCENARIO_CVARS[scenario] = {'ui_hudLayout': '1', 'ui_hudScale': '1', 'ui_hudSafeInset': '0', 'ui_hudWidth': '1'}
        # A live match at once (no warm-up), timed, so the feed, notices and clock run.
        capture.SCENARIO_CVARS[scenario].update({'si_warmup': '0', 'si_timeLimit': '10', 'si_fragLimit': '10'})
        if scenario == 'mp-ctf':
            capture.SCENARIO_CVARS[scenario]['si_gameType'] = 'CTF'
        if scenario == 'mp-tourney':
            capture.SCENARIO_CVARS[scenario]['si_gameType'] = 'Tourney'
        result = capture.run(args, scenario)
        log = (ROOT / result['log']).read_text(encoding='utf-8', errors='replace') if result['log'] else ''
        result['remastered_warnings'] = [line for line in log.splitlines() if 'Remastered HUD' in line or 'retained:' in line.lower()]
        result['loaded'] = re.findall(r'RETAINED_GUI_LOADED (\S*hud\S*)', log)
        results.append(result)
        (args.output / 'report.json').write_text(json.dumps(results, indent=2) + '\n')
        print(scenario, 'complete' if result['complete'] else 'INCOMPLETE', 'rc', result['returncode'],
              len(result['screenshots']), 'shots', 'loaded', result['loaded'], 'warnings', result['remastered_warnings'][:5],
              'errors', result['errors'][:5], flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

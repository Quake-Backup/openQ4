# Input Settings

This guide covers the input options most players are likely to use: rebinding keys, mouse feel, controller setup, and common troubleshooting.

## Quick Start

- To rebind actions, open `Settings -> Controls`.
- To tune mouse and controller feel, open `Settings -> Game Options`.
- Start with sensitivity and dead zone before changing advanced values.
- Use `Load Defaults` if the controls get into a bad state. This also resets custom binds.

## Where Things Are

| Menu | Use it for |
|---|---|
| `Settings -> Controls -> Movement` | Forward, back, strafe, jump, crouch, turn, run/walk. |
| `Settings -> Controls -> Weapons` | Direct weapon slots and buy menu. |
| `Settings -> Controls -> Attack` | Attack, reload, zoom, next/previous weapon, flashlight, objectives, weapon wheel. |
| `Settings -> Controls -> Other` | Quick save/load, chat, vote, ready, stats, emotes, voice chat. |
| `Settings -> Game Options` | Free look, toggle zoom, mouse input tuning, controller tuning, console access. |

To rebind an action, select its row and press the key, mouse button, or controller button you want to use. Analog sticks are configured through the controller settings instead of being rebound as buttons.

## Mouse Settings

| Setting | Good starting point | What it changes |
|---|---:|---|
| Free Look | On | Lets the mouse control view direction by default. |
| Mouse Pitch | Normal or Inverted | Flips vertical mouse look. |
| Mouse Smooth | 1 | Higher values smooth motion but can feel less direct. |
| Mouse Sensitivity | Personal preference | Main mouse look speed. With Mouse CPI enabled, this is degrees per centimeter. |
| Mouse CPI | 0 | Set this to your mouse CPI/DPI for physical sensitivity. `0` keeps classic raw-count behavior. |
| View Filter | 0 | Optional QuakeLive-style view-angle history filtering. Leave it off for the most direct raw input. |
| Mouse Accel | 0 | QuakeLive-style acceleration. Positive values add sensitivity as movement rate rises; negative values subtract it. |
| Accel Offset | 0 | Movement-rate threshold before acceleration starts. |
| Accel Curve | 2 | Acceleration curve shape. `2` matches QuakeLive's default power. |
| Sensitivity Cap | 0 | Caps accelerated sensitivity when set above `0`. |

Useful console-only fallback:

```cfg
seta m_maxMouseDelta 0
```

`m_maxMouseDelta 0` keeps the modern high-DPI path unclamped. Set a positive value only if a faulty mouse or driver creates large camera jumps.

These menu controls write the following cvars:

| Cvar | Default | What it changes |
|---|---:|---|
| `m_cpi` | `0` | Set this to your mouse CPI/DPI to make `sensitivity` a physical degrees-per-centimeter value. `0` keeps the classic raw-count behavior. |
| `m_filter` | `0` | Optional QuakeLive-style view-angle history filtering. Leave it off for the most direct raw input. |
| `cl_mouseAccel` | `0` | QuakeLive-style acceleration. Positive values add sensitivity as movement rate rises; negative values subtract it. |
| `cl_mouseAccelOffset` | `0` | Movement-rate threshold before acceleration starts. |
| `cl_mouseAccelPower` | `2` | Acceleration curve shape. `2` matches QuakeLive's default power. |
| `cl_mouseSensCap` | `0` | Caps accelerated sensitivity when set above `0`. |

Debug-only console fallback:

```cfg
seta cl_mouseAccelDebug 0
```

`cl_mouseAccelDebug 1` writes acceleration samples to `logs/mouse.log` under the active save path.

For CPI-normalized tuning, set `m_cpi` to the mouse hardware value and use `360 / sensitivity` as an approximate centimeters-per-360 target.

## Controller Settings

Controller tuning lives in `Settings -> Game Options -> Controller`.

| Setting | Default | What it changes |
|---|---:|---|
| Controller | On | Enables SDL3 gamepad/joystick input and hotplug support. |
| Stick Layout | Default | Use `Southpaw` to swap movement and look sticks. |
| Stick Dead Zone | `0.18` | Raises or lowers the center area ignored by analog sticks. |
| Look Sensitivity | `0.75` | Scales controller turn speed. |
| Look Curve | `1.35` | Higher values damp small aim movements; full-stick turn speed is controlled by Look Sensitivity. |
| Invert Look | Off | Flips vertical controller look. |
| Trigger Press | `0.35` | Controls how far LT/RT must be pressed before they count as buttons. |
| Rumble | On | Enables gameplay rumble when the device supports it. |
| Rumble Strength | `1.0` | Scales rumble intensity from `0.0` to `2.0`. |

Suggested adjustments:

- If the view or movement drifts, raise `Stick Dead Zone` a little.
- If aiming feels sluggish, raise `Look Sensitivity`.
- If small aim corrections are too twitchy, raise `Look Curve` or `Stick Dead Zone`.
- If you cannot reach full turn speed comfortably, lower `Look Curve` toward `1.0`.
- If triggers fire too easily, raise `Trigger Press`.
- If triggers feel unresponsive, lower `Trigger Press`.

Advanced movement tuning is available from the console:

```cfg
seta in_joystickMoveCurve 1.0
```

`1.0` is linear movement. Higher values make walking easier near the center of the stick but require more stick travel to reach full movement.

## Default Controller Buttons

SDL gamepads use stable `JOY` button names so binds work across common Xbox, PlayStation, and Steam Input layouts.

| Button | Default action |
|---|---|
| `JOY1` / LB | Weapon wheel |
| `JOY2` / RB | Last weapon |
| `JOY3` / A or Cross | Jump |
| `JOY4` / B or Circle | Crouch |
| `JOY5` / Y or Triangle | Flashlight |
| `JOY6` / X or Square | Reload |
| `JOY9` / D-pad Up | Objectives or scores |
| `JOY10` / D-pad Down | Center view |
| `JOY11` / D-pad Right | Previous weapon |
| `JOY12` / D-pad Left | Next weapon |
| `JOY13` / Left stick click | Run or walk modifier |
| `JOY14` / Right stick click | Center view |
| `JOY15` / RT | Attack |
| `JOY16` / LT | Zoom |
| `JOY18` / Touchpad | Objectives or scores, when available |

Menu behavior:

- `JOY7` and `JOY8` open the in-game menu.
- `JOY3` activates the focused menu item.
- `JOY4`, `JOY7`, and `JOY8` back out of menus.
- The D-pad and movement stick move menu focus.
- Holding the D-pad, movement stick, or shoulder buttons repeats navigation for long lists.

## Console Examples

You can use these in the console or place them in a config file:

```cfg
seta sensitivity 3.5
seta m_smooth 1
seta m_cpi 0
seta m_filter 0
seta cl_mouseAccel 0
seta cl_mouseAccelOffset 0
seta cl_mouseAccelPower 2
seta cl_mouseSensCap 0
seta in_joystickDeadZone 0.22
seta in_joystickLookSensitivity 0.75
seta in_joystickLookCurve 1.35
seta in_joystickTriggerThreshold 0.45
seta in_joystickRumbleScale 0.75
```

Most input cvars are archived, so changes made through the menu or with `seta` persist after exit.

## Troubleshooting

| Problem | Try this |
|---|---|
| Controller is not detected | Make sure `Controller` is enabled, reconnect the device, and check whether Steam Input is remapping it. |
| Movement or aim drifts | Raise `Stick Dead Zone` until the drift stops. |
| Aim feels too slow | Raise `Look Sensitivity`. |
| Aim feels too twitchy near center | Raise `Look Curve` or `Stick Dead Zone`. |
| Triggers activate by accident | Raise `Trigger Press`. |
| Rumble does not work | Enable `Rumble`, set `Rumble Strength` above `0`, confirm the device supports haptics, and reset `s_controllerRumble` to `1` if it was changed from the console. |
| Mouse look is backwards vertically | Change `Mouse Pitch` or `Invert Look`, depending on the device. |
| Console is hard to open | Set `Console Access` in `Settings -> Game Options` to the tilde option you prefer. |

For Steam Deck launcher details and Deck-specific notes, see [steam-deck.md](steam-deck.md).

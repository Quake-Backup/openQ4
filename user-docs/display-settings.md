# Display Settings and Multi-Screen Guide

This guide covers OpenQ4 display/window settings for end users, including multi-monitor behavior and modern fullscreen/window handling.

## Quick Start

- Press `Alt+Enter` to toggle fullscreen/windowed mode (fast path uses `vid_restart partial`).
- Run `listDisplays` in the console to list monitor indices for `r_screen`.
- On SDL3 builds, run `listDisplayModes [displayIndex]` to list available exclusive fullscreen modes.
- After changing video cvars, run `vid_restart` (or `vid_restart partial` for quick window/fullscreen transitions).

## Core Display Settings

| Setting | Default | What it does |
|---|---:|---|
| `r_fullscreen` | `1` | `1` = fullscreen, `0` = windowed. |
| `r_fullscreenDesktop` | `1` | `1` = native desktop fullscreen (recommended). `0` = exclusive fullscreen using `r_mode`/`r_custom*`. |
| `r_borderless` | `0` | Borderless window mode when `r_fullscreen 0`. |
| `r_windowWidth` | `1280` | Windowed width. |
| `r_windowHeight` | `720` | Windowed height. |
| `win_xpos` | (auto) | Window X position (updated automatically when you move the window). |
| `win_ypos` | (auto) | Window Y position (updated automatically when you move the window). |
| `r_mode` | `3` | Preset mode index. Use `-1` for custom width/height. |
| `r_customWidth` | `720` | Custom width used when `r_mode -1`. |
| `r_customHeight` | `486` | Custom height used when `r_mode -1`. |
| `r_displayRefresh` | `0` | Requested fullscreen refresh rate (0 = default/driver choice). |
| `r_screen` | `-1` | SDL3 monitor target (`-1` auto/current, `0..N` explicit index). |

## Fullscreen Policy (Desktop vs Exclusive)

- Default behavior is **desktop-native fullscreen** (`r_fullscreenDesktop 1`): fullscreen matches your current desktop resolution and does not change Windows display mode.
- For **exclusive fullscreen** (explicit mode switch), set `r_fullscreenDesktop 0`. In this mode, `r_mode`/`r_customWidth`/`r_customHeight` control the requested fullscreen resolution.

Notes:
- When `r_fullscreenDesktop 1`, `r_mode` and `r_custom*` are ignored for fullscreen sizing (they still exist for legacy configs and exclusive mode).
- Use `listDisplayModes` to see what your monitor actually supports in exclusive mode.

## Windowed Sizing and Placement

- When windowed (`r_fullscreen 0`, `r_borderless 0`), resizing updates `r_windowWidth`/`r_windowHeight` automatically.
- Moving the window updates `win_xpos`/`win_ypos` automatically.
- When switching fullscreen -> windowed, OpenQ4 restores the last remembered windowed size/position (it should not come back as a fullscreen-sized window).
- If you unplug/rearrange monitors and the saved window position becomes off-screen, OpenQ4 will recover by clamping/recentering the window back onto a valid display.
- If you set `r_screen` to an explicit display index (`0..N`), window placement is constrained to that display's usable area. With `r_screen -1`, placement is respected unless it becomes invalid/off-screen.
- SDL3 tip: hold `Shift` while resizing to snap the window aspect ratio to common targets (4:3, 16:9, 16:10, 21:9, etc.).

## Aspect Ratio and FOV

- `r_aspectRatio` is **deprecated/ignored**. Aspect ratio and FOV behavior are derived automatically from the current render size, so the game follows any aspect ratio without manual selection.

## UI Aspect Correction (New)

This controls 2D UI layout behavior (menu, HUD, console, loading/initializing screens):

| Setting | Default | What it does |
|---|---:|---|
| `ui_aspectCorrection` | `1` | `1` keeps classic 4:3-style correction for all 2D UI. `0` stretches 2D UI to the full 2D draw region. |

## Multi-Monitor Behavior (New)

When the render surface spans multiple monitors:

- 2D elements (console, HUD, menus, loading/initializing UI) are constrained to the **primary display region**.
- 2D aspect behavior inside that region is controlled by `ui_aspectCorrection`.
- Menu cursor mapping follows the same 2D region so mouse interaction stays aligned.

3D world rendering is unchanged by these UI cvars.

## Useful Console Examples

### Recommended Modern Defaults

```cfg
seta r_screen -1
seta r_fullscreenDesktop 1
seta r_fullscreen 1
seta ui_aspectCorrection 1
vid_restart
```

### Borderless Window on a Specific Monitor

```cfg
seta r_fullscreen 0
seta r_borderless 1
seta r_screen 1
vid_restart
```

### Custom Fullscreen Resolution

```cfg
seta r_fullscreen 1
seta r_fullscreenDesktop 0
seta r_mode -1
seta r_customWidth 2560
seta r_customHeight 1440
vid_restart
```

### Stretch Menu + HUD (No 4:3 Correction)

```cfg
seta ui_aspectCorrection 0
```

## Troubleshooting

- If a display change does not apply, run `vid_restart`.
- If monitor targeting looks wrong, run `listDisplays`, then set `r_screen` to the correct index and restart video.
- If UI appears too centered/boxed on wide displays, set `ui_aspectCorrection 0`.
- If the window opens off-screen after a monitor change, set `r_screen` explicitly to the target monitor and restart video; OpenQ4 will also attempt to recover automatically.

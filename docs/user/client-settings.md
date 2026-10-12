# Client Settings Guide

This guide helps players find the most useful openQ4 settings without digging through every advanced option.

## Best Places to Start

If you are setting up openQ4 for the first time, these are the areas most players adjust first:

- **Display** for fullscreen mode, resolution, monitor selection, and image quality
- **Controls** for keyboard, mouse, controller, and bindings
- **Game Options** for gameplay comfort settings and a few audio-related tweaks

## Recommended First Checks

- Set your preferred fullscreen or borderless mode
- Run **Auto-Detect** for the Performance Preset if this is a new machine or you are near the [minimum requirements](getting-started.md#system-requirements)
- Adjust mouse sensitivity or controller dead zone
- Confirm your display target if you use multiple monitors
- Pick whether you want extra visual effects such as bloom or shadow mapping
- Lower or raise music volume to taste

## Which Guide Covers What?

- [Display Settings](display-settings.md) - resolution, fullscreen, windowed mode, multi-monitor, anti-aliasing, and aspect behavior
- [Input Settings](input-settings.md) - keybinds, mouse tuning, controller support, and config-file behavior
- [Gameplay Settings](gameplay-settings.md) - convenience toggles and a few gameplay/audio settings
- [Shadow Mapping](shadow-mapping.md) - optional advanced shadow-map tuning
- [Cel Shading](cel-shading.md) - banded lighting and outline settings for the cel-shaded look
- [DDS Texture Replacements](texture-replacements.md) - DXT/BC7 texture-pack layout and diagnostics
- [Steam Deck](steam-deck.md) - launcher and handheld-specific input notes

## In-Game Menus

Most player-facing options are available directly in the in-game menus:

- `Settings -> System`
- `Settings -> Controls`
- `Settings -> Game Options`
- `Difficulty` (single-player) - restart the current level at another difficulty; see [Gameplay Settings](gameplay-settings.md#difficulty)

## Modern and Classic Screens

The classic Quake 4 interface is the default (`ui_retained 0`). The experimental modern UI is disabled by default because its gameplay overhead is not yet negligible compared with legacy. Existing saved preferences are preserved: if an earlier version saved the modern UI as enabled, enter `seta ui_retained 0` in the console to switch back.

Opting into the modern UI replaces the title screen, single-player pause menu, loading screens and gameplay HUD. HUD coverage includes crosshairs, objectives, radio, powerups, boss and vehicle displays, multiplayer scores/chat/summary panels and Awakening overlays. See [Gameplay Status Bar](gameplay-settings.md#gameplay-status-bar) for the centered ammo/health/armor bar's independent size and safe-inset settings. The other menus, including `Settings -> System`, keep the classic Quake 4 look for now.

- To try the experimental modern UI, enter `seta ui_retained 1`; enter `seta ui_retained 0` to return to classic. The choice is saved. An explicit launch option overrides the saved choice; the VS Code retained-UI configurations intentionally opt in for development testing.
- If a modern screen can't be shown, openQ4 shows the classic one instead, with nothing to fix on your side. This happens when a mod brings its own main menu, or when a modern screen's files are missing or damaged.

If you want deeper technical details or console-level configuration, see [TECHNICAL.md](../../TECHNICAL.md).

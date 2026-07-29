# Gameplay Settings and Runtime Toggles

This guide covers a small set of gameplay and audio cvars that are useful for testing, accessibility, and personal preference.

The auto-skip cinematics, corpse cleanup, and corpse sink controls are also available in the in-game menu at `Settings -> Game Options`.

## Quick Reference

| Setting | Default | Scope | What it does |
|---|---:|---|---|
| `g_autoSkipCinematics` | `0` | SP and MP game code | Automatically skips cinematics as soon as they begin. Disabled by default. |
| `g_corpseRemoveDelaySP` | `0` | Single-player | Controls how long SP corpses remain before disappearing. `0` uses stock timing, `-1` disables corpse removal. |
| `g_corpseRemoveDelayMP` | `0` | Multiplayer | Controls how long MP corpses remain before disappearing. `0` uses stock timing, `-1` disables corpse removal. |
| `g_corpseSink` | `0` | SP and MP game code | Selects corpse sink mode instead of the normal dissolve or burn-away behavior. |
| `s_musicVolume` | `0.5` | Client audio | Controls music volume independently of the main sound mix. |
| `hud_damageNumbers` | `0` | Multiplayer client | Floating damage numbers over the players you hit. `0` off, `1` opponents only, `2` all damage you deal. |
| `hud_damageNumberStyle` | `1` | Multiplayer client | How damage numbers are coloured. `1` white through red, `2` one colour per damage band, `3` one colour per weapon. |
| `hud_damageNumberScale` | `1` | Multiplayer client | Damage number size multiplier, `0.25` to `4`. |
| `g_hitFeedback` | `2` | Multiplayer server | Whether the server tells attackers about their hits at all. `0` none, `1` without the amount, `2` with the amount. |

## Cinematics

`g_autoSkipCinematics` is intended for repeat testing runs, speed-focused replays, and development workflows where you do not want to manually skip every scripted sequence.

The in-game menu exposes this as `Settings -> Game Options -> Auto Skip Cinematics`.

Behavior:
- `0`: normal behavior, cinematics play as authored.
- `1`: cinematics are skipped automatically when they start.

Notes:
- The cvar is archived, but it is disabled by default.
- The change affects future cinematics. It does not retroactively alter one that has already finished.

Example:

```cfg
seta g_autoSkipCinematics 1
```

## Corpse Cleanup

openQ4 now exposes separate corpse-removal timing controls for single-player and multiplayer.

### Single-player

`g_corpseRemoveDelaySP` accepts three useful ranges:
- `0`: use stock timing.
- `-1`: never remove corpses automatically.
- `> 0`: override the delay in seconds before corpse removal begins.

Example:

```cfg
seta g_corpseRemoveDelaySP 20
```

### Multiplayer

`g_corpseRemoveDelayMP` behaves the same way, but applies to the multiplayer game module.

Example:

```cfg
seta g_corpseRemoveDelayMP 10
```

Notes:
- These cvars affect corpse cleanup timing. They do not change health, gibbing, or damage behavior.
- New values are most useful for newly created corpses. Existing corpses may already be partway through their current cleanup path.

## Corpse Sinking

`g_corpseSink` switches corpse disappearance to a Quake 3 style sink animation.

Behavior:
- `0`: use the normal stock-style dissolve or burn-away path.
- `1`: sink corpses into the floor before removal while keeping ragdoll active.
- `2`: sink corpses into the floor before removal after stopping ragdoll first.

Notes:
- This cvar is shared by SP and MP.
- The configured SP or MP corpse delay still controls when the sink starts.
- If the relevant corpse-delay cvar is `-1`, corpse removal is disabled and sinking will not start.
- Mode `2` uses the actor's normal corpse physics while the sink runs, so the body no longer keeps simulating as a ragdoll during the sink.

Example:

```cfg
seta g_corpseSink 1
seta g_corpseRemoveDelaySP 15
```

No-ragdoll sink example:

```cfg
seta g_corpseSink 2
seta g_corpseRemoveDelaySP 15
```

## Music Volume

`s_musicVolume` controls the volume of music-tagged sound shaders without changing the rest of the game mix.

Behavior:
- `0`: mute music.
- `0.5`: default music level.
- `1`: full music level.

Notes:
- This is separate from the master sound volume.
- It applies to music shaders authored under the stock Quake 4 music paths, including both `sound/musical/` and `sound/ambience/musical/`.
- The setting is live and can be adjusted while the game is running.

Examples:

```cfg
seta s_musicVolume 0.2
```

```cfg
seta s_musicVolume 0
```

## Damage Numbers

In multiplayer, openQ4 can float the damage you deal above the player you hit, the
way Quake Live's damage plums do. The numbers are projected from the point of
impact and then drift along a short arc before fading, so they stay readable
during a fight without ever sitting on top of your crosshair.

This is a display choice and it is off by default.

`hud_damageNumbers` decides which of your hits are shown:
- `0`: off.
- `1`: opponents only. This is what Quake Live shows.
- `2`: everything you deal, adding team damage and self damage.

`hud_damageNumberStyle` decides how they are coloured:
- `1`: white for a scratch through red at 100 damage.
- `2`: one colour per damage band - blue up to 25, yellow to 50, orange to 75, red above.
- `3`: one colour per weapon.

`hud_damageNumberScale` scales the text between `0.25` and `4`.

The number is the full worth of the hit, so armour counts towards it rather than
being subtracted from it.

### Server permission

`g_hitFeedback` is a **server** setting and it decides whether attackers are told
about their hits at all:
- `0`: no hit feedback. Damage numbers show nothing regardless of client settings.
- `1`: the hit is announced but the amount is withheld, so damage numbers stay blank.
- `2`: the amount is included. This is the default.

A server that wants to keep exact damage off the table sets `g_hitFeedback 1` or
`0`; no client setting can override it.

Example:

```cfg
seta hud_damageNumbers 1
seta hud_damageNumberStyle 2
seta hud_damageNumberScale 1.25
```

## Example Presets

### Fast Testing Setup

```cfg
seta g_autoSkipCinematics 1
seta g_corpseRemoveDelaySP 2
seta g_corpseSink 1
seta s_musicVolume 0.2
```

### Keep Corpses Around

```cfg
seta g_corpseRemoveDelaySP -1
seta g_corpseRemoveDelayMP -1
```

## Troubleshooting

- If `g_autoSkipCinematics 1` appears to do nothing, verify you are testing a real in-game cinematic and not a normal scripted gameplay event.
- If corpses still disappear quickly, check whether you are in SP or MP and set the matching delay cvar for that game module.
- If `g_corpseSink 1` or `g_corpseSink 2` is enabled but corpses never sink, make sure the relevant corpse delay is not set to `-1`.
- If music does not respond to `s_musicVolume`, test on a map or menu that is actively playing music rather than general ambience or voice-over audio.
- If damage numbers never appear, check `g_hitFeedback` on the server: at `0` or `1` the amount never reaches you. They are also multiplayer only, and they only cover damage dealt to players.
- If damage numbers appear for opponents but not team mates, that is `hud_damageNumbers 1` working as intended. Set it to `2` to include team and self damage.

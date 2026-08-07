# Server Setup Guide

This guide covers a simple way to host an openQ4 dedicated server.

## What You Need

- A working openQ4 install
- Access to the original Quake 4 retail assets
- The `openQ4-ded_<arch>` executable from your openQ4 release or local build
- A 64-bit host that matches the package architecture

## Dedicated Server Requirements

The dedicated server does not need a GPU or an OpenGL-capable desktop session, but it still needs the same retail `q4base/` assets and matching openQ4 game modules as the client.

For light testing, plan on a modern 64-bit host with at least 2 GB RAM available to the operating system and server, plus the same package and Quake 4 asset storage described in the [Getting Started system requirements](getting-started.md#system-requirements). For public servers, prefer 4 GB+ RAM, a stable wired connection, and enough upload bandwidth for the player count you advertise.

## Quick Start

1. Make sure openQ4 can see your Quake 4 assets.
2. Launch the dedicated server executable.
3. Set a server name, map, and game type.
4. Start the server with `spawnServer`.

Example startup flow:

```text
openQ4-ded_x64 +set si_name "My openQ4 Server" +set si_map mp/q4dm1 +set si_gameType dm +spawnServer
```

## Common Server Variables

| Variable | What it controls |
|---|---|
| `si_name` | Server name shown to players |
| `si_map` | Starting map |
| `si_gameType` | Multiplayer game type |
| `si_fragLimit` | Frag limit |
| `si_timeLimit` | Time limit |
| `si_warmup` | Whether warmup is used |
| `g_mapCycle` | Map cycle script |
| `bot_minPlayers` | Keep the match topped up to this many players with bots (`0` disables) |
| `bot_skill` | Bot difficulty, 1 (harmless) to 5 (unpleasant) |
| `bot_skillVariance` | Spread bot skill this many levels either side of `bot_skill`, so a match is not all one difficulty |
| `bot_characters` | Give bots named characters with their own play style and voice (`0` for plain skill-curve bots) |
| `bot_chat` | Bot chat: `0` silent, `1` normal, `2` chatty |
| `bot_chatCPM` | Bot typing speed in visible characters per minute (`900` by default) |

Default multiplayer values are seeded from `content/baseoq4/pak0/default.cfg`.

## Useful Console Commands

| Command | What it does |
|---|---|
| `spawnServer` | Starts the server |
| `disconnect` | Shuts the server down |
| `serverMapRestart` | Restarts the current map |
| `serverNextMap` | Advances to the next map |
| `kick` | Kicks a client by slot number |
| `gameKick` | Kicks a client by player name |
| `addbot` | Adds one bot, optionally by name and skill |
| `kickbots` | Removes every bot |
| `botlist` | Lists the bots and their characters |

## Bots

openQ4 ships bots that navigate any multiplayer map with no per-map setup, so a
server can stay populated while it is quiet. Set `bot_minPlayers` to the player
count you want the match held at and the server fills the rest, releasing the
slots again as real players connect.

Bots are entirely server-side; clients need nothing installed and see them as
ordinary players. Each one gets a name, a play style and its own chat lines.
They can answer common conversational phrases from people or other bots, and
addressing one by name makes that character the preferred responder. Team chat
stays inside the team, and replies cannot trigger reply chains. Set
`bot_characters 0` for anonymous bots on the plain skill curve, or `bot_chat 0`
to keep them quiet. Messages wait briefly according to their visible length,
and the stock Quake 4 typing icon appears above the bot until the line is sent.

The bot cvars are archived, so setting them once in your server config is
enough. For the full command list, the character file format, and how to add
your own characters, see [Multiplayer bots](../dev/mp-bots.md).

## Competitive Match Management (Development Preview)

Managed competitive profiles, server-owned readiness, tactical timeouts,
structured match evidence and automatic multi-view recording are under active
development. They are not tournament-qualified yet, and several captain,
referee, series and spectator workflows do not have their finished interface.

See [Competitive Matches](competitive-matches.md) for the currently usable
profile and readiness path, the exact unfinished boundaries, and the operator
test checklist. Casual servers do not opt into managed-match policy unless an
operator selects a competitive profile.

## Multiplayer Tuning

If you want to tune prediction or lag compensation behavior, see [Multiplayer Networking](multiplayer-networking.md).

## Notes

- openQ4 uses its own engine and game modules.
- openQ4 is not a drop-in runtime for the original proprietary Quake 4 DLL mods.
- For advanced configuration, file layout, and path behavior, see [TECHNICAL.md](../../TECHNICAL.md).

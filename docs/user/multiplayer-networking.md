# Multiplayer Networking Guide

This guide covers openQ4 multiplayer networking behavior and the cvars used to tune or revert prediction/lag-comp behavior.

## Quick Summary

- Direct connections accept numeric IPv4 addresses and hostnames with IPv4 DNS records.
- IPv4 ports use the complete unsigned 16-bit range, including high ports above `32767`.
- Server-side hitscan lag compensation is enabled by default.
- Remote-client prediction runs in enhanced mode by default.
- Both systems can be switched back to legacy behavior with cvars.

## IPv4 Connections

Use `connect <address>:<port>` for a direct IPv4 connection. These forms are supported:

```text
connect 192.168.1.50:28004
connect 203.0.113.25:32000
connect play.example.net:28004
```

The first form is suitable for a local network, the second illustrates an internet-facing IPv4 address, and the third uses a hostname with an IPv4 DNS record. If the port is omitted, openQ4 uses the default server port `28004`.

Endpoint ports are parsed as unsigned 16-bit values from `0` through `65535`. For a direct connection, an omitted port or literal `:0` selects `28004`; ports from `1` through `65535` select that exact destination. Internet server-list entries also preserve the full range, so servers using ports `32768` through `65535` remain connectable.

Malformed addresses, non-numeric port text, and ports outside the valid range are rejected. An unresolved remote-console address is rejected as well, rather than sending a command to an unintended wildcard address.

Server operators should configure `net_ip`, `net_port`, host firewall rules, and any router port forward as described in the [Server Setup Guide](server-setup.md#ipv4-binding-and-ports).

## CVar Reference

| Setting | Default | Range | Scope | What it does |
|---|---:|---:|---|---|
| `net_mpLagCompensation` | `1` | `0..1` | Server gameplay | Enables server-side lag compensation for multiplayer hitscan traces. |
| `net_mpLagCompMaxMS` | `200` | `0..1000` | Server gameplay | Caps rewind window in milliseconds used by lag compensation. |
| `net_mpLagCompBiasMS` | `0` | `-200..200` | Server gameplay | Adds/subtracts additional rewind bias in milliseconds. |
| `net_mpLagCompDebug` | `0` | `0..2` | Server gameplay | Debug logging for lag compensation (`0` off, `1` summary, `2` verbose). |
| `net_mpPredictMode` | `1` | `0..1` | MP client prediction | Selects remote-player prediction mode (`0` legacy limited, `1` enhanced per-frame). |

## Legacy Compatibility Switch

Use this to restore legacy multiplayer behavior:

```cfg
seta net_mpLagCompensation 0
seta net_mpPredictMode 0
```

## Recommended Starting Presets

### Default Internet Play

```cfg
seta net_mpLagCompensation 1
seta net_mpLagCompMaxMS 200
seta net_mpLagCompBiasMS 0
seta net_mpPredictMode 1
```

### Low-Latency/LAN

```cfg
seta net_mpLagCompensation 1
seta net_mpLagCompMaxMS 80
seta net_mpLagCompBiasMS 0
seta net_mpPredictMode 1
```

## Notes

- Lag compensation applies to authoritative multiplayer hitscan traces on the server.
- `net_mpLagCompDebug` output is intended for server diagnostics and tuning.
- Tune `net_mpLagCompMaxMS` before using large `net_mpLagCompBiasMS` offsets.

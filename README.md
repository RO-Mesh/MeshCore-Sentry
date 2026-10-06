# MeshCore — Repeater Firmware with a Remote-Configurable Packet Filter

This is a feature fork of [MeshCore](https://github.com/meshcore-dev/MeshCore).
It tracks upstream automatically (synced weekly) and adds repeater-focused
capabilities on top of stock firmware. Everything else — supported boards,
clients, protocols, build system — is identical to upstream; see the
[upstream README](https://github.com/meshcore-dev/MeshCore#readme) for the
general project, hardware support, and getting-started guide.

## What this fork adds

### 📡 Packet filter

A remote-configurable drop-rule engine for the `simple_repeater` firmware.
Rules match on packet type, route type, region, hop count, channel, and —
on decrypted payloads — channel identity and sender/text patterns. Rules are
managed over the repeater CLI, locally via serial or **remotely over the mesh**
by an authenticated admin. No rebuild, no physical access.

Independently of the rules, flood-route adverts can be rate-limited per
originating node: each node's advert is forwarded at most once per window
(up to 30 days), taming advert storms on busy channels while every node
stays discoverable.

→ Full guide, including the complete predicate reference and CLI commands:
**[FILTER.md](./FILTER.md)**

### 🔋 Battery gate

A configurable low-battery cut-off: below a set voltage the repeater stops
forwarding (but stays reachable and keeps advertising); when the battery
recovers it resumes automatically. Debounce + hysteresis keep it from
flapping, and the admin can check voltage and tune thresholds over the mesh.

→ Full guide: **[BATTERY.md](./BATTERY.md)**

### 🎚️ RO-Mesh Sentry

Optional remote radio-profile switching for repeaters. Sentry listens on a
configured MeshCore group channel for low-profile trigger messages, burns a
single-use per-repeater token, stages one of three stored radio presets, then
reboots after a configurable delay so later hops have time to hear the trigger.

Everything is CLI-driven. There is no UI. Token rotation and import/export are
available through the repeater CLI.

→ Full guide: **[SENTRY.md](./SENTRY.md)**

→ Development loop and field-test report template:
**[docs/sentry/DEVELOPMENT.md](./docs/sentry/DEVELOPMENT.md)**

## Why run this instead of stock repeater firmware?

Stock repeaters forward everything they hear, until the battery dies. This
fork is for operators who want policy and resilience at the node:

- **Shared / community nodes** — mute noisy senders or channels that eat your
  airtime, without asking anyone to change their setup.
- **OpenHop-style policy, nothing extra to run** — the rule engine is modelled
  after the policy filters in [OpenHop](https://github.com/openhop-dev) and
  lives entirely in the repeater firmware, so you get about the same
  functionality with just the node itself — no computer, companion app, or
  other hardware needed.
- **Solar / off-grid sites** — the node goes quiet *while it can still answer
  you*, instead of disappearing dark, and wakes itself when there's sun again.
- **Coordinated emergency retunes** — selected repeaters can switch between
  preconfigured radio profiles using per-repeater one-time triggers instead of
  a shared fleet password.

## Getting the firmware

Prebuilt binaries for all supported boards are on the
[Releases page](https://github.com/jhuebert/MeshCore/releases). Two channels:

| Channel | Branch | Based on | Tags |
|---|---|---|---|
| dev | `repeater-filter` | upstream `dev` (merged weekly by CI) | `filter-v*-dev.N` |
| stable | `repeater-filter-stable` | upstream `main` releases | `filter-v*` |

Flash as you would stock firmware, then follow
[FILTER.md](./FILTER.md) to set your first rules over the CLI.

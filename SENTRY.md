# RO-Mesh Sentry

RO-Mesh Sentry is an optional repeater feature for remote radio-profile
switching from low-profile group messages. It is disabled unless the firmware is
built with `RO_MESH_SENTRY`.

It is intended for repeaters, so there is no UI. Configuration is through the
existing repeater CLI, locally over serial or remotely through authenticated
admin CLI.

## Safety model

- Trigger material is per repeater, not fleet-wide.
- Tokens are single-use and are burned before a profile switch is staged.
- Optional `sentry.auth` adds a per-repeater secret-derived signature.
- Profile application is delayed before reboot so early-hop repeaters stay on
  the current profile long enough for later hops to hear the same trigger.

Default apply delay is 30 seconds.

## Trigger formats

Unsigned token-only trigger:

```text
WX: Temp 21, Hum 40, Stn# 2-RIVER31
chk 2 RIVER31 normal chatter
```

With `sentry.auth` enabled:

```text
WX: Temp 21, Hum 40, Stn# 2-RIVER31-1A2B3C4D
chk 2 RIVER31 sig:1A2B3C4D normal chatter
```

The signature is the first 4 bytes of SHA-256 over the per-repeater auth secret
and `<preset>:<token>`, encoded as 8 hex characters.

## CLI

Core configuration:

```text
set sentry.channel <#name|Public|name psk_hex>
set sentry.tokens <t1> <t2> ...
set sentry.auth <secret|off>
set sentry.delay <5..300>
set preset <id> <freq> <bw> <sf> <cr>
sentry status
```

Token maintenance:

```text
sentry tokens add <t1> <t2> ...
sentry tokens del <token|#index>
sentry tokens clear
sentry tokens rotate <new1> <new2> ...
```

`rotate` replaces spent tokens first, then appends into free slots. Active
unspent tokens are preserved. This lets an operator top up a repeater without
replacing still-valid field tokens.

Import/export:

```text
sentry export config
sentry export presets
sentry export tokens [start_index]
sentry import channel <#name|Public|name psk_hex>
sentry import delay <5..300>
sentry import auth <secret|off>
sentry import preset <id> <freq> <bw> <sf> <cr>
sentry import tokens <t1> <t2> ...
```

Token export is paginated because MeshCore CLI replies are small:

```text
sentry export tokens 0
sentry export tokens 4
sentry export tokens 8
```

Export marks active tokens with `+` and spent tokens with `-`.

## Default presets

```text
Preset 1: 869.525 MHz, 62.5 kHz, SF8, CR 4/5
Preset 2: 869.650 MHz, 62.5 kHz, SF9, CR 4/5
Preset 3: 869.400 MHz, 62.5 kHz, SF7, CR 4/5
```

## Build status

Current branch status:

- `SentryManager.cpp` passes host static compilation.
- `platformio test -e native_sentry` passes for token burn, replay rejection,
  delay gate, wrong channel rejection, rotation, atomic token import, preset
  import, and export.
- Existing native guard suites still pass:
  `platformio test -e native_packet_filter` and
  `platformio test -e native_battery_gate`.
- Release-path firmware builds pass with `RO_MESH_SENTRY` for
  `Heltec_v3_repeater` and `Xiao_nrf52_repeater`.
- No hardware/RF validation has been performed yet.

## Development loop

Development and field validation are tracked as small prerelease iterations.
The current release targets are:

```text
ESP32: Heltec_v3_repeater
nRF52: Xiao_nrf52_repeater
```

Use the development loop and field report template in
[`docs/sentry/DEVELOPMENT.md`](docs/sentry/DEVELOPMENT.md) for every new
Sentry iteration. Each report should include the tag, asset name, hardware,
flash method, CLI snapshots, pass/fail test cases, logs/screenshots, and the
requested next iteration.

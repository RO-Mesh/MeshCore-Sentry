# RO-Mesh Sentry Development Loop

Use this loop for every Sentry iteration until the feature has field-proven
behavior on the real repeater fleet.

## Release Targets

Current RO-Mesh Sentry release targets are:

```text
ESP32: Heltec_v3_repeater
nRF52: Xiao_nrf52_repeater
```

Do not substitute RAK nRF targets unless the field hardware changes.

## Iteration Rules

- Ship small iterations. Each release candidate should answer one field
  question or fix one observed issue.
- Keep Sentry behind `RO_MESH_SENTRY`.
- Keep feature logic in `examples/simple_repeater/SentryManager.*`.
- Keep upstream hook changes minimal.
- Treat every published tag as immutable. If a target or behavior is wrong,
  publish the next tag instead of replacing the old one.
- Mark prereleases that have not been tested on hardware as software-verified
  only.

Recommended tag format:

```text
sentry-v0.1.1
sentry-v0.1.2
sentry-v0.2.0
```

Use patch bumps for target, CLI, and behavior corrections. Use minor bumps
when changing field semantics, trigger format, or operator workflow.

## Local Verification

Run native tests first:

```bash
/tmp/meshcore-pio-venv/bin/platformio test -e native_sentry
/tmp/meshcore-pio-venv/bin/platformio test -e native_packet_filter
/tmp/meshcore-pio-venv/bin/platformio test -e native_battery_gate
```

Then run release-path firmware builds:

```bash
cd /Users/bisar/Codex/research/mesh/MeshCore-Sentry
PATH=/tmp/meshcore-pio-venv/bin:$PATH FIRMWARE_VERSION=sentry-v0.1.N PLATFORMIO_BUILD_FLAGS=-DRO_MESH_SENTRY bash build.sh build-firmware Heltec_v3_repeater
PATH=/tmp/meshcore-pio-venv/bin:$PATH FIRMWARE_VERSION=sentry-v0.1.N PLATFORMIO_BUILD_FLAGS=-DRO_MESH_SENTRY bash build.sh build-firmware Xiao_nrf52_repeater
```

Expected local artifacts:

```text
Heltec_v3_repeater-...bin
Heltec_v3_repeater-...-merged.bin
Xiao_nrf52_repeater-...uf2
Xiao_nrf52_repeater-...zip
```

ESP32 notes:

- Use the non-merged `.bin` for normal app/OTA-style updates.
- Use the `-merged.bin` for clean flash from offset `0x0`.

nRF52 XIAO notes:

- Use `.zip` for DFU update tools.
- Use `.uf2` for bootloader drag-and-drop when available.

## Publish Flow

After local verification:

```bash
git push origin sentry/initial-module
git tag -a sentry-v0.1.N -m "RO-Mesh Sentry Firmware v0.1.N"
git push origin sentry-v0.1.N
```

The GitHub release workflow must publish only the current target matrix:

```text
Heltec_v3_repeater
Xiao_nrf52_repeater
```

After the Actions run completes, record:

```text
tag:
commit:
workflow run URL:
release URL:
assets:
software tests:
hardware/RF status:
```

## Field Test Plan

Run field tests on one repeater first, then on a short multi-hop path.

Single repeater checks:

```text
boot after flash
serial CLI reachable
sentry status
sentry export config
sentry export presets
sentry export tokens
set sentry.channel ...
sentry tokens rotate ...
set preset ...
set sentry.delay ...
```

Trigger checks:

```text
wrong channel does nothing
wrong token does nothing
wrong auth signature does nothing
valid token stages the expected preset
used token cannot be replayed
apply delay waits before reboot/profile switch
profile after reboot is the expected preset
```

Multi-hop checks:

```text
early-hop repeater stays on old profile during delay
last-hop repeater hears the same trigger before apply
all targeted repeaters converge to the same preset
non-targeted repeaters ignore the trigger
```

Do not run repeated trigger tests on the live mesh without an explicit test
window and fresh disposable tokens.

## Test Result Report

Send one report per flashed device or per coordinated path test.

```text
Iteration/tag:
Release URL:
Tester:
Date/time:

Device:
Hardware:
Previous firmware:
Flashed asset:
Flash method:
Boot result:

CLI snapshot:
- sentry status:
- sentry export config:
- sentry export presets:
- sentry export tokens:

Tests:
1. wrong channel:
   expected:
   observed:
   result:
2. wrong token:
   expected:
   observed:
   result:
3. valid trigger:
   expected:
   observed:
   result:
4. replay used token:
   expected:
   observed:
   result:
5. delay / last-hop propagation:
   expected:
   observed:
   result:

Logs/screenshots:
CoreScope packet links:
Failures/blockers:
Requested next iteration:
```

Use result labels:

```text
PASS: behavior matches expected result
FAIL: behavior is wrong or unsafe
BLOCKED: test could not be completed
OBSERVED: useful finding, not pass/fail yet
```

## Feedback Triage

Map field reports into the next action:

```text
flash/boot failure -> target/build issue
CLI command failure -> parser/config issue
wrong trigger accepted -> security issue, stop release line
valid trigger ignored -> channel/token/auth parse issue
early hop switches too fast -> delay/propagation issue
last hop misses trigger -> increase delay or change trigger strategy
token replay works -> security issue, stop release line
```

Security issues get a new patch iteration before any more field expansion.

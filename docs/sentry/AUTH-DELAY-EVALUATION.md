# RO-Mesh Sentry Auth And Delay Evaluation

## Decision

Use per-repeater credentials, not a universal fleet trigger password.

The accepted trigger path is:

1. MeshCore group decrypt/MAC must succeed for the configured Sentry channel.
2. The message must contain a preset id and a locally configured single-use token.
3. If `set sentry.auth <secret>` is configured on that repeater, the message must
   include `sig:<8 hex>` where the signature is computed over `<preset>:<token>`.
4. The token is burned in flash before the preset is staged.
5. The target preset is copied into normal radio prefs immediately, but reboot is
   delayed by `set sentry.delay <seconds>`; default is 30 seconds.

## Why The Delay Changes The Original Plan

The initial prompt suggested a 3-5 second reboot after token burn. That can split
a multi-hop propagation path: an early repeater may hear the command and leave the
current frequency before the last repeater in the hop list has heard it.

The revised default is 30 seconds, configurable from 5 to 300 seconds. This keeps
the first repeater on the old profile long enough for the same flooded message to
settle across the path before any node reboots onto the new profile.

## Trigger Formats

- `WX: Temp 21, Hum 40, Stn# 2-RIVER31`
- `WX: Temp 21, Hum 40, Stn# 2-RIVER31-1A2B3C4D`
- `chk 2 RIVER31 normal chatter`
- `chk 2 RIVER31 sig:1A2B3C4D normal chatter`

When `sentry.auth` is on, unsigned trigger messages are silently ignored.

## CLI

- `set sentry.channel <#name|Public|name psk_hex>`
- `set sentry.tokens <t1> <t2> ...`
- `set sentry.auth <secret|off>`
- `set sentry.delay <5..300>`
- `set preset <id> <freq> <bw> <sf> <cr>`
- `sentry status`

## Residual Risk

The single-byte MeshCore channel hash is not enough by itself, so Sentry also
stores and compares the derived channel secret after group decrypt. A stolen
token is still enough if `sentry.auth` is off; production repeaters should enable
per-repeater `sentry.auth` and provision different token sets per repeater.

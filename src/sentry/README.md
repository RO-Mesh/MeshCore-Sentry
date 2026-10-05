# RO-Mesh Sentry module

The build-owned implementation is in `examples/simple_repeater/SentryManager.*`
because this MeshCore fork compiles repeater-owned extensions from the
`examples/simple_repeater` source filter.

The module is enabled only when the firmware build defines `RO_MESH_SENTRY`.
Hooks are limited to:

- `MyMesh::onGroupDataRecv()` after keyed group-message decryption.
- `MyMesh::handleCommand()` before the common CLI fallback.
- `MyMesh::loop()` for delayed reboot after the selected preset was persisted.

Trigger authentication is local to each repeater. Tokens and optional
`sentry.auth` are configured independently per node; there is no fleet-wide
trigger password.

# skill-lora-packet-parser

Parser scope:

- Inspect only decrypted MeshCore group text delivered to
  `MyMesh::onGroupDataRecv()`.
- Accepted low-profile formats:
  - `WX: Temp <val>, Hum <val>, Stn# <preset>-<token>[-<sig8>]`
  - `chk <preset> <token> [sig:<sig8>] [chatter]`
- Reject partial tokens longer than the fixed token buffer.
- Never emit a visible error or reply for rejected trigger messages.

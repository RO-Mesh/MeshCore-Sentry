# skill-nvs-token-validator

Validation checklist:

- Fresh config creates three default presets and no live tokens.
- `set sentry.tokens` replaces the active token table and clears spent state.
- First matching trigger burns the token and schedules exactly one profile switch.
- Replaying the same message after burn is silently rejected.
- Invalid preset id, wrong channel, wrong signature, and exhausted token table
  do not modify persisted config.
- Flash save failure must leave a trigger unapplied.

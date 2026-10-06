# gc-local-bot-difficulty-team-apply requirements

## Scope

Reduce Local lobby write scatter by moving 7047 team-based bot difficulty writes behind a named state helper.

## Requirements

- 7047 set team slot shall apply bot difficulty through a `gbe::dota_lobby_state` helper.
- The helper shall update dire bot difficulty for dire teams.
- The helper shall update radiant bot difficulty for other teams.
- Existing request guard, bot team derivation, member update, normalize, publish, details update, and ack order shall remain unchanged.

## Non-Goals

- Do not change 7047 request parsing.
- Do not change lobby member slot update behavior.
- Do not change publish or response behavior.

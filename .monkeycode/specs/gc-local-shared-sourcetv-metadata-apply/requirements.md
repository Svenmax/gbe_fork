# gc-local-shared-sourcetv-metadata-apply requirements

## Scope

Start reducing Local/shared dual-track risk by moving one direct Local SourceTV metadata write field group behind a named state helper.

## Requirements

- `tv_secret_code` and `tv_port` shall be applied to `GBE_LocalLobby` through a `gbe::dota_lobby_state` helper.
- Zero `tv_secret_code` and zero `tv_port` inputs shall preserve existing lobby values.
- The helper shall report whether any field changed.
- 4508 post-login handling shall keep existing publish timing and behavior.
- Production shared Store behavior shall remain generation-gated through existing publish paths.

## Non-Goals

- Do not remove `GBE_local_lobby`.
- Do not change SourceTV watch response behavior.
- Do not change shared Store APIs or production DI.

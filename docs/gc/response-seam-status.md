# Dota Response Seam Status

This document replaces the phase-style response seam notes with a review matrix. Use it to decide whether a response call site should move behind a named helper, stay direct, or wait for stronger recorder coverage.

## Current Helpers

| Helper | Inner emsg | Production owner | Covered call sites | Coverage contract | Continue condition |
| --- | --- | --- | --- | --- | --- |
| `GBE_PushDotaCacheUnsubscribedResponse(...)` | `GBE_kDotaCacheUnsubscribed` (`25`) | `GBE_PushDotaResponse(...)` | `7040_leave_25`, `7040_leave_after_lobby_list_25` | `test_lobby_leave_queues_25_then_clears_local_lobby` verifies emsg `25`, wrapped/session metadata, reason, suppression before response, and cleanup after response. | Add more call sites only when recorder coverage proves the same metadata and cleanup order for that path. |
| `GBE_PushDotaOtherLeftChannelResponse(...)` | `GBE_kDotaOtherLeftChannel` (`7014`) | `GBE_PushDotaResponse(...)` | `7272_7014`, `stale_7272_7014` | `test_chat_leave_postgame_channel_order` and `test_chat_leave_postgame_skips_stale_republish_after_shared_clear` verify emsg `7014`, wrapped/session metadata, reason, and response before cleanup or publish. | Add more call sites only after the target path proves postgame response ordering. |
| `GBE_PushDotaPracticeLobbyResponse(...)` | `GBE_kDotaPracticeLobbyResponse` (`7055`) | `GBE_PushDotaResponse(...)` | `7038_7055`, `7047_7055`, `7149_7055` | `test_lobby_create_records_cache_subscription_before_pushes`, `test_lobby_set_team_slot_publishes_before_details_and_ack`, and `test_lobby_join_broadcast_publishes_before_details_and_ack` verify emsg `7055`, wrapped/session metadata, reason, and ordering after cache subscription or details update. | Add more call sites only inside a path with existing recorder coverage for order, metadata, and request-job behavior. |
| `GBE_PushDotaJoinChatChannelResponse(...)` | `GBE_kDotaJoinChatChannelResponse` (`7010`) | `GBE_PushDotaResponse(...)` | `7009_7010` | `test_chat_join_channel` verifies publish before response plus emsg `7010`, wrapped/session metadata, and reason. | Keep legacy signout paths direct until recorder coverage proves their ordering. |

## Intentional Direct Calls

| Call site | Current reason | Required coverage before changing |
| --- | --- | --- |
| `postgame_7010_after_signout` in `GBE_HandleDotaLeaveChatChannelRequest(...)` | Legacy signout chat recovery path lacks recorder coverage for the postgame `7010` order and metadata. | A smoke test should prove the legacy signout branch queues `7010` with wrapped/session metadata and preserves the expected cleanup or publish order. |
| `GBE_SendDotaPracticeLobbyDetailsUpdate(...)` | Details update is already a named member helper and carries lobby state metadata through `GBE_PushDotaResponse(...)`. | Change only if a details-update facade can preserve reason, wrapped/session, apply-lobby-state, and ordering after publish. |
| Launch/custom-game event responses in `GBE_SendDotaCustomGameLaunchSetupFlow(...)` | Event plan responses carry dynamic emsg, reason, and apply-lobby-state fields. | Change only through a tested event executor that preserves dynamic response metadata. |
| Template replay and misc direct responses | These paths are replay or standalone canned-response flows; many have low lifecycle coupling. | Add helper only when repeated behavior creates review pain and tests can prove payload and reason equivalence. |

## Stop Rule

Do not add another response helper just to hide `GBE_PushDotaResponse(...)`. A helper is worthwhile only when it names a reviewed side effect, preserves immediate versus delayed behavior, and has recorder coverage for emsg, wrapped/session metadata, reason string, and surrounding lifecycle order.

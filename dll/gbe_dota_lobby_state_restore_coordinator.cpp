/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

// Split from gbe_dota_lobby_state_coordinator.cpp (stage D.10.2). Behavior unchanged.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lifecycle_state_machine.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_reconnect_context.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include <algorithm>
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

void Steam_Game_Coordinator::GBE_RestoreSharedDotaLobbyState(const char *reason)
{
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();

    if (!shared_lobby.valid) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped this=%p shared_lobby=%p reason=%s valid=0",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_SharedLobbyStore()),
            reason ? reason : "unknown"
        );
        return;
    }

    if (GBE_ShouldSuppressDotaAbandonedLobby(shared_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped for suppressed abandoned lobby this=%p shared_lobby=%p reason=%s lobby_id=%llu active=%u state=%u game_state=%u",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_SharedLobbyStore()),
            reason ? reason : "unknown",
            static_cast<unsigned long long>(shared_lobby.lobby_id),
            shared_lobby.active ? 1u : 0u,
            shared_lobby.state,
            shared_lobby.game_state
        );
        return;
    }

    if (!is_server) {
        if (!shared_lobby.active)
            return;

        gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
        const GBE_LocalLobby &initial_lobby_snapshot = local_lobby.snapshot();
        if (initial_lobby_snapshot.active && initial_lobby_snapshot.lobby_id != 0 && initial_lobby_snapshot.lobby_id != shared_lobby.lobby_id)
            return;

        if (!initial_lobby_snapshot.active || initial_lobby_snapshot.lobby_id == 0) {
            // Only adopt if this GC instance belongs to a lobby member (or owner).
            // Bot/fake-player GC instances have steam_ids that are not lobby members
            // and should not adopt the shared lobby state (avoids hundreds of redundant
            // adopt cycles during post-game GC re-initialization).
            const uint64 local_sid = settings ? settings->get_local_steam_id().ConvertToUint64() : 0;
            if (local_sid != 0 && local_sid != shared_lobby.owner_steam_id) {
                bool is_member = false;
                for (const auto &m : shared_lobby.members) {
                    if (m.steam_id == local_sid) { is_member = true; break; }
                }
                if (!is_member) {
                    return;  // Not a lobby participant; skip adopt
                }
            }

            local_lobby.apply(reason ? reason : "restore_client_full_adopt", [&shared_lobby](GBE_LocalLobby &lobby) {
                gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared_lobby, false, true, lobby);
            });
            const GBE_LocalLobby &adopted_lobby_snapshot = local_lobby.snapshot();
            const std::uint64_t recovery_base_generation = std::max(
                static_cast<std::uint64_t>(GBE_CurrentDotaLobbyGeneration()),
                adopted_lobby_snapshot.generation);
            GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter(
                gbe::dota_lobby_generation::Generation{recovery_base_generation});
            if (GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Recover, reason) == GBE_DotaGenerationAdvanceResult::Exhausted) {
                local_lobby.replace_for_reset(GBE_LocalLobby{});
                return;
            }
            local_lobby.apply(reason ? reason : "restore_client_full_adopt", [this](GBE_LocalLobby &lobby) {
                gbe::dota_lobby_state::apply_lobby_generation(lobby, GBE_CurrentDotaLobbyGeneration());
            });
            const GBE_LocalLobby &restored_lobby_snapshot = local_lobby.snapshot();

            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted full shared lobby on client this=%p shared_lobby=%p reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_SharedLobbyStore()),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(restored_lobby_snapshot.lobby_id),
                restored_lobby_snapshot.state,
                restored_lobby_snapshot.game_state,
                static_cast<unsigned long long>(restored_lobby_snapshot.server_id)
            );
            GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_full_adopt");
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_full_adopt");
            GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_full_adopt");
            GBE_PushDotaLoginSyncMessages();
            return;
        }

        const GBE_LocalLobby &local_lobby_snapshot = local_lobby.snapshot();
        bool changed = false;
        const gbe::dota_lobby_state::SourceAwareSharedRuntimeRestorePlan runtime_restore_plan =
            gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
                local_lobby_snapshot,
                shared_lobby,
                GBE_kDotaLaunchPhaseRunQueued);
        const std::uint64_t synchronized_generation = std::max(
            static_cast<std::uint64_t>(GBE_CurrentDotaLobbyGeneration()),
            shared_lobby.generation);
        if (synchronized_generation != GBE_CurrentDotaLobbyGeneration()) {
            GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter(
                gbe::dota_lobby_generation::Generation{synchronized_generation});
        }
        {
            changed = local_lobby.apply(
                reason ? reason : "restore_client_runtime_identity",
                [synchronized_generation, &shared_lobby](GBE_LocalLobby &lobby) {
                    bool local_changed = false;
                    if (gbe::dota_lobby_state::restore_lobby_generation(lobby, synchronized_generation)) {
                        local_changed = true;
                    }
                    if (gbe::dota_lobby_state::restore_lobby_generic_lobby_id(lobby, shared_lobby.generic_lobby_id)) {
                        local_changed = true;
                    }
                    return local_changed;
                }) || changed;
        }
        const GBE_LocalLobby &post_identity_snapshot = local_lobby.snapshot();
        const uint64 previous_server_id = post_identity_snapshot.server_id;
        const std::string previous_connect = post_identity_snapshot.connect;

        if (runtime_restore_plan.ignored_readyup_regression) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored shared READYUP regression reason=%s lobby_id=%llu local_state=%u local_game_state=%u shared_state=%u shared_game_state=%u launch_phase=%s",
                reason ? reason : "restore_shared_lobby_state",
                static_cast<unsigned long long>(post_identity_snapshot.lobby_id),
                post_identity_snapshot.state,
                post_identity_snapshot.game_state,
                shared_lobby.state,
                shared_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(post_identity_snapshot.launch_phase)
            );
        }
        {
            gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
            changed = local_lobby.apply(
                reason ? reason : "restore_client_runtime",
                [&runtime_restore_plan, &shared_lobby](GBE_LocalLobby &lobby) {
                    bool local_changed = false;
                    local_changed = gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(
                        lobby,
                        runtime_restore_plan) || local_changed;
                    local_changed = gbe::dota_lobby_state::apply_shared_lobby_options_restore_plan(
                        lobby,
                        gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(
                            lobby,
                            shared_lobby)) || local_changed;
                    if (gbe::dota_lobby_state::restore_lobby_custom_game(lobby, shared_lobby.custom_game)) {
                        local_changed = true;
                    }
                    if (gbe::dota_lobby_state::restore_lobby_owner_connected(lobby, shared_lobby.owner_connected)) {
                        local_changed = true;
                    }
                    if (gbe::dota_lobby_state::restore_launch_4511_seen(lobby, shared_lobby.launch_4511_seen)) {
                        local_changed = true;
                    }
                    if (gbe::dota_lobby_state::restore_lobby_owner_team(lobby, shared_lobby.owner_team)) {
                        local_changed = true;
                    }
                    if (gbe::dota_lobby_state::restore_lobby_owner_slot(lobby, shared_lobby.owner_slot)) {
                        local_changed = true;
                    }
                    local_changed = gbe::dota_lobby_state::apply_owner_hero_from_shared(lobby, shared_lobby) || local_changed;
                    if (gbe::dota_lobby_state::restore_lobby_members(lobby, shared_lobby.members)) {
                        local_changed = true;
                    }
                    local_changed = gbe::dota_lobby_state::apply_shared_lobby_cache_restore_plan(
                        lobby,
                        gbe::dota_lobby_state::compose_shared_lobby_cache_restore_plan(
                            lobby,
                            shared_lobby)) || local_changed;
                    return local_changed;
                }) || changed;
        }

        if (changed) {
            const GBE_LocalLobby &restored_lobby_snapshot = local_lobby.snapshot();
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted shared runtime on client this=%p shared_lobby=%p reason=%s lobby_id=%llu generic_lobby_id=%llu old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_SharedLobbyStore()),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(restored_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(restored_lobby_snapshot.generic_lobby_id),
                static_cast<unsigned long long>(previous_server_id),
                static_cast<unsigned long long>(restored_lobby_snapshot.server_id),
                previous_connect.c_str(),
                restored_lobby_snapshot.connect.c_str()
            );
        }

        // NOTE: Signature clearing was previously done here for LAN reconnect,
        // but it caused every restore_client_runtime invocation (including
        // routine lobby state syncs while the player is still connected) to
        // re-fire GameServerChangeRequested_t, producing an automatic
        // reconnect loop.  Manual reconnect works through Dota's own rich
        // presence "connect" field (+connect <LAN IP>:<port>) which is kept
        // up-to-date by GBE_ReapplyDotaPracticeLobbyLaunchRichPresence below,
        // so GameServerChangeRequested_t does not need to fire again.

        GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_runtime");
        GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_runtime");
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_runtime");
        GBE_PushDotaLoginSyncMessages();
        return;
    }

    {
        gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
        local_lobby.apply(reason ? reason : "restore_server_or_full", [&shared_lobby](GBE_LocalLobby &lobby) {
            gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared_lobby, true, false, lobby);
        });
        const GBE_LocalLobby &restored_lobby_snapshot = local_lobby.snapshot();

        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restored shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_SharedLobbyStore()),
            reason ? reason : "unknown",
            restored_lobby_snapshot.active ? 1u : 0u,
            static_cast<unsigned long long>(restored_lobby_snapshot.lobby_id),
            static_cast<unsigned long long>(restored_lobby_snapshot.generic_lobby_id),
            static_cast<unsigned long long>(restored_lobby_snapshot.match_id),
            static_cast<unsigned long long>(restored_lobby_snapshot.owner_steam_id),
            restored_lobby_snapshot.owner_account_id,
            restored_lobby_snapshot.state,
            restored_lobby_snapshot.game_state,
            restored_lobby_snapshot.owner_team,
            restored_lobby_snapshot.owner_slot,
            restored_lobby_snapshot.connect.c_str()
        );
    }
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_server_or_full");
    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_server_or_full");
}

uint64 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerSteamId() const
{
    const GBE_LocalLobby &local_lobby_snapshot = GBE_local_lobby;
    if (local_lobby_snapshot.owner_steam_id != 0)
        return local_lobby_snapshot.owner_steam_id;

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (shared_lobby.owner_steam_id != 0)
        return shared_lobby.owner_steam_id;

    return settings->get_local_steam_id().ConvertToUint64();
}

uint32 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerAccountId() const
{
    const GBE_LocalLobby &local_lobby_snapshot = GBE_local_lobby;
    if (local_lobby_snapshot.owner_account_id != 0)
        return local_lobby_snapshot.owner_account_id;

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (shared_lobby.owner_account_id != 0)
        return shared_lobby.owner_account_id;

    return settings->get_local_steam_id().GetAccountID();
}

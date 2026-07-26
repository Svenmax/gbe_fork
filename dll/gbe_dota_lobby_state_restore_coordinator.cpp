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
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lifecycle_state_machine.h"
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
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.lobby_id != shared_lobby.lobby_id)
            return;

        if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
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

            gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared_lobby, false, true, GBE_local_lobby);
            const std::uint64_t recovery_base_generation = std::max(
                static_cast<std::uint64_t>(GBE_CurrentDotaLobbyGeneration()),
                GBE_local_lobby.generation);
            GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter(
                gbe::dota_lobby_generation::Generation{recovery_base_generation});
            if (GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Recover, reason) == GBE_DotaGenerationAdvanceResult::Exhausted) {
                GBE_local_lobby = {};
                return;
            }
            gbe::dota_lobby_state::apply_lobby_generation(GBE_local_lobby, GBE_CurrentDotaLobbyGeneration());

            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted full shared lobby on client this=%p shared_lobby=%p reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_SharedLobbyStore()),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.server_id)
            );
            GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_full_adopt");
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_full_adopt");
            GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_full_adopt");
            GBE_PushDotaLoginSyncMessages();
            return;
        }

        bool changed = false;
        const gbe::dota_lobby_state::SourceAwareSharedRuntimeRestorePlan runtime_restore_plan =
            gbe::dota_lobby_state::compose_source_aware_shared_runtime_restore_plan(
                GBE_local_lobby,
                shared_lobby,
                GBE_kDotaLaunchPhaseRunQueued);
        const std::uint64_t synchronized_generation = std::max(
            static_cast<std::uint64_t>(GBE_CurrentDotaLobbyGeneration()),
            shared_lobby.generation);
        if (synchronized_generation != GBE_CurrentDotaLobbyGeneration()) {
            GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter(
                gbe::dota_lobby_generation::Generation{synchronized_generation});
        }
        if (gbe::dota_lobby_state::restore_lobby_generation(
                GBE_local_lobby,
                synchronized_generation)) {
            changed = true;
        }
        if (gbe::dota_lobby_state::restore_lobby_generic_lobby_id(
                GBE_local_lobby,
                shared_lobby.generic_lobby_id)) {
            changed = true;
        }
        const uint64 previous_server_id = GBE_local_lobby.server_id;
        const std::string previous_connect = GBE_local_lobby.connect;

        if (runtime_restore_plan.ignored_readyup_regression) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored shared READYUP regression reason=%s lobby_id=%llu local_state=%u local_game_state=%u shared_state=%u shared_game_state=%u launch_phase=%s",
                reason ? reason : "restore_shared_lobby_state",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                shared_lobby.state,
                shared_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
            );
        }
        changed = gbe::dota_lobby_state::apply_source_aware_shared_runtime_restore_plan(
            GBE_local_lobby,
            runtime_restore_plan) || changed;

        changed = gbe::dota_lobby_state::apply_shared_lobby_options_restore_plan(
            GBE_local_lobby,
            gbe::dota_lobby_state::compose_shared_lobby_options_restore_plan(
                GBE_local_lobby,
                shared_lobby)) || changed;

        if (gbe::dota_lobby_state::restore_lobby_custom_game(
                GBE_local_lobby,
                shared_lobby.custom_game)) {
            changed = true;
        }

        if (gbe::dota_lobby_state::restore_lobby_owner_connected(
                GBE_local_lobby,
                shared_lobby.owner_connected)) {
            changed = true;
        }

        if (gbe::dota_lobby_state::restore_launch_4511_seen(
                GBE_local_lobby,
                shared_lobby.launch_4511_seen)) {
            changed = true;
        }

        if (gbe::dota_lobby_state::restore_lobby_owner_team(
                GBE_local_lobby,
                shared_lobby.owner_team)) {
            changed = true;
        }

        if (gbe::dota_lobby_state::restore_lobby_owner_slot(
                GBE_local_lobby,
                shared_lobby.owner_slot)) {
            changed = true;
        }

        if (gbe::dota_lobby_state::apply_owner_hero_from_shared(GBE_local_lobby, shared_lobby))
            changed = true;

        if (gbe::dota_lobby_state::restore_lobby_members(
                GBE_local_lobby,
                shared_lobby.members)) {
            changed = true;
        }

        changed = gbe::dota_lobby_state::apply_shared_lobby_cache_restore_plan(
            GBE_local_lobby,
            gbe::dota_lobby_state::compose_shared_lobby_cache_restore_plan(
                GBE_local_lobby,
                shared_lobby)) || changed;

        if (changed) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted shared runtime on client this=%p shared_lobby=%p reason=%s lobby_id=%llu generic_lobby_id=%llu old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_SharedLobbyStore()),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                static_cast<unsigned long long>(previous_server_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                previous_connect.c_str(),
                GBE_local_lobby.connect.c_str()
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

    gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared_lobby, true, false, GBE_local_lobby);

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "restored shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_SharedLobbyStore()),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_server_or_full");
    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_server_or_full");
}

uint64 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerSteamId() const
{
    if (GBE_local_lobby.owner_steam_id != 0)
        return GBE_local_lobby.owner_steam_id;

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (shared_lobby.owner_steam_id != 0)
        return shared_lobby.owner_steam_id;

    return settings->get_local_steam_id().ConvertToUint64();
}

uint32 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerAccountId() const
{
    if (GBE_local_lobby.owner_account_id != 0)
        return GBE_local_lobby.owner_account_id;

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (shared_lobby.owner_account_id != 0)
        return shared_lobby.owner_account_id;

    return settings->get_local_steam_id().GetAccountID();
}

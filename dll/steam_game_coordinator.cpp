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

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_handler_registry.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_runtime_state.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include "gbe_dota_payload_wire_helpers.h"
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
#include <stdexcept>
#include <string>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;
namespace registry = gbe::dota_handler_registry;

constexpr int GC_MIN_VERSION = 20091217;

const GBE_DotaLootListData &GBE_GetDotaVpkLootData()
{
    return GBE_DotaRuntimeState().vpk_loot_data;
}

void GBE_SetDotaVpkLootData(GBE_DotaLootListData &&loot_data)
{
    GBE_DotaRuntimeState().vpk_loot_data = std::move(loot_data);
}

bool GBE_GetRecentDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    const auto &state = GBE_DotaRuntimeState();
    if (!out || !state.recent_reconnect_context_valid)
        return false;

    *out = state.recent_reconnect_context;
    return true;
}

void GBE_SetRecentDotaReconnectContext(const GBE_DotaReconnectContext &ctx)
{
    auto &state = GBE_DotaRuntimeState();
    state.recent_reconnect_context = ctx;
    state.recent_reconnect_context_valid = true;
}

void GBE_ClearRecentDotaReconnectContext()
{
    auto &state = GBE_DotaRuntimeState();
    state.recent_reconnect_context_valid = false;
    state.recent_reconnect_context = GBE_DotaReconnectContext{};
}

bool GBE_IsDotaReconnectEligible()
{
    return GBE_DotaRuntimeState().reconnect_eligible.load();
}

void GBE_SetDotaReconnectEligible(bool eligible)
{
    GBE_DotaRuntimeState().reconnect_eligible.store(eligible);
}

bool GBE_ConsumeDotaReconnectEligibility()
{
    bool expected = true;
    return GBE_DotaRuntimeState().reconnect_eligible.compare_exchange_strong(expected, false);
}

#pragma pack( push, 1 )
//-----------------------------------------------------------------------------
// Purpose: Header for messages from a client or gameserver to or from the GC
//-----------------------------------------------------------------------------
struct GCMsgHdr_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
};

#pragma pack(pop)

bool GBE_HasLastDotaServerHelloContext()
{
    return GBE_DotaRuntimeState().last_server_hello_context.valid;
}

const GBE_DotaServerHelloContext &GBE_GetLastDotaServerHelloContext()
{
    return GBE_DotaRuntimeState().last_server_hello_context;
}

void GBE_SetLastDotaServerHelloContext(const GBE_DotaServerHelloContext &context)
{
    GBE_DotaRuntimeState().last_server_hello_context = context;
}

void GBE_ClearLastDotaServerHelloContext()
{
    GBE_DotaRuntimeState().last_server_hello_context = GBE_DotaServerHelloContext{};
}

using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

using GBE_DotaPracticeLobbyBroadcastChannelRequest = gbe::proto_wire::DotaPracticeLobbyBroadcastChannelRequest;

using GBE_DotaJoinChatChannelRequest = gbe::proto_wire::DotaJoinChatChannelRequest;
using GBE_DotaLeaveChatChannelRequest = gbe::proto_wire::DotaLeaveChatChannelRequest;
using GBE_DotaChatMessageRequest = gbe::proto_wire::DotaChatMessageRequest;

using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;

// --- Phase 3.3: lightweight post-login dispatch table ---------------------
//
// The Dota post-login dispatcher routes incoming GC requests (identified by
// `inner_emsg`) to the appropriate `GBE_HandleDota*Request` member function.
// Before Phase 3.3 this was a 16-arm switch where every arm repeated the same
// shape: `log_lobby_request(); return GBE_HandleDotaXxx(...);`. Adding a new
// handler meant editing the switch and re-deriving the call site by hand.
//
// The table inside GBE_DispatchDotaPostLoginRequest replaces that switch. Each
bool Steam_Game_Coordinator::gc_enabled()
{
    if (gc_profile == GC_PROFILE_DOTA2)
        return settings && settings->get_local_game_id().AppID() == GBE_kDotaAppId;

    return (gc_version >= GC_MIN_VERSION && gc_profile != GC_PROFILE_INVALID);
}

Steam_User_Items *Steam_Game_Coordinator::client_items()
{
    return get_steam_client()->steam_user_items;
}

Steam_GameServer_Items *Steam_Game_Coordinator::server_items()
{
    return get_steam_client()->steam_gameserver_items;
}

void Steam_Game_Coordinator::parse_gc_config()
{
    gc_profile = GC_PROFILE_INVALID;
    gc_version = 0;
    is_portal2 = false;

    std::string file_path = Local_Storage::get_game_settings_path() + gc_config_file;
    nlohmann::json gc_json;
    const uint32 app_id = settings ? settings->get_local_game_id().AppID() : 0u;
    const bool loaded = local_storage && local_storage->load_json(file_path, gc_json);
    const gbe::gc_config::ParseResult result = gbe::gc_config::parse_gc_config(loaded, gc_json, app_id, GBE_kDotaAppId);

    gc_version = result.version;
    is_portal2 = result.is_portal2;
    switch (result.profile) {
        case gbe::gc_config::Profile::Tf2:
            gc_profile = GC_PROFILE_TF2;
            break;
        case gbe::gc_config::Profile::Dota2:
            gc_profile = GC_PROFILE_DOTA2;
            break;
        default:
            gc_profile = GC_PROFILE_INVALID;
            break;
    }

    if (!result.error_message.empty())
        PRINT_DEBUG("error parsing GC config: %s", result.error_message.c_str());

    if (result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::MissingConfig) {
        GBE_GC_DebugLog("GC_CONFIG", "auto-enabled Dota2 GC profile for app %u", app_id);
    } else if (result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::InvalidProfile) {
        GBE_GC_DebugLog("GC_CONFIG", "fell back to Dota2 GC profile for app %u", app_id);
    }
}

bool Steam_Game_Coordinator::is_welcome_message(const GC_Message &message)
{
    uint32 msg_type = message.msg_type & (~protobuf_mask);
    return (msg_type == 4004 ||
        msg_type == 4005);
}

uint64 Steam_Game_Coordinator::GBE_CurrentDotaLobbyGeneration() const
{
    return GBE_dota_lobby_generation_counter.current().value;
}

Steam_Game_Coordinator::GBE_DotaGenerationAdvanceResult Steam_Game_Coordinator::GBE_AdvanceDotaLobbyGeneration(
    gbe::dota_lobby_generation::Boundary boundary,
    const char *reason)
{
    const auto result = GBE_dota_lobby_generation_counter.advance(boundary);
    if (!result.advanced) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Refusing lifecycle generation reuse after counter exhaustion boundary=%u generation=%llu reason=%s",
            static_cast<unsigned int>(boundary),
            static_cast<unsigned long long>(result.current.value),
            reason ? reason : "unknown");
        return GBE_DotaGenerationAdvanceResult::Exhausted;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Advanced lifecycle generation boundary=%u previous=%llu current=%llu reason=%s",
        static_cast<unsigned int>(boundary),
        static_cast<unsigned long long>(result.previous.value),
        static_cast<unsigned long long>(result.current.value),
        reason ? reason : "unknown");
    return GBE_DotaGenerationAdvanceResult::Advanced;
}

bool Steam_Game_Coordinator::GBE_IsQueuedLobbyMessageCurrent(const GC_Message &message, const char *stage) const
{
    if (!message.validate_lobby_generation)
        return true;

    const bool current = message.lobby_id != 0 &&
        message.lobby_id == GBE_local_lobby.lobby_id &&
        message.generation == GBE_CurrentDotaLobbyGeneration();
    if (!current) {
        GBE_ReconnectLogEvent(gbe::dota_diagnostic::with_message_id({
            "reconnect.stale_generation",
            gbe::dota_diagnostic::Reason::StaleGeneration,
            gbe::dota_diagnostic::Source::CallbackQueue,
            message.lobby_id,
            message.generation,
            0u,
            {},
            std::to_string(GBE_CurrentDotaLobbyGeneration()),
        }, GBE_GC_MaskedEMsg(message.msg_type)));
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "rejected stale lobby message reason=stale_generation stage=%s msg=%u queued_lobby_id=%llu queued_generation=%llu current_lobby_id=%llu current_generation=%llu",
            stage ? stage : "unknown",
            GBE_GC_MaskedEMsg(message.msg_type),
            static_cast<unsigned long long>(message.lobby_id),
            static_cast<unsigned long long>(message.generation),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_CurrentDotaLobbyGeneration()));
    }
    return current;
}

void Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState(const GC_Message &message)
{
    if (!message.apply_lobby_state)
        return;

    if (gc_profile == GC_PROFILE_DOTA2 && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)) {
        const auto shared_snapshot = GBE_SharedLobbyStore().snapshot();
        if (shared_snapshot.valid && shared_snapshot.active && shared_snapshot.lobby_id != 0) {
            GBE_RestoreSharedDotaLobbyState("queued_state_preapply");
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "queued lobby state has no full local/shared lobby context this=%p shared_valid=%u shared_active=%u shared_lobby_id=%llu msg=%u state=%u game_state=%u",
                static_cast<void *>(this),
                shared_snapshot.valid ? 1u : 0u,
                shared_snapshot.active ? 1u : 0u,
                static_cast<unsigned long long>(shared_snapshot.lobby_id),
                GBE_GC_MaskedEMsg(message.msg_type),
                message.lobby_state,
                message.lobby_game_state
            );
        }
    }

    const uint32 previous_game_state = GBE_local_lobby.game_state;
    const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan apply_plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
        GBE_local_lobby,
        message.lobby_state,
        message.lobby_game_state,
        gc_profile == GC_PROFILE_DOTA2,
        GBE_kDotaLaunchPhaseSetupSynced,
        GBE_kDotaLaunchPhaseRunQueued);
    if (apply_plan.preserved_game_state) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "preserving monotonic launch game_state on queued apply msg=%u state=%u previous_game_state=%u queued_game_state=%u lobby_id=%llu",
            GBE_GC_MaskedEMsg(message.msg_type),
            message.lobby_state,
            previous_game_state,
            message.lobby_game_state,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
    }

    GBE_local_lobby.state = apply_plan.state;
    GBE_local_lobby.game_state = apply_plan.game_state;
    GBE_local_lobby.launch_phase = apply_plan.launch_phase;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Applied queued state=%u game_state=%u launch_phase=%s msg=%u active=%u lobby_id=%llu server_id=%llu",
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
        GBE_GC_MaskedEMsg(message.msg_type),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );

    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence("queued_state");

    if (gc_profile == GC_PROFILE_DOTA2 &&
        is_server &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u) {
        GBE_PushDotaLaunchStateToClientPeer("queued_state");
    }

    GBE_PublishSharedDotaLobbyState("queued_state");
}

void Steam_Game_Coordinator::push_incoming(uint32 msg_type, const std::string &message, double delay, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state)
{
    PRINT_DEBUG("%u %.2f", msg_type, delay);

    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = delay;
    new_item.sequence = ++pending_message_sequence;
    new_item.apply_lobby_state = apply_lobby_state;
    new_item.lobby_state = lobby_state;
    new_item.lobby_game_state = lobby_game_state;
    new_item.lobby_id = GBE_local_lobby.lobby_id;
    new_item.generation = GBE_CurrentDotaLobbyGeneration();
    new_item.validate_lobby_generation = gc_profile == GC_PROFILE_DOTA2 && apply_lobby_state;
    pending_messages.push_back(new_item);

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "queued delayed msg=%u this=%p delay=%.3f pending_size=%zu incoming_size=%zu apply_state=%u lobby_state=%u game_state=%u",
        GBE_GC_MaskedEMsg(msg_type),
        static_cast<void *>(this),
        delay,
        pending_messages.size(),
        incoming_messages.size(),
        apply_lobby_state ? 1u : 0u,
        lobby_state,
        lobby_game_state
    );
}

bool Steam_Game_Coordinator::GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(uint32 masked_emsg) const
{
    switch (masked_emsg) {
        case GBE_kDotaCacheSubscribed:
        case GBE_kDotaPracticeLobbyDetailsUpdate:
        case 7034u:
        case 5501u:
        case 5575u:
        case 779u:
        case 766u:
            return true;
        default:
            return false;
    }
}

void Steam_Game_Coordinator::GBE_DiscardQueuedDotaLaunchMessagesForAbandon(const char *reason)
{
    auto discard_for_instance = [reason](Steam_Game_Coordinator *coordinator) {
        if (!coordinator)
            return;

        size_t removed_pending = 0;
        for (auto it = coordinator->pending_messages.begin(); it != coordinator->pending_messages.end();) {
            if (coordinator->GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(GBE_GC_MaskedEMsg(it->msg_type))) {
                it = coordinator->pending_messages.erase(it);
                ++removed_pending;
            } else {
                ++it;
            }
        }

        size_t removed_incoming = 0;
        std::queue<GC_Message> filtered_incoming;
        while (!coordinator->incoming_messages.empty()) {
            GC_Message queued = coordinator->incoming_messages.front();
            coordinator->incoming_messages.pop();
            if (coordinator->GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(GBE_GC_MaskedEMsg(queued.msg_type))) {
                ++removed_incoming;
                continue;
            }
            filtered_incoming.push(std::move(queued));
        }
        coordinator->incoming_messages.swap(filtered_incoming);

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Discarded queued launch messages for abandon reason=%s this=%p is_server=%u removed_pending=%zu removed_incoming=%zu remaining_pending=%zu remaining_incoming=%zu",
            reason ? reason : "unknown",
            static_cast<void *>(coordinator),
            coordinator->is_server ? 1u : 0u,
            removed_pending,
            removed_incoming,
            coordinator->pending_messages.size(),
            coordinator->incoming_messages.size()
        );
    };

    discard_for_instance(this);

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client)
        return;

    Steam_Game_Coordinator *peer = is_server ? steam_client->steam_game_coordinator : steam_client->steam_gameserver_game_coordinator;
    if (peer && peer != this)
        discard_for_instance(peer);
}

bool Steam_Game_Coordinator::GBE_ShouldSuppressDotaAbandonedLobby(uint64 lobby_id) const
{
    return gc_profile == GC_PROFILE_DOTA2 && lobby_id != 0 && GBE_suppressed_dota_abandon_lobby_id == lobby_id;
}

bool Steam_Game_Coordinator::GBE_HostHasActiveDotaServerLobby(uint64 lobby_id) const
{
    if (lobby_id == 0)
        return false;
    Steam_Client *steam_client = get_steam_client();
    return steam_client &&
        steam_client->steam_gameserver_game_coordinator &&
        steam_client->steam_gameserver_game_coordinator->GBE_HasActiveServerLobby(lobby_id);
}

void Steam_Game_Coordinator::GBE_MarkDotaAbandonedLobbySuppressed(uint64 lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2 || lobby_id == 0)
        return;

    auto mark_for_instance = [lobby_id, reason](Steam_Game_Coordinator *coordinator) {
        if (!coordinator || coordinator->gc_profile != GC_PROFILE_DOTA2)
            return;

        coordinator->GBE_suppressed_dota_abandon_lobby_id = lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Suppressing stale abandoned lobby updates reason=%s this=%p is_server=%u lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(coordinator),
            coordinator->is_server ? 1u : 0u,
            static_cast<unsigned long long>(lobby_id)
        );
    };

    mark_for_instance(this);

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client)
        return;

    Steam_Game_Coordinator *peer = is_server ? steam_client->steam_game_coordinator : steam_client->steam_gameserver_game_coordinator;
    if (peer && peer != this)
        mark_for_instance(peer);
}

void Steam_Game_Coordinator::GBE_ClearDotaAbandonedLobbySuppression(uint64 lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2 || lobby_id == 0 || GBE_suppressed_dota_abandon_lobby_id != lobby_id)
        return;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Clearing abandoned lobby suppression reason=%s this=%p is_server=%u lobby_id=%llu",
        reason ? reason : "unknown",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        static_cast<unsigned long long>(lobby_id)
    );
    GBE_suppressed_dota_abandon_lobby_id = 0;
}

bool Steam_Game_Coordinator::GBE_HasPendingDotaAbandonFinalizeAfterOtherLeftChannel() const
{
    return GBE_pending_dota_abandon_finalize_after_7014;
}

bool Steam_Game_Coordinator::GBE_HasSentDotaLoginSync() const
{
    return GBE_dota_login_sync_sent;
}

void Steam_Game_Coordinator::GBE_MarkDotaLoginSyncSent()
{
    GBE_dota_login_sync_sent = true;
}

void Steam_Game_Coordinator::GBE_ClearDotaLoginSyncSent()
{
    GBE_dota_login_sync_sent = false;
}

bool Steam_Game_Coordinator::GBE_HasPushedDotaHostShowcaseEquip() const
{
    return GBE_dota_host_showcase_equip_generation == GBE_CurrentDotaLobbyGeneration() &&
        GBE_dota_host_showcase_equip_lobby_id == GBE_local_lobby.lobby_id &&
        GBE_dota_host_showcase_equip_steam_id == GBE_local_lobby.owner_steam_id &&
        GBE_dota_host_showcase_equip_hero_id == GBE_local_lobby.owner_hero_id &&
        GBE_local_lobby.owner_hero_id != 0u;
}

void Steam_Game_Coordinator::GBE_MarkDotaHostShowcaseEquipPushed()
{
    GBE_dota_host_showcase_equip_generation = GBE_CurrentDotaLobbyGeneration();
    GBE_dota_host_showcase_equip_lobby_id = GBE_local_lobby.lobby_id;
    GBE_dota_host_showcase_equip_steam_id = GBE_local_lobby.owner_steam_id;
    GBE_dota_host_showcase_equip_hero_id = GBE_local_lobby.owner_hero_id;
}

void Steam_Game_Coordinator::GBE_ClearDotaHostShowcaseEquipPushed()
{
    GBE_dota_host_showcase_equip_generation = 0;
    GBE_dota_host_showcase_equip_lobby_id = 0;
    GBE_dota_host_showcase_equip_steam_id = 0;
    GBE_dota_host_showcase_equip_hero_id = 0;
}

bool Steam_Game_Coordinator::GBE_HasRefreshedDotaHostLocalWearables(uint64 steam_id, uint32 hero_id) const
{
    return GBE_dota_host_local_wearable_refresh_generation == GBE_CurrentDotaLobbyGeneration() &&
        GBE_dota_host_local_wearable_refresh_steam_id == steam_id &&
        GBE_dota_host_local_wearable_refresh_hero_id == hero_id;
}

void Steam_Game_Coordinator::GBE_MarkDotaHostLocalWearablesRefreshed(uint64 steam_id, uint32 hero_id)
{
    GBE_dota_host_local_wearable_refresh_generation = GBE_CurrentDotaLobbyGeneration();
    GBE_dota_host_local_wearable_refresh_steam_id = steam_id;
    GBE_dota_host_local_wearable_refresh_hero_id = hero_id;
}

void Steam_Game_Coordinator::GBE_ClearDotaHostLocalWearablesRefreshed()
{
    GBE_dota_host_local_wearable_refresh_generation = 0;
    GBE_dota_host_local_wearable_refresh_steam_id = 0;
    GBE_dota_host_local_wearable_refresh_hero_id = 0;
}

bool Steam_Game_Coordinator::GBE_HasReplayedDotaPrivateLobbySnapshot() const
{
    return GBE_dota_private_lobby_snapshot_replayed;
}

void Steam_Game_Coordinator::GBE_MarkDotaPrivateLobbySnapshotReplayed()
{
    GBE_dota_private_lobby_snapshot_replayed = true;
}

void Steam_Game_Coordinator::GBE_ClearDotaPrivateLobbySnapshotReplayed()
{
    GBE_dota_private_lobby_snapshot_replayed = false;
}

uint32 Steam_Game_Coordinator::GBE_GetLastDotaLaunchStatePushedGameState() const
{
    return GBE_last_dota_launch_state_pushed_game_state;
}

void Steam_Game_Coordinator::GBE_SetLastDotaLaunchStatePushedGameState(uint32 game_state)
{
    GBE_last_dota_launch_state_pushed_game_state = game_state;
}

void Steam_Game_Coordinator::GBE_ClearLastDotaLaunchStatePushedGameState()
{
    GBE_last_dota_launch_state_pushed_game_state = 0;
}

const std::string &Steam_Game_Coordinator::GBE_GetLastDotaLaunchPersonaSignature() const
{
    return GBE_last_dota_launch_persona_signature;
}

void Steam_Game_Coordinator::GBE_SetLastDotaLaunchPersonaSignature(const std::string &signature)
{
    GBE_last_dota_launch_persona_signature = signature;
}

void Steam_Game_Coordinator::GBE_ClearLastDotaLaunchPersonaSignature()
{
    GBE_last_dota_launch_persona_signature.clear();
}

const gbe::dota_connection::DedupKey &Steam_Game_Coordinator::GBE_GetLastDotaDirectConnectCallbackKey() const
{
    return GBE_last_dota_direct_connect_callback_key;
}

void Steam_Game_Coordinator::GBE_SetLastDotaDirectConnectCallbackKey(const gbe::dota_connection::DedupKey &key)
{
    GBE_last_dota_direct_connect_callback_key = key;
}

void Steam_Game_Coordinator::GBE_ClearLastDotaDirectConnectCallbackKey()
{
    GBE_last_dota_direct_connect_callback_key = {};
}

bool Steam_Game_Coordinator::GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed() const
{
    return GBE_pending_dota_normal_signout_finalize_after_25;
}

bool Steam_Game_Coordinator::GBE_HasPendingResetAfterCacheUnsubscribed() const
{
    return GBE_pending_reset_after_cache_unsubscribed;
}

void Steam_Game_Coordinator::GBE_SetPendingDotaAbandonFinalizeAfterOtherLeftChannel(uint64 lobby_id)
{
    GBE_pending_dota_abandon_finalize_after_7014 = true;
    GBE_pending_dota_abandon_finalize_lobby_id = lobby_id;
    GBE_pending_dota_abandon_finalize_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
}

Steam_Game_Coordinator::GBE_DotaDeferredTaskConsumeResult Steam_Game_Coordinator::GBE_ConsumePendingDotaAbandonFinalizeAfterOtherLeftChannel()
{
    const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_dota_abandon_finalize_slot, "abandon_finalize_after_7014");
    GBE_pending_dota_abandon_finalize_after_7014 = false;
    GBE_pending_dota_abandon_finalize_lobby_id = 0;
    return result;
}

void Steam_Game_Coordinator::GBE_ClearPendingDotaAbandonFinalizeAfterOtherLeftChannel()
{
    GBE_pending_dota_abandon_finalize_after_7014 = false;
    GBE_pending_dota_abandon_finalize_lobby_id = 0;
    GBE_pending_dota_abandon_finalize_slot = {};
}

void Steam_Game_Coordinator::GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(uint64 lobby_id)
{
    GBE_pending_dota_normal_signout_finalize_after_25 = true;
    GBE_pending_dota_normal_signout_finalize_lobby_id = lobby_id;
    GBE_pending_dota_normal_signout_finalize_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
}

Steam_Game_Coordinator::GBE_DotaDeferredTaskConsumeResult Steam_Game_Coordinator::GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed()
{
    const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_dota_normal_signout_finalize_slot, "normal_signout_finalize_after_25");
    GBE_pending_dota_normal_signout_finalize_after_25 = false;
    GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
    return result;
}

void Steam_Game_Coordinator::GBE_ClearPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed()
{
    GBE_pending_dota_normal_signout_finalize_after_25 = false;
    GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
    GBE_pending_dota_normal_signout_finalize_slot = {};
}

void Steam_Game_Coordinator::GBE_SetPendingResetAfterCacheUnsubscribed(uint64 lobby_id)
{
    GBE_pending_reset_after_cache_unsubscribed = true;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
    GBE_pending_reset_after_cache_unsubscribed_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
}

void Steam_Game_Coordinator::GBE_ClearPendingResetAfterCacheUnsubscribed(uint64 retained_lobby_id)
{
    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = retained_lobby_id;
    GBE_pending_reset_after_cache_unsubscribed_slot = {};
}

Steam_Game_Coordinator::GBE_DotaDeferredTaskConsumeResult Steam_Game_Coordinator::GBE_ConsumePendingResetAfterCacheUnsubscribed()
{
    const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_reset_after_cache_unsubscribed_slot, "reset_after_25");
    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
    return result;
}

Steam_Game_Coordinator::GBE_DotaDeferredTaskConsumeResult Steam_Game_Coordinator::GBE_ConsumeDotaDeferredTask(
    GBE_DotaDeferredTaskSlot &slot,
    const char *task_name)
{
    if (!slot.pending)
        return {};

    GBE_DotaDeferredTaskConsumeResult result;
    result.lobby_id = slot.lobby_id;
    result.generation = slot.generation;
    result.status = slot.lobby_id == GBE_local_lobby.lobby_id && slot.generation == GBE_CurrentDotaLobbyGeneration()
        ? GBE_DotaDeferredTaskStatus::Current
        : GBE_DotaDeferredTaskStatus::Stale;
    slot = {};

    if (result.status == GBE_DotaDeferredTaskStatus::Stale) {
        GBE_ReconnectLogEvent({
            "reconnect.stale_generation",
            gbe::dota_diagnostic::Reason::StaleGeneration,
            gbe::dota_diagnostic::Source::DelayedTask,
            result.lobby_id,
            result.generation,
            0u,
            {},
            std::to_string(GBE_CurrentDotaLobbyGeneration()),
        });
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Rejected stale deferred task reason=stale_generation task=%s queued_lobby_id=%llu queued_generation=%llu current_lobby_id=%llu current_generation=%llu",
            task_name ? task_name : "unknown",
            static_cast<unsigned long long>(result.lobby_id),
            static_cast<unsigned long long>(result.generation),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_CurrentDotaLobbyGeneration()));
    }
    return result;
}

void Steam_Game_Coordinator::push_incoming_now(uint32 msg_type, const std::string &message, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state)
{
    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = 0.0;
    new_item.sequence = ++pending_message_sequence;
    new_item.apply_lobby_state = apply_lobby_state;
    new_item.lobby_state = lobby_state;
    new_item.lobby_game_state = lobby_game_state;
    new_item.lobby_id = GBE_local_lobby.lobby_id;
    new_item.generation = GBE_CurrentDotaLobbyGeneration();
    new_item.validate_lobby_generation = gc_profile == GC_PROFILE_DOTA2 && apply_lobby_state;
    if (!GBE_IsQueuedLobbyMessageCurrent(new_item, "enqueue_immediate"))
        return;
    GBE_ApplyQueuedLobbyState(new_item);
    incoming_messages.push(new_item);

    GCMessageAvailable_t data{};
    data.m_nMessageSize = static_cast<uint32>(message.size());
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "queued msg=%u this=%p size=%u queue_size=%zu pending_size=%zu apply_state=%u lobby_state=%u game_state=%u and posted GCMessageAvailable_t",
        GBE_GC_MaskedEMsg(msg_type),
        static_cast<void *>(this),
        static_cast<uint32>(message.size()),
        incoming_messages.size(),
        pending_messages.size(),
        apply_lobby_state ? 1u : 0u,
        lobby_state,
        lobby_game_state
    );
}

std::string Steam_Game_Coordinator::build_msg_header(JobID_t target_job, JobID_t source_job)
{
    std::string message;
    GCMsgHdrEx_t hdr{};
    hdr.m_nHdrVersion = 1;
    hdr.m_JobIDTarget = target_job;
    hdr.m_JobIDSource = source_job;
    ser_var<GCMsgHdrEx_t>(message, hdr);
    return message.substr(sizeof(GCMsgHdr_t));
}

GCMsgHdrEx_t Steam_Game_Coordinator::parse_msg_header(const char *&p)
{
    size_t write_offset = sizeof(GCMsgHdr_t);
    size_t hdr_size = sizeof(GCMsgHdrEx_t) - write_offset;
    GCMsgHdrEx_t hdr{};
    memcpy(reinterpret_cast<char *>(&hdr) + write_offset, p, hdr_size);
    p += hdr_size;
    return hdr;
}

std::string Steam_Game_Coordinator::build_protomsg_header(uint32 msg_type, JobID_t target_job, JobID_t source_job)
{
    std::string message;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = msg_type;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(settings->get_local_steam_id().ConvertToUint64());
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(settings->get_local_game_id().AppID());
    protohdr.set_job_id_source(source_job);
    protohdr.set_job_id_target(target_job);
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    return message;
}

template <class T>
std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, T, bool> Steam_Game_Coordinator::parse_protomsg(const void *input, uint32 input_size)
{
    T protomsg;

    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(input, input_size, context))
        return { context.hdr, context.protohdr, protomsg, false };

    if (!protomsg.ParseFromArray(context.body, static_cast<int>(context.body_size)))
        return { context.hdr, context.protohdr, protomsg, false };

    return { context.hdr, context.protohdr, protomsg, true };
}

// Explicit instantiations of parse_protomsg<T> for types used by split TUs
// (gbe_dota_inventory_coordinator.cpp). The template definition lives here;
// these force the compiler to emit the symbols so the linker can resolve
// cross-TU references.
template std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, gamecoordinator::tf2::CMsgAdjustItemEquippedState, bool> Steam_Game_Coordinator::parse_protomsg<gamecoordinator::tf2::CMsgAdjustItemEquippedState>(const void *, uint32);
template std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, gamecoordinator::tf2::CMsgSetItemPositions, bool> Steam_Game_Coordinator::parse_protomsg<gamecoordinator::tf2::CMsgSetItemPositions>(const void *, uint32);

void Steam_Game_Coordinator::handle_motd_request(const void *input, uint32 input_size)
{
    if (is_server || input_size < 24)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint32 last_req_time = deser_var<uint32>(p);
    uint16 language = deser_var<uint16>(p);
    PRINT_DEBUG("%u %u", last_req_time, language);

    uint32 msg_type = EGCItemMsg::k_EMsgGCMOTDRequestResponse;
    std::string message = build_msg_header();
    uint16 num_entries = 0;
    ser_var<uint16>(message, num_entries);

    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::handle_respawn(const void *input, uint32 input_size)
{
    if (is_server || input_size < 19)
        return;

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Request_Respawn);
    gameserver_items_msg->set_is_gc(true);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    network->sendToAllGameservers(&msg, true);
}

void Steam_Game_Coordinator::callback_respawn_request(CSteamID steam_id, double delay)
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange;
    std::string message = build_msg_header();
    ser_var<uint64>(message, steam_id.ConvertToUint64());

    push_incoming(msg_type, message, delay);
}

void Steam_Game_Coordinator::steam_network_callback(void *object, Common_Message *msg)
{
    //PRINT_DEBUG_ENTRY();

    auto inst = (Steam_Game_Coordinator *)object;
    inst->network_callback(msg);
}

void Steam_Game_Coordinator::steam_run_every_runcb(void *object)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Game_Coordinator *steam_gamecoordinator = (Steam_Game_Coordinator *)object;
    steam_gamecoordinator->RunCallbacks();
}

Steam_Game_Coordinator::Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, gbe::dota_lobby_state::Store &shared_lobby_store, registry::View handler_registry, gbe::dota_lifecycle::Executor &lifecycle_executor, bool is_server)
{
    if (!handler_registry.entries || handler_registry.size == 0u)
        throw std::invalid_argument("Dota handler registry is required");

    this->settings = settings;
    this->network = network;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;
    this->shared_lobby_store = &shared_lobby_store;
    this->handler_registry = handler_registry;
    this->lifecycle_executor = &lifecycle_executor;
    this->is_server = is_server;

    this->network->setCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_FRIEND_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_STEAM_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->add(&Steam_Game_Coordinator::steam_run_every_runcb, this);

    parse_gc_config();

    if (gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog(
            "GC_INIT",
            "eagerly initializing Dota2 GC from constructor this=%p is_server=%u",
            static_cast<void *>(this),
            this->is_server ? 1u : 0u
        );
        initialize_gc();
    }

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "coordinator init this=%p is_server=%u shared_lobby=%p shared_valid=%u active=%u lobby_id=%llu match_id=%llu state=%u game_state=%u",
        static_cast<void *>(this),
        this->is_server ? 1u : 0u,
        static_cast<void *>(&GBE_SharedLobbyStore()),
        shared_lobby.valid ? 1u : 0u,
        shared_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(shared_lobby.lobby_id),
        static_cast<unsigned long long>(shared_lobby.match_id),
        shared_lobby.state,
        shared_lobby.game_state
    );
}

gbe::dota_lobby_state::Store &Steam_Game_Coordinator::GBE_SharedLobbyStore() const
{
    return *shared_lobby_store;
}

Steam_Game_Coordinator::~Steam_Game_Coordinator()
{
    this->network->rmCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_FRIEND_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_STEAM_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->remove(&Steam_Game_Coordinator::steam_run_every_runcb, this);
}

void Steam_Game_Coordinator::initialize_gc()
{
    if (!gc_enabled() || gc_initialized)
        return;

    gc_initialized = true;

    if (gc_profile == GC_PROFILE_DOTA2) {
        GBE_RestoreSharedDotaLobbyState("initialize_gc");
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot("initialize_gc");
        GBE_GC_DebugLog("GC_INIT", "initialized Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
        return;
    }

    if (is_server) {
        callback_server_welcome();
    } else {
        callback_client_welcome();

        // Load user's items.
        const auto &items = load_items_from_file();
        callback_items_received(settings->get_local_steam_id(), items);
    }

    // Wait a bit until after the game has received welcome message from us before posting anything else.
    // This avoids a race condition that can cause the game to receive inventory before parsing item schema.
    // For old versions of TF2, we can't do that because they instead have a different bug where receiving
    // the inventory late causes erroneous "new items" notifications.
    if (gc_version >= 20110414) {
        delay_init = true;
    }
}

void Steam_Game_Coordinator::GBE_ClearDotaLobbyRuntimeState()
{
    const uint64 generation = GBE_local_lobby.generation;
    GBE_local_lobby = GBE_LocalLobby{};
    const auto clear_result = GBE_SharedLobbyStore().compare_clear(generation);
    if (clear_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration) {
        GBE_GC_DebugLog(
            "DOTA_LOBBY_STORE",
            "skip stale runtime clear generation=%llu",
            static_cast<unsigned long long>(generation));
    }
    GBE_ClearLastDotaLaunchStatePushedGameState();
    GBE_ClearDotaHostShowcaseEquipPushed();
    GBE_ClearDotaHostLocalWearablesRefreshed();
}

void Steam_Game_Coordinator::GBE_ClearSettingsLobbyForDotaSignout()
{
    if (settings && settings->get_lobby().ConvertToUint64() != 0)
        settings->set_lobby(k_steamIDNil);
}

void Steam_Game_Coordinator::clear_dota_runtime_state(bool preserve_reconnect_context)
{
    GBE_ClearDotaLobbyRuntimeState();
    if (!preserve_reconnect_context)
        GBE_ClearRecentDotaReconnectContext();
    GBE_ClearDotaPrivateLobbySnapshotReplayed();
    GBE_ClearPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();
}

void Steam_Game_Coordinator::shutdown_gc()
{
    if (!gc_initialized)
        return;

    GBE_GC_DebugLog(
        "GC_INIT",
        "shutdown GC this=%p is_server=%u profile=%u pending_size=%zu incoming_size=%zu active=%u lobby_id=%llu state=%u game_state=%u",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        static_cast<uint32>(gc_profile),
        pending_messages.size(),
        incoming_messages.size(),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state
    );

    items_loaded = false;
    items.clear();
    all_user_items.clear();
    pending_items_requests.clear();
    pending_messages.clear();
    while (incoming_messages.size())
        incoming_messages.pop();

    welcome_received = false;
    delay_init = false;
    GBE_ClearDotaLoginSyncSent();
    GBE_ClearDotaHostShowcaseEquipPushed();
    GBE_ClearDotaHostLocalWearablesRefreshed();
    GBE_ClearDotaPrivateLobbySnapshotReplayed();
    GBE_ClearLastDotaLaunchStatePushedGameState();
    GBE_last_lobby_poll_time = {};
    if (gc_profile == GC_PROFILE_DOTA2) {
        const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
        GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
        clear_dota_runtime_state(false);
        GBE_ClearSettingsLobbyForDotaSignout();
        GBE_GC_DebugLog(
            "GC_INIT",
            "cleared Dota2 GC runtime on shutdown this=%p previous_lobby_id=%llu",
            static_cast<void *>(this),
            static_cast<unsigned long long>(previous_lobby_id)
        );
    }
    gc_initialized = false;
}

void Steam_Game_Coordinator::on_appid_changed(uint32 appid)
{
    if (!appid)
        return;

    const uint32 previous_profile = static_cast<uint32>(gc_profile);
    parse_gc_config();
    GBE_GC_DebugLog(
        "GC_CONFIG",
        "refreshed GC config after appid change this=%p is_server=%u appid=%u previous_profile=%u new_profile=%u initialized=%u",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        appid,
        previous_profile,
        static_cast<uint32>(gc_profile),
        gc_initialized ? 1u : 0u
    );

    if (gc_profile == GC_PROFILE_DOTA2)
        initialize_gc();
}

std::string Steam_Game_Coordinator::GBE_GetDotaJoinableCustomLobbiesHTTPJSON(uint64 requested_custom_game_id)
{
    nlohmann::json response = nlohmann::json::object();
    response["lobbies"] = nlohmann::json::array();

    std::vector<GBE_LocalLobby> lobbies = GBE_GetDotaGenericLobbySnapshots("http_joinable_custom_lobbies");
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull)
        lobbies.push_back(GBE_local_lobby);

    std::vector<std::uint64_t> seen_lobby_ids;
    for (const GBE_LocalLobby &lobby : lobbies) {
        if (!gbe::dota_custom_game::should_include_joinable_custom_lobby(
                lobby.active,
                lobby.lobby_id,
                lobby.custom_game.game_id,
                requested_custom_game_id,
                seen_lobby_ids))
            continue;

        const GBE_DotaJoinableCustomLobbyItemData item_data = gbe::dota_custom_game::compose_joinable_custom_lobby_item_data(
            lobby.custom_game,
            lobby.members.size(),
            lobby.owner_account_id,
            settings ? settings->get_local_steam_id().GetAccountID() : 0u,
            lobby.owner_name,
            settings ? std::string(settings->get_local_name()) : std::string(),
            lobby.room_name,
            lobby.game_start_time,
            static_cast<uint32>(std::time(nullptr)));
        const std::string display_name = GBE_DotaCustomGameDisplayName(settings, lobby.custom_game, item_data.room_name);

        nlohmann::json item = gbe::dota_custom_lobby_http::compose_joinable_custom_lobby_json_item(
            item_data,
            lobby.custom_game,
            display_name,
            lobby.lobby_id,
            lobby.custom_game.game_id,
            lobby.server_region,
            !lobby.pass_key.empty(),
            lobby.lan_host_ping_location,
            lobby.custom_game.timestamp,
            std::to_string(lobby.custom_game.crc),
            lobby.custom_game.penalties);
        response["lobbies"].push_back(std::move(item));
        seen_lobby_ids.push_back(lobby.lobby_id);
    }

    return response.dump();
}

bool Steam_Game_Coordinator::ResetGCMemory(
    const char *reason,
    bool leave_generic_lobby,
    bool clear_queued_messages,
    gbe::dota_lobby_generation::Boundary generation_boundary,
    bool generation_already_advanced)
{
    const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
    const uint64 previous_match_id = GBE_local_lobby.match_id;
    const uint64 previous_generic_lobby_id = GBE_local_lobby.generic_lobby_id;
    const size_t previous_pending_count = pending_messages.size();
    const size_t previous_incoming_count = incoming_messages.size();
    if (!generation_already_advanced &&
        GBE_AdvanceDotaLobbyGeneration(generation_boundary, reason) == GBE_DotaGenerationAdvanceResult::Exhausted)
        return false;
    const uint64 next_generation = GBE_CurrentDotaLobbyGeneration();

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    GBE_ClearDotaPracticeLobbyLaunchRichPresence();

    if (leave_generic_lobby && GBE_local_lobby.generic_lobby_id != 0)
        GBE_LeaveGenericLobby();

    if (clear_queued_messages) {
        pending_messages.clear();
        std::queue<GC_Message> empty_messages;
        incoming_messages.swap(empty_messages);
        pending_message_sequence = 0;
    }

    const gbe::dota_lobby_state::RuntimeResetDecision reset_decision =
        gbe::dota_lobby_state::compute_runtime_reset_decision(
            gbe::dota_diagnostic::reason_from_string(reason ? reason : ""));
    clear_dota_runtime_state(reset_decision.preserve_reconnect_context);
    GBE_local_lobby.generation = next_generation;

    std::size_t equipped_item_count = 0;
    std::size_t equip_state_count = 0;
    std::uint64_t equipped_snapshot_hash = 1469598103934665603ull;
    for (const Econ_Item &item : items) {
        if (item.equip_states.empty())
            continue;

        ++equipped_item_count;
        equip_state_count += item.equip_states.size();
        equipped_snapshot_hash ^= item.id;
        equipped_snapshot_hash *= 1099511628211ull;
        equipped_snapshot_hash ^= item.def;
        equipped_snapshot_hash *= 1099511628211ull;
        equipped_snapshot_hash ^= item.style;
        equipped_snapshot_hash *= 1099511628211ull;
        for (const auto &[class_id, slot_id] : item.equip_states) {
            equipped_snapshot_hash ^= (static_cast<std::uint64_t>(class_id) << 16u) | slot_id;
            equipped_snapshot_hash *= 1099511628211ull;
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_EQUIP_BASELINE",
        "reset generation=%llu previous_lobby_id=%llu total_items=%zu equipped_items=%zu equip_states=%zu snapshot=%016llx cache_version=%llu reason=%s",
        static_cast<unsigned long long>(next_generation),
        static_cast<unsigned long long>(previous_lobby_id),
        items.size(),
        equipped_item_count,
        equip_state_count,
        static_cast<unsigned long long>(equipped_snapshot_hash),
        static_cast<unsigned long long>(GBE_DotaRuntimeState().equip_cache_version),
        reason ? reason : "unknown"
    );

    GBE_ClearPendingResetAfterCacheUnsubscribed();
    if (previous_lobby_id != 0 && GBE_suppressed_dota_abandon_lobby_id != previous_lobby_id)
        GBE_ClearDotaAbandonedLobbySuppression(previous_lobby_id, reason ? reason : "reset_gc_memory");
    GBE_ClearPendingDotaAbandonFinalizeAfterOtherLeftChannel();
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "reset_gc_memory");
    GBE_ResetDotaPracticeLobbyLaunchRichPresenceToServerSetup();

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] ResetGCMemory reason=%s leave_generic=%d clear_queues=%d previous_lobby_id=%llu previous_match_id=%llu previous_generic_lobby_id=%llu pending_count=%zu incoming_count=%zu",
        reason ? reason : "unknown",
        leave_generic_lobby ? 1 : 0,
        clear_queued_messages ? 1 : 0,
        static_cast<unsigned long long>(previous_lobby_id),
        static_cast<unsigned long long>(previous_match_id),
        static_cast<unsigned long long>(previous_generic_lobby_id),
        previous_pending_count,
        previous_incoming_count
    );
    return true;
}

std::string Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerName() const
{
    if (!GBE_local_lobby.owner_name.empty())
        return GBE_local_lobby.owner_name;

    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (!shared_lobby.owner_name.empty())
        return shared_lobby.owner_name;

    return std::string(settings->get_local_name());
}

bool Steam_Game_Coordinator::handle_dota_client_message(uint32 unMsgType, const void *pubData, uint32 cubData)
{
    const uint32 masked_emsg = GBE_GC_MaskedEMsg(unMsgType);
    GBE_GC_DebugLog("GC_SEND_DOTA", "outer_emsg=%u len=%u", masked_emsg, cubData);

    GBE_RestoreSharedDotaLobbyState("handle_dota_client_message");

    if (masked_emsg == GBE_kEMsgGCServerHello) {
        GBE_RestoreSharedDotaLobbyState("server_hello");

        GBE_DotaServerHelloContext server_hello_context{};
        if (!GBE_ExtractDirectDotaServerHelloContext(unMsgType, pubData, cubData, server_hello_context)) {
            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored direct ServerHello payload because parsing failed");
            return false;
        }

        GBE_SetLastDotaServerHelloContext(server_hello_context);

        if (is_server && welcome_received) {
            GBE_GC_DebugLog(
                "GC_DOTA_SERVER_HELLO",
                "skipping ServerWelcome replay because server GC is already connected active_version=%u",
                server_hello_context.active_version
            );
            return true;
        }

        if (is_server) {
            std::queue<GC_Message> queued_messages = incoming_messages;
            while (!queued_messages.empty()) {
                if (GBE_GC_MaskedEMsg(queued_messages.front().msg_type) == EGCBaseClientMsg::k_EMsgGCServerWelcome) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "skipping ServerWelcome replay because one is already queued active_version=%u",
                        server_hello_context.active_version
                    );
                    return true;
                }
                queued_messages.pop();
            }
        }

        std::string welcome_message;
        const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
        const uint32 app_id = settings->get_local_game_id().AppID();
        if (!GBE_BuildDirectDotaServerWelcome(steam_id, app_id, server_hello_context, welcome_message)) {
            GBE_GC_DebugLog(
                "GC_SEND_DOTA",
                "failed to build ServerWelcome active_version=%u min_allowed=%u steamid=%llu",
                server_hello_context.active_version,
                server_hello_context.min_allowed_version,
                static_cast<unsigned long long>(steam_id)
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_SEND_DOTA",
            "replaying ServerWelcome active_version=%u min_allowed=%u target_job=%llu direct=1",
            server_hello_context.active_version,
            server_hello_context.min_allowed_version,
            static_cast<unsigned long long>(server_hello_context.has_source_job ? server_hello_context.source_job_id : 0ull)
        );

        push_incoming_now(EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask, welcome_message);
        if (is_server && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_SERVER_HELLO",
                "queued immediate ServerWelcome for active lobby this=%p lobby_id=%llu size=%zu",
                static_cast<void *>(this),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                welcome_message.size()
            );
        }

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
            if (is_server) {
                std::string runtime_cache_message;
                const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
                const uint32 owner_account_id = GBE_GetDotaLobbyOwnerAccountId();
                const bool launch_started = GBE_local_lobby.match_id != 0;
                const bool built_runtime_cache = GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(
                    GBE_local_lobby,
                    GBE_local_lobby.owner_name,
                    runtime_cache_message,
                    true);

                if (built_runtime_cache) {
                    GBE_RecordDotaLobbyCacheSubscriptionState(runtime_cache_message, "server_welcome_current_cache_subscribed");
                    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, runtime_cache_message);
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "queued synthetic CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_started=%u owner_steam_id=%llu owner_account_id=%u size=%zu",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        static_cast<unsigned long long>(GBE_local_lobby.match_id),
                        static_cast<unsigned long long>(GBE_local_lobby.server_id),
                        launch_started ? 1u : 0u,
                        static_cast<unsigned long long>(owner_steam_id),
                        owner_account_id,
                        runtime_cache_message.size()
                    );
                } else {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "failed building synthetic CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        static_cast<unsigned long long>(GBE_local_lobby.match_id),
                        static_cast<unsigned long long>(GBE_local_lobby.server_id)
                    );
                }

                GBE_GC_DebugLog(
                    "GC_DOTA_SERVER_HELLO",
                    "skipping synthetic direct 7034 after ServerWelcome to match official launch timing lobby_id=%llu state=%u game_state=%u team=%u slot=%u",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot
                );
            }
        }

        return true;
    }

    if (masked_emsg != GBE_kEMsgGCClientHello && masked_emsg != GBE_kEMsgClientToGC)
        return GBE_HandleDotaDirectPostLoginRequest(unMsgType, pubData, cubData);

    GBE_DotaHelloContext hello_context{};
    bool direct_message = false;
    if (masked_emsg == GBE_kEMsgGCClientHello) {
        direct_message = true;
        if (!GBE_ExtractDirectDotaHelloContext(unMsgType, pubData, cubData, hello_context)) {
            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored direct ClientHello payload because parsing failed");
            return false;
        }
    } else if (masked_emsg == GBE_kEMsgClientToGC) {
        if (!GBE_ExtractDotaHelloContext(pubData, cubData, hello_context)) {
            if (GBE_HandleDotaWrappedPostLoginRequest(pubData, cubData))
                return true;

            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored ClientToGC payload because it was not a valid Dota ClientHello or supported wrapped request");
            return false;
        }
    } else {
        GBE_GC_DebugLog("GC_SEND_DOTA", "ignored non-Dota-GC message emsg=%u", masked_emsg);
        return false;
    }

    std::string welcome_message;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();
    const uint32 app_id = settings->get_local_game_id().AppID();

    const bool built = direct_message
        ? GBE_BuildDirectDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message)
        : GBE_ComposeDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message);

    if (!built) {
        GBE_GC_DebugLog(
            "GC_SEND_DOTA",
            "failed to build ClientWelcome version=%u steamid=%llu accountid=%u",
            hello_context.version,
            static_cast<unsigned long long>(steam_id),
            account_id
        );
        return true;
    }

    GBE_GC_DebugLog(
        "GC_SEND_DOTA",
        "replaying ClientWelcome version=%u steamid=%llu accountid=%u target_job=%llu direct=%d",
        hello_context.version,
        static_cast<unsigned long long>(steam_id),
        account_id,
        static_cast<unsigned long long>(hello_context.has_source_job ? hello_context.source_job_id : 0ull),
        direct_message ? 1 : 0
    );

    push_incoming_now((direct_message ? GBE_kEMsgGCClientWelcome : GBE_kEMsgClientFromGC) | GBE_kProtoMask, welcome_message);

    std::string top_custom_games_message;
    size_t top_custom_games_count = 0;
    if (GBE_AdaptDotaTopCustomGamesListPayload(settings, top_custom_games_message, top_custom_games_count)) {
        GBE_PushDotaResponse(GBE_kDotaTopCustomGamesList, top_custom_games_message, false, nullptr, "top_custom_games_after_welcome");
        GBE_GC_DebugLog(
            "GC_DOTA_CUSTOM_GAMES",
            "queued top custom games list count=%zu direct=%d",
            top_custom_games_count,
            direct_message ? 1 : 0);
    }

    if (direct_message)
        GBE_PushDotaLoginSyncMessages();
    return true;
}

// sends a message to the Game Coordinator
EGCResults Steam_Game_Coordinator::SendMessage_( uint32 unMsgType, const void *pubData, uint32 cubData )
{
    PRINT_DEBUG("0x%08X %u len %u", unMsgType, (~protobuf_mask) & unMsgType, cubData);
    GBE_GC_DebugLog("GC_SEND", "outer_emsg=%u len=%u", GBE_GC_MaskedEMsg(unMsgType), cubData);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if ((unMsgType & protobuf_mask) != 0) {
        GBE_LogGCProtoBoundary(
            "GC_SEND_HEADER",
            "send",
            static_cast<void *>(this),
            is_server,
            GBE_GC_MaskedEMsg(unMsgType),
            pubData,
            cubData
        );
    }

    if (!gc_initialized && gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog("GC_SEND", "initializing GC lazily for Dota2 profile");
        initialize_gc();
    }

    if (!gc_initialized) {
        GBE_GC_DebugLog("GC_SEND", "gc not initialized, swallowing msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    if (gc_profile == GC_PROFILE_DOTA2 && handle_dota_client_message(unMsgType, pubData, cubData)) {
        GBE_GC_DebugLog("GC_SEND", "handled by Dota2 replay path msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    switch (unMsgType) {
        case EGCItemMsg::k_EMsgGCSetSingleItemPosition:
            PRINT_DEBUG("k_EMsgGCSetSingleItemPosition");
            handle_set_item_pos(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCDelete:
            PRINT_DEBUG("k_EMsgGCDelete");
            handle_delete_item(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCMOTDRequest:
            PRINT_DEBUG("k_EMsgGCMOTDRequest");
            handle_motd_request(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange:
            PRINT_DEBUG("k_EMsgGCRespawnPostLoadoutChange");
            handle_respawn(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemStyle:
            PRINT_DEBUG("k_EMsgGCSetItemStyle");
            handle_set_item_style(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCAdjustItemEquippedState | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCAdjustItemEquippedState");
            handle_adjust_equip_state(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemPositions | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCSetItemPositions");
            handle_set_multiple_item_pos(pubData, cubData);
            break;
        default:
            break;
    }

    return k_EGCResultOK;
}

// returns true if there is a message waiting from the game coordinator
bool Steam_Game_Coordinator::IsMessageAvailable( uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!gc_initialized || incoming_messages.empty()) {
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "IsMessageAvailable this=%p gc_initialized=%u queue_size=%zu returning=0",
            static_cast<void *>(this),
            gc_initialized ? 1u : 0u,
            incoming_messages.size()
        );
        *pcubMsgSize = 0;
        return false;
    }

    GC_Message &message = incoming_messages.front();
    *pcubMsgSize = static_cast<uint32>(message.msg_body.size());

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "IsMessageAvailable this=%p gc_initialized=%u queue_size=%zu front_emsg=%u size=%u returning=1",
        static_cast<void *>(this),
        gc_initialized ? 1u : 0u,
        incoming_messages.size(),
        GBE_GC_MaskedEMsg(message.msg_type),
        *pcubMsgSize
    );

    return true;
}

// fills the provided buffer with the first message in the queue and returns k_EGCResultOK or 
// returns k_EGCResultNoMessage if there is no message waiting. pcubMsgSize is filled with the message size.
// If the provided buffer is not large enough to fit the entire message, k_EGCResultBufferTooSmall is returned
// and the message remains at the head of the queue.
EGCResults Steam_Game_Coordinator::RetrieveMessage( uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    const uint32 queued_emsg = incoming_messages.empty() ? 0u : GBE_GC_MaskedEMsg(incoming_messages.front().msg_type);
    GBE_GC_DebugLog(
        "GC_RETRIEVE",
        "queued_emsg=%u this=%p queue_size=%zu pending_size=%zu cubDest=%u",
        queued_emsg,
        static_cast<void *>(this),
        incoming_messages.size(),
        pending_messages.size(),
        cubDest
    );

    if (!gc_initialized || incoming_messages.empty()) {
        *pcubMsgSize = 0;
        return k_EGCResultNoMessage;
    }

    GC_Message message = incoming_messages.front();

    uint32 outsize = static_cast<uint32>(message.msg_body.size());
    if (outsize > cubDest) {
        return k_EGCResultBufferTooSmall;
    }

    if (is_welcome_message(message)) {
        welcome_received = true;
        welcome_time = std::chrono::high_resolution_clock::now();
    }

    *punMsgType = message.msg_type;
    *pcubMsgSize = outsize;
    message.msg_body.copy(reinterpret_cast<char *>(pubDest), cubDest);

    const uint32 retrieved_emsg = GBE_GC_MaskedEMsg(*punMsgType);
    const bool retrieved_other_left_matches_abandon_channel =
        retrieved_emsg == GBE_kDotaOtherLeftChannel &&
        GBE_IsDotaOtherLeftChannelPayloadForChannel(message.msg_body, GBE_local_lobby.abandon_pre_postgame_chat_channel_id);
    const gbe::dota_lobby_state::TeardownRetrievalDecision teardown_decision =
        gbe::dota_lobby_state::compute_teardown_retrieval_decision(
            gc_profile == GC_PROFILE_DOTA2,
            GBE_HasPendingDotaAbandonFinalizeAfterOtherLeftChannel(),
            GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(),
            GBE_HasPendingResetAfterCacheUnsubscribed(),
            retrieved_emsg,
            retrieved_other_left_matches_abandon_channel);

    incoming_messages.pop();

    if ((message.msg_type & protobuf_mask) != 0) {
        GBE_LogGCProtoBoundary(
            "GC_RETRIEVE_HEADER",
            "recv",
            static_cast<void *>(this),
            is_server,
            GBE_GC_MaskedEMsg(message.msg_type),
            message.msg_body.data(),
            outsize
        );
    }

    if (!incoming_messages.empty()) {
        GCMessageAvailable_t data{};
        data.m_nMessageSize = static_cast<uint32>(incoming_messages.front().msg_body.size());
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "reposted GCMessageAvailable_t after retrieving msg=%u this=%p next_emsg=%u remaining_queue=%zu pending_size=%zu size=%u",
            GBE_GC_MaskedEMsg(*punMsgType),
            static_cast<void *>(this),
            GBE_GC_MaskedEMsg(incoming_messages.front().msg_type),
            incoming_messages.size(),
            pending_messages.size(),
            data.m_nMessageSize
        );
    }

    if (teardown_decision.finalize_abandon_after_7014) {
        const auto pending = GBE_ConsumePendingDotaAbandonFinalizeAfterOtherLeftChannel();
        if (pending.status != GBE_DotaDeferredTaskStatus::Current)
            return k_EGCResultOK;
        const uint64 finalize_lobby_id = pending.lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed pending 7014; finalizing abandon teardown LobbyID=%llu channel=%llu",
            static_cast<unsigned long long>(finalize_lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.abandon_pre_postgame_chat_channel_id)
        );
        GBE_FinalizeDotaAbandonAfterOtherLeftChannel(finalize_lobby_id, "7014_pre_postgame_retrieved");
    }

    if (teardown_decision.finalize_normal_signout_after_25) {
        const auto pending = GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();
        if (pending.status != GBE_DotaDeferredTaskStatus::Current)
            return k_EGCResultOK;
        const uint64 finalize_lobby_id = pending.lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed normal signout 25; finalizing postgame teardown LobbyID=%llu",
            static_cast<unsigned long long>(finalize_lobby_id)
        );
        GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(finalize_lobby_id, "7004_signout_after_25_retrieved");
    }

    if (teardown_decision.reset_after_cache_unsubscribed) {
        const auto pending = GBE_ConsumePendingResetAfterCacheUnsubscribed();
        if (pending.status != GBE_DotaDeferredTaskStatus::Current)
            return k_EGCResultOK;
        const uint64 pending_lobby_id = pending.lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed pending 25; applying deferred current-game reset for LobbyID=%llu",
            static_cast<unsigned long long>(pending_lobby_id)
        );
        ResetGCMemory("7035_disconnect_current_game_after_25", true, true);
    }

    GBE_GC_DebugLog(
        "GC_RETRIEVE",
        "returned_emsg=%u this=%p size=%u remaining_queue=%zu pending_size=%zu",
        GBE_GC_MaskedEMsg(*punMsgType),
        static_cast<void *>(this),
        outsize,
        incoming_messages.size(),
        pending_messages.size()
    );

    return k_EGCResultOK;
}

void Steam_Game_Coordinator::RunCallbacks()
{
    if (!gc_initialized)
        return;

    if (delay_init && welcome_received && check_timedout(welcome_time, 0.2)) {
        delay_init = false;
    }

    // [Dota 2 LAN] Throttle lobby state polling during an active LAN match to
    // reduce mutex contention with the engine's networking thread.  Lobby state
    // changes (player disconnect, game_state advance, PostGame transition) are
    // second-granularity events; 500ms latency is imperceptible.
    {
        bool skip_lobby_poll = false;
        if (!is_server && gc_profile == GC_PROFILE_DOTA2 &&
            GBE_local_lobby.active && GBE_local_lobby.lan &&
            GBE_local_lobby.state == 2u && GBE_local_lobby.match_id != 0ull) {
            if (!check_timedout(GBE_last_lobby_poll_time, 0.5))
                skip_lobby_poll = true;
            else
                GBE_last_lobby_poll_time = std::chrono::high_resolution_clock::now();
        }
        if (!skip_lobby_poll) {
            if (!GBE_MaybeHandleDotaPracticeLobbyKicked("run_callbacks_generic_lobby_members_changed"))
                GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("run_callbacks_generic_lobby_members_changed");
        }
    }

    // [Dota 2 LAN] Once per match, when the client detects an active lobby with
    // a server, broadcast equipped items to the gameserver so it can build
    // CacheSubscribed for this player.  This handles items equipped in the armory
    // before the game starts (when no gameserver existed yet to receive them).
    if (!is_server && gc_profile == GC_PROFILE_DOTA2 &&
        GBE_local_lobby.active && GBE_local_lobby.server_id != 0 && GBE_local_lobby.state >= 1u) {
        if (GBE_last_broadcast_dota_match_id != GBE_local_lobby.match_id && GBE_local_lobby.match_id != 0) {
            GBE_last_broadcast_dota_match_id = GBE_local_lobby.match_id;

            std::vector<const Econ_Item *> equipped_items;
            for (const auto &item : items) {
                if (!item.equip_states.empty())
                    equipped_items.push_back(&item);
            }

            if (!equipped_items.empty()) {
                auto response_msg = new GameServer_Items_Messages::InventoryResponse();
                response_msg->set_steam_api_call(0);  // unsolicited push

                for (const Econ_Item *ep : equipped_items) {
                    auto new_item = response_msg->add_items();
                    new_item->set_id(ep->id);
                    new_item->set_def(ep->def);
                    new_item->set_level(ep->level);
                    new_item->set_quality(static_cast<int32>(ep->quality));
                    new_item->set_inv_pos(ep->inv_pos);
                    new_item->set_quantity(ep->quantity);
                    new_item->set_flags(ep->flags);
                    new_item->set_origin(ep->origin);
                    new_item->set_original_id(ep->original_id);
                    new_item->set_in_use(ep->in_use);
                    new_item->set_style(ep->style);

                    for (const auto &[class_id, slot_id] : ep->equip_states) {
                        auto new_state = new_item->add_equip_states();
                        new_state->set_class_id(class_id);
                        new_state->set_slot_id(slot_id);
                    }

                    for (const Econ_Item_Attribute &attr : ep->attributes) {
                        auto new_attr = new_item->add_attributes();
                        new_attr->set_def(attr.def);
                        new_attr->set_value(attr.value);
                        new_attr->set_value_bytes(attr.value_bytes);
                    }
                }

                auto gameserver_items_msg = new GameServer_Items_Messages();
                gameserver_items_msg->set_type(GameServer_Items_Messages::Response_Inventory);
                gameserver_items_msg->set_is_gc(true);
                gameserver_items_msg->set_allocated_inventory_response(response_msg);

                Common_Message msg{};
                msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
                msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
                network->sendToAllGameservers(&msg, true);

                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "broadcast initial equipped items to gameservers: steam64=%llu equipped_items=%zu match_id=%llu",
                    static_cast<unsigned long long>(settings->get_local_steam_id().ConvertToUint64()),
                    equipped_items.size(),
                    static_cast<unsigned long long>(GBE_local_lobby.match_id)
                );
            }
        }
    }

    auto due_time = [](const GC_Message &message) {
        return message.created + std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(message.post_in));
    };

    std::stable_sort(
        pending_messages.begin(),
        pending_messages.end(),
        [&due_time](const GC_Message &lhs, const GC_Message &rhs) {
            const auto lhs_due = due_time(lhs);
            const auto rhs_due = due_time(rhs);
            if (lhs_due != rhs_due)
                return lhs_due < rhs_due;
            return lhs.sequence < rhs.sequence;
        }
    );

    for (auto it = pending_messages.begin(); it != pending_messages.end();) {
        if (delay_init && !is_welcome_message(*it)) {
            it++;
            continue;
        }

        if (check_timedout(it->created, it->post_in)) {
            if (!GBE_IsQueuedLobbyMessageCurrent(*it, "delay_expired")) {
                it = pending_messages.erase(it);
                continue;
            }
            GBE_ApplyQueuedLobbyState(*it);
            if (!GBE_IsQueuedLobbyMessageCurrent(*it, "before_incoming_queue")) {
                it = pending_messages.erase(it);
                continue;
            }
            incoming_messages.push(*it);

            GCMessageAvailable_t data{};
            data.m_nMessageSize = static_cast<uint32>(it->msg_body.size());
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

            GBE_GC_DebugLog(
                "GC_CALLBACK",
                "moved pending msg=%u this=%p to incoming queue_size=%zu pending_size_before_erase=%zu size=%u and posted GCMessageAvailable_t",
                GBE_GC_MaskedEMsg(it->msg_type),
                static_cast<void *>(this),
                incoming_messages.size(),
                pending_messages.size(),
                data.m_nMessageSize
            );

            it = pending_messages.erase(it);
        } else {
            it++;
        }
    }

    for (auto it = pending_items_requests.begin(); it != pending_items_requests.end();) {
        if (check_timedout(it->created, 7.0)) {
            if (!it->is_gc) {
                server_items()->callback_items_received(it->steam_id, items.size(), it->steam_api_call, false);
            }

            PRINT_DEBUG("player inventory request timeout %llu", it->steam_id);
            it = pending_items_requests.erase(it);
        } else {
            it++;
        }
    }
}

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
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
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
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

constexpr int GC_MIN_VERSION = 20091217;
bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;
bool GBE_recent_dota_reconnect_context_valid = false;
GBE_DotaReconnectContext GBE_recent_dota_reconnect_context{};
bool GBE_pending_dota_normal_signout_finalize_after_25 = false;
uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
GBE_DotaLootListData GBE_vpk_loot_data;

// --- Dota reconnect shared state ---
std::atomic<bool> GBE_dota_reconnect_eligible{true};

bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out)
        return false;

    if (GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        (GBE_shared_dota_lobby_state.state >= 2u || GBE_shared_dota_lobby_state.game_state >= 2u) &&
        !GBE_shared_dota_lobby_state.connect.empty() &&
        GBE_shared_dota_lobby_state.server_id != 0) {
        out->server_id = GBE_shared_dota_lobby_state.server_id;
        out->lobby_state = GBE_shared_dota_lobby_state.state;
        out->game_state = GBE_shared_dota_lobby_state.game_state;
        out->custom_game_id = GBE_shared_dota_lobby_state.custom_game.game_id;
        const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_shared_dota_lobby_state.connect);
        std::strncpy(out->connect, endpoint.c_str(), sizeof(out->connect) - 1);
        out->connect[sizeof(out->connect) - 1] = '\0';
        out->owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
        return true;
    }

    if (GBE_recent_dota_reconnect_context_valid &&
        GBE_DotaReconnectContextIsStarted(GBE_recent_dota_reconnect_context) &&
        GBE_recent_dota_reconnect_context.connect[0] != '\0' &&
        GBE_recent_dota_reconnect_context.server_id != 0) {
        *out = GBE_recent_dota_reconnect_context;
        return true;
    }

    return false;
}

bool GBE_IsDotaArcadeLobbyActive()
{
    return GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        GBE_shared_dota_lobby_state.custom_game.game_id != 0ull;
}

bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64_t local_steam_id, GBE_DotaReconnectContext *out)
{
    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_game_coordinator)
        return false;

    return steam_client->steam_game_coordinator->GBE_TryRecoverDotaReconnectContextFromGenericLobbies(local_steam_id, out);
}
// --- End Dota reconnect shared state ---

const char *GBE_DescribeDotaLaunchPhase(uint32 phase)
{
    switch (phase) {
        case GBE_kDotaLaunchPhaseRequested:
            return "requested";
        case GBE_kDotaLaunchPhaseSetupSynced:
            return "serversetup_synced";
        case GBE_kDotaLaunchPhaseRunQueued:
            return "run_queued";
        case GBE_kDotaLaunchPhaseLoaded:
            return "loaded";
        default:
            return "none";
    }
}

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);

extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34 };
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64 = { 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64 = { 0x91, 0x1D, 0xDF, 0x05, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt = "29822498642855090";
extern const uint32 GBE_kSteamTicketAuthComplete = 5429u;
extern const char *GBE_kDotaAbandonPersonaStateInitHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112a50209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131c1040000000000000000c9040000000000000000f80400800500880500980501";
extern const char *GBE_kDotaOfficial032PracticeLobby26Hex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04109a808080081a80041a00008000000000120508dd0f120012e90108d40f12e30108d6f9ac9f95a6fc34180120022a273138322e34322e3232342e31333a3237303135203139322e3136382e342e3136383a3237303135310eac2b7cdec1400159f5b621080100100160016800700082010531313131318a010240008a01024000a80100b0010ae00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f80200800300980300a80300c80301f2030708f54412020800880400d80400900500b805f7e6cbcf06c00500e80503f00500f80500880600b80600c00637f00600880700c2070d09f5b621080100100118003801c80700f807008008d5e6cbcf06121208de0f120d0a090a075376656e6d61781000120708df0f12020a0012cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed864110001800200138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838121908f5ed864110e8071800200138016000d00100d80100e00100190439f15331f16900320b080310d6f9ac9f95a6fc34";
// Removed: GBE_kDota7388Profile20Template (had owned=0, caused "Unavailable")
// Removed: GBE_kDota7388Profile37Template (had owned=1 but used hardcoded account_id)
// All 7387->7388 event queries now use gbe::gc_message::build_dota_7388_minimal_response_payload.

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


GBE_DotaServerHelloContext GBE_last_dota_server_hello_context;

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

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    const char *log_scope = scope ? scope : "GC";
    if (std::strcmp(log_scope, "GC_SEND") == 0 ||
        std::strcmp(log_scope, "GC_SEND_DOTA") == 0 ||
        std::strcmp(log_scope, "GC_CONFIG") == 0 ||
        std::strcmp(log_scope, "GC_INIT") == 0 ||
        std::strcmp(log_scope, "CREATE_INTERFACE") == 0 ||
        std::strcmp(log_scope, "NETSOCK_CTOR") == 0) {
        return;
    }

    FILE *file = std::fopen(GBE_kGcDebugLogPath, "a");
    if (!file)
        return;

    std::fprintf(file, "[%s] ", log_scope);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);

    std::fprintf(file, "\n");
    std::fclose(file);
}

bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count)
{
    replacement_count = 0;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    const std::string body(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size);
    std::string rewritten_body;
    if (!gbe::proto_wire::rewrite_varint_bytes_recursive(
            body,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_body,
            replacement_count))
        return false;

    if (replacement_count == 0)
        return true;

    message.resize(proto_context.body_offset);
    message.append(rewritten_body);
    return true;
}

bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    std::string encoded_account_raw;
    gbe::proto_wire::append_varuint(encoded_account_raw, account_id);

    std::string rewritten_message;
    size_t replacement_count = 0;
    const bool full_message_parse_ok = gbe::proto_wire::rewrite_varint_bytes_recursive(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_message,
            replacement_count);
    if (full_message_parse_ok && replacement_count > 0) {
        message.swap(rewritten_message);
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=full_message",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            replacement_count,
            1u
        );
        return true;
    }

    size_t direct_body_replacements = 0;
    if (GBE_RewriteAccountIdVarintInDirectProtoBody(message, account_id, direct_body_replacements) && direct_body_replacements > 0) {
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=direct_body",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            direct_body_replacements,
            1u
        );
        return true;
    }

    if (!full_message_parse_ok) {
        GBE_GC_DebugLog(
            log_scope,
            "account_id semantic rewrite skipped full parse req=%u resp=%u note=%s account_id=%u encoded_size=%zu donor_size=%zu",
            request_emsg,
            response_emsg,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size()
        );
    }

    GBE_GC_DebugLog(
        log_scope,
        "no semantic account_id varint replacements req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu target_field=%u",
        request_emsg,
        response_emsg,
        body_size,
        context_note ? context_note : "",
        account_id,
        encoded_account_raw.size(),
        GBE_kOldDotaAccountIdVarint.size(),
        1u
    );
    return true;
}

bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope)
{
    const std::vector<uint8> old_account_id_fixed32 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdFixed32.data(), GBE_kOldDotaAccountIdFixed32.size());
    size_t match_count = 0;
    if (!gbe::proto_wire::patch_fixed32_template_value(message, old_account_id_fixed32, account_id, match_count)) {
        GBE_GC_DebugLog(log_scope, "failed replacing account_id fixed32 bytes account_id=%u matches=%zu", account_id, match_count);
        return false;
    }

    return true;
}

using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;

std::string GBE_DotaCustomGameDisplayName(class Settings *settings, const GBE_DotaCustomGameDetails &custom_game, const std::string &fallback)
{
    if (settings && custom_game.game_id != 0ull && settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id))) {
        Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
        std::string display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "display_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", "");
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
    }

    return gbe::dota_custom_game::custom_game_display_name_from_details(custom_game, fallback);
}

bool GBE_PatchDotaTemplateIdentifiers(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    if (replace_account) {
        if (!GBE_TryPatchDotaAccountIdVarint(message, account_id, "GC_DOTA_PATCH", request_emsg, response_emsg, body_size, context_note))
            return false;
        if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH"))
            return false;
    }

    if (replace_steam_id) {
        const std::vector<uint8> old_steam_id_varint = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size());
        size_t steam_id_match_count = 0;
        bool steam_id_size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(message, old_steam_id_varint, steam_id, steam_id_match_count, steam_id_size_ok)) {
            if (!steam_id_size_ok) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped due to size mismatch req=%u resp=%u note=%s steam_id=%llu encoded_expected=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
                return false;
            }

            if (steam_id_match_count == 0) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped; donor does not expose expected varint req=%u resp=%u note=%s steam_id=%llu expected_size=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
            } else {
                return false;
            }
        }
    }

    return true;
}

void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return;

    GBE_GC_DebugLog(
        tag,
        "%s owner_type=%u owner_id=%llu objects=%d version_present=%u version=%llu service_id_present=%u service_id=%u service_list_count=%d sync_version_present=%u sync_version=%llu",
        label ? label : "dota_cache_subscribed_summary",
        protomsg.has_owner_soid() ? protomsg.owner_soid().type() : 0u,
        static_cast<unsigned long long>(protomsg.has_owner_soid() ? protomsg.owner_soid().id() : 0ull),
        protomsg.objects_size(),
        protomsg.has_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_version() ? protomsg.version() : 0ull),
        protomsg.has_service_id() ? 1u : 0u,
        protomsg.has_service_id() ? protomsg.service_id() : 0u,
        protomsg.service_list_size(),
        protomsg.has_sync_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_sync_version() ? protomsg.sync_version() : 0ull)
    );

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        GBE_GC_DebugLog(
            tag,
            "%s object[%d] type=%d object_data_count=%d",
            label ? label : "dota_cache_subscribed_summary",
            object_index,
            object.type_id(),
            object.object_data_size()
        );

        for (int data_index = 0; data_index < object.object_data_size(); ++data_index) {
            const std::string &object_data = object.object_data(data_index);
            GBE_GC_DebugLog(
                tag,
                "%s object[%d] data[%d] size=%zu",
                label ? label : "dota_cache_subscribed_summary",
                object_index,
                data_index,
                object_data.size()
            );

            if (object.type_id() == 2004) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint64 lobby_id = 0;
                uint32 lobby_state = 0;
                std::string connect;
                uint64 server_id = 0;
                uint32 game_state = 0;
                uint64 match_id = 0;
                uint32 game_start_time = 0;
                const uint32 team_details_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 17u);
                std::string owner_state;
                const bool has_connect = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 5u, connect);
                const bool has_owner_state = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 120u, owner_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 1u, lobby_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 4u, lobby_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 6u, server_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 22u, game_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 30u, match_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 87u, game_start_time);
                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2004 lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu game_start_time=%u connect=%s team_details=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    static_cast<unsigned long long>(lobby_id),
                    lobby_state,
                    game_state,
                    static_cast<unsigned long long>(match_id),
                    static_cast<unsigned long long>(server_id),
                    game_start_time,
                    has_connect ? connect.c_str() : "",
                    team_details_count
                );
                if (has_owner_state) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2004 owner_state{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_lobby_member_state_summary(owner_state).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2014) {
                const uint32 member_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 1u);
                std::string first_member;
                if (!gbe::proto_wire::read_bytes_field(reinterpret_cast<const uint8 *>(object_data.data()), object_data.size(), 1u, first_member)) {
                    first_member.clear();
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2014 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2014 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_static_lobby_member_summary(first_member).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2016) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint32 member_count = 0;
                std::string first_member;
                size_t offset = 0;
                while (offset < object_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(object_bytes, object_size, offset, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 2u) {
                        ++member_count;
                        if (first_member.empty())
                            first_member.assign(object_data.data() + field.value_offset, field.value_size);
                    }
                    offset = field_end;
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2016 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2016 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_server_static_lobby_member_summary(first_member).c_str()
                    );
                }
            }
        }
    }
}

bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(
    const char *wrapped_template_hex,
    const char *stage_note,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 lobby_state,
    uint32 lobby_game_state,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (!wrapped_template_hex)
        return false;

    std::string wrapped_message;
    if (!gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message))
        return false;

    std::string inner_payload;
    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaPracticeLobbyDetailsUpdate, inner_payload))
        return false;

    if (!GBE_PatchDotaPracticeLobbyLaunchTemplate(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            server_id,
            match_id,
            game_start_time,
            connect,
            true,
            true,
            true,
            stage_note))
        return false;

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            true,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            std::vector<GBE_DotaLobbyMemberState>(),
            rewrite_2015,
            extra_startup_account_id,
            pass_key,
            custom_game))
        return false;

    message.swap(inner_payload);
    return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
}

bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    bool patch_server_id,
    std::string &message)
{
    if (!gbe::proto_wire::decode_hex_string(template_hex, message))
        return false;

    const std::vector<std::string> old_lobby_id_texts = {
        GBE_kOldDotaPracticeLobbyLobbyIdText,
        GBE_kOldDotaPracticeLobbyLobbyIdTextAlt,
    };
    gbe::proto_wire::DotaPracticeLobbyPeripheralTemplatePatchResult patch_result{};
    if (!gbe::proto_wire::patch_dota_practice_lobby_peripheral_template(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPersonaSteamIdFixed64.data(), GBE_kOldDotaPersonaSteamIdFixed64.size()),
            steam_id,
            patch_server_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
            server_id,
            old_lobby_id_texts,
            lobby_id,
            patch_result))
        return false;

    return true;
}

bool GBE_PrepareDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(template_hex, steam_id, lobby_id, 0u, false, message);
}

void GBE_LogDotaResponsePacket(
    const char *reason,
    uint32 inner_emsg,
    bool wrapped,
    const std::string &inner_payload,
    const std::string &outbound_payload,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state)
{
    const std::string body_prefix = gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(inner_payload.data()), inner_payload.size(), 32);
    const std::string packet_prefix = wrapped
        ? gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(outbound_payload.data()), outbound_payload.size(), 32)
        : "-";
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent response reason=%s path=%s inner_emsg=%u wrapped=%u payload_size=%zu lobby_id=%llu state=%u game_state=%u body_prefix=%s packet_prefix=%s",
        reason ? reason : "unknown",
        wrapped ? "wrapped" : "direct",
        inner_emsg,
        wrapped ? 1u : 0u,
        outbound_payload.size(),
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        body_prefix.c_str(),
        packet_prefix.c_str());
}

bool GBE_AdaptDotaJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 generic_lobby_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    uint64 owner_steam_id,
    const std::string &owner_name,
    uint32 channel_type,
    std::string &message)
{
    std::vector<GBE_DotaChatMemberState> resolved_remote_names;
    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == steam_id)
            continue;
        std::string generic_member_name;
        std::string friend_member_name;
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking && generic_lobby_id != 0ull) {
            CSteamID generic_lobby((uint64)generic_lobby_id);
            CSteamID member_id((uint64)channel_member.steam_id);
            if (generic_lobby.IsLobby() && member_id.IsValid()) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, member_id, GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    generic_member_name = generic_name;
            }
        }

        if (steam_client && steam_client->steam_friends) {
            const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)channel_member.steam_id));
            if (friend_name && friend_name[0] != '\0')
                friend_member_name = friend_name;
        }

        resolved_remote_names.push_back({
            channel_member.steam_id,
            gbe::dota_lobby_flow::resolve_chat_member_display_name(
                channel_member.steam_id,
                steam_id,
                player_name,
                owner_steam_id,
                owner_name,
                generic_member_name,
                friend_member_name,
                std::string()) });
    }

    const std::vector<GBE_DotaChatMemberState> chat_member_states = gbe::dota_lobby_flow::compose_join_chat_channel_members(
        steam_id,
        player_name,
        channel_members,
        owner_steam_id,
        owner_name,
        resolved_remote_names);
    std::vector<gbe::gc_message::DotaChatMember> chat_members;
    chat_members.reserve(chat_member_states.size());
    for (const GBE_DotaChatMemberState &chat_member : chat_member_states) {
        chat_members.push_back({ chat_member.steam_id, chat_member.name });
    }

    return gbe::gc_message::build_dota_join_chat_channel_response_payload(channel_id, channel_name, chat_members, channel_type, message);
}

bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason)
{
    if (!target_gc || !player_steam_id.BIndividualAccount())
        return false;

    std::vector<const Econ_Item *> equipped_items;
    for (const Econ_Item &item : source_items) {
        if (!item.equip_states.empty())
            equipped_items.push_back(&item);
    }

    if (equipped_items.empty())
        return false;

    const uint64 player_steam64 = player_steam_id.ConvertToUint64();

    if (unsubscribe_first) {
        std::string unsub_message;
        gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(1u, player_steam64, unsub_message);
        target_gc->push_incoming_message(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, unsub_message);
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "pushed player CacheUnsubscribed to target GC: steam64=%llu reason=%s message_size=%zu",
            static_cast<unsigned long long>(player_steam64),
            reason ? reason : "unknown",
            unsub_message.size()
        );
    }

    std::string owner_soid;
    gbe::proto_wire::append_varint_field(owner_soid, 1u, 1u);
    gbe::proto_wire::append_varint_field(owner_soid, 2u, player_steam64);

    std::string subscribed_type;
    gbe::proto_wire::append_varint_field(subscribed_type, 1u, 1u);
    for (const Econ_Item *ep : equipped_items) {
        std::string serialized = target_gc->serialize_item_to_gcprotobuf(*ep, player_steam_id);
        gbe::proto_wire::append_bytes_field(subscribed_type, 2u, serialized);
    }

    std::string cache_body;
    gbe::proto_wire::append_bytes_field(cache_body, 2u, subscribed_type);
    gbe::proto_wire::append_fixed64_field(cache_body, 3u, 1ull);
    gbe::proto_wire::append_bytes_field(cache_body, 4u, owner_soid);

    std::string cache_message;
    gbe::gc_message::build_dota_zero_header_payload(GBE_kDotaCacheSubscribed, cache_body, cache_message);
    target_gc->push_incoming_message(GBE_kDotaCacheSubscribed | GBE_kProtoMask, cache_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "pushed player item CacheSubscribed to target GC: steam64=%llu equipped_items=%zu reason=%s message_size=%zu unsub_first=%u",
        static_cast<unsigned long long>(player_steam64),
        equipped_items.size(),
        reason ? reason : "unknown",
        cache_message.size(),
        unsubscribe_first ? 1u : 0u
    );
    return true;
}

bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)extra_startup_account_id;

    return GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex,
        "practice lobby launch official cache template",
        false,
        true,
        true,
        false,
        account_id,
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)has_broadcast_channel;
    (void)broadcast_channel_id;
    (void)broadcast_country_code;
    (void)broadcast_description;
    (void)broadcast_language_code;

    const uint32 startup_account_id = gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(account_id, lobby_state, lobby_game_state);

    if (gbe::proto_wire::is_dota_practice_lobby_prelaunch_state(server_id, match_id, game_start_time, connect)) {
        gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;

        GBE_ComposeDotaPracticeLobbySOObjects(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            custom_game,
            lobby_objects);

        return gbe::gc_message::build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2016, lobby_objects.object_2015, lobby_objects.object_2004, message)
            && GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
    }

    if (GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            true,
            message,
            custom_game)) {
        return true;
    }

    return GBE_ReplayDotaPracticeLobbyOfficial26Payload(
        GBE_kDotaOfficial032PracticeLobby26Hex,
        "current direct 26 details update",
        account_id,
        steam_id,
        lobby_id,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        lobby_state,
        lobby_game_state,
        startup_account_id != 0u,
        startup_account_id,
        message,
        custom_game);
}

bool GBE_PrepareDotaDirectReplayMessage(
    const uint8 *template_bytes,
    size_t template_size,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    bool has_target_job,
    uint64 target_job,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note,
    std::string &message)
{
    message.assign(reinterpret_cast<const char *>(template_bytes), template_size);

    if (!GBE_PatchDotaTemplateIdentifiers(message, account_id, steam_id, replace_account, replace_steam_id, request_emsg, response_emsg, body_size, context_note))
        return false;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgProtoBufHeader protohdr = proto_context.protohdr;

    if (has_target_job) {
        protohdr.set_job_id_target(target_job);
    } else {
        protohdr.clear_job_id_target();
    }
    protohdr.clear_job_id_source();

    const char *body_ptr = reinterpret_cast<const char *>(proto_context.body);
    const size_t serialized_body_size = proto_context.body_size;

    std::string updated;
    ProtoBufMsgHeader_t hdr = proto_context.hdr;
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    ser_var<ProtoBufMsgHeader_t>(updated, hdr);
    protohdr.AppendToString(&updated);
    updated.append(body_ptr, serialized_body_size);
    message.swap(updated);
    return true;
}

bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message)
{
    if (!context.valid) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "invalid server hello context");
        return false;
    }

    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(context.has_client_steam_id ? context.client_steam_id : steam_id);
    if (context.has_client_session_id) {
        protohdr.set_client_session_id(context.client_session_id);
    } else {
        protohdr.set_client_session_id(1);
    }
    protohdr.set_source_app_id(context.has_source_app_id ? context.source_app_id : app_id);
    if (context.has_source_job)
        protohdr.set_job_id_target(context.source_job_id);
    if (context.has_gc_msg_src)
        protohdr.set_gc_msg_src(static_cast<GCProtoBufMsgSrc>(context.gc_msg_src));
    if (context.has_gc_dir_index_source)
        protohdr.set_gc_dir_index_source(context.gc_dir_index_source);

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(context.min_allowed_version);
    protomsg.set_active_version(context.active_version);
    protomsg.AppendToString(&message);

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "built direct ServerWelcome active_version=%u min_allowed=%u target_job=%llu client_steam_id=%llu client_session_id=%d source_app_id=%u gc_msg_src=%u gc_dir_index_source=%u total=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.has_source_job ? context.source_job_id : 0ull),
        static_cast<unsigned long long>(context.has_client_steam_id ? context.client_steam_id : steam_id),
        context.has_client_session_id ? context.client_session_id : 1,
        context.has_source_app_id ? context.source_app_id : app_id,
        context.has_gc_msg_src ? context.gc_msg_src : 0u,
        context.has_gc_dir_index_source ? context.gc_dir_index_source : 0u,
        message.size()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const gbe::dota_gc_router::DotaGcRequestContext &context)
{
    if (!context.valid)
        return false;

    const std::string *outer_session_field_raw = gbe::dota_gc_router::outer_session_field_or_null(context);
    const char *path = context.wrapped ? "wrapped" : "direct";
    auto log_lobby_request = [path, &context]() {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received %s %u has_job=%u request_job=%llu session_raw_size=%zu body_size=%zu body_prefix=%s",
            path,
            context.inner_emsg,
            context.has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            context.body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.body.data()), context.body.size(), 48).c_str()
        );
    };

    switch (context.inner_emsg) {
        case GBE_kDotaJoinChatChannel:
            log_lobby_request();
            return GBE_HandleDotaJoinChatChannelRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyCreate:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyCreateRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaLobbyList:
            log_lobby_request();
            return GBE_HandleDotaLobbyListRequest(context.has_request_job, context.request_job_id, context.wrapped, outer_session_field_raw);
        case GBE_kDotaCustomLobbyListRequest:
            log_lobby_request();
            return GBE_HandleDotaCustomLobbyListRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaFriendPracticeLobbyListRequest:
            log_lobby_request();
            return GBE_HandleDotaFriendPracticeLobbyListRequest(context.wrapped, outer_session_field_raw);
        case GBE_kGCInviteToLobby:
            log_lobby_request();
            return GBE_HandleDotaInviteToLobbyRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kGCLobbyInviteResponse:
            log_lobby_request();
            return GBE_HandleDotaLobbyInviteResponseRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyJoin:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyJoinRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyLeave:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyLeaveRequest(context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyLaunch:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyLaunchRequest(context.body, context.wrapped, outer_session_field_raw, context.has_request_job, context.request_job_id);
        case GBE_kDotaPracticeLobbySetDetails:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbySetDetailsRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbySetTeamSlot:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbySetTeamSlotRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyKick:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyKickRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyJoinBroadcastChannel:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaLobbyUpdateBroadcastChannelInfo:
            log_lobby_request();
            return GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyCloseBroadcastChannel:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(context.body, context.wrapped, outer_session_field_raw);
        default:
            return false;
    }
}

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

void Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState(const GC_Message &message)
{
    if (!message.apply_lobby_state)
        return;

    if (gc_profile == GC_PROFILE_DOTA2 && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)) {
        if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.active && GBE_shared_dota_lobby_state.lobby_id != 0) {
            GBE_RestoreSharedDotaLobbyState("queued_state_preapply");
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "queued lobby state has no full local/shared lobby context this=%p shared_valid=%u shared_active=%u shared_lobby_id=%llu msg=%u state=%u game_state=%u",
                static_cast<void *>(this),
                GBE_shared_dota_lobby_state.valid ? 1u : 0u,
                GBE_shared_dota_lobby_state.active ? 1u : 0u,
                static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
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

void Steam_Game_Coordinator::callback_respawn_request(CSteamID steam_id)
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange;
    std::string message = build_msg_header();
    ser_var<uint64>(message, steam_id.ConvertToUint64());

    push_incoming(msg_type, message);
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

Steam_Game_Coordinator::Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, bool is_server)
{
    this->settings = settings;
    this->network = network;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;
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

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "coordinator init this=%p is_server=%u shared_lobby=%p shared_valid=%u active=%u lobby_id=%llu match_id=%llu state=%u game_state=%u",
        static_cast<void *>(this),
        this->is_server ? 1u : 0u,
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        GBE_shared_dota_lobby_state.valid ? 1u : 0u,
        GBE_shared_dota_lobby_state.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
        static_cast<unsigned long long>(GBE_shared_dota_lobby_state.match_id),
        GBE_shared_dota_lobby_state.state,
        GBE_shared_dota_lobby_state.game_state
    );
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
    GBE_dota_login_sync_sent = false;
    GBE_dota_host_showcase_equip_pushed = false;
    GBE_dota_private_lobby_snapshot_replayed = false;
    GBE_last_dota_launch_state_pushed_game_state = 0;
    GBE_last_lobby_poll_time = {};
    if (gc_profile == GC_PROFILE_DOTA2) {
        const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
        GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
        GBE_recent_dota_reconnect_context_valid = false;
        GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        if (settings && settings->get_lobby().ConvertToUint64() != 0)
            settings->set_lobby(k_steamIDNil);
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


bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_runtime_fields,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || account_id == 0 || lobby_id == 0)
        return false;

    if (!GBE_PrepareDotaDirectReplayMessage(
            GBE_kDotaPracticeLobbyCacheSubscribedTemplate,
            sizeof(GBE_kDotaPracticeLobbyCacheSubscribedTemplate),
            account_id,
            steam_id,
            false,
            false,
            false,
            0,
            7038u,
            24u,
            0,
            "practice lobby cache template",
            message)) {
        return false;
    }

    if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id))
        return false;

    return GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game);
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    return GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}








std::string Steam_Game_Coordinator::GBE_GetDotaJoinableCustomLobbiesHTTPJSON(uint64 requested_custom_game_id)
{
    nlohmann::json response = nlohmann::json::object();
    response["lobbies"] = nlohmann::json::array();

    std::vector<GBE_LocalLobby> lobbies = GBE_GetDotaGenericLobbySnapshots("http_joinable_custom_lobbies");
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull)
        lobbies.push_back(GBE_local_lobby);

    std::vector<uint64> seen_lobby_ids;
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


void Steam_Game_Coordinator::ResetGCMemory(const char *reason, bool leave_generic_lobby, bool clear_queued_messages)
{
    const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
    const uint64 previous_match_id = GBE_local_lobby.match_id;
    const uint64 previous_generic_lobby_id = GBE_local_lobby.generic_lobby_id;
    const size_t previous_pending_count = pending_messages.size();
    const size_t previous_incoming_count = incoming_messages.size();

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

    GBE_local_lobby = GBE_LocalLobby{};
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
    if (!reason || std::strcmp(reason, "7035_disconnect_current_game_after_25") != 0) {
        GBE_recent_dota_reconnect_context_valid = false;
        GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
    }
    GBE_dota_private_lobby_snapshot_replayed = false;
    GBE_last_dota_launch_state_pushed_game_state = 0;
    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
    GBE_pending_dota_normal_signout_finalize_after_25 = false;
    GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
    if (previous_lobby_id != 0 && GBE_suppressed_dota_abandon_lobby_id != previous_lobby_id)
        GBE_ClearDotaAbandonedLobbySuppression(previous_lobby_id, reason ? reason : "reset_gc_memory");
    GBE_pending_dota_abandon_finalize_after_7014 = false;
    GBE_pending_dota_abandon_finalize_lobby_id = 0;
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "reset_gc_memory");
    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);

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
}



std::string Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerName() const
{
    if (!GBE_local_lobby.owner_name.empty())
        return GBE_local_lobby.owner_name;

    if (!GBE_shared_dota_lobby_state.owner_name.empty())
        return GBE_shared_dota_lobby_state.owner_name;

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

        GBE_last_dota_server_hello_context = server_hello_context;

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

    GC_Message &message = incoming_messages.front();

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

    const bool should_finalize_dota_abandon_after_7014 =
        gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_dota_abandon_finalize_after_7014 &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaOtherLeftChannel &&
        GBE_IsDotaOtherLeftChannelPayloadForChannel(message.msg_body, GBE_local_lobby.abandon_pre_postgame_chat_channel_id);
    const bool should_finalize_dota_normal_signout_after_25 =
        gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_dota_normal_signout_finalize_after_25 &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaCacheUnsubscribed;

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

    if (should_finalize_dota_abandon_after_7014) {
        const uint64 finalize_lobby_id = GBE_pending_dota_abandon_finalize_lobby_id;
        GBE_pending_dota_abandon_finalize_after_7014 = false;
        GBE_pending_dota_abandon_finalize_lobby_id = 0;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed pending 7014; finalizing abandon teardown LobbyID=%llu channel=%llu",
            static_cast<unsigned long long>(finalize_lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.abandon_pre_postgame_chat_channel_id)
        );
        GBE_FinalizeDotaAbandonAfterOtherLeftChannel(finalize_lobby_id, "7014_pre_postgame_retrieved");
    }

    if (should_finalize_dota_normal_signout_after_25) {
        const uint64 finalize_lobby_id = GBE_pending_dota_normal_signout_finalize_lobby_id;
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed normal signout 25; finalizing postgame teardown LobbyID=%llu",
            static_cast<unsigned long long>(finalize_lobby_id)
        );
        GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(finalize_lobby_id, "7004_signout_after_25_retrieved");
    }

    if (gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_reset_after_cache_unsubscribed &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaCacheUnsubscribed) {
        const uint64 pending_lobby_id = GBE_pending_reset_after_cache_unsubscribed_lobby_id;
        GBE_pending_reset_after_cache_unsubscribed = false;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
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
        static uint64 s_last_broadcast_match_id = 0;
        if (s_last_broadcast_match_id != GBE_local_lobby.match_id && GBE_local_lobby.match_id != 0) {
            s_last_broadcast_match_id = GBE_local_lobby.match_id;

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
            GBE_ApplyQueuedLobbyState(*it);
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

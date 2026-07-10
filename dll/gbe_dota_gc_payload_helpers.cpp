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
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_reconnect_context.h"
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
#include <mutex>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

extern std::recursive_mutex global_mutex;

// --- Phase 3.2.2 migration: pure wire helpers + static byte arrays moved to
// dll/gbe_dota_payload_wire_helpers.cpp.
// Moved constants: GBE_kDotaClientWelcomeTemplate, GBE_kOldDotaVersionVarint,
// GBE_kOldDotaPracticeLobbyLobbyIdVarint, GBE_kOldDotaPracticeLobbyGameStartTimeVarint,
// GBE_kOldDotaPracticeLobbyConnect.


const char * const GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba041098808080081ae9061800008000000000\n"
    "12bd0108d40f12b70108d6f9ac9f95a6fc3418012001310eac2b7cdec1400159f5b62108010010016001680070008201\n"
    "0531313131318a010240008a01024000a80100e00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f8\n"
    "0200800300980300a80300c80301f2030708f54412020800d80400900500b805f7e6cbcf06c00500e80503f00500f805\n"
    "00880600b80600c00637f00600880700c2071009f5b621080100100118003801800101c80700f807008008d5e6cbcf06\n"
    "120508dd0f1200121208de0f120d0a090a075376656e6d61781000129b0308df0f1295030a0012900308a545128a0308\n"
    "f5ed864112bc010a05080210c00c0a05080510c8010a04080a10640a04080b10640a05080c10de020a04082210640a04\n"
    "082310320a05082510ee050a05082810c00c0a04082a10320a05082c10db030a05082f10ac020a05083510de020a0408\n"
    "4510640a04084b10640a05085110d8040a05085310db030a05085410bd150a0508551096010a04086810320a0508c302\n"
    "10010a0508900310010a0508910310010a0508920310010a05089a0310060a0508cd0310030a0508ce0310080a0508cf\n"
    "0310161a060886011086011a0608d10f10d20f1a06088927108a271a0608914e10924e1a0608f95510fa551a0608e15d\n"
    "10e25d1a0808d1890210d289021a0808b9910210ba91021a080889a102108aa1021a0808c1b80210c2b8021a080891c8\n"
    "021092c8021a0808e1d70210e2d7021a080899ef02109aef021a0808899e03108a9e031a0808899b04108a9b041a0808\n"
    "f9c90410fac9041a0808e9f80410eaf8041a0808b9880510ba88051a0808a1900510a290051a0808899805108a98051a\n"
    "0808c1ac0610c2ac0612cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000\n"
    "e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038\n"
    "006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed86411000180020\n"
    "0138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838\n"
    "121908f5ed864110e8071800200138016000d00100d80100e0010019e0a6ef5331f16900220b080310d6f9ac9f95a6fc\n"
    "34";
extern const uint8 GBE_kDotaPracticeLobbyCacheSubscribedTemplate[] = {
    0x18, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x12, 0xC2, 0x01, 0x08, 0xD4, 0x0F, 0x12, 0xBC,
    0x01, 0x08, 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34, 0x18, 0x01, 0x20, 0x00, 0x59, 0xF5,
    0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01, 0x60, 0x01, 0x68, 0x00, 0x70, 0x01, 0x82, 0x01, 0x05,
    0x30, 0x30, 0x31, 0x32, 0x35, 0xA8, 0x01, 0x05, 0xE0, 0x01, 0x00, 0xF8, 0x01, 0x01, 0xA0, 0x02,
    0x04, 0xD0, 0x02, 0x00, 0xD8, 0x02, 0x00, 0xE0, 0x02, 0x00, 0xF0, 0x02, 0x00, 0xF8, 0x02, 0x00,
    0x80, 0x03, 0x00, 0x98, 0x03, 0x00, 0xA8, 0x03, 0x00, 0xC8, 0x03, 0x00, 0xF2, 0x03, 0x07, 0x08,
    0xF5, 0x44, 0x12, 0x02, 0x08, 0x00, 0xD8, 0x04, 0x00, 0x90, 0x05, 0x00, 0xC0, 0x05, 0x00, 0xE8,
    0x05, 0x04, 0xF0, 0x05, 0x8A, 0xB6, 0xFB, 0x8B, 0x0C, 0xF8, 0x05, 0xCD, 0xFB, 0x92, 0xDA, 0x0A,
    0x88, 0x06, 0x00, 0xF0, 0x06, 0x00, 0x88, 0x07, 0x00, 0xC2, 0x07, 0x10, 0x09, 0xF5, 0xB6, 0x21,
    0x08, 0x01, 0x00, 0x10, 0x01, 0x18, 0x00, 0x38, 0x01, 0x80, 0x01, 0x01, 0xC8, 0x07, 0x00, 0xE0,
    0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07,
    0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00,
    0xE0, 0x07, 0x00, 0xF8, 0x07, 0x00, 0x80, 0x08, 0xF2, 0xA7, 0xFE, 0xCE, 0x06, 0x12, 0x05, 0x08,
    0xDD, 0x0F, 0x12, 0x00, 0x12, 0x12, 0x08, 0xDE, 0x0F, 0x12, 0x0D, 0x0A, 0x09, 0x0A, 0x07, 0x53,
    0x76, 0x65, 0x6E, 0x6D, 0x61, 0x78, 0x10, 0x00, 0x12, 0x07, 0x08, 0xDF, 0x0F, 0x12, 0x02, 0x0A,
    0x00, 0x12, 0x2F, 0x08, 0xE0, 0x0F, 0x12, 0x2A, 0x0A, 0x23, 0x09, 0xF5, 0xB6, 0x21, 0x08, 0x01,
    0x00, 0x10, 0x01, 0x48, 0x00, 0x58, 0x00, 0x60, 0xE1, 0xAC, 0x8B, 0x84, 0xD0, 0x85, 0x40, 0x68,
    0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x15, 0x00, 0x00,
    0x00, 0x00, 0x19, 0x74, 0x1F, 0xDE, 0x53, 0xB9, 0xDE, 0x69, 0x00, 0x22, 0x0B, 0x08, 0x03, 0x10,
    0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34,
};

// --- Phase 3.2.2 migration: GBE_PatchDotaWelcomeAccountObjects moved to
// dll/gbe_dota_payload_wire_helpers.cpp (stays file-scope static there because
// it is only called from GBE_PrepareDotaWelcomeBody within that TU).

void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size)
{
    if (!scope || !direction || !data || size < sizeof(ProtoBufMsgHeader_t) || !gbe::gc_message::should_trace_dota_proto_boundary(emsg))
        return;

    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(data, size, context)) {
        GBE_GC_DebugLog(
            scope,
            "%s this=%p is_server=%u emsg=%u size=%u parse=0",
            direction,
            self,
            is_server ? 1u : 0u,
            emsg,
            size
        );
        return;
    }

    GBE_GC_DebugLog(
        scope,
        "%s this=%p is_server=%u emsg=%u size=%u ext=%u body=%zu has_job_src=%u job_src=%llu has_job_tgt=%u job_tgt=%llu client_steam_id=%llu session=%d app_id=%u",
        direction,
        self,
        is_server ? 1u : 0u,
        emsg,
        size,
        context.hdr.m_cubProtoBufExtHdr,
        context.body_size,
        context.protohdr.has_job_id_source() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_source() ? context.protohdr.job_id_source() : 0ull),
        context.protohdr.has_job_id_target() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_target() ? context.protohdr.job_id_target() : 0ull),
        static_cast<unsigned long long>(context.protohdr.has_client_steam_id() ? context.protohdr.client_steam_id() : 0ull),
        context.protohdr.has_client_session_id() ? context.protohdr.client_session_id() : 0,
        context.protohdr.has_source_app_id() ? context.protohdr.source_app_id() : 0u
    );
}

// --- Phase 3.2.2 migration: GBE_PatchDotaLobbyTemplateIdentifiers,
// GBE_PatchDotaLobbyTemplateIdentifiersIfPresent, GBE_ForceDotaLobbyCacheOwnerSOID,
// GBE_ForceDotaLobbyUpdateOwnerSOID moved to dll/gbe_dota_payload_wire_helpers.cpp.
// Declarations live in gbe_dota_gc_internal.h.


// --- Phase 3.2.2 migration: GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState
// and GBE_PatchDotaPracticeLobbyLaunchTemplate moved to
// dll/gbe_dota_payload_wire_helpers.cpp.


bool GBE_AdaptDotaTopCustomGamesListPayload(class Settings *settings, std::string &message, size_t &game_count)
{
    std::vector<std::uint64_t> mod_ids;
    if (settings) {
        for (PublishedFileId_t mod_id : settings->modSet()) {
            if (mod_id == 0ull || mod_id == k_PublishedFileIdInvalid)
                continue;
            mod_ids.push_back(static_cast<std::uint64_t>(mod_id));
        }
    }
    return gbe::gc_message::build_dota_top_custom_games_list_payload(mod_ids, message, game_count);
}
bool GBE_IsDotaOtherLeftChannelPayloadForChannel(const std::string &message, uint64 channel_id)
{
    if (message.size() < 8u)
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(message.data());
    uint32 inner_emsg = 0;
    uint32 header_length = 0;
    std::memcpy(&inner_emsg, bytes, sizeof(inner_emsg));
    std::memcpy(&header_length, bytes + sizeof(inner_emsg), sizeof(header_length));
    if (GBE_GC_MaskedEMsg(inner_emsg) != GBE_kDotaOtherLeftChannel)
        return false;

    const size_t body_offset = 8u + header_length;
    if (body_offset + 9u > message.size())
        return false;

    const uint8 *body = bytes + body_offset;
    if (body[0] != 0x09u)
        return false;

    uint64 payload_channel_id = 0;
    std::memcpy(&payload_channel_id, body + 1u, sizeof(payload_channel_id));
    return payload_channel_id == channel_id;
}
// --- Phase 3.2.4 migration: pure lobby payload helpers moved to
// dll/gbe_dota_payload_lobby_helpers.cpp. Moved functions:
// GBE_ComposeDotaPracticeLobbySOObjects, GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload,
// GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload,
// GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate,
// GBE_ExtractDotaHelloContext, GBE_ExtractDirectDotaHelloContext,
// GBE_ExtractDirectDotaServerHelloContext, GBE_BuildDirectDotaClientWelcome,
// GBE_ComposeDotaClientWelcome, GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate,
// GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload, GBE_BuildDirectDotaServerWelcome,
// GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl,
// GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl.
// Declarations remain in gbe_dota_gc_internal.h.


// --- Externalized from steam_game_coordinator.cpp (helper cleanup phase) ---
// These file-scope helpers were previously defined in the GC main TU.
// They have no dependency on Steam_Game_Coordinator class internals.

bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out)
        return false;

    const GBE_DotaReconnectSharedStateSnapshot shared = GBE_GetSharedDotaReconnectStateSnapshot();
    GBE_DotaReconnectContext recent_context{};
    const bool has_recent_context = GBE_GetRecentDotaReconnectContext(&recent_context);
    const auto selection = gbe::dota_reconnect::select_context({
        gbe::dota_reconnect::source_from_shared_snapshot(shared),
        gbe::dota_reconnect::source_from_context(
            recent_context,
            has_recent_context,
            gbe::dota_reconnect::SourceKind::Recent),
    });
    if (!selection.selected)
        return false;
    *out = selection.context;
    return true;
}

bool GBE_HasSharedDotaLobbyState()
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return GBE_shared_dota_lobby_state.valid;
}

uint64 GBE_GetSharedDotaLobbyIdOrZero()
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return GBE_shared_dota_lobby_state.valid ? GBE_shared_dota_lobby_state.lobby_id : 0ull;
}

uint64 GBE_GetSharedDotaGenericLobbyIdOrZero()
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return GBE_shared_dota_lobby_state.valid ? GBE_shared_dota_lobby_state.generic_lobby_id : 0ull;
}

GBE_DotaSharedLobbyScalarSnapshot GBE_GetSharedDotaLobbyScalarSnapshot()
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    GBE_DotaSharedLobbyScalarSnapshot snapshot{};
    snapshot.valid = GBE_shared_dota_lobby_state.valid;
    snapshot.active = GBE_shared_dota_lobby_state.active;
    snapshot.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
    snapshot.generic_lobby_id = GBE_shared_dota_lobby_state.generic_lobby_id;
    snapshot.lobby_state = GBE_shared_dota_lobby_state.state;
    snapshot.game_state = GBE_shared_dota_lobby_state.game_state;
    return snapshot;
}

GBE_DotaReconnectSharedStateSnapshot GBE_GetSharedDotaReconnectStateSnapshot()
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    GBE_DotaReconnectSharedStateSnapshot snapshot{};
    snapshot.valid = GBE_shared_dota_lobby_state.valid;
    snapshot.active = GBE_shared_dota_lobby_state.active;
    snapshot.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
    snapshot.lobby_state = GBE_shared_dota_lobby_state.state;
    snapshot.game_state = GBE_shared_dota_lobby_state.game_state;
    snapshot.server_id = GBE_shared_dota_lobby_state.server_id;
    snapshot.has_connect = !GBE_shared_dota_lobby_state.connect.empty();
    snapshot.custom_game_id = GBE_shared_dota_lobby_state.custom_game.game_id;
    snapshot.owner_connected = GBE_shared_dota_lobby_state.owner_connected;
    snapshot.launch_phase = GBE_shared_dota_lobby_state.launch_phase;
    snapshot.owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
    const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_shared_dota_lobby_state.connect);
    std::strncpy(snapshot.connect, endpoint.c_str(), sizeof(snapshot.connect) - 1);
    snapshot.connect[sizeof(snapshot.connect) - 1] = '\0';
    return snapshot;
}

bool GBE_IsSharedDotaArcadeLobbyActive()
{
    return GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        GBE_shared_dota_lobby_state.custom_game.game_id != 0ull;
}

bool GBE_IsDotaArcadeLobbyActive()
{
    return GBE_IsSharedDotaArcadeLobbyActive();
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

const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };
const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34 };
const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64 = { 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01 };
const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64 = { 0x91, 0x1D, 0xDF, 0x05, 0x01, 0x00, 0x10, 0x01 };
const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
const char * const GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
const char * const GBE_kOldDotaPracticeLobbyLobbyIdTextAlt = "29822498642855090";
const uint32 GBE_kSteamTicketAuthComplete = 5429u;
const char * const GBE_kDotaAbandonPersonaStateInitHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112a50209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131c1040000000000000000c9040000000000000000f80400800500880500980501";
const char * const GBE_kDotaOfficial032PracticeLobby26Hex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04109a808080081a80041a00008000000000120508dd0f120012e90108d40f12e30108d6f9ac9f95a6fc34180120022a273138322e34322e3232342e31333a3237303135203139322e3136382e342e3136383a3237303135310eac2b7cdec1400159f5b621080100100160016800700082010531313131318a010240008a01024000a80100b0010ae00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f80200800300980300a80300c80301f2030708f54412020800880400d80400900500b805f7e6cbcf06c00500e80503f00500f80500880600b80600c00637f00600880700c2070d09f5b621080100100118003801c80700f807008008d5e6cbcf06121208de0f120d0a090a075376656e6d61781000120708df0f12020a0012cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed864110001800200138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838121908f5ed864110e8071800200138016000d00100d80100e00100190439f15331f16900320b080310d6f9ac9f95a6fc34";
// Removed: GBE_kDota7388Profile20Template (had owned=0, caused "Unavailable")
// Removed: GBE_kDota7388Profile37Template (had owned=1 but used hardcoded account_id)
// All 7387->7388 event queries now use gbe::gc_message::build_dota_7388_minimal_response_payload.

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
// --- Phase 3.2.2 migration: GBE_TryPatchDotaAccountIdVarint moved to
// dll/gbe_dota_payload_wire_helpers.cpp.


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

// --- Phase 3.2.4 migration: GBE_ReplayDotaPracticeLobbyOfficial26Payload
// moved to dll/gbe_dota_payload_lobby_helpers.cpp.
// Declaration remains in gbe_dota_gc_internal.h.


// --- Phase 3.2.2 migration: GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage
// and GBE_PrepareDotaPersonaStatePeripheralMessage moved to
// dll/gbe_dota_payload_wire_helpers.cpp (pure wire helpers TU).

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

// =====================================================================
// Phase 2.13 pure item helpers (GBE_ParseDotaEquipOps,
// GBE_ApplyDotaUnlockStyleBitmask, GBE_SerializeEconItemToGcprotobuf,
// GBE_BuildSOSingleObjectFromItem) moved to
// dll/gbe_dota_payload_item_helpers.cpp in Phase 3.2.3. Declarations remain
// in dll/gbe_dota_gc_internal.h. See that TU for the responsibility boundary
// and the resolution of the TODO(phase-3.2) stub duplication.
// =====================================================================

/* Split helpers from gbe_dota_lobby_handlers.cpp (stage D.10.1). */

#include "gbe_dota_lobby_handler_helpers.h"

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_internal.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_wire.h"
#include "gbe_gc_message_utils.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;

void GBE_ApplyDotaCustomGameDetailsRequest(const GBE_DotaPracticeLobbyDetailsRequest &request, GBE_DotaCustomGameDetails &custom_game)
{
    if (request.has_custom_game_mode)
        custom_game.mode = request.custom_game_mode;
    if (request.has_custom_map_name)
        custom_game.map_name = request.custom_map_name;
    if (request.has_custom_difficulty)
        custom_game.difficulty = request.custom_difficulty;
    if (request.has_custom_game_id)
        custom_game.game_id = request.custom_game_id;
    if (request.has_custom_min_players)
        custom_game.min_players = request.custom_min_players;
    if (request.has_custom_max_players)
        custom_game.max_players = request.custom_max_players;
    if (request.has_custom_game_crc)
        custom_game.crc = request.custom_game_crc;
    if (request.has_custom_game_timestamp)
        custom_game.timestamp = request.custom_game_timestamp;
    if (request.has_custom_game_penalties)
        custom_game.penalties = request.custom_game_penalties;
}


void GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(class Settings *settings, GBE_DotaCustomGameDetails &custom_game)
{
    if (!settings || custom_game.game_id == 0ull || !settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id)))
        return;

    Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
    const std::string addon_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
    const std::string metadata_map_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", addon_name);

    if (gbe::proto_wire::dota_is_readable_custom_game_name(addon_name) && (custom_game.mode.empty() || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.mode)))
        custom_game.mode = addon_name;
    if (gbe::proto_wire::dota_is_readable_custom_game_name(metadata_map_name) && (custom_game.map_name.empty() || custom_game.map_name == "dota" || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.map_name)))
        custom_game.map_name = metadata_map_name;
}


uint64 GBE_GenerateDotaLobbyId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 64; ++attempt) {
        const uint64 candidate = (generator() & 0x00FFFFFFFFFFFFFFull) | 0x0002000000000000ull;
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaLobbyIdVarint.size(), encoded))
            return candidate;
    }

    return 29799760111995806ull;
}


uint64 GBE_GenerateDotaMatchId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 128; ++attempt) {
        const uint64 candidate = 0x100000000ull + (generator() & 0x00000003FFFFFFFFull);
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded))
            return candidate;
    }

    return 8781757536ull;
}


bool GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
    uint64 lobby_id,
    uint64 inviter_steam_id,
    uint64 invitee_steam_id,
    const std::string &inviter_name,
    const std::vector<std::pair<uint64, std::string>> &members,
    std::string &message)
{
    uint64 cache_version = 0;
    uint64 invite_gid = 0;
    std::vector<std::pair<std::uint64_t, std::string>> parsed_members;
    parsed_members.reserve(members.size());
    for (const auto &member : members)
        parsed_members.push_back({ member.first, member.second });
    std::uint64_t parsed_invite_gid = 0;
    std::uint64_t parsed_cache_version = 0;
    if (!gbe::gc_message::build_dota_lobby_invite_cache_subscribed_payload(lobby_id, inviter_steam_id, invitee_steam_id, inviter_name, parsed_members, message, &parsed_invite_gid, &parsed_cache_version))
        return false;
    invite_gid = static_cast<uint64>(parsed_invite_gid);
    cache_version = static_cast<uint64>(parsed_cache_version);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Built 2011 lobby invite lobby_id=%llu invitee=%llu inviter=%llu invite_gid=%llu cache_version=%llu members=%zu",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(invitee_steam_id),
        static_cast<unsigned long long>(inviter_steam_id),
        static_cast<unsigned long long>(invite_gid),
        static_cast<unsigned long long>(cache_version),
        members.size());
    return true;
}


bool GBE_IsDotaLobbyInviteCacheSubscribedPayload(const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        if (object.type_id() == 2011 && object.object_data_size() > 0)
            return true;
    }

    return false;
}

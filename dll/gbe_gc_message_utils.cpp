#include "gbe_gc_message_utils.h"

#include "gbe_proto_wire.h"

#include <atomic>
#include <algorithm>
#include <cstring>
#include <limits>

namespace gbe::gc_message {

namespace {

constexpr std::uint32_t kGcInvitationCreated = 4502u;
constexpr std::uint32_t kDotaPracticeLobbyDetailsUpdate = 26u;
constexpr std::uint32_t kDotaOtherJoinedChannel = 7013u;
constexpr std::uint32_t kDotaOtherLeftChannel = 7014u;
constexpr std::uint32_t kDotaJoinChatChannelResponse = 7010u;
constexpr std::uint32_t kDotaChatMessage = 7273u;
constexpr std::uint32_t kDotaCacheSubscribed = 24u;
constexpr std::uint32_t kDotaCacheUnsubscribed = 25u;
constexpr std::uint32_t kDotaCacheSubscribedUpToDate = 29u;
constexpr std::uint32_t kDotaPopup = 7102u;
constexpr std::uint32_t kDotaPracticeLobbyResponse = 7055u;
constexpr std::uint32_t kDotaPracticeLobbyJoinResponse = 7113u;
constexpr std::uint32_t kDotaDestroyLobbyResponse = 8247u;
constexpr std::uint32_t kDotaFriendPracticeLobbyListResponse = 7112u;
constexpr std::uint32_t kDotaCustomLobbyListResponse = 7043u;
constexpr std::uint32_t kDotaCustomGameInfoResponse = 8021u;
constexpr std::uint32_t kDotaLobbyListResponse = 8012u;

constexpr unsigned char kDotaPracticeLobbyResponseTemplate[] = {
    0x8F, 0x1B, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01,
};

} // namespace

bool has_proto_mask(std::uint32_t emsg)
{
    return (emsg & kProtoMask) != 0;
}

std::uint32_t without_proto_mask(std::uint32_t emsg)
{
    return emsg & ~kProtoMask;
}

std::uint32_t with_proto_mask(std::uint32_t emsg)
{
    return emsg | kProtoMask;
}

bool should_trace_dota_proto_boundary(std::uint32_t emsg)
{
    switch (without_proto_mask(emsg)) {
        case 24u:
        case 4005u:
        case 4511u:
        case 7034u:
        case 7450u:
        case 7451u:
            return true;
        default:
            return false;
    }
}

bool is_supported_dota_wrapped_post_login_request(std::uint32_t inner_emsg)
{
    switch (inner_emsg) {
        case 7004u:
        case 7009u:
        case 7035u:
        case 7038u:
        case 7040u:
        case 7041u:
        case 7044u:
        case 7046u:
        case 7047u:
        case 7070u:
        case 7081u:
        case 7091u:
        case 7111u:
        case 7149u:
        case 7272u:
        case 7367u:
        case 8009u:
        case 8011u:
        case 8052u:
        case 8053u:
        case 8054u:
        case 8246u:
            return true;
        default:
            return false;
    }
}

bool build_dota_zero_header_payload(std::uint32_t emsg, const std::string &body, std::string &message)
{
    message.clear();
    proto_wire::append_little_endian32(message, with_proto_mask(emsg));
    proto_wire::append_little_endian32(message, 0u);
    message.append(body);
    return true;
}

bool build_dota_job_reply_payload(std::uint32_t emsg, std::uint64_t request_job_id, const std::string &body, std::string &message)
{
    message.clear();
    proto_wire::append_little_endian32(message, with_proto_mask(emsg));
    proto_wire::append_little_endian32(message, 9u);
    message.push_back(static_cast<char>(0x59));
    message.resize(17u);
    std::memcpy(message.data() + 9, &request_job_id, sizeof(request_job_id));
    message.append(body);
    return true;
}

bool build_dota_job_reply_or_zero_header_payload(std::uint32_t emsg, bool has_request_job, std::uint64_t request_job_id, const std::string &body, std::string &message)
{
    if (has_request_job)
        return build_dota_job_reply_payload(emsg, request_job_id, body, message);

    return build_dota_zero_header_payload(emsg, body, message);
}

bool build_wrapped_dota_replay_message(std::uint32_t outer_emsg, std::uint32_t app_id, const std::string &inner_payload, const std::string &outer_session_field_raw, std::uint64_t steam_id, std::string &message)
{
    if (inner_payload.size() < sizeof(std::uint32_t))
        return false;

    std::uint32_t inner_emsg_flagged = 0;
    std::memcpy(&inner_emsg_flagged, inner_payload.data(), sizeof(inner_emsg_flagged));

    std::string outer_body;
    proto_wire::append_varint_field(outer_body, 1u, app_id);
    proto_wire::append_varint_field(outer_body, 2u, inner_emsg_flagged);
    proto_wire::append_bytes_field(outer_body, 3u, inner_payload);

    std::string outer_header;
    proto_wire::append_fixed64_field(outer_header, 1u, steam_id);
    proto_wire::append_varuint(outer_header, (static_cast<std::uint64_t>(2u) << 3) | 0u);
    outer_header.append(outer_session_field_raw);

    message.clear();
    proto_wire::append_little_endian32(message, with_proto_mask(outer_emsg));
    proto_wire::append_little_endian32(message, static_cast<std::uint32_t>(outer_header.size()));
    message.append(outer_header);
    message.append(outer_body);
    return true;
}

bool build_dota_varint_response_payload(std::uint32_t emsg, std::uint32_t field_number, std::uint64_t value, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, field_number, value);
    return build_dota_job_reply_or_zero_header_payload(emsg, has_request_job, request_job_id, body, message);
}

bool build_dota_fixed32_response_payload(std::uint32_t emsg, std::uint32_t field_number, std::uint32_t value, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_fixed32_field(body, field_number, value);
    return build_dota_job_reply_or_zero_header_payload(emsg, has_request_job, request_job_id, body, message);
}

bool build_dota_bytes_response_payload(std::uint32_t emsg, std::uint32_t field_number, const std::string &value, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_bytes_field(body, field_number, value);
    return build_dota_job_reply_or_zero_header_payload(emsg, has_request_job, request_job_id, body, message);
}

bool build_dota_lobby_additional_startup_account_data_payload(std::uint32_t account_id, std::string &payload)
{
    payload.clear();
    if (account_id == 0)
        return true;

    proto_wire::append_varint_field(payload, 1u, account_id);
    proto_wire::append_bytes_field(payload, 2u, std::string());
    return true;
}

bool build_dota_server_lobby_object_2015(std::size_t member_count, std::uint32_t extra_startup_account_id, std::string &object_2015)
{
    object_2015.clear();
    const std::size_t effective_member_count = std::max<std::size_t>(member_count, 1u);
    for (std::size_t i = 0; i < effective_member_count; ++i)
        proto_wire::append_bytes_field(object_2015, 1u, std::string());

    if (extra_startup_account_id == 0)
        return true;

    std::string startup_payload;
    if (!build_dota_lobby_additional_startup_account_data_payload(extra_startup_account_id, startup_payload))
        return false;

    std::string startup_message;
    proto_wire::append_varint_field(startup_message, 1u, 8869u);
    proto_wire::append_bytes_field(startup_message, 2u, startup_payload);
    proto_wire::append_bytes_field(object_2015, 2u, startup_message);
    return true;
}

static void append_dota_lobby_event_periodic_resource(std::string &account_points, std::uint32_t periodic_resource_id, std::uint32_t remaining, std::uint32_t max)
{
    std::string resource;
    proto_wire::append_varint_field(resource, 1u, periodic_resource_id);
    proto_wire::append_varint_field(resource, 2u, remaining);
    proto_wire::append_varint_field(resource, 3u, max);
    proto_wire::append_bytes_field(account_points, 31u, resource);
}

static void append_dota_lobby_event_account_points(std::string &event_points, std::uint32_t account_id, std::uint32_t normal_points, std::uint32_t premium_points, bool owned, std::uint32_t event_level, bool include_periodic_resources)
{
    std::string account_points;
    proto_wire::append_varint_field(account_points, 1u, account_id);
    proto_wire::append_varint_field(account_points, 2u, normal_points);
    proto_wire::append_varint_field(account_points, 3u, premium_points);
    proto_wire::append_varint_field(account_points, 4u, owned ? 1u : 0u);
    proto_wire::append_varint_field(account_points, 7u, event_level);
    proto_wire::append_varint_field(account_points, 12u, 0u);
    proto_wire::append_varint_field(account_points, 26u, 0u);
    proto_wire::append_varint_field(account_points, 27u, 0u);
    proto_wire::append_varint_field(account_points, 28u, 0u);
    if (include_periodic_resources) {
        append_dota_lobby_event_periodic_resource(account_points, 15u, 10u, 10u);
        append_dota_lobby_event_periodic_resource(account_points, 28u, 1000u, 1000u);
    }

    proto_wire::append_bytes_field(event_points, 2u, account_points);
}

static void append_dota_lobby_event_points(std::string &object_2016, std::uint32_t event_id, const std::vector<DotaStaticLobbyMember> &members, std::uint32_t owner_account_id, std::uint32_t normal_points, std::uint32_t premium_points, bool owner_owned, bool non_owner_owned, std::uint32_t event_level, bool include_periodic_resources)
{
    std::string event_points;
    proto_wire::append_varint_field(event_points, 1u, event_id);
    for (const DotaStaticLobbyMember &member : members) {
        if (member.steam_id == 0ull || member.account_id == 0u)
            continue;
        append_dota_lobby_event_account_points(event_points, member.account_id, normal_points, premium_points, member.account_id == owner_account_id ? owner_owned : non_owner_owned, event_level, include_periodic_resources);
    }
    proto_wire::append_bytes_field(object_2016, 3u, event_points);
}

void build_dota_server_static_lobby_object_2016(std::uint32_t account_id, std::uint64_t steam_id, std::uint32_t game_mode, const std::vector<DotaStaticLobbyMember> &members, std::string &object_2016, bool is_custom_game)
{
    object_2016.clear();

    std::vector<DotaStaticLobbyMember> effective_members = members;
    if (effective_members.empty()) {
        DotaStaticLobbyMember owner{};
        owner.steam_id = steam_id;
        owner.account_id = account_id;
        effective_members.push_back(owner);
    }

    bool wrote_owner_event_points = false;
    bool include_event_points = false;
    for (const DotaStaticLobbyMember &member : effective_members) {
        if (member.steam_id == 0ull)
            continue;

        if (member.connected)
            include_event_points = true;

        std::string member_bytes;
        proto_wire::append_fixed64_field(member_bytes, 1u, member.steam_id);
        proto_wire::append_varint_field(member_bytes, 9u, 0u);
        proto_wire::append_varint_field(member_bytes, 11u, 1u);
        proto_wire::append_fixed64_field(member_bytes, 12u, 0ull);
        proto_wire::append_varint_field(member_bytes, 13u, 0u);
        if (game_mode != 2u)
            proto_wire::append_fixed32_field(member_bytes, 16u, 0u);
        for (std::size_t i = 0; i < 4; ++i)
            proto_wire::append_varint_field(member_bytes, 19u, 0u);
        proto_wire::append_bytes_field(object_2016, 1u, member_bytes);

        if (!wrote_owner_event_points && member.account_id == account_id)
            wrote_owner_event_points = true;
    }

    if (account_id != 0u)
        wrote_owner_event_points = true;

    proto_wire::append_fixed32_field(object_2016, 2u, 0u);

    if (!is_custom_game && include_event_points && account_id != 0u && wrote_owner_event_points) {
        for (std::uint32_t eid = 7; eid <= 60; eid++)
            append_dota_lobby_event_points(object_2016, eid, effective_members, account_id, 1000u, 0u, true, true, 1u, eid == 19u);
    }
}

std::string build_dota_lobby_team_details_payload(bool is_home_team)
{
    std::string team_details;
    proto_wire::append_varint_field(team_details, 8u, 0u);
    proto_wire::append_varint_field(team_details, 17u, is_home_team ? 1u : 0u);
    return team_details;
}

void build_dota_static_lobby_object_2014(const std::string &player_name, std::size_t member_count, std::string &object_2014)
{
    object_2014.clear();
    const std::size_t effective_member_count = std::max<std::size_t>(member_count, 1u);
    for (std::size_t i = 0; i < effective_member_count; ++i) {
        std::string name_entry;
        proto_wire::append_bytes_field(name_entry, 1u, i == 0 ? player_name : std::string());
        proto_wire::append_varint_field(name_entry, 2u, 0u);
        proto_wire::append_bytes_field(object_2014, 1u, name_entry);
    }
}

void build_dota_lobby_member_object_2004(const DotaLobbyMemberObjectState &member, std::uint32_t lobby_state, std::uint32_t lobby_game_state, std::string &member_state)
{
    member_state.clear();
    if (member.steam_id == 0ull) {
        member_state.assign(1u, '\0');
        return;
    }

    proto_wire::append_fixed64_field(member_state, 1u, member.steam_id);
    if (member.hero_id != 0u)
        proto_wire::append_varint_field(member_state, 2u, member.hero_id);
    proto_wire::append_varint_field(member_state, 3u, member.team);
    if (member.slot != 0u)
        proto_wire::append_varint_field(member_state, 7u, member.slot);

    std::uint32_t effective_leaver = member.leaver_status;
    if (effective_leaver == 0u && !member.connected && lobby_state == 2u && lobby_game_state >= 1u)
        effective_leaver = 1u;
    if (effective_leaver != 0u) {
        proto_wire::append_varint_field(member_state, 16u, effective_leaver);
        if (lobby_state == 2u && lobby_game_state >= 1u)
            proto_wire::append_varint_field(member_state, 28u, 0u);
    } else if (lobby_state != 2u || lobby_game_state < 1u) {
        proto_wire::append_varint_field(member_state, 16u, 1u);
    }
}

void build_dota_lobby_object_2004(const DotaLobbyObject2004Options &options, const std::vector<DotaLobbyMemberObjectState> &members, std::string &object_2004)
{
    static const unsigned char kDotaLobbyField62Value[] = { 0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00 };
    static constexpr std::uint32_t kDotaLobbyField128Value = 1776809986u;

    object_2004.clear();
    proto_wire::append_varint_field(object_2004, 1u, options.lobby_id);
    proto_wire::append_varint_field(object_2004, 3u, options.game_mode);
    proto_wire::append_varint_field(object_2004, 4u, options.lobby_state);
    if (!options.connect.empty())
        proto_wire::append_bytes_field(object_2004, 5u, options.connect);
    if (options.server_id != 0ull)
        proto_wire::append_fixed64_field(object_2004, 6u, options.server_id);
    proto_wire::append_fixed64_field(object_2004, 11u, options.steam_id);
    proto_wire::append_varint_field(object_2004, 12u, 1u);
    proto_wire::append_varint_field(object_2004, 13u, options.allow_cheats ? 1u : 0u);
    proto_wire::append_varint_field(object_2004, 14u, options.fill_with_bots ? 1u : 0u);
    proto_wire::append_bytes_field(object_2004, 16u, options.room_name);
    if (options.lobby_state != 0u) {
        proto_wire::append_bytes_field(object_2004, 17u, build_dota_lobby_team_details_payload(true));
        proto_wire::append_bytes_field(object_2004, 17u, build_dota_lobby_team_details_payload(false));
    }
    proto_wire::append_varint_field(object_2004, 21u, options.server_region);
    if (options.lobby_state != 0u || options.lobby_game_state != 0u)
        proto_wire::append_varint_field(object_2004, 22u, options.lobby_game_state);
    proto_wire::append_varint_field(object_2004, 28u, 0u);
    if (options.match_id != 0ull)
        proto_wire::append_varint_field(object_2004, 30u, options.match_id);
    proto_wire::append_varint_field(object_2004, 31u, options.allow_spectating ? 1u : 0u);
    proto_wire::append_varint_field(object_2004, 36u, options.bot_difficulty_radiant);
    proto_wire::append_bytes_field(object_2004, 39u, options.pass_key);
    proto_wire::append_varint_field(object_2004, 42u, 0u);
    proto_wire::append_varint_field(object_2004, 43u, 0u);
    proto_wire::append_varint_field(object_2004, 44u, 0u);
    proto_wire::append_varint_field(object_2004, 46u, 0u);
    proto_wire::append_varint_field(object_2004, 47u, 0u);
    proto_wire::append_varint_field(object_2004, 48u, 0u);
    proto_wire::append_varint_field(object_2004, 51u, 0u);
    proto_wire::append_varint_field(object_2004, 53u, 0u);
    if (options.custom_game) {
        if (!options.custom_game->mode.empty())
            proto_wire::append_bytes_field(object_2004, 54u, options.custom_game->mode);
        if (!options.custom_game->map_name.empty())
            proto_wire::append_bytes_field(object_2004, 55u, options.custom_game->map_name);
        if (options.custom_game->difficulty != 0u)
            proto_wire::append_varint_field(object_2004, 56u, options.custom_game->difficulty);
    }
    proto_wire::append_varint_field(object_2004, 57u, options.lan ? 1u : 0u);
    if (options.has_broadcast_channel) {
        std::string broadcast_info;
        proto_wire::append_varint_field(broadcast_info, 1u, options.broadcast_channel_id);
        proto_wire::append_bytes_field(broadcast_info, 2u, options.broadcast_country_code);
        proto_wire::append_bytes_field(broadcast_info, 3u, options.broadcast_description);
        proto_wire::append_bytes_field(broadcast_info, 4u, options.broadcast_language_code);
        proto_wire::append_bytes_field(object_2004, 58u, broadcast_info);
    }
    proto_wire::append_bytes_field(object_2004, 62u, std::string(reinterpret_cast<const char *>(kDotaLobbyField62Value), sizeof(kDotaLobbyField62Value)));
    if (options.lobby_state == 2u && options.lobby_game_state >= 1u)
        proto_wire::append_varint_field(object_2004, 65u, 0u);
    if (options.lobby_state == 3u && options.lobby_game_state == 6u)
        proto_wire::append_varint_field(object_2004, 70u, 2u);
    if (options.custom_game) {
        if (options.custom_game->game_id != 0ull)
            proto_wire::append_varint_field(object_2004, 68u, options.custom_game->game_id);
        if (options.custom_game->min_players != 0u)
            proto_wire::append_varint_field(object_2004, 71u, options.custom_game->min_players);
        if (options.custom_game->max_players != 0u)
            proto_wire::append_varint_field(object_2004, 72u, options.custom_game->max_players);
    }
    proto_wire::append_varint_field(object_2004, 75u, options.visibility);
    if (options.custom_game && options.custom_game->crc != 0ull)
        proto_wire::append_fixed64_field(object_2004, 76u, options.custom_game->crc);
    if (options.custom_game && options.custom_game->timestamp != 0u)
        proto_wire::append_fixed32_field(object_2004, 80u, options.custom_game->timestamp);
    proto_wire::append_varint_field(object_2004, 82u, 0u);
    if (options.game_start_time != 0u)
        proto_wire::append_varint_field(object_2004, 87u, options.game_start_time);
    proto_wire::append_varint_field(object_2004, 88u, 0u);
    proto_wire::append_varint_field(object_2004, 93u, options.bot_difficulty_dire);
    proto_wire::append_varint_field(object_2004, 94u, options.bot_radiant);
    proto_wire::append_varint_field(object_2004, 95u, options.bot_dire);
    proto_wire::append_varint_field(object_2004, 97u, 0u);
    if (options.lobby_state != 0u) {
        proto_wire::append_varint_field(object_2004, 103u, 0u);
        proto_wire::append_varint_field(object_2004, 104u, 0u);
    }
    if (!options.lan_host_ping_location.empty())
        proto_wire::append_bytes_field(object_2004, 109u, options.lan_host_ping_location);
    if (options.custom_game)
        proto_wire::append_varint_field(object_2004, 107u, options.custom_game->penalties ? 1u : 0u);
    proto_wire::append_varint_field(object_2004, 110u, 0u);
    if (options.lobby_state == 3u && options.lobby_game_state == 6u && options.game_start_time != 0u)
        proto_wire::append_varint_field(object_2004, 111u, options.elapsed_game_time);
    proto_wire::append_varint_field(object_2004, 113u, 0u);

    for (const DotaLobbyMemberObjectState &member : members) {
        std::string member_state;
        build_dota_lobby_member_object_2004(member, options.lobby_state, options.lobby_game_state, member_state);
        if (!member_state.empty())
            proto_wire::append_bytes_field(object_2004, 120u, member_state);
    }

    for (std::size_t i = 0; i < members.size(); ++i) {
        if (members[i].steam_id == 0ull)
            proto_wire::append_varint_field(object_2004, 123u, static_cast<std::uint64_t>(i));
        else if (members[i].leaver_status >= 5u)
            proto_wire::append_varint_field(object_2004, 122u, static_cast<std::uint64_t>(i));
        else
            proto_wire::append_varint_field(object_2004, 121u, static_cast<std::uint64_t>(i));
    }
    proto_wire::append_varint_field(object_2004, 127u, 0u);
    proto_wire::append_varint_field(object_2004, 128u, kDotaLobbyField128Value);
}

bool build_dota_practice_lobby_objects(const DotaPracticeLobbyObjectOptions &options, DotaPracticeLobbyObjects &objects)
{
    if (!build_dota_server_lobby_object_2015(options.static_lobby_members.size(), options.extra_startup_account_id, objects.object_2015))
        objects.object_2015.clear();

    build_dota_server_static_lobby_object_2016(
        options.extra_startup_account_id,
        options.steam_id,
        options.game_mode,
        options.static_lobby_members,
        objects.object_2016,
        options.is_custom_game);
    build_dota_lobby_object_2004(options.object_2004_options, options.lobby_members, objects.object_2004);
    build_dota_static_lobby_object_2014(options.player_name, options.lobby_members.size(), objects.object_2014);
    return true;
}

bool build_dota_top_custom_games_list_payload(const std::vector<std::uint64_t> &mod_ids, std::string &message, std::size_t &game_count)
{
    game_count = 0;
    std::string body;
    std::uint64_t game_of_the_day = 0ull;

    for (std::uint64_t mod_id : mod_ids) {
        if (mod_id == 0ull)
            continue;
        if (game_of_the_day == 0ull)
            game_of_the_day = mod_id;
        proto_wire::append_varint_field(body, 1u, mod_id);
        ++game_count;
    }

    if (game_of_the_day != 0ull)
        proto_wire::append_varint_field(body, 2u, game_of_the_day);

    return build_dota_zero_header_payload(8024u, body, message);
}

static std::string build_soid_owner(std::uint32_t type, std::uint64_t id)
{
    std::string owner;
    proto_wire::append_varint_field(owner, 1u, type);
    proto_wire::append_varint_field(owner, 2u, id);
    return owner;
}

static std::string build_cache_subscribed_type(std::uint32_t type_id, const std::string &object_data)
{
    std::string object;
    proto_wire::append_varint_field(object, 1u, type_id);
    proto_wire::append_bytes_field(object, 2u, object_data);
    return object;
}

static std::string build_multiple_objects_entry(std::uint32_t type_id, const std::string &object_data)
{
    std::string object;
    proto_wire::append_varint_field(object, 1u, type_id);
    proto_wire::append_bytes_field(object, 2u, object_data);
    return object;
}

bool build_dota_practice_lobby_cache_subscribed_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2004, const std::string &object_2015, const std::string &object_2014, const std::string &object_2016, std::string &message)
{
    if (lobby_id == 0ull)
        return false;

    std::string body;
    proto_wire::append_bytes_field(body, 4u, build_soid_owner(3u, lobby_id));
    proto_wire::append_bytes_field(body, 2u, build_cache_subscribed_type(2004u, object_2004));
    proto_wire::append_bytes_field(body, 2u, build_cache_subscribed_type(2015u, object_2015));
    proto_wire::append_bytes_field(body, 2u, build_cache_subscribed_type(2013u, std::string()));
    proto_wire::append_bytes_field(body, 2u, build_cache_subscribed_type(2014u, object_2014));
    proto_wire::append_bytes_field(body, 2u, build_cache_subscribed_type(2016u, object_2016));
    return build_dota_zero_header_payload(24u, body, message);
}

bool build_dota_practice_lobby_details_update_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2014, const std::string &object_2015, const std::string &object_2004, const std::string &object_2016, bool include_server_lobby_placeholder, std::string &message)
{
    if (lobby_id == 0ull)
        return false;

    std::string body;
    proto_wire::append_bytes_field(body, 6u, build_soid_owner(3u, lobby_id));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2014u, object_2014));
    if (include_server_lobby_placeholder)
        proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2013u, std::string()));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2015u, object_2015));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2004u, object_2004));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2016u, object_2016));
    return build_dota_zero_header_payload(26u, body, message);
}

bool build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2014, const std::string &object_2016, const std::string &object_2015, const std::string &object_2004, std::string &message)
{
    static constexpr std::uint64_t kDotaLobbyDetailsTimestamp = 0x0069E7F5C567E78Bull;
    if (lobby_id == 0ull)
        return false;

    std::string body;
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2014u, object_2014));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2016u, object_2016));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2015u, object_2015));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2013u, std::string()));
    proto_wire::append_bytes_field(body, 2u, build_multiple_objects_entry(2004u, object_2004));
    proto_wire::append_fixed64_field(body, 3u, kDotaLobbyDetailsTimestamp);
    proto_wire::append_bytes_field(body, 6u, build_soid_owner(3u, lobby_id));
    return build_dota_zero_header_payload(26u, body, message);
}

std::size_t build_dota_joinable_custom_game_modes_body(const std::vector<DotaJoinableCustomGameMode> &modes, std::string &body)
{
    body.clear();
    std::vector<std::uint64_t> seen_game_ids;
    for (const DotaJoinableCustomGameMode &mode : modes) {
        if (mode.custom_game_id == 0ull)
            continue;
        if (std::find(seen_game_ids.begin(), seen_game_ids.end(), mode.custom_game_id) != seen_game_ids.end())
            continue;

        std::string entry;
        proto_wire::append_varint_field(entry, 1u, mode.custom_game_id);
        proto_wire::append_varint_field(entry, 2u, 1u);
        proto_wire::append_varint_field(entry, 3u, mode.member_count == 0u ? 1u : mode.member_count);
        proto_wire::append_bytes_field(body, 1u, entry);
        seen_game_ids.push_back(mode.custom_game_id);
    }
    return seen_game_ids.size();
}

std::size_t build_dota_joinable_custom_lobbies_body(const std::vector<DotaJoinableCustomLobby> &lobbies, std::uint64_t requested_custom_game_id, std::string &body)
{
    body.clear();
    std::vector<std::uint64_t> seen_lobby_ids;
    for (const DotaJoinableCustomLobby &lobby : lobbies) {
        if (requested_custom_game_id != 0ull && lobby.custom_game_id != requested_custom_game_id)
            continue;
        if (lobby.lobby_id == 0ull)
            continue;
        if (std::find(seen_lobby_ids.begin(), seen_lobby_ids.end(), lobby.lobby_id) != seen_lobby_ids.end())
            continue;

        std::string entry;
        proto_wire::append_fixed64_field(entry, 1u, lobby.lobby_id);
        proto_wire::append_varint_field(entry, 2u, lobby.custom_game_id);
        proto_wire::append_bytes_field(entry, 3u, lobby.display_name);
        proto_wire::append_varint_field(entry, 4u, lobby.member_count);
        proto_wire::append_varint_field(entry, 5u, lobby.owner_account_id);
        proto_wire::append_bytes_field(entry, 6u, lobby.owner_name);
        if (!lobby.map_name.empty())
            proto_wire::append_bytes_field(entry, 7u, lobby.map_name);
        proto_wire::append_varint_field(entry, 8u, lobby.max_players == 0u ? 10u : lobby.max_players);
        proto_wire::append_varint_field(entry, 9u, lobby.server_region);
        proto_wire::append_varint_field(entry, 11u, lobby.has_pass_key ? 1u : 0u);
        if (!lobby.lan_host_ping_location.empty())
            proto_wire::append_bytes_field(entry, 12u, lobby.lan_host_ping_location);
        proto_wire::append_varint_field(entry, 13u, lobby.created_time);
        if (lobby.custom_game_timestamp != 0u)
            proto_wire::append_varint_field(entry, 14u, lobby.custom_game_timestamp);
        if (lobby.custom_game_crc != 0ull)
            proto_wire::append_fixed64_field(entry, 15u, lobby.custom_game_crc);
        if (lobby.min_players != 0u)
            proto_wire::append_varint_field(entry, 16u, lobby.min_players);
        proto_wire::append_varint_field(entry, 17u, lobby.penalties ? 1u : 0u);

        proto_wire::append_bytes_field(body, 1u, entry);
        seen_lobby_ids.push_back(lobby.lobby_id);
    }
    return seen_lobby_ids.size();
}

void build_dota_find_top_source_tv_games_body(const DotaSourceTVGame *game, std::string &body)
{
    body.clear();
    if (!game)
        return;

    std::string game_entry;
    proto_wire::append_varint_field(game_entry, 1u, game->start_time);
    proto_wire::append_varint_field(game_entry, 3u, game->server_id);
    proto_wire::append_varint_field(game_entry, 4u, game->lobby_id);
    proto_wire::append_varint_field(game_entry, 6u, 1u);
    proto_wire::append_varint_field(game_entry, 7u, game->game_time);
    proto_wire::append_varint_field(game_entry, 8u, 0u);
    proto_wire::append_varint_field(game_entry, 9u, 0u);
    proto_wire::append_varint_field(game_entry, 10u, game->game_mode);
    proto_wire::append_varint_field(game_entry, 12u, game->match_id);
    for (const DotaSourceTVPlayer &player : game->players) {
        if (player.account_id == 0u)
            continue;
        std::string player_entry;
        proto_wire::append_varint_field(player_entry, 1u, player.account_id);
        proto_wire::append_varint_field(player_entry, 2u, player.hero_id);
        proto_wire::append_varint_field(player_entry, 3u, player.slot);
        proto_wire::append_varint_field(player_entry, 4u, player.team);
        proto_wire::append_bytes_field(game_entry, 22u, player_entry);
    }
    proto_wire::append_bytes_field(body, 7u, game_entry);
    proto_wire::append_varint_field(body, 5u, 1u);
}

void build_dota_find_top_source_tv_games_empty_body(std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 5u, 0u);
}

void build_dota_spectate_friend_game_response_body(std::uint64_t server_steam_id, std::string &body)
{
    body.clear();
    proto_wire::append_fixed64_field(body, 4u, server_steam_id);
}

void build_dota_watch_game_pending_response_body(std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);
}

void build_dota_watch_game_ready_response_body(std::uint32_t source_tv_addr, std::uint32_t source_tv_port, std::uint64_t watch_server_steam_id, std::uint64_t secret_code, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 1u);
    if (source_tv_addr != 0u) {
        proto_wire::append_varint_field(body, 2u, source_tv_addr);
        proto_wire::append_varint_field(body, 3u, source_tv_addr);
    }
    proto_wire::append_varint_field(body, 4u, source_tv_port);
    proto_wire::append_fixed64_field(body, 5u, watch_server_steam_id);
    proto_wire::append_fixed64_field(body, 6u, watch_server_steam_id);
    proto_wire::append_fixed64_field(body, 7u, secret_code);
}

void build_dota_claim_event_action_response_body(std::uint32_t action_id, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);

    std::string reward_data;
    proto_wire::append_varint_field(reward_data, 1u, 0u);
    proto_wire::append_varint_field(reward_data, 2u, 0u);
    proto_wire::append_varint_field(reward_data, 3u, 0u);
    proto_wire::append_varint_field(reward_data, 5u, action_id);
    proto_wire::append_bytes_field(body, 2u, reward_data);
    proto_wire::append_varint_field(body, 3u, action_id);
}

void build_dota_claim_event_action_using_item_response_body(std::uint32_t action_id, std::string &body)
{
    std::string action_results;
    build_dota_claim_event_action_response_body(action_id, action_results);
    body.clear();
    proto_wire::append_bytes_field(body, 1u, action_results);
}

bool build_dota_practice_lobby_response_payload(std::uint64_t request_job_id, bool has_request_job, std::string &message)
{
    message.assign(reinterpret_cast<const char *>(kDotaPracticeLobbyResponseTemplate), sizeof(kDotaPracticeLobbyResponseTemplate));
    if (message.size() != sizeof(kDotaPracticeLobbyResponseTemplate) || message.size() < 17)
        return false;

    if (!has_request_job) {
        const std::string body = message.substr(17);
        return build_dota_zero_header_payload(kDotaPracticeLobbyResponse, body, message);
    }

    message[8] = static_cast<char>(0x59);
    std::memcpy(message.data() + 9, &request_job_id, sizeof(request_job_id));
    return true;
}

bool build_dota_practice_lobby_join_response_payload(bool has_request_job, std::uint64_t request_job_id, std::uint32_t result, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, result);
    return build_dota_job_reply_or_zero_header_payload(kDotaPracticeLobbyJoinResponse, has_request_job, request_job_id, body, message);
}

bool build_dota_invitation_created_payload(std::uint64_t group_id, std::uint64_t steam_id, bool user_offline, std::string &message)
{
    std::string body;
    if (group_id != 0)
        proto_wire::append_varint_field(body, 1u, group_id);
    if (steam_id != 0)
        proto_wire::append_fixed64_field(body, 2u, steam_id);
    proto_wire::append_varint_field(body, 3u, user_offline ? 1u : 0u);
    return build_dota_zero_header_payload(kGcInvitationCreated, body, message);
}

bool build_dota_lobby_invite_cache_subscribed_payload(std::uint64_t lobby_id, std::uint64_t inviter_steam_id, std::uint64_t invitee_steam_id, const std::string &inviter_name, const std::vector<std::pair<std::uint64_t, std::string>> &members, std::string &message, std::uint64_t *invite_gid_out, std::uint64_t *cache_version_out)
{
    if (lobby_id == 0 || inviter_steam_id == 0 || invitee_steam_id == 0)
        return false;

    const std::string fallback_name = inviter_name.empty() ? std::string("Lobby Host") : inviter_name;

    std::string invite_object;
    proto_wire::append_varint_field(invite_object, 1u, lobby_id);
    proto_wire::append_fixed64_field(invite_object, 2u, inviter_steam_id);
    proto_wire::append_bytes_field(invite_object, 3u, fallback_name);

    bool added_member = false;
    for (const auto &member : members) {
        if (member.first == 0)
            continue;
        std::string member_object;
        std::string member_name = member.second;
        if (member_name.empty() && member.first == inviter_steam_id)
            member_name = inviter_name;
        proto_wire::append_bytes_field(member_object, 1u, member_name.empty() ? std::string("Lobby Host") : member_name);
        proto_wire::append_fixed64_field(member_object, 2u, member.first);
        proto_wire::append_bytes_field(invite_object, 4u, member_object);
        added_member = true;
    }
    if (!added_member) {
        std::string member_object;
        proto_wire::append_bytes_field(member_object, 1u, fallback_name);
        proto_wire::append_fixed64_field(member_object, 2u, inviter_steam_id);
        proto_wire::append_bytes_field(invite_object, 4u, member_object);
    }

    std::uint64_t cache_version = 0;
    const std::uint64_t invite_gid = generate_dota_lobby_invite_gid(lobby_id, invitee_steam_id, cache_version);
    proto_wire::append_varint_field(invite_object, 5u, 0u);
    proto_wire::append_fixed64_field(invite_object, 6u, invite_gid);
    proto_wire::append_fixed64_field(invite_object, 7u, 0u);
    proto_wire::append_fixed32_field(invite_object, 8u, 0u);

    std::string owner_soid;
    proto_wire::append_varint_field(owner_soid, 1u, 4u);
    proto_wire::append_varint_field(owner_soid, 2u, invitee_steam_id);

    std::string invite_entry;
    proto_wire::append_varint_field(invite_entry, 1u, 2011u);
    proto_wire::append_bytes_field(invite_entry, 2u, invite_object);

    std::string body;
    proto_wire::append_bytes_field(body, 2u, invite_entry);
    proto_wire::append_fixed64_field(body, 3u, cache_version);
    proto_wire::append_bytes_field(body, 4u, owner_soid);
    const bool built = build_dota_zero_header_payload(kDotaCacheSubscribed, body, message);
    if (invite_gid_out)
        *invite_gid_out = invite_gid;
    if (cache_version_out)
        *cache_version_out = cache_version;
    return built;
}

bool build_dota_other_joined_channel_payload(std::uint64_t channel_id, const std::string &persona_name, std::uint64_t steam_id, std::string &message)
{
    std::string body;
    proto_wire::append_fixed64_field(body, 1u, channel_id);
    if (!persona_name.empty())
        proto_wire::append_bytes_field(body, 2u, persona_name);
    proto_wire::append_fixed64_field(body, 3u, steam_id);
    proto_wire::append_varint_field(body, 5u, 0u);
    return build_dota_zero_header_payload(kDotaOtherJoinedChannel, body, message);
}

bool build_dota_other_left_channel_payload(std::uint64_t channel_id, std::uint64_t steam_id, std::string &message)
{
    std::string body;
    proto_wire::append_fixed64_field(body, 1u, channel_id);
    proto_wire::append_fixed64_field(body, 2u, steam_id);
    return build_dota_zero_header_payload(kDotaOtherLeftChannel, body, message);
}

std::string build_dota_7034_leaver_state_payload(std::uint32_t lobby_state, std::uint32_t game_state)
{
    std::string leaver_state;
    proto_wire::append_varint_field(leaver_state, 1u, lobby_state);
    proto_wire::append_varint_field(leaver_state, 2u, game_state);
    proto_wire::append_varint_field(leaver_state, 3u, 0u);
    proto_wire::append_varint_field(leaver_state, 4u, 0u);
    proto_wire::append_varint_field(leaver_state, 5u, 0u);
    proto_wire::append_varint_field(leaver_state, 6u, 0u);
    return leaver_state;
}

bool build_dota_7034_connected_players_response_payload(
    const std::vector<Dota7034Player> &connected_players,
    const std::vector<Dota7034Player> &disconnected_players,
    const std::vector<Dota7034Player> &draft_players,
    std::uint32_t game_state,
    const Dota7034ExtraState &extra_state,
    bool has_request_job,
    std::uint64_t request_job_id,
    std::string &message)
{
    std::string body;
    for (const Dota7034Player &connected_player : connected_players) {
        std::string player;
        proto_wire::append_fixed64_field(player, 1u, connected_player.steam_id);
        if (connected_player.hero_id != 0u)
            proto_wire::append_varint_field(player, 2u, connected_player.hero_id);
        proto_wire::append_bytes_field(player, 3u, build_dota_7034_leaver_state_payload(connected_player.lobby_state, connected_player.game_state));
        proto_wire::append_varint_field(player, 4u, 0u);
        proto_wire::append_bytes_field(body, 1u, player);

        if (connected_player.include_draft) {
            std::string draft;
            proto_wire::append_fixed64_field(draft, 1u, connected_player.steam_id);
            proto_wire::append_varint_field(draft, 2u, connected_player.team);
            proto_wire::append_varint_field(draft, 3u, connected_player.slot);
            proto_wire::append_bytes_field(body, 16u, draft);
        }
    }

    for (const Dota7034Player &disconnected_player : disconnected_players) {
        std::string player;
        proto_wire::append_fixed64_field(player, 1u, disconnected_player.steam_id);
        proto_wire::append_bytes_field(player, 3u, build_dota_7034_leaver_state_payload(disconnected_player.lobby_state, disconnected_player.game_state));
        proto_wire::append_varint_field(player, 4u, 0u);
        proto_wire::append_bytes_field(body, 7u, player);
    }

    proto_wire::append_varint_field(body, 2u, game_state);
    if (extra_state.has_first_blood_happened)
        proto_wire::append_varint_field(body, 6u, extra_state.first_blood_happened);
    proto_wire::append_varint_field(body, 8u, extra_state.has_send_reason ? extra_state.send_reason : 2u);
    if (extra_state.has_radiant_kills)
        proto_wire::append_varint_field(body, 11u, extra_state.radiant_kills);
    if (extra_state.has_dire_kills)
        proto_wire::append_varint_field(body, 12u, extra_state.dire_kills);
    if (extra_state.has_radiant_lead)
        proto_wire::append_varint_field(body, 14u, extra_state.radiant_lead);
    if (extra_state.has_building_state)
        proto_wire::append_varint_field(body, 15u, extra_state.building_state);

    for (const Dota7034Player &draft_player : draft_players) {
        std::string draft;
        proto_wire::append_fixed64_field(draft, 1u, draft_player.steam_id);
        proto_wire::append_varint_field(draft, 2u, draft_player.team);
        proto_wire::append_varint_field(draft, 3u, draft_player.slot);
        proto_wire::append_bytes_field(body, 16u, draft);
    }

    return build_dota_job_reply_or_zero_header_payload(7034u, has_request_job, request_job_id, body, message);
}

bool build_dota_join_chat_channel_response_payload(std::uint64_t channel_id, const std::string &channel_name, const std::vector<DotaChatMember> &members, std::uint32_t channel_type, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 0u);
    proto_wire::append_bytes_field(body, 2u, channel_name);
    proto_wire::append_fixed64_field(body, 3u, channel_id);
    proto_wire::append_varint_field(body, 4u, 200u);

    std::vector<std::uint64_t> written_members;
    for (const DotaChatMember &chat_member : members) {
        if (chat_member.steam_id == 0 || std::find(written_members.begin(), written_members.end(), chat_member.steam_id) != written_members.end())
            continue;

        const std::string effective_member_name = chat_member.persona_name.empty() ? std::to_string(chat_member.steam_id) : chat_member.persona_name;
        std::string member;
        proto_wire::append_fixed64_field(member, 1u, chat_member.steam_id);
        proto_wire::append_bytes_field(member, 2u, effective_member_name);
        proto_wire::append_varint_field(member, 3u, 0u);
        proto_wire::append_bytes_field(body, 5u, member);
        written_members.push_back(chat_member.steam_id);
    }

    proto_wire::append_varint_field(body, 6u, channel_type);
    proto_wire::append_varint_field(body, 7u, 0u);
    proto_wire::append_varint_field(body, 9u, 0u);
    proto_wire::append_varint_field(body, 11u, 0u);
    return build_dota_zero_header_payload(kDotaJoinChatChannelResponse, body, message);
}

bool build_dota_post_game_join_chat_channel_response_payload(std::uint64_t steam_id, std::uint64_t channel_id, const std::string &channel_name, const std::string &player_name, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 0u);
    proto_wire::append_bytes_field(body, 2u, channel_name);
    proto_wire::append_fixed64_field(body, 3u, channel_id);
    proto_wire::append_varint_field(body, 4u, 200u);

    std::string member;
    proto_wire::append_fixed64_field(member, 1u, steam_id);
    proto_wire::append_bytes_field(member, 2u, player_name);
    proto_wire::append_varint_field(member, 3u, 0u);
    proto_wire::append_bytes_field(body, 5u, member);

    proto_wire::append_varint_field(body, 6u, 18u);
    proto_wire::append_varint_field(body, 7u, 0u);
    proto_wire::append_varint_field(body, 8u, 1u);
    proto_wire::append_varint_field(body, 9u, 0u);
    proto_wire::append_varint_field(body, 11u, 0u);
    return build_dota_zero_header_payload(kDotaJoinChatChannelResponse, body, message);
}

bool build_dota_chat_message_payload(const std::string &request_body, std::uint64_t channel_id, std::uint32_t account_id, const std::string &persona_name, std::string &message)
{
    std::string body;
    bool saw_channel_id = false;
    bool saw_persona_name = false;

    proto_wire::append_varint_field(body, 1u, account_id);

    std::size_t offset = 0;
    const std::uint8_t *bytes = reinterpret_cast<const std::uint8_t *>(request_body.data());
    while (offset < request_body.size()) {
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(bytes, request_body.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u) {
            offset = field_end;
            continue;
        }

        if (field.number == 2u && field.wire_type == 0u) {
            saw_channel_id = true;
            proto_wire::append_varint_field(body, 2u, channel_id);
            offset = field_end;
            continue;
        }

        if (field.number == 3u) {
            saw_persona_name = true;
            if (!persona_name.empty())
                proto_wire::append_bytes_field(body, 3u, persona_name);
            else
                body.append(request_body.data() + field_offset, field_end - field_offset);
            offset = field_end;
            continue;
        }

        body.append(request_body.data() + field_offset, field_end - field_offset);
        offset = field_end;
    }

    if (!saw_channel_id)
        proto_wire::append_varint_field(body, 2u, channel_id);
    if (!saw_persona_name && !persona_name.empty())
        proto_wire::append_bytes_field(body, 3u, persona_name);

    return build_dota_zero_header_payload(kDotaChatMessage, body, message);
}

bool build_dota_so_owner_cache_unsubscribed_payload(std::uint32_t owner_type, std::uint64_t owner_id, std::string &message)
{
    std::string owner_soid;
    proto_wire::append_varint_field(owner_soid, 1u, owner_type);
    proto_wire::append_varint_field(owner_soid, 2u, owner_id);

    std::string body;
    proto_wire::append_bytes_field(body, 2u, owner_soid);
    return build_dota_zero_header_payload(kDotaCacheUnsubscribed, body, message);
}

bool build_dota_lobby_cache_unsubscribed_payload(std::uint64_t lobby_id, std::string &message)
{
    return build_dota_so_owner_cache_unsubscribed_payload(3u, lobby_id, message);
}

bool build_dota_lobby_cache_subscribed_up_to_date_payload(std::uint64_t lobby_id, std::string &message)
{
    return build_dota_lobby_cache_subscribed_up_to_date_payload(lobby_id, false, 0u, false, 0u, {}, false, 0u, message);
}

bool build_dota_lobby_cache_subscribed_up_to_date_payload(
    std::uint64_t lobby_id,
    bool has_version,
    std::uint64_t version,
    bool has_service_id,
    std::uint32_t service_id,
    const std::vector<std::uint32_t> &service_list,
    bool has_sync_version,
    std::uint64_t sync_version,
    std::string &message)
{
    std::string owner_soid;
    proto_wire::append_varint_field(owner_soid, 1u, 3u);
    proto_wire::append_varint_field(owner_soid, 2u, lobby_id);

    std::string body;
    if (has_version)
        proto_wire::append_fixed64_field(body, 1u, version);
    proto_wire::append_bytes_field(body, 2u, owner_soid);
    if (has_service_id)
        proto_wire::append_varint_field(body, 3u, service_id);
    for (std::uint32_t listed_service_id : service_list)
        proto_wire::append_varint_field(body, 4u, listed_service_id);
    if (has_sync_version)
        proto_wire::append_fixed64_field(body, 5u, sync_version);
    return build_dota_zero_header_payload(kDotaCacheSubscribedUpToDate, body, message);
}

bool build_dota_practice_lobby_kicked_popup_payload(std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 1u);
    return build_dota_zero_header_payload(kDotaPopup, body, message);
}

bool build_dota_destroy_lobby_response_payload(std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 0u);
    return build_dota_job_reply_payload(kDotaDestroyLobbyResponse, request_job_id, body, message);
}

bool build_dota_ready_up_status_payload(bool has_request_job, std::uint64_t request_job_id, std::uint64_t lobby_id, std::uint32_t state, std::uint32_t local_ready_state, std::string &message)
{
    std::string body;
    if (lobby_id != 0)
        proto_wire::append_fixed64_field(body, 1u, lobby_id);
    proto_wire::append_varint_field(body, 4u, state);
    proto_wire::append_varint_field(body, 6u, local_ready_state);
    return build_dota_job_reply_or_zero_header_payload(7170u, has_request_job, request_job_id, body, message);
}

bool build_dota_7428_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string update;
    proto_wire::append_varint_field(update, 1u, 0u);
    return build_dota_bytes_response_payload(7428u, 1u, update, has_request_job, request_job_id, message);
}

bool build_dota_4524_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    const float upload_rate_modifier = 1.0f;
    std::uint32_t upload_rate_modifier_raw = 0;
    std::memcpy(&upload_rate_modifier_raw, &upload_rate_modifier, sizeof(upload_rate_modifier_raw));
    return build_dota_fixed32_response_payload(4524u, 1u, upload_rate_modifier_raw, has_request_job, request_job_id, message);
}

bool build_dota_7388_minimal_response_payload(std::uint32_t event_id, std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 1000u);
    proto_wire::append_varint_field(body, 2u, 0u);
    proto_wire::append_varint_field(body, 3u, event_id);
    proto_wire::append_varint_field(body, 4u, 1000u);
    proto_wire::append_varint_field(body, 5u, 0u);
    proto_wire::append_varint_field(body, 7u, account_id);
    proto_wire::append_varint_field(body, 8u, 1u);
    return build_dota_job_reply_or_zero_header_payload(7388u, has_request_job, request_job_id, body, message);
}

bool build_dota_2582_lookup_account_name_response_payload(std::uint32_t account_id, const std::string &account_name, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, account_id);
    proto_wire::append_bytes_field(body, 2u, account_name);
    return build_dota_job_reply_payload(2582u, has_request_job ? request_job_id : UINT64_MAX, body, message);
}

void build_dota_unlock_item_style_response_body(std::uint64_t item_id, std::uint32_t style_index, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);
    if (item_id != 0ull)
        proto_wire::append_varint_field(body, 2u, item_id);
    if (style_index != 255u)
        proto_wire::append_varint_field(body, 3u, style_index);
}

void build_dota_set_item_style_response_body(std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);
}

bool build_dota_7504_response_payload(std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string emoticon_access;
    proto_wire::append_varint_field(emoticon_access, 1u, account_id);
    return build_dota_bytes_response_payload(7504u, 1u, emoticon_access, has_request_job, request_job_id, message);
}

bool build_dota_8096_response_payload(std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, account_id);
    proto_wire::append_varint_field(body, 17u, 12000u);
    proto_wire::append_varint_field(body, 18u, 12000u);
    proto_wire::append_varint_field(body, 21u, 0u);
    return build_dota_job_reply_or_zero_header_payload(8096u, has_request_job, request_job_id, body, message);
}

bool build_dota_game_match_sign_out_response_payload(std::uint64_t match_id, std::uint32_t duration, std::uint32_t signout_time, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    if (match_id != 0)
        proto_wire::append_varint_field(body, 1u, match_id);
    proto_wire::append_fixed32_field(body, 2u, signout_time);
    proto_wire::append_varint_field(body, 5u, 0u);
    proto_wire::append_fixed32_field(body, 7u, 0u);

    std::string signout_summary;
    proto_wire::append_varint_field(signout_summary, 1u, duration);
    proto_wire::append_bytes_field(body, 8u, signout_summary);

    std::string zero_summary;
    proto_wire::append_varint_field(zero_summary, 1u, 0u);
    proto_wire::append_bytes_field(body, 9u, zero_summary);
    proto_wire::append_bytes_field(body, 10u, zero_summary);
    proto_wire::append_bytes_field(body, 14u, zero_summary);
    proto_wire::append_bytes_field(body, 15u, zero_summary);
    return build_dota_job_reply_or_zero_header_payload(7009u, has_request_job, request_job_id, body, message);
}

bool build_dota_8880_response_payload(bool request_valid, bool has_rank_type, bool supported_rank_type, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    const std::uint32_t result = (!request_valid || !has_rank_type || !supported_rank_type) ? 2u : 0u;
    proto_wire::append_varint_field(body, 1u, result);
    proto_wire::append_varint_field(body, 2u, 0u);
    proto_wire::append_varint_field(body, 3u, 0u);
    proto_wire::append_varint_field(body, 4u, 0u);
    proto_wire::append_varint_field(body, 5u, 0u);
    return build_dota_job_reply_or_zero_header_payload(8880u, has_request_job, request_job_id, body, message);
}

bool build_dota_give_tip_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 0u);
    return build_dota_job_reply_or_zero_header_payload(8219u, has_request_job, request_job_id, body, message);
}

bool build_dota_rank_request_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    proto_wire::append_varint_field(body, 1u, 0u);
    proto_wire::append_varint_field(body, 2u, 10000u);
    proto_wire::append_varint_field(body, 3u, 10000u);
    proto_wire::append_varint_field(body, 4u, 0u);
    proto_wire::append_varint_field(body, 5u, 0u);
    return build_dota_job_reply_or_zero_header_payload(8880u, has_request_job, request_job_id, body, message);
}

bool build_dota_submit_player_report_response_v2_payload(const std::uint8_t *request_body, std::size_t request_body_size, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::uint64_t target_account_id = 0;
    proto_wire::Field target_field{};
    if (proto_wire::find_field(request_body, request_body_size, 1u, target_field))
        proto_wire::read_field_uint64(request_body, request_body_size, target_field, target_account_id);

    std::string body;
    if (target_account_id != 0)
        proto_wire::append_varint_field(body, 1u, target_account_id);

    std::size_t offset = 0;
    while (offset < request_body_size) {
        proto_wire::Field field{};
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(request_body, request_body_size, offset, field, nullptr, &field_end))
            break;
        if (field.number == 2u && field.wire_type == 0) {
            std::uint64_t reason = 0;
            if (proto_wire::read_field_uint64(request_body, request_body_size, field, reason))
                proto_wire::append_varint_field(body, 2u, reason);
        }
        offset = field_end;
    }

    proto_wire::append_varint_field(body, 5u, 1u);
    return build_dota_job_reply_or_zero_header_payload(7083u, has_request_job, request_job_id, body, message);
}

void build_dota_store_purchase_init_response_body(std::uint64_t transaction_id, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 1u);
    proto_wire::append_varint_field(body, 2u, transaction_id);
}

void build_dota_crate_items_response_body(const std::vector<std::uint32_t> &item_defs, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);
    for (std::uint32_t item_def : item_defs) {
        if (item_def != 0u)
            proto_wire::append_varint_field(body, 2u, item_def);
    }
}

void build_dota_use_item_response_body(bool items_granted, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, items_granted ? 4u : 0u);
}

void build_dota_unlock_crate_response_body(const std::vector<std::uint32_t> &granted_defs, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, 0u);
    for (std::uint32_t granted_def : granted_defs) {
        if (granted_def == 0u)
            continue;
        std::string item;
        proto_wire::append_varint_field(item, 2u, granted_def);
        proto_wire::append_bytes_field(body, 2u, item);
    }
}

void build_dota_unpack_bundle_response_body(const std::vector<std::uint32_t> &granted_defs, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 2u, 0u);
    for (std::uint32_t granted_def : granted_defs) {
        if (granted_def != 0u)
            proto_wire::append_varint_field(body, 3u, granted_def);
    }
}

void build_dota_add_socket_response_body(std::uint32_t result, std::uint64_t subject_item_id, std::uint32_t socket_attr_def, std::string &body)
{
    body.clear();
    proto_wire::append_varint_field(body, 1u, result);
    if (subject_item_id != 0ull)
        proto_wire::append_varint_field(body, 2u, subject_item_id);
    if (socket_attr_def != 0u)
        proto_wire::append_varint_field(body, 3u, socket_attr_def);
}

bool build_dota_7451_batch_player_resources_response_payload(const std::vector<std::uint32_t> &account_ids, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    for (std::uint32_t account_id : account_ids) {
        if (account_id == 0u)
            continue;

        std::string result;
        proto_wire::append_varint_field(result, 1u, account_id);
        proto_wire::append_varint_field(result, 6u, 0u);
        proto_wire::append_varint_field(result, 9u, 5u);
        proto_wire::append_varint_field(result, 10u, 5u);
        proto_wire::append_varint_field(result, 14u, 12000u);
        proto_wire::append_varint_field(result, 15u, 12000u);
        proto_wire::append_bytes_field(body, 6u, result);
    }

    return build_dota_job_reply_or_zero_header_payload(7451u, has_request_job, request_job_id, body, message);
}

std::uint64_t generate_dota_so_change_version(std::uint64_t lobby_id, std::uint64_t owner_id)
{
    static std::atomic<std::uint64_t> so_change_sequence{0};

    const std::uint64_t sequence = so_change_sequence.fetch_add(1, std::memory_order_relaxed) + 1u;
    const std::uint64_t owner_component = owner_id & 0xffull;
    const std::uint64_t version = lobby_id + 2781515ull + (sequence << 16) + owner_component;
    return version != 0 ? version : sequence;
}

std::uint64_t generate_dota_lobby_invite_gid(std::uint64_t lobby_id, std::uint64_t invitee_steam_id, std::uint64_t &cache_version)
{
    cache_version = generate_dota_so_change_version(lobby_id, invitee_steam_id);
    return cache_version > 2ull ? cache_version - 2ull : cache_version;
}

bool build_dota_remove_lobby_invite_payload(std::uint64_t lobby_id, std::uint64_t owner_steam_id, std::string &message)
{
    std::string invite_key;
    proto_wire::append_varint_field(invite_key, 1u, lobby_id);

    std::string removed_object;
    proto_wire::append_varint_field(removed_object, 1u, 2011u);
    proto_wire::append_bytes_field(removed_object, 2u, invite_key);

    std::string owner_soid;
    proto_wire::append_varint_field(owner_soid, 1u, 4u);
    proto_wire::append_varint_field(owner_soid, 2u, owner_steam_id);

    std::string body;
    proto_wire::append_fixed64_field(body, 3u, generate_dota_so_change_version(lobby_id, owner_steam_id));
    proto_wire::append_bytes_field(body, 5u, removed_object);
    proto_wire::append_bytes_field(body, 6u, owner_soid);
    return build_dota_zero_header_payload(kDotaPracticeLobbyDetailsUpdate, body, message);
}

std::string build_dota_practice_lobby_list_entry_body(
    std::uint64_t lobby_id,
    std::uint32_t account_id,
    const std::string &player_name,
    const std::string &room_name,
    std::uint32_t game_mode,
    std::uint32_t server_region,
    bool requires_pass_key,
    std::uint32_t player_count,
    std::uint32_t max_player_count,
    const std::string &lan_host_ping_location)
{
    std::string member;
    proto_wire::append_varint_field(member, 1u, account_id);
    proto_wire::append_bytes_field(member, 2u, player_name);

    std::string entry;
    proto_wire::append_varint_field(entry, 1u, lobby_id);
    proto_wire::append_bytes_field(entry, 5u, member);
    proto_wire::append_varint_field(entry, 6u, requires_pass_key ? 1u : 0u);
    proto_wire::append_varint_field(entry, 7u, account_id);
    proto_wire::append_bytes_field(entry, 10u, room_name.empty() ? std::string("Lobby") : room_name);
    proto_wire::append_varint_field(entry, 12u, game_mode);
    proto_wire::append_varint_field(entry, 13u, 1u);
    proto_wire::append_varint_field(entry, 14u, player_count == 0u ? 1u : player_count);
    proto_wire::append_varint_field(entry, 16u, max_player_count == 0u ? 10u : max_player_count);
    proto_wire::append_varint_field(entry, 17u, server_region);
    if (!lan_host_ping_location.empty())
        proto_wire::append_bytes_field(entry, 20u, lan_host_ping_location);
    proto_wire::append_varint_field(entry, 21u, 1u);
    proto_wire::append_varint_field(entry, 22u, 0u);
    return entry;
}

bool build_dota_lobby_list_response_payload(const std::vector<std::string> &entries, std::string &message)
{
    std::string body;
    proto_wire::append_fixed64_field(body, 11u, std::numeric_limits<std::uint64_t>::max());
    for (const std::string &entry : entries)
        proto_wire::append_bytes_field(body, 1u, entry);
    return build_dota_zero_header_payload(kDotaLobbyListResponse, body, message);
}

bool build_dota_lobby_list_response_payload(const std::string &entry, bool include_entry, std::string &message)
{
    std::vector<std::string> entries;
    if (include_entry)
        entries.push_back(entry);
    return build_dota_lobby_list_response_payload(entries, message);
}

bool build_dota_friend_practice_lobby_list_response_payload(const std::vector<std::string> &entries, std::string &message)
{
    std::string body;
    for (const std::string &entry : entries)
        proto_wire::append_bytes_field(body, 1u, entry);
    return build_dota_zero_header_payload(kDotaFriendPracticeLobbyListResponse, body, message);
}

bool build_dota_friend_practice_lobby_list_response_payload(const std::string &entry, bool include_entry, std::string &message)
{
    std::vector<std::string> entries;
    if (include_entry)
        entries.push_back(entry);
    return build_dota_friend_practice_lobby_list_response_payload(entries, message);
}

bool build_dota_custom_game_info_response_payload(std::uint64_t custom_game_id, bool has_request_job, std::uint64_t request_job_id, std::string &message)
{
    std::string body;
    if (custom_game_id != 0)
        proto_wire::append_varint_field(body, 1u, custom_game_id);
    return build_dota_job_reply_or_zero_header_payload(kDotaCustomGameInfoResponse, has_request_job, request_job_id, body, message);
}

std::string build_dota_custom_lobby_list_entry_body(std::uint64_t lobby_id, std::uint32_t account_id, const std::string &player_name, bool requires_pass_key, const std::string &lan_host_ping_location)
{
    std::string member;
    proto_wire::append_varint_field(member, 1u, account_id);
    proto_wire::append_bytes_field(member, 2u, player_name.empty() ? std::string("Lobby Host") : player_name);

    std::string entry;
    proto_wire::append_fixed64_field(entry, 1u, lobby_id);
    proto_wire::append_bytes_field(entry, 5u, member);
    proto_wire::append_varint_field(entry, 6u, requires_pass_key ? 1u : 0u);
    proto_wire::append_varint_field(entry, 7u, account_id);
    proto_wire::append_varint_field(entry, 17u, 0u);
    proto_wire::append_bytes_field(entry, 20u, lan_host_ping_location);
    return entry;
}

bool build_dota_custom_lobby_list_response_payload(std::uint64_t request_list_job_id, const std::vector<std::string> &entries, std::string &message)
{
    std::string body;
    proto_wire::append_fixed64_field(body, 11u, request_list_job_id);
    for (const std::string &entry : entries)
        proto_wire::append_bytes_field(body, 2u, entry);
    return build_dota_zero_header_payload(kDotaCustomLobbyListResponse, body, message);
}

} // namespace gbe::gc_message

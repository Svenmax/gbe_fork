#include "gbe_proto_wire.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace gbe::proto_wire {

bool read_varuint(const std::vector<std::uint8_t> &bytes, std::size_t &offset, std::uint64_t &value, std::size_t *raw_size)
{
    value = 0;
    const std::size_t begin = offset;
    for (std::uint32_t shift = 0; shift < 64 && offset < bytes.size(); shift += 7) {
        const std::uint8_t byte = bytes[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            if (raw_size)
                *raw_size = offset - begin;
            return true;
        }
    }
    return false;
}

bool read_varuint(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint64_t &value, std::size_t *raw_begin, std::size_t *raw_end)
{
    if (!data || offset >= size)
        return false;

    const std::size_t begin = offset;
    value = 0;
    for (std::uint32_t shift = 0; shift < 64 && offset < size; shift += 7) {
        const std::uint8_t byte = data[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            if (raw_begin)
                *raw_begin = begin;
            if (raw_end)
                *raw_end = offset;
            return true;
        }
    }
    return false;
}

std::uint64_t read_little_endian(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < size && offset + index < bytes.size() && index < sizeof(value); ++index)
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
    return value;
}

std::vector<Field> parse_fields(const std::vector<std::uint8_t> &bytes)
{
    std::vector<Field> fields;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        Field field{};
        if (!read_next_field(bytes.data(), bytes.size(), offset, field))
            return fields;
        fields.push_back(field);
    }
    return fields;
}

bool read_next_field(const std::uint8_t *data, std::size_t size, std::size_t &offset, Field &field, std::size_t *field_offset, std::size_t *field_end)
{
    if (!data || offset >= size)
        return false;

    const std::size_t start = offset;
    std::uint64_t tag = 0;
    if (!read_varuint(data, size, offset, tag) || tag == 0)
        return false;

    field = {};
    field.number = static_cast<std::uint32_t>(tag >> 3);
    field.wire_type = static_cast<std::uint32_t>(tag & 0x07);
    field.value_offset = offset;

    switch (field.wire_type) {
        case 0: {
            std::size_t raw_begin = offset;
            std::size_t raw_end = offset;
            std::uint64_t ignored = 0;
            if (!read_varuint(data, size, offset, ignored, &raw_begin, &raw_end))
                return false;
            field.value_offset = raw_begin;
            field.value_size = raw_end - raw_begin;
            break;
        }
        case 1:
            if (size - offset < 8)
                return false;
            field.value_size = 8;
            offset += 8;
            break;
        case 2: {
            std::uint64_t length = 0;
            if (!read_varuint(data, size, offset, length))
                return false;
            if (length > size - offset)
                return false;
            field.value_offset = offset;
            field.value_size = static_cast<std::size_t>(length);
            offset += static_cast<std::size_t>(length);
            break;
        }
        case 5:
            if (size - offset < 4)
                return false;
            field.value_size = 4;
            offset += 4;
            break;
        default:
            return false;
    }

    if (field_offset)
        *field_offset = start;
    if (field_end)
        *field_end = offset;
    return true;
}

bool find_field(const std::uint8_t *data, std::size_t size, std::uint32_t wanted_field, Field &field)
{
    std::size_t offset = 0;
    while (offset < size) {
        Field current{};
        if (!read_next_field(data, size, offset, current))
            return false;
        if (current.number == wanted_field) {
            field = current;
            return true;
        }
    }
    return false;
}

bool read_field_uint64(const std::uint8_t *data, std::size_t size, const Field &field, std::uint64_t &value)
{
    if (!data || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    if (field.wire_type == 0) {
        std::size_t offset = field.value_offset;
        return read_varuint(data, size, offset, value);
    }

    if (field.wire_type == 1 && field.value_size == sizeof(value)) {
        std::memcpy(&value, data + field.value_offset, sizeof(value));
        return true;
    }

    return false;
}

bool read_field_uint32(const std::uint8_t *data, std::size_t size, const Field &field, std::uint32_t &value)
{
    if (!data || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    if (field.wire_type == 0) {
        std::uint64_t wide_value = 0;
        if (!read_field_uint64(data, size, field, wide_value))
            return false;
        value = static_cast<std::uint32_t>(wide_value > 0xffffffffull ? 0xffffffffu : wide_value);
        return true;
    }

    if (field.wire_type == 5 && field.value_size == sizeof(value)) {
        std::memcpy(&value, data + field.value_offset, sizeof(value));
        return true;
    }

    return false;
}

bool read_field_bytes(const std::uint8_t *data, std::size_t size, const Field &field, std::string &value)
{
    if (!data || field.wire_type != 2 || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    value.assign(reinterpret_cast<const char *>(data + field.value_offset), field.value_size);
    return true;
}

bool read_uint64_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::uint64_t &value)
{
    Field field{};
    return find_field(data, size, field_number, field) && read_field_uint64(data, size, field, value);
}

bool read_uint32_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::uint32_t &value)
{
    Field field{};
    return find_field(data, size, field_number, field) && read_field_uint32(data, size, field, value);
}

bool read_bytes_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::string &value)
{
    Field field{};
    return find_field(data, size, field_number, field) && read_field_bytes(data, size, field, value);
}

DotaEmptyRequestShape parse_dota_empty_request_shape(const std::uint8_t *data, std::size_t size)
{
    DotaEmptyRequestShape shape{};
    shape.valid = true;
    if (!data || size == 0)
        return shape;

    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field)) {
            shape.valid = false;
            break;
        }
        ++shape.field_count;
    }

    return shape;
}

DotaRankRequestShape parse_dota_rank_request_shape(const std::uint8_t *data, std::size_t size)
{
    DotaRankRequestShape shape{};
    shape.valid = true;
    if (!data || size == 0)
        return shape;

    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field)) {
            shape.valid = false;
            break;
        }

        ++shape.field_count;
        if (field.number == 1u && read_field_uint32(data, size, field, shape.rank_type))
            shape.has_rank_type = true;
    }

    return shape;
}

Dota7070ReadyUpRequest parse_dota7070_ready_up_request(const std::uint8_t *data, std::size_t size)
{
    Dota7070ReadyUpRequest request{};
    if (read_uint32_field(data, size, 1u, request.ready_state))
        request.has_ready_state = true;
    return request;
}

Dota8052StartedLoadingRequest parse_dota8052_started_loading_request(const std::uint8_t *data, std::size_t size)
{
    Dota8052StartedLoadingRequest request{};
    if (read_uint64_field(data, size, 1u, request.lobby_id))
        request.has_lobby_id = true;
    if (read_uint64_field(data, size, 2u, request.custom_game_id))
        request.has_custom_game_id = true;
    if (read_uint64_field(data, size, 4u, request.start_time))
        request.has_start_time = true;
    return request;
}

Dota8053FinishedLoadingRequest parse_dota8053_finished_loading_request(const std::uint8_t *data, std::size_t size)
{
    Dota8053FinishedLoadingRequest request{};
    if (read_uint64_field(data, size, 1u, request.lobby_id))
        request.has_lobby_id = true;
    if (read_uint64_field(data, size, 2u, request.loading_duration))
        request.has_loading_duration = true;
    if (read_uint64_field(data, size, 3u, request.result_code))
        request.has_result_code = true;
    if (read_bytes_field(data, size, 4u, request.result_text))
        request.has_result_text = true;
    if (read_uint64_field(data, size, 5u, request.signon_states))
        request.has_signon_states = true;
    request.result_text = sanitize_proto_log_string(request.result_text);
    return request;
}

Dota8053Result parse_dota8053_result(const std::uint8_t *data, std::size_t size)
{
    const Dota8053FinishedLoadingRequest request = parse_dota8053_finished_loading_request(data, size);
    Dota8053Result result{};
    result.lobby_id = request.lobby_id;
    result.loading_duration = request.loading_duration;
    result.result_code = request.result_code;
    result.signon_states = request.signon_states;
    result.result_text = request.result_text;
    return result;
}

Dota7034RequestShape parse_dota7034_request_shape(const std::uint8_t *data, std::size_t size)
{
    Dota7034RequestShape shape{};
    if (!data || size == 0)
        return shape;

    if (read_uint32_field(data, size, 2u, shape.game_state))
        shape.has_game_state = true;
    if (read_uint32_field(data, size, 8u, shape.send_reason))
        shape.has_send_reason = true;
    if (read_uint32_field(data, size, 6u, shape.first_blood_happened))
        shape.has_first_blood_happened = true;
    if (read_uint32_field(data, size, 11u, shape.radiant_kills))
        shape.has_radiant_kills = true;
    if (read_uint32_field(data, size, 12u, shape.dire_kills))
        shape.has_dire_kills = true;
    if (read_uint32_field(data, size, 14u, shape.radiant_lead))
        shape.has_radiant_lead = true;
    if (read_uint32_field(data, size, 15u, shape.building_state))
        shape.has_building_state = true;

    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field))
            break;
        if (field.wire_type != 2u || field.value_offset > size || field.value_size > size - field.value_offset)
            continue;

        const std::uint8_t *nested_data = data + field.value_offset;
        const std::size_t nested_size = field.value_size;
        if (field.number == 1u) {
            Dota7034ConnectedPlayer player{};
            if (read_uint64_field(nested_data, nested_size, 1u, player.steam_id))
                player.has_steam_id = true;
            if (read_uint32_field(nested_data, nested_size, 2u, player.hero_id))
                player.has_hero_id = true;
            shape.has_connected_player = true;
            shape.connected_players.push_back(player);
        } else if (field.number == 7u) {
            Dota7034DisconnectedPlayer player{};
            if (read_uint64_field(nested_data, nested_size, 1u, player.steam_id))
                player.has_steam_id = true;

            std::string leaver_state_raw;
            if (read_bytes_field(nested_data, nested_size, 3u, leaver_state_raw) && !leaver_state_raw.empty()) {
                const std::uint8_t *leaver_state_data = reinterpret_cast<const std::uint8_t *>(leaver_state_raw.data());
                const std::size_t leaver_state_size = leaver_state_raw.size();
                if (read_uint32_field(leaver_state_data, leaver_state_size, 1u, player.lobby_state))
                    player.has_lobby_state = true;
                if (read_uint32_field(leaver_state_data, leaver_state_size, 2u, player.game_state))
                    player.has_game_state = true;
            }

            shape.has_disconnected_player = true;
            shape.disconnected_players.push_back(player);
        }
    }

    std::string draft_raw;
    if (read_bytes_field(data, size, 16u, draft_raw) && !draft_raw.empty()) {
        shape.has_draft = true;
        const std::uint8_t *draft_data = reinterpret_cast<const std::uint8_t *>(draft_raw.data());
        const std::size_t draft_size = draft_raw.size();
        if (read_uint64_field(draft_data, draft_size, 1u, shape.draft_steam_id))
            shape.has_draft_steam_id = true;
        if (read_uint32_field(draft_data, draft_size, 2u, shape.draft_team))
            shape.has_draft_team = true;
        if (read_uint32_field(draft_data, draft_size, 3u, shape.draft_team_slot))
            shape.has_draft_team_slot = true;
    }

    return shape;
}

Dota7034RuntimeRequest parse_dota7034_runtime_request(const std::uint8_t *data, std::size_t size)
{
    const Dota7034RequestShape shape = parse_dota7034_request_shape(data, size);
    Dota7034RuntimeRequest request{};
    request.game_state = shape.game_state;
    request.send_reason = shape.send_reason;
    request.first_blood_happened = shape.first_blood_happened;
    request.radiant_kills = shape.radiant_kills;
    request.dire_kills = shape.dire_kills;
    request.radiant_lead = shape.radiant_lead;
    request.building_state = shape.building_state;
    request.draft_steam_id = shape.draft_steam_id;
    request.draft_team = shape.draft_team;
    request.draft_team_slot = shape.draft_team_slot;
    request.connected_players = shape.connected_players;
    request.disconnected_players = shape.disconnected_players;
    request.has_game_state = shape.has_game_state;
    request.has_send_reason = shape.has_send_reason;
    request.has_first_blood_happened = shape.has_first_blood_happened;
    request.has_radiant_kills = shape.has_radiant_kills;
    request.has_dire_kills = shape.has_dire_kills;
    request.has_radiant_lead = shape.has_radiant_lead;
    request.has_building_state = shape.has_building_state;
    request.has_connected_player = shape.has_connected_player;
    request.has_draft = shape.has_draft;
    request.has_draft_steam_id = shape.has_draft_steam_id;
    request.has_draft_team = shape.has_draft_team;
    request.has_draft_team_slot = shape.has_draft_team_slot;
    request.has_disconnected_player = shape.has_disconnected_player;
    return request;
}

bool parse_dota_practice_lobby_set_team_slot_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbySetTeamSlotRequest &request)
{
    request = {};

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_team = true;
        request.team = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 2u, value)) {
        request.has_slot = true;
        request.slot = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 3u, value)) {
        request.has_bot_difficulty = true;
        request.bot_difficulty = static_cast<std::uint32_t>(value);
    }

    return request.has_team || request.has_slot || request.has_bot_difficulty;
}

bool parse_dota_practice_lobby_kick_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyKickRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (!read_uint64_field(data, size, 3u, value))
        return false;

    request.has_account_id = true;
    request.account_id = static_cast<std::uint32_t>(value);
    return true;
}

bool parse_dota_practice_lobby_join_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyJoinRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (read_bytes_field(data, size, 3u, request.pass_key))
        request.has_pass_key = true;

    return request.has_lobby_id || request.has_pass_key;
}

bool parse_dota_invite_to_lobby_body(const std::uint8_t *data, std::size_t size, DotaInviteToLobbyRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_steam_id = true;
        request.steam_id = value;
    }

    if (read_uint64_field(data, size, 2u, value)) {
        request.has_client_version = true;
        request.client_version = static_cast<std::uint32_t>(value);
    }

    return request.has_steam_id;
}

bool parse_dota_lobby_invite_response_body(const std::uint8_t *data, std::size_t size, DotaLobbyInviteResponseRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (read_uint64_field(data, size, 2u, value)) {
        request.has_accept = true;
        request.accept = value != 0;
    }

    if (read_uint64_field(data, size, 3u, value)) {
        request.has_client_version = true;
        request.client_version = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 6u, value)) {
        request.has_custom_game_crc = true;
        request.custom_game_crc = value;
    }

    if (read_uint64_field(data, size, 7u, value)) {
        request.has_custom_game_timestamp = true;
        request.custom_game_timestamp = static_cast<std::uint32_t>(value);
    }

    return request.has_lobby_id || request.has_accept;
}

bool parse_dota_practice_lobby_join_broadcast_channel_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_channel = true;
        request.channel = static_cast<std::uint32_t>(value);
    }

    if (read_bytes_field(data, size, 2u, request.description))
        request.has_description = true;
    if (read_bytes_field(data, size, 3u, request.country_code))
        request.has_country_code = true;
    if (read_bytes_field(data, size, 4u, request.language_code))
        request.has_language_code = true;

    return request.has_channel;
}

bool parse_dota_lobby_update_broadcast_channel_info_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_channel = true;
        request.channel = static_cast<std::uint32_t>(value);
    }

    if (read_bytes_field(data, size, 2u, request.country_code))
        request.has_country_code = true;
    if (read_bytes_field(data, size, 3u, request.description))
        request.has_description = true;
    if (read_bytes_field(data, size, 4u, request.language_code))
        request.has_language_code = true;

    return request.has_channel;
}

bool parse_dota_practice_lobby_close_broadcast_channel_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    std::uint64_t value = 0;
    if (!read_uint64_field(data, size, 1u, value))
        return false;

    request.has_channel = true;
    request.channel = static_cast<std::uint32_t>(value);
    return true;
}

bool parse_dota_practice_lobby_set_details_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyDetailsRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (read_bytes_field(data, size, 2u, request.room_name))
        request.has_room_name = true;

    if (read_uint64_field(data, size, 4u, value)) {
        request.has_server_region = true;
        request.server_region = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 25u, value)) {
        request.has_lan = true;
        request.lan = (value != 0);
    }

    if (read_bytes_field(data, size, 48u, request.lan_host_ping_location))
        request.has_lan_host_ping_location = true;

    if (read_uint64_field(data, size, 5u, value)) {
        request.has_game_mode = true;
        request.game_mode = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 9u, value)) {
        request.has_bot_difficulty_radiant = true;
        request.bot_difficulty_radiant = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 10u, value)) {
        request.has_allow_cheats = true;
        request.allow_cheats = (value != 0);
    }

    if (read_uint64_field(data, size, 11u, value)) {
        request.has_fill_with_bots = true;
        request.fill_with_bots = (value != 0);
    }

    if (read_uint64_field(data, size, 13u, value)) {
        request.has_allow_spectating = true;
        request.allow_spectating = (value != 0);
    }

    if (read_bytes_field(data, size, 15u, request.pass_key))
        request.has_pass_key = true;

    if (read_uint64_field(data, size, 33u, value)) {
        request.has_visibility = true;
        request.visibility = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 43u, value)) {
        request.has_bot_difficulty_dire = true;
        request.bot_difficulty_dire = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 44u, value)) {
        request.has_bot_radiant = true;
        request.bot_radiant = value;
    }

    if (read_uint64_field(data, size, 45u, value)) {
        request.has_bot_dire = true;
        request.bot_dire = value;
    }

    if (read_bytes_field(data, size, 26u, request.custom_game_mode))
        request.has_custom_game_mode = true;

    if (read_bytes_field(data, size, 27u, request.custom_map_name))
        request.has_custom_map_name = true;

    if (read_uint64_field(data, size, 28u, value)) {
        request.has_custom_difficulty = true;
        request.custom_difficulty = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 29u, value)) {
        request.has_custom_game_id = true;
        request.custom_game_id = value;
    }

    if (read_uint64_field(data, size, 30u, value)) {
        request.has_custom_min_players = true;
        request.custom_min_players = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 31u, value)) {
        request.has_custom_max_players = true;
        request.custom_max_players = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 34u, value)) {
        request.has_custom_game_crc = true;
        request.custom_game_crc = value;
    }

    std::uint32_t fixed32_value = 0;
    if (read_uint32_field(data, size, 37u, fixed32_value)) {
        request.has_custom_game_timestamp = true;
        request.custom_game_timestamp = fixed32_value;
    }

    if (read_uint64_field(data, size, 47u, value)) {
        request.has_custom_game_penalties = true;
        request.custom_game_penalties = (value != 0);
    }

    return request.has_lobby_id || request.has_room_name || request.has_server_region || request.has_lan ||
        request.has_lan_host_ping_location || request.has_game_mode || request.has_bot_difficulty_radiant ||
        request.has_allow_cheats || request.has_fill_with_bots || request.has_allow_spectating || request.has_pass_key ||
        request.has_visibility || request.has_bot_difficulty_dire || request.has_bot_radiant || request.has_bot_dire ||
        request.has_custom_game_mode || request.has_custom_map_name || request.has_custom_difficulty || request.has_custom_game_id ||
        request.has_custom_min_players || request.has_custom_max_players || request.has_custom_game_crc ||
        request.has_custom_game_timestamp || request.has_custom_game_penalties;
}

bool parse_dota_practice_lobby_create_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyCreateRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    if (read_bytes_field(data, size, 5u, request.pass_key))
        request.has_pass_key = true;

    std::string lobby_details_raw;
    if (read_bytes_field(data, size, 7u, lobby_details_raw)) {
        request.has_lobby_details = parse_dota_practice_lobby_set_details_body(
            reinterpret_cast<const std::uint8_t *>(lobby_details_raw.data()),
            lobby_details_raw.size(),
            request.lobby_details);
    }

    return request.has_lobby_details || request.has_pass_key;
}

bool parse_dota_join_chat_channel_body(const std::uint8_t *data, std::size_t size, DotaJoinChatChannelRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_bytes_field(data, size, 2u, request.channel_name))
        request.has_channel_name = true;

    if (read_uint64_field(data, size, 4u, value)) {
        request.has_channel_type = true;
        request.channel_type = static_cast<std::uint32_t>(value);
    }

    return request.has_channel_name;
}

bool parse_dota_leave_chat_channel_body(const std::uint8_t *data, std::size_t size, DotaLeaveChatChannelRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (!read_uint64_field(data, size, 1u, value))
        return false;

    request.has_channel_id = true;
    request.channel_id = value;
    return true;
}

bool parse_dota_chat_message_body(const std::uint8_t *data, std::size_t size, DotaChatMessageRequest &request)
{
    request = {};
    if (!data || size == 0)
        return false;

    std::uint64_t value = 0;
    if (read_uint64_field(data, size, 1u, value)) {
        request.has_account_id = true;
        request.account_id = static_cast<std::uint32_t>(value);
    }

    if (read_uint64_field(data, size, 2u, value)) {
        request.has_channel_id = true;
        request.channel_id = value;
    }

    if (read_bytes_field(data, size, 3u, request.persona_name))
        request.has_persona_name = true;

    if (read_bytes_field(data, size, 4u, request.text))
        request.has_text = true;

    return request.has_channel_id && request.has_text;
}

bool extract_packed_uint32_field(const std::uint8_t *data, std::size_t size, const Field &field, std::vector<std::uint32_t> &values)
{
    values.clear();
    if (!data || field.wire_type != 2 || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    std::size_t offset = field.value_offset;
    const std::size_t end = field.value_offset + field.value_size;
    while (offset < end) {
        std::uint64_t value = 0;
        if (!read_varuint(data, end, offset, value))
            return false;
        values.push_back(static_cast<std::uint32_t>(value > 0xffffffffull ? 0xffffffffu : value));
    }

    return true;
}

void append_varuint(std::string &buffer, std::uint64_t value)
{
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7f);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buffer.push_back(static_cast<char>(byte));
    } while (value != 0);
}

void append_little_endian32(std::string &buffer, std::uint32_t value)
{
    for (int index = 0; index < 4; ++index)
        buffer.push_back(static_cast<char>((value >> (index * 8)) & 0xffu));
}

void append_little_endian64(std::string &buffer, std::uint64_t value)
{
    for (int index = 0; index < 8; ++index)
        buffer.push_back(static_cast<char>((value >> (index * 8)) & 0xffu));
}

void append_varint_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 0u);
    append_varuint(buffer, value);
}

void append_fixed64_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 1u);
    append_little_endian64(buffer, value);
}

void append_bytes_field(std::string &buffer, std::uint32_t field_number, const std::string &value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 2u);
    append_varuint(buffer, static_cast<std::uint64_t>(value.size()));
    buffer.append(value);
}

void append_fixed32_field(std::string &buffer, std::uint32_t field_number, std::uint32_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 5u);
    append_little_endian32(buffer, value);
}

bool rewrite_varint_fields(const std::string &input, const std::vector<std::uint32_t> &field_numbers, std::uint64_t value, std::string &output, bool *rewrote)
{
    output.clear();
    if (rewrote)
        *rewrote = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.wire_type == 0u && std::find(field_numbers.begin(), field_numbers.end(), field.number) != field_numbers.end()) {
            append_varint_field(output, field.number, value);
            if (rewrote)
                *rewrote = true;
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool rewrite_varint_bytes_recursive(
    const std::string &input,
    const std::vector<std::uint8_t> &old_encoded,
    std::uint32_t target_field_number,
    std::uint64_t new_value,
    std::string &output,
    std::size_t &replacement_count)
{
    output.clear();
    replacement_count = 0;

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.wire_type == 0u) {
            append_varuint(output, (static_cast<std::uint64_t>(field.number) << 3) | field.wire_type);

            const std::uint8_t *raw_value = reinterpret_cast<const std::uint8_t *>(input.data() + field.value_offset);
            if (field.number == target_field_number
                && field.value_size == old_encoded.size()
                && std::memcmp(raw_value, old_encoded.data(), field.value_size) == 0) {
                append_varuint(output, new_value);
                ++replacement_count;
            } else {
                output.append(input.data() + field.value_offset, field.value_size);
            }

            continue;
        }

        if (field.wire_type == 2u) {
            std::string nested_input(input.data() + field.value_offset, field.value_size);
            std::string nested_output;
            std::size_t nested_replacement_count = 0;
            if (rewrite_varint_bytes_recursive(
                    nested_input,
                    old_encoded,
                    target_field_number,
                    new_value,
                    nested_output,
                    nested_replacement_count)
                && nested_replacement_count > 0) {
                append_bytes_field(output, field.number, nested_output);
                replacement_count += nested_replacement_count;
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool rewrite_dota_account_bound_object_data(const std::string &input, int type_id, std::uint32_t account_id, std::string &output)
{
    std::string account_output;
    bool saw_account_id = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 0u) {
            saw_account_id = true;
            append_varint_field(account_output, 1u, account_id);
            continue;
        }

        account_output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_account_id)
        append_varint_field(account_output, 1u, account_id);

    if (type_id == 2012) {
        std::string plus_output;
        std::size_t ao_offset = 0;
        while (ao_offset < account_output.size()) {
            Field field{};
            std::size_t field_offset = 0;
            std::size_t field_end = 0;
            if (!read_next_field(reinterpret_cast<const std::uint8_t *>(account_output.data()), account_output.size(), ao_offset, field, &field_offset, &field_end))
                return false;
            if (field.number == 1u)
                plus_output.append(account_output.data() + field_offset, field_end - field_offset);
        }
        append_varint_field(plus_output, 2u, 1522540800u);
        append_varint_field(plus_output, 3u, 1u);
        append_varint_field(plus_output, 4u, 1u);
        append_varint_field(plus_output, 5u, 0u);
        append_varint_field(plus_output, 6u, 0u);
        append_fixed32_field(plus_output, 7u, 1893456000u);
        append_fixed64_field(plus_output, 8u, 0ull);
        output.swap(plus_output);
        return true;
    }

    if (type_id != 2002) {
        output.swap(account_output);
        return true;
    }

    bool rewrote_conduct_score = false;
    std::string conduct_output;
    if (!rewrite_varint_fields(account_output, { 72u }, 12000u, conduct_output, &rewrote_conduct_score))
        return false;
    if (!rewrote_conduct_score)
        append_varint_field(conduct_output, 72u, 12000u);

    return rewrite_varint_fields(conduct_output, { 18u, 20u, 21u, 38u, 39u, 41u, 42u, 48u, 86u, 89u, 105u, 122u }, 0u, output, nullptr);
}

bool patch_dota_lobby_template_identifiers(
    std::string &message,
    const std::vector<std::uint8_t> &old_lobby_id_varint,
    std::uint64_t lobby_id,
    bool require_lobby_id,
    const std::vector<std::uint8_t> &old_steam_id_fixed64,
    std::uint64_t steam_id,
    bool require_steam_id_fixed64,
    PatchTemplateIdentifierResult &result)
{
    result = {};

    std::vector<std::uint8_t> encoded_lobby_id;
    if (!encode_varuint_with_expected_size(lobby_id, old_lobby_id_varint.size(), encoded_lobby_id))
        return false;

    result.lobby_id_match_count = count_byte_pattern_matches(message, old_lobby_id_varint);
    if (result.lobby_id_match_count != 0) {
        if (!find_and_overwrite_bytes(message, old_lobby_id_varint, encoded_lobby_id))
            return false;
    } else if (require_lobby_id) {
        return false;
    }

    std::string steam_id_fixed64_raw;
    append_little_endian64(steam_id_fixed64_raw, steam_id);
    const std::vector<std::uint8_t> encoded_steam_id_fixed64(steam_id_fixed64_raw.begin(), steam_id_fixed64_raw.end());

    result.steam_id_fixed64_match_count = count_byte_pattern_matches(message, old_steam_id_fixed64);
    if (result.steam_id_fixed64_match_count != 0) {
        if (!find_and_overwrite_bytes(message, old_steam_id_fixed64, encoded_steam_id_fixed64))
            return false;
    } else if (require_steam_id_fixed64) {
        return false;
    }

    return true;
}

bool patch_fixed32_template_value(std::string &message, const std::vector<std::uint8_t> &old_fixed32, std::uint32_t value, std::size_t &match_count)
{
    match_count = count_byte_pattern_matches(message, old_fixed32);
    if (match_count == 0)
        return true;

    std::string encoded_raw;
    append_little_endian32(encoded_raw, value);
    return find_and_overwrite_bytes(message, old_fixed32, std::vector<std::uint8_t>(encoded_raw.begin(), encoded_raw.end()));
}

bool patch_varint_template_value(std::string &message, const std::vector<std::uint8_t> &old_varint, std::uint64_t value, std::size_t &match_count, bool &size_ok)
{
    match_count = 0;
    std::vector<std::uint8_t> encoded;
    size_ok = encode_varuint_with_expected_size(value, old_varint.size(), encoded);
    if (!size_ok)
        return false;

    match_count = count_byte_pattern_matches(message, old_varint);
    if (match_count == 0)
        return false;

    return find_and_overwrite_bytes(message, old_varint, encoded);
}

bool patch_dota_practice_lobby_launch_template(
    std::string &message,
    const std::vector<std::uint8_t> &old_steam_id_fixed64,
    std::uint64_t steam_id,
    const std::vector<std::uint8_t> &old_lobby_id_varint,
    std::uint64_t lobby_id,
    const std::vector<std::uint8_t> &old_match_id_varint,
    std::uint64_t match_id,
    bool patch_server_id,
    const std::vector<std::uint8_t> &old_server_id_fixed64,
    std::uint64_t server_id,
    bool patch_game_start_time,
    const std::vector<std::uint8_t> &old_game_start_time_varint,
    std::uint32_t game_start_time,
    bool patch_connect,
    const std::string &old_connect,
    const std::string &connect,
    DotaPracticeLobbyLaunchTemplatePatchResult &result)
{
    result = {};

    std::string steam_id_fixed64_raw;
    append_little_endian64(steam_id_fixed64_raw, steam_id);
    result.steam_id_fixed64_match_count = count_byte_pattern_matches(message, old_steam_id_fixed64);
    if (result.steam_id_fixed64_match_count != 0
        && !find_and_overwrite_bytes(message, old_steam_id_fixed64, std::vector<std::uint8_t>(steam_id_fixed64_raw.begin(), steam_id_fixed64_raw.end())))
        return false;

    std::vector<std::uint8_t> encoded_lobby_id;
    result.lobby_id_size_ok = encode_varuint_with_expected_size(lobby_id, old_lobby_id_varint.size(), encoded_lobby_id);
    if (result.lobby_id_size_ok) {
        result.lobby_id_match_count = count_byte_pattern_matches(message, old_lobby_id_varint);
        if (result.lobby_id_match_count != 0 && !find_and_overwrite_bytes(message, old_lobby_id_varint, encoded_lobby_id))
            return false;
    }

    std::vector<std::uint8_t> encoded_match_id;
    result.match_id_size_ok = encode_varuint_with_expected_size(match_id, old_match_id_varint.size(), encoded_match_id);
    if (result.match_id_size_ok) {
        result.match_id_match_count = count_byte_pattern_matches(message, old_match_id_varint);
        if (result.match_id_match_count != 0 && !find_and_overwrite_bytes(message, old_match_id_varint, encoded_match_id))
            return false;
    }

    if (patch_server_id) {
        std::string server_id_raw;
        append_little_endian64(server_id_raw, server_id);
        result.server_id_fixed64_match_count = count_byte_pattern_matches(message, old_server_id_fixed64);
        if (result.server_id_fixed64_match_count != 0
            && !find_and_overwrite_bytes(message, old_server_id_fixed64, std::vector<std::uint8_t>(server_id_raw.begin(), server_id_raw.end())))
            return false;
    }

    result.game_start_time_size_ok = true;
    if (patch_game_start_time) {
        std::vector<std::uint8_t> encoded_game_start_time;
        result.game_start_time_size_ok = encode_varuint_with_expected_size(game_start_time, old_game_start_time_varint.size(), encoded_game_start_time);
        if (result.game_start_time_size_ok) {
            result.game_start_time_match_count = count_byte_pattern_matches(message, old_game_start_time_varint);
            if (result.game_start_time_match_count != 0 && !find_and_overwrite_bytes(message, old_game_start_time_varint, encoded_game_start_time))
                return false;
        }
    }

    result.connect_size_ok = true;
    if (patch_connect) {
        result.connect_size_ok = connect.size() == old_connect.size();
        if (result.connect_size_ok) {
            result.connect_match_count = 0;
            std::size_t offset = 0;
            while ((offset = message.find(old_connect, offset)) != std::string::npos) {
                ++result.connect_match_count;
                offset += old_connect.size();
            }
            if (result.connect_match_count != 0 && !find_and_overwrite_string(message, old_connect, connect))
                return false;
        }
    }

    return true;
}

bool patch_dota_practice_lobby_peripheral_template(
    std::string &message,
    const std::vector<std::uint8_t> &old_steam_id_fixed64,
    const std::vector<std::uint8_t> &old_persona_steam_id_fixed64,
    std::uint64_t steam_id,
    bool patch_server_id,
    const std::vector<std::uint8_t> &old_server_id_fixed64,
    std::uint64_t server_id,
    const std::vector<std::string> &old_lobby_id_texts,
    std::uint64_t lobby_id,
    DotaPracticeLobbyPeripheralTemplatePatchResult &result)
{
    result = {};

    std::string steam_id_fixed64_raw;
    append_little_endian64(steam_id_fixed64_raw, steam_id);
    const std::vector<std::uint8_t> steam_id_fixed64_replacement(steam_id_fixed64_raw.begin(), steam_id_fixed64_raw.end());
    result.steam_id_fixed64_match_count = count_byte_pattern_matches(message, old_steam_id_fixed64)
        + count_byte_pattern_matches(message, old_persona_steam_id_fixed64);
    const bool patched_steam_id =
        find_and_overwrite_bytes(message, old_steam_id_fixed64, steam_id_fixed64_replacement) ||
        find_and_overwrite_bytes(message, old_persona_steam_id_fixed64, steam_id_fixed64_replacement);
    if (!patched_steam_id)
        return false;

    if (patch_server_id) {
        std::string server_id_raw;
        append_little_endian64(server_id_raw, server_id);
        const std::vector<std::uint8_t> server_id_replacement(server_id_raw.begin(), server_id_raw.end());
        result.server_id_fixed64_match_count = count_byte_pattern_matches(message, old_server_id_fixed64);
        if (!find_and_overwrite_bytes(message, old_server_id_fixed64, server_id_replacement))
            return false;
    }

    const std::string new_lobby_id_text = std::to_string(lobby_id);
    for (const std::string &old_lobby_id_text : old_lobby_id_texts) {
        if (old_lobby_id_text.size() != new_lobby_id_text.size())
            continue;
        std::size_t offset = 0;
        while ((offset = message.find(old_lobby_id_text, offset)) != std::string::npos) {
            ++result.lobby_id_text_match_count;
            offset += old_lobby_id_text.size();
        }
        find_and_overwrite_string(message, old_lobby_id_text, new_lobby_id_text);
    }

    return true;
}

std::string summarize_repeated_varint_field(const std::string &input, std::uint32_t target_field)
{
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::ostringstream stream;
    bool first = true;
    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(data, size, offset, field, &field_offset, &field_end))
            break;

        if (field.number == target_field && field.wire_type == 0u) {
            std::uint64_t value = 0;
            std::size_t field_value_offset = field.value_offset;
            if (read_varuint(data, size, field_value_offset, value)) {
                if (!first)
                    stream << ',';
                first = false;
                stream << value;
            }
        }
    }

    if (first)
        return "-";

    return stream.str();
}

std::uint32_t count_repeated_bytes_field(const std::string &input, std::uint32_t target_field)
{
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::uint32_t count = 0;
    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field))
            break;

        if (field.number == target_field && field.wire_type == 2u)
            ++count;
    }

    return count;
}

std::string format_field_layout_summary(const std::string &input)
{
    std::ostringstream stream;
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::size_t offset = 0;
    bool first = true;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field))
            break;

        if (!first)
            stream << ',';
        first = false;
        stream << field.number << ':' << field.wire_type << ':' << field.value_size;
    }

    return stream.str();
}

std::string format_top_level_field_summary(const std::uint8_t *data, std::size_t size)
{
    if (!data || size == 0)
        return "empty";

    std::ostringstream stream;
    bool first = true;
    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        if (!read_next_field(data, size, offset, field))
            break;

        if (!first)
            stream << ',';
        first = false;
        stream << field.number << ':' << field.wire_type << ':' << field.value_size;

        if (field.wire_type == 0u) {
            std::uint64_t value = 0;
            std::size_t field_value_offset = field.value_offset;
            if (read_varuint(data, size, field_value_offset, value))
                stream << '=' << value;
        } else if (field.wire_type == 1u && field.value_size == 8u) {
            std::uint64_t value = 0;
            std::memcpy(&value, data + field.value_offset, sizeof(value));
            stream << '=' << value;
        } else if (field.wire_type == 5u && field.value_size == 4u) {
            std::uint32_t value = 0;
            std::memcpy(&value, data + field.value_offset, sizeof(value));
            stream << '=' << value;
        }
    }

    if (first)
        return "empty";

    return stream.str();
}

std::string format_dota_lobby_member_state_summary(const std::string &input)
{
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::uint64_t steam_id = 0;
    std::uint32_t account_id = 0;
    std::uint32_t hero_id = 0;
    std::uint32_t team = 0;
    std::uint32_t slot = 0;
    std::uint32_t leaver_status = 0;
    std::uint32_t leaver_actions = 0;

    Field field{};
    const bool has_steam_id = find_field(data, size, 1u, field) && read_field_uint64(data, size, field, steam_id);
    const bool has_account_id = find_field(data, size, 55u, field) && read_field_uint32(data, size, field, account_id);
    const bool has_hero_id = find_field(data, size, 2u, field) && read_field_uint32(data, size, field, hero_id);
    const bool has_team = find_field(data, size, 3u, field) && read_field_uint32(data, size, field, team);
    const bool has_slot = find_field(data, size, 7u, field) && read_field_uint32(data, size, field, slot);
    const bool has_leaver_status = find_field(data, size, 16u, field) && read_field_uint32(data, size, field, leaver_status);
    const bool has_leaver_actions = find_field(data, size, 28u, field) && read_field_uint32(data, size, field, leaver_actions);

    char buffer[320];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu account_id=%u hero_id=%u team=%u slot=%u leaver_status=%u leaver_actions=%u flags[steam_id=%u account_id=%u hero_id=%u team=%u slot=%u leaver_status=%u leaver_actions=%u]",
        static_cast<unsigned long long>(steam_id),
        account_id,
        hero_id,
        team,
        slot,
        leaver_status,
        leaver_actions,
        has_steam_id ? 1u : 0u,
        has_account_id ? 1u : 0u,
        has_hero_id ? 1u : 0u,
        has_team ? 1u : 0u,
        has_slot ? 1u : 0u,
        has_leaver_status ? 1u : 0u,
        has_leaver_actions ? 1u : 0u);
    return std::string(buffer);
}

std::string format_dota_lobby_aux_field_summary(const std::string &input)
{
    std::ostringstream stream;
    stream << "121=[" << summarize_repeated_varint_field(input, 121u)
           << "] 122=[" << summarize_repeated_varint_field(input, 122u)
           << "] 123=[" << summarize_repeated_varint_field(input, 123u)
           << "] 124=[" << summarize_repeated_varint_field(input, 124u)
           << "] 132=[" << summarize_repeated_varint_field(input, 132u)
           << ']';
    return stream.str();
}

std::string format_dota_static_lobby_member_summary(const std::string &input)
{
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::string name;
    std::uint64_t party_id = 0;

    Field field{};
    const bool has_name = find_field(data, size, 1u, field) && read_field_bytes(data, size, field, name);
    const bool has_party_id = find_field(data, size, 2u, field) && read_field_uint64(data, size, field, party_id);

    char buffer[320];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "name=%s party_id=%llu flags[name=%u party_id=%u]",
        has_name ? name.c_str() : "",
        static_cast<unsigned long long>(party_id),
        has_name ? 1u : 0u,
        has_party_id ? 1u : 0u);
    return std::string(buffer);
}

std::string format_dota_server_static_lobby_member_summary(const std::string &input)
{
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();

    std::uint64_t steam_id = 0;
    std::uint32_t rank_tier = 0;
    std::uint32_t coach_rating = 0;
    std::uint32_t favorite_team_packed_lo = 0;

    Field field{};
    const bool has_steam_id = find_field(data, size, 1u, field) && read_field_uint64(data, size, field, steam_id);
    const bool has_rank_tier = find_field(data, size, 3u, field) && read_field_uint32(data, size, field, rank_tier);
    const bool has_coach_rating = find_field(data, size, 7u, field) && read_field_uint32(data, size, field, coach_rating);
    const bool has_favorite_team_packed = find_field(data, size, 12u, field) && read_field_uint32(data, size, field, favorite_team_packed_lo);

    char buffer[256];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu rank_tier=%u coach_rating=%u favorite_team_packed_lo=%u flags[steam_id=%u rank_tier=%u coach_rating=%u favorite_team_packed=%u]",
        static_cast<unsigned long long>(steam_id),
        rank_tier,
        coach_rating,
        favorite_team_packed_lo,
        has_steam_id ? 1u : 0u,
        has_rank_tier ? 1u : 0u,
        has_coach_rating ? 1u : 0u,
        has_favorite_team_packed ? 1u : 0u);
    return std::string(buffer);
}

std::string format_dota7034_leaver_state_summary(const std::string &input)
{
    std::uint32_t lobby_state = 0;
    std::uint32_t game_state = 0;
    std::uint32_t leaver_detected = 0;
    std::uint32_t first_blood_happened = 0;
    std::uint32_t discard_match_results = 0;
    std::uint32_t mass_disconnect = 0;

    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();
    Field field{};
    if (find_field(data, size, 1u, field))
        read_field_uint32(data, size, field, lobby_state);
    if (find_field(data, size, 2u, field))
        read_field_uint32(data, size, field, game_state);
    if (find_field(data, size, 3u, field))
        read_field_uint32(data, size, field, leaver_detected);
    if (find_field(data, size, 4u, field))
        read_field_uint32(data, size, field, first_blood_happened);
    if (find_field(data, size, 5u, field))
        read_field_uint32(data, size, field, discard_match_results);
    if (find_field(data, size, 6u, field))
        read_field_uint32(data, size, field, mass_disconnect);

    char buffer[192];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "lobby_state=%u game_state=%u leaver_detected=%u first_blood=%u discard=%u mass_disconnect=%u",
        lobby_state,
        game_state,
        leaver_detected,
        first_blood_happened,
        discard_match_results,
        mass_disconnect);
    return std::string(buffer);
}

std::string format_dota7034_player_summary(const std::string &input)
{
    std::uint64_t steam_id = 0;
    std::uint32_t hero_id = 0;
    std::uint32_t disconnect_reason = 0;
    std::string leaver_state_raw;

    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();
    Field field{};
    if (find_field(data, size, 1u, field))
        read_field_uint64(data, size, field, steam_id);
    if (find_field(data, size, 2u, field))
        read_field_uint32(data, size, field, hero_id);
    if (find_field(data, size, 4u, field))
        read_field_uint32(data, size, field, disconnect_reason);
    if (find_field(data, size, 3u, field))
        read_field_bytes(data, size, field, leaver_state_raw);

    std::ostringstream stream;
    stream << "steam_id=" << static_cast<unsigned long long>(steam_id)
           << " hero_id=" << hero_id
           << " disconnect_reason=" << disconnect_reason;
    if (!leaver_state_raw.empty())
        stream << " leaver_state{" << format_dota7034_leaver_state_summary(leaver_state_raw) << '}';
    return stream.str();
}

std::string format_dota7034_draft_summary(const std::string &input)
{
    std::uint64_t steam_id = 0;
    std::uint32_t team = 0;
    std::uint32_t team_slot = 0;

    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(input.data());
    const std::size_t size = input.size();
    Field field{};
    if (find_field(data, size, 1u, field))
        read_field_uint64(data, size, field, steam_id);
    if (find_field(data, size, 2u, field))
        read_field_uint32(data, size, field, team);
    if (find_field(data, size, 3u, field))
        read_field_uint32(data, size, field, team_slot);

    char buffer[128];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu team=%u team_slot=%u",
        static_cast<unsigned long long>(steam_id),
        team,
        team_slot);
    return std::string(buffer);
}

std::string format_dota7034_summary(const std::uint8_t *data, std::size_t size)
{
    if (!data || size == 0)
        return "empty";

    std::uint32_t game_state = 0;
    std::uint32_t send_reason = 0;
    std::uint32_t radiant_kills = 0;
    std::uint32_t dire_kills = 0;
    std::uint32_t radiant_lead = 0;
    std::uint32_t building_state = 0;
    std::uint32_t connected_count = 0;
    std::uint32_t disconnected_count = 0;
    std::uint32_t draft_count = 0;
    std::vector<std::string> connected_summaries;
    std::vector<std::string> disconnected_summaries;
    std::vector<std::string> draft_summaries;

    std::size_t offset = 0;
    while (offset < size) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(data, size, offset, field, &field_offset, &field_end))
            break;

        if (field.number == 1u && field.wire_type == 2u) {
            ++connected_count;
            if (connected_summaries.size() < 4u)
                connected_summaries.push_back(format_dota7034_player_summary(std::string(reinterpret_cast<const char *>(data + field.value_offset), field.value_size)));
            continue;
        }

        if (field.number == 7u && field.wire_type == 2u) {
            ++disconnected_count;
            if (disconnected_summaries.size() < 4u)
                disconnected_summaries.push_back(format_dota7034_player_summary(std::string(reinterpret_cast<const char *>(data + field.value_offset), field.value_size)));
            continue;
        }

        if (field.number == 16u && field.wire_type == 2u) {
            ++draft_count;
            if (draft_summaries.size() < 4u)
                draft_summaries.push_back(format_dota7034_draft_summary(std::string(reinterpret_cast<const char *>(data + field.value_offset), field.value_size)));
            continue;
        }

        if (field.number == 2u)
            read_field_uint32(data, size, field, game_state);
        else if (field.number == 8u)
            read_field_uint32(data, size, field, send_reason);
        else if (field.number == 11u)
            read_field_uint32(data, size, field, radiant_kills);
        else if (field.number == 12u)
            read_field_uint32(data, size, field, dire_kills);
        else if (field.number == 14u)
            read_field_uint32(data, size, field, radiant_lead);
        else if (field.number == 15u)
            read_field_uint32(data, size, field, building_state);
    }

    std::ostringstream stream;
    stream << "game_state=" << game_state
           << " send_reason=" << send_reason
           << " connected=" << connected_count
           << " disconnected=" << disconnected_count
           << " drafts=" << draft_count
           << " radiant_kills=" << radiant_kills
           << " dire_kills=" << dire_kills
           << " radiant_lead=" << radiant_lead
           << " building_state=" << building_state;
    for (std::size_t i = 0; i < connected_summaries.size(); ++i)
        stream << " connected" << i << "{" << connected_summaries[i] << '}';
    for (std::size_t i = 0; i < disconnected_summaries.size(); ++i)
        stream << " disconnected" << i << "{" << disconnected_summaries[i] << '}';
    for (std::size_t i = 0; i < draft_summaries.size(); ++i)
        stream << " draft" << i << "{" << draft_summaries[i] << '}';
    return stream.str();
}

std::uint64_t fnv1a64(const std::vector<std::uint8_t> &bytes)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex_prefix(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size, std::size_t max_bytes)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setfill('0');
    const std::size_t end = std::min(bytes.size(), offset + std::min(size, max_bytes));
    for (std::size_t index = offset; index < end; ++index)
        out << std::setw(2) << static_cast<unsigned>(bytes[index]);
    if (size > max_bytes)
        out << "...";
    return out.str();
}

std::string format_hex_prefix(const std::uint8_t *data, std::size_t size, std::size_t max_bytes)
{
    if (!data || size == 0 || max_bytes == 0)
        return std::string();

    const std::size_t bytes_to_dump = std::min(size, max_bytes);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes_to_dump; ++index) {
        if (index != 0)
            stream << ' ';
        stream << std::setw(2) << static_cast<unsigned int>(data[index]);
    }
    if (size > bytes_to_dump)
        stream << " ...";
    return stream.str();
}

std::string format_hex(const std::uint8_t *data, std::size_t size)
{
    if (!data || size == 0)
        return std::string();

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        if (index != 0)
            stream << ' ';
        stream << std::setw(2) << static_cast<unsigned int>(data[index]);
    }
    return stream.str();
}

std::string format_ipv4(std::uint32_t value)
{
    std::ostringstream stream;
    stream
        << ((value >> 24) & 0xFFu)
        << '.'
        << ((value >> 16) & 0xFFu)
        << '.'
        << ((value >> 8) & 0xFFu)
        << '.'
        << (value & 0xFFu);
    return stream.str();
}

std::string sanitize_proto_log_string(const std::string &value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    for (char ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        sanitized.push_back(byte >= 32u && byte <= 126u ? ch : '.');
    }
    return sanitized;
}

std::uint64_t parse_uint64_or_zero(const char *text)
{
    if (!text || !*text)
        return 0;

    char *end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (!end || *end != '\0')
        return 0;
    return static_cast<std::uint64_t>(value);
}

std::uint32_t parse_uint32_or_zero(const char *text)
{
    const std::uint64_t value = parse_uint64_or_zero(text);
    if (value > UINT32_MAX)
        return 0;
    return static_cast<std::uint32_t>(value);
}

bool dota_string_is_unsigned_integer(const std::string &value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

bool dota_is_readable_custom_game_name(const std::string &value)
{
    return !value.empty() && !dota_string_is_unsigned_integer(value) && value != "dota";
}

bool dota_is_rank_type_supported(std::uint32_t rank_type)
{
    switch (rank_type) {
        case 1u:
        case 2u:
        case 3u:
        case 4u:
        case 5u:
        case 6u:
        case 100u:
        case 101u:
            return true;
        default:
            return false;
    }
}

bool dota_is_dire_team(std::uint32_t team)
{
    return team == 1u || team == 3u;
}

bool dota8053_indicates_load_failure(std::uint64_t result_code, const std::string &result_text)
{
    if (result_text.empty())
        return false;
    if (result_text.find("#GameUI_Disconnect") != std::string::npos)
        return true;
    return result_code != 1u;
}

bool is_dota_practice_lobby_prelaunch_state(std::uint64_t server_id, std::uint64_t match_id, std::uint32_t game_start_time, const std::string &connect)
{
    return server_id == 0ull && match_id == 0ull && game_start_time == 0u && connect.empty();
}

bool decode_hex_string(const char *hex, std::string &decoded)
{
    decoded.clear();
    if (!hex)
        return false;

    int high_nibble = -1;
    for (const char *cursor = hex; *cursor != '\0'; ++cursor) {
        const unsigned char ch = static_cast<unsigned char>(*cursor);
        if (std::isspace(ch))
            continue;

        int value = -1;
        if (ch >= '0' && ch <= '9')
            value = ch - '0';
        else if (ch >= 'a' && ch <= 'f')
            value = 10 + (ch - 'a');
        else if (ch >= 'A' && ch <= 'F')
            value = 10 + (ch - 'A');
        else
            return false;

        if (high_nibble < 0) {
            high_nibble = value;
            continue;
        }

        decoded.push_back(static_cast<char>((high_nibble << 4) | value));
        high_nibble = -1;
    }

    return high_nibble < 0;
}

bool find_and_overwrite_bytes(std::string &buffer, const std::vector<std::uint8_t> &needle, const std::vector<std::uint8_t> &replacement)
{
    if (needle.empty() || needle.size() != replacement.size())
        return false;

    bool replaced = false;
    for (std::size_t offset = 0; offset + needle.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < needle.size(); ++index) {
            if (static_cast<std::uint8_t>(buffer[offset + index]) != needle[index]) {
                matched = false;
                break;
            }
        }

        if (!matched)
            continue;

        for (std::size_t index = 0; index < replacement.size(); ++index)
            buffer[offset + index] = static_cast<char>(replacement[index]);

        offset += replacement.size() - 1;
        replaced = true;
    }

    return replaced;
}

bool find_and_overwrite_string(std::string &buffer, const std::string &needle, const std::string &replacement)
{
    if (needle.empty() || needle.size() != replacement.size())
        return false;

    bool replaced = false;
    std::size_t offset = 0;
    while ((offset = buffer.find(needle, offset)) != std::string::npos) {
        buffer.replace(offset, replacement.size(), replacement);
        offset += replacement.size();
        replaced = true;
    }

    return replaced;
}

bool encode_varuint_with_expected_size(std::uint64_t value, std::size_t expected_size, std::vector<std::uint8_t> &encoded)
{
    std::string encoded_raw;
    append_varuint(encoded_raw, value);
    if (encoded_raw.size() != expected_size)
        return false;

    encoded.assign(encoded_raw.begin(), encoded_raw.end());
    return true;
}

std::vector<std::uint8_t> vector_from_bytes(const std::uint8_t *data, std::size_t size)
{
    return std::vector<std::uint8_t>(data, data + size);
}

std::size_t count_byte_pattern_matches(const std::string &buffer, const std::vector<std::uint8_t> &needle)
{
    if (needle.empty())
        return 0;

    std::size_t matches = 0;
    for (std::size_t offset = 0; offset + needle.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < needle.size(); ++index) {
            if (static_cast<std::uint8_t>(buffer[offset + index]) != needle[index]) {
                matched = false;
                break;
            }
        }

        if (!matched)
            continue;

        ++matches;
        offset += needle.size() - 1;
    }

    return matches;
}

std::string normalize_dota_practice_lobby_connect(const std::string &connect)
{
    const std::size_t first = connect.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    const std::size_t last = connect.find_first_of(" \t\r\n", first);
    return connect.substr(first, last == std::string::npos ? std::string::npos : last - first);
}

std::string normalize_dota_practice_lobby_connect_pair(const std::string &connect)
{
    const std::string first_endpoint = normalize_dota_practice_lobby_connect(connect);
    if (first_endpoint.empty())
        return std::string();

    const std::size_t first = connect.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return first_endpoint;

    const std::size_t second_start = connect.find_first_not_of(" \t\r\n", first + first_endpoint.size());
    if (second_start == std::string::npos)
        return first_endpoint + " " + first_endpoint;

    const std::size_t second_end = connect.find_first_of(" \t\r\n", second_start);
    const std::string second_endpoint = connect.substr(second_start, second_end == std::string::npos ? std::string::npos : second_end - second_start);
    if (second_endpoint.empty())
        return first_endpoint + " " + first_endpoint;

    return first_endpoint + " " + second_endpoint;
}

std::string format_dota_practice_lobby_connect_pair(const std::string &endpoint)
{
    const std::string normalized_endpoint = normalize_dota_practice_lobby_connect(endpoint);
    if (normalized_endpoint.empty())
        return std::string();
    return normalized_endpoint + " " + normalized_endpoint;
}

std::string format_dota_practice_lobby_loopback_connect()
{
    return normalize_dota_practice_lobby_connect("127.0.0.1:27015");
}

std::string format_dota_practice_lobby_connect_from_endpoint(const char *endpoint)
{
    if (!endpoint || endpoint[0] == '\0')
        return format_dota_practice_lobby_loopback_connect();

    return normalize_dota_practice_lobby_connect(endpoint);
}

std::string format_dota_practice_lobby_endpoint_from_ip(std::uint32_t ip, std::uint32_t port)
{
    if (ip == 0)
        return std::string();

    const std::uint32_t octet1 = (ip >> 24) & 0xFFu;
    const std::uint32_t octet2 = (ip >> 16) & 0xFFu;
    const std::uint32_t octet3 = (ip >> 8) & 0xFFu;
    const std::uint32_t octet4 = ip & 0xFFu;

    char endpoint[32] = {};
    std::snprintf(
        endpoint,
        sizeof(endpoint),
        "%u.%u.%u.%u:%u",
        octet1,
        octet2,
        octet3,
        octet4,
        port == 0u ? 27015u : port);

    return normalize_dota_practice_lobby_connect(endpoint);
}

std::string format_dota_practice_lobby_connect_from_ips(std::uint32_t public_ip, std::uint32_t private_ip, std::uint32_t port)
{
    if (public_ip == 0u && private_ip == 0u)
        return format_dota_practice_lobby_loopback_connect();

    if (public_ip == 0u)
        public_ip = private_ip;
    if (private_ip == 0u)
        private_ip = public_ip;

    const std::string public_endpoint = format_dota_practice_lobby_endpoint_from_ip(public_ip, port);
    const std::string private_endpoint = format_dota_practice_lobby_endpoint_from_ip(private_ip, port);
    if (public_endpoint.empty())
        return format_dota_practice_lobby_connect_pair(private_endpoint);
    if (private_endpoint.empty())
        return format_dota_practice_lobby_connect_pair(public_endpoint);
    return public_endpoint + " " + private_endpoint;
}

std::string format_dota_practice_lobby_connect_from_ip(std::uint32_t ip)
{
    if (ip == 0u)
        return format_dota_practice_lobby_loopback_connect();
    return format_dota_practice_lobby_endpoint_from_ip(ip, 27015u);
}

std::uint32_t parse_dota_practice_lobby_connect_ipv4(const std::string &connect)
{
    const std::string endpoint = normalize_dota_practice_lobby_connect(connect);
    unsigned int octet1 = 0;
    unsigned int octet2 = 0;
    unsigned int octet3 = 0;
    unsigned int octet4 = 0;
    unsigned int port = 0;
    if (std::sscanf(endpoint.c_str(), "%u.%u.%u.%u:%u", &octet1, &octet2, &octet3, &octet4, &port) != 5)
        return 0;

    if (octet1 > 255u || octet2 > 255u || octet3 > 255u || octet4 > 255u || port == 0u || port > 65535u)
        return 0;

    return (octet1 << 24) | (octet2 << 16) | (octet3 << 8) | octet4;
}

std::string get_dota_practice_lobby_first_connect_endpoint(const std::string &connect)
{
    return normalize_dota_practice_lobby_connect(connect);
}

std::string select_dota_arcade_connect_endpoint_for_local_player(const std::string &connect, std::uint64_t local_steam_id, std::uint64_t owner_steam_id)
{
    const std::string endpoint = get_dota_practice_lobby_first_connect_endpoint(connect);
    if (endpoint.empty() || local_steam_id == 0ull || local_steam_id != owner_steam_id)
        return endpoint;

    const std::size_t port_pos = endpoint.rfind(':');
    if (port_pos == std::string::npos || port_pos + 1 >= endpoint.size())
        return endpoint;

    return std::string("127.0.0.1") + endpoint.substr(port_pos);
}

} // namespace gbe::proto_wire

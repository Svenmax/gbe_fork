#include "dll/gbe_dota_gc_router.h"
#include "dll/gbe_dota_gc_wire.h"
#include "dll/gbe_dota_lobby_state.h"
#include "dll/gbe_dota_protocol_constants.h"
#include "dll/gbe_dota_reconnect_context.h"
#include "dll/gbe_gc_message_utils.h"
#include "dll/gbe_proto_wire.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string hex_string(const std::string &bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : bytes)
        out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_size(std::size_t actual, std::size_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_string(const std::string &actual, const std::string &expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool baseline_rewrite_dota_lobby_template_member_object(
    const std::string &input,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool force_connected_leaver_state,
    std::string &output)
{
    using namespace gbe::proto_wire;

    output.clear();
    bool saw_team = false;
    bool saw_slot = false;
    bool saw_hero_id = false;
    bool saw_leaver_status = false;
    bool saw_leaver_actions = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u) {
            if (field.wire_type == 1u) {
                append_fixed64_field(output, 1u, steam_id);
                continue;
            }

            if (field.wire_type == 0u) {
                append_varint_field(output, 1u, account_id);
                continue;
            }
        }

        if (field.number == 55u && field.wire_type == 0u) {
            append_varint_field(output, 55u, account_id);
            continue;
        }

        if (field.number == 2u && field.wire_type == 0u) {
            saw_hero_id = true;
            if (owner_hero_id != 0u)
                append_varint_field(output, 2u, owner_hero_id);
            continue;
        }

        if (field.number == 3u && field.wire_type == 0u) {
            saw_team = true;
            append_varint_field(output, 3u, owner_team);
            continue;
        }

        if (field.number == 7u && field.wire_type == 0u) {
            saw_slot = true;
            append_varint_field(output, 7u, owner_slot);
            continue;
        }

        if (field.number == 16u && field.wire_type == 5u) {
            saw_leaver_status = true;
            if (force_connected_leaver_state) {
                append_fixed32_field(output, 16u, 0u);
                continue;
            }
        }

        if (field.number == 28u && field.wire_type == 0u) {
            saw_leaver_actions = true;
            if (force_connected_leaver_state) {
                append_varint_field(output, 28u, 0u);
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_team)
        append_varint_field(output, 3u, owner_team);

    if (!saw_slot)
        append_varint_field(output, 7u, owner_slot);

    if (!saw_hero_id && owner_hero_id != 0u)
        append_varint_field(output, 2u, owner_hero_id);

    if (force_connected_leaver_state && !saw_leaver_status)
        append_fixed32_field(output, 16u, 0u);

    if (force_connected_leaver_state && !saw_leaver_actions)
        append_varint_field(output, 28u, 0u);

    return true;
}

bool baseline_rewrite_dota_server_static_lobby_member_object(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output)
{
    using namespace gbe::proto_wire;

    std::string account_rewritten_input = input;
    if (account_id != 0) {
        std::string rewritten_account_fields;
        std::size_t replacement_count = 0;
        if (!rewrite_varint_bytes_recursive(
                input,
                old_account_id_varint,
                1u,
                account_id,
                rewritten_account_fields,
                replacement_count))
            return false;
        if (replacement_count > 0)
            account_rewritten_input.swap(rewritten_account_fields);
    }

    output.clear();
    bool saw_steam_id = false;

    std::size_t offset = 0;
    while (offset < account_rewritten_input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(account_rewritten_input.data()), account_rewritten_input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 1u) {
            saw_steam_id = true;
            append_fixed64_field(output, 1u, steam_id);
            continue;
        }

        if (field.number == 11u && field.wire_type == 0u) {
            append_varint_field(output, 11u, 1u);
            continue;
        }

        output.append(account_rewritten_input.data() + field_offset, field_end - field_offset);
    }

    if (saw_steam_id)
        return true;

    append_fixed64_field(output, 1u, steam_id);
    return true;
}

bool baseline_read_varuint64(
    const std::uint8_t *data,
    std::size_t size,
    std::size_t &offset,
    std::uint64_t &value,
    std::size_t *raw_begin = nullptr,
    std::size_t *raw_end = nullptr)
{
    std::uint64_t parsed_value = 0;
    if (!gbe::proto_wire::read_varuint(data, size, offset, parsed_value, raw_begin, raw_end))
        return false;
    value = parsed_value;
    return true;
}

bool baseline_read_next_proto_field(
    const std::uint8_t *data,
    std::size_t size,
    std::size_t &offset,
    std::uint32_t &field_number,
    std::uint32_t &wire_type,
    std::size_t &field_offset,
    std::size_t &value_offset,
    std::size_t &value_size,
    std::size_t &field_end)
{
    gbe::proto_wire::Field field{};
    if (!gbe::proto_wire::read_next_field(data, size, offset, field, &field_offset, &field_end))
        return false;
    field_number = field.number;
    wire_type = field.wire_type;
    value_offset = field.value_offset;
    value_size = field.value_size;
    return true;
}

bool baseline_rewrite_varint_fields(
    const std::string &input,
    const std::vector<std::uint32_t> &field_numbers,
    std::uint64_t value,
    std::string &output,
    bool *rewrote = nullptr)
{
    using namespace gbe::proto_wire;

    output.clear();
    if (rewrote)
        *rewrote = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        std::uint32_t field_number = 0;
        std::uint32_t wire_type = 0;
        std::size_t field_offset = 0;
        std::size_t value_offset = 0;
        std::size_t value_size = 0;
        std::size_t field_end = 0;
        if (!baseline_read_next_proto_field(
                reinterpret_cast<const std::uint8_t *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (wire_type == 0u && std::find(field_numbers.begin(), field_numbers.end(), field_number) != field_numbers.end()) {
            append_varint_field(output, field_number, value);
            if (rewrote)
                *rewrote = true;
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool baseline_rewrite_varint_bytes_recursive(
    const std::string &input,
    const std::vector<std::uint8_t> &old_encoded,
    std::uint32_t target_field_number,
    std::uint64_t new_value,
    std::string &output,
    std::size_t &replacement_count)
{
    using namespace gbe::proto_wire;

    output.clear();
    replacement_count = 0;

    std::size_t offset = 0;
    while (offset < input.size()) {
        std::uint32_t field_number = 0;
        std::uint32_t wire_type = 0;
        std::size_t field_offset = 0;
        std::size_t value_offset = 0;
        std::size_t value_size = 0;
        std::size_t field_end = 0;
        if (!baseline_read_next_proto_field(
                reinterpret_cast<const std::uint8_t *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (wire_type == 0u) {
            append_varuint(output, (static_cast<std::uint64_t>(field_number) << 3) | wire_type);

            const std::uint8_t *raw_value = reinterpret_cast<const std::uint8_t *>(input.data() + value_offset);
            if (field_number == target_field_number
                && value_size == old_encoded.size()
                && std::memcmp(raw_value, old_encoded.data(), value_size) == 0) {
                append_varuint(output, new_value);
                ++replacement_count;
            } else {
                output.append(input.data() + value_offset, value_size);
            }

            continue;
        }

        if (wire_type == 2u) {
            std::string nested_input(input.data() + value_offset, value_size);
            std::string nested_output;
            std::size_t nested_replacement_count = 0;
            if (baseline_rewrite_varint_bytes_recursive(
                    nested_input,
                    old_encoded,
                    target_field_number,
                    new_value,
                    nested_output,
                    nested_replacement_count)
                && nested_replacement_count > 0) {
                append_bytes_field(output, field_number, nested_output);
                replacement_count += nested_replacement_count;
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool baseline_find_and_overwrite_bytes(std::string &buffer, const std::vector<std::uint8_t> &from, const std::vector<std::uint8_t> &to)
{
    if (from.empty() || from.size() != to.size())
        return false;

    bool found = false;
    for (std::size_t offset = 0; offset + from.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (std::size_t i = 0; i < from.size(); ++i) {
            if (static_cast<std::uint8_t>(buffer[offset + i]) != from[i]) {
                matched = false;
                break;
            }
        }

        if (!matched)
            continue;

        for (std::size_t i = 0; i < to.size(); ++i)
            buffer[offset + i] = static_cast<char>(to[i]);
        found = true;
        offset += from.size() - 1;
    }

    return found;
}

std::size_t baseline_count_byte_pattern_matches(const std::string &buffer, const std::vector<std::uint8_t> &needle)
{
    if (needle.empty())
        return 0;

    std::size_t matches = 0;
    for (std::size_t offset = 0; offset + needle.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (std::size_t i = 0; i < needle.size(); ++i) {
            if (static_cast<std::uint8_t>(buffer[offset + i]) != needle[i]) {
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

bool baseline_patch_template_identifiers(
    std::string &message,
    const std::vector<std::uint8_t> &old_lobby_id,
    std::uint64_t lobby_id,
    bool require_lobby_id,
    const std::vector<std::uint8_t> &old_steam_id_fixed64,
    std::uint64_t steam_id,
    bool require_steam_id_fixed64,
    std::size_t &lobby_id_match_count,
    std::size_t &steam_id_fixed64_match_count)
{
    std::vector<std::uint8_t> encoded_lobby_id;
    if (!gbe::proto_wire::encode_varuint_with_expected_size(lobby_id, old_lobby_id.size(), encoded_lobby_id))
        return false;

    lobby_id_match_count = baseline_count_byte_pattern_matches(message, old_lobby_id);
    if (lobby_id_match_count != 0 || require_lobby_id) {
        if (!baseline_find_and_overwrite_bytes(message, old_lobby_id, encoded_lobby_id))
            return false;
    }

    std::string steam_id_fixed64_raw;
    gbe::proto_wire::append_little_endian64(steam_id_fixed64_raw, steam_id);
    const std::vector<std::uint8_t> encoded_steam_id_fixed64(steam_id_fixed64_raw.begin(), steam_id_fixed64_raw.end());
    steam_id_fixed64_match_count = baseline_count_byte_pattern_matches(message, old_steam_id_fixed64);
    if (steam_id_fixed64_match_count != 0 || require_steam_id_fixed64) {
        if (!baseline_find_and_overwrite_bytes(message, old_steam_id_fixed64, encoded_steam_id_fixed64))
            return false;
    }

    return true;
}

bool baseline_rewrite_dota_lobby_template_object_2004(
    const std::string &input,
    const gbe::dota_gc_wire::DotaLobbyTemplateObject2004RewriteOptions &options,
    std::string &output)
{
    using namespace gbe::proto_wire;

    output.clear();
    bool saw_lobby_id = false;
    bool saw_state = false;
    bool saw_connect = false;
    bool saw_server_id = false;
    bool saw_game_state = false;
    bool saw_match_id = false;
    bool saw_game_start_time = false;
    bool saw_lan = false;
    bool saw_lan_host_ping_location = false;
    bool saw_room_name = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;
        const std::uint32_t field_number = field.number;
        const std::uint32_t wire_type = field.wire_type;

        if (field_number == 1u && wire_type == 0u) {
            saw_lobby_id = true;
            if (options.rewrite_runtime_fields) {
                append_varint_field(output, 1u, options.lobby_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 3u && wire_type == 0u) {
            append_varint_field(output, 3u, options.game_mode);
            continue;
        }

        if (field_number == 4u && wire_type == 0u) {
            saw_state = true;
            if (options.rewrite_runtime_fields) {
                append_varint_field(output, 4u, options.lobby_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 5u && wire_type == 2u) {
            saw_connect = true;
            if (options.rewrite_runtime_fields) {
                append_bytes_field(output, 5u, normalize_dota_practice_lobby_connect(options.connect));
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 6u && wire_type == 1u) {
            saw_server_id = true;
            if (options.rewrite_runtime_fields) {
                if (options.server_id != 0)
                    append_fixed64_field(output, 6u, options.server_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 13u && wire_type == 0u) {
            append_varint_field(output, 13u, options.allow_cheats ? 1u : 0u);
            continue;
        }

        if (field_number == 16u && wire_type == 2u) {
            saw_room_name = true;
            append_bytes_field(output, 16u, options.room_name);
            continue;
        }

        if (field_number == 11u && wire_type == 1u) {
            append_fixed64_field(output, 11u, options.steam_id);
            continue;
        }

        if (field_number == 14u && wire_type == 0u) {
            append_varint_field(output, 14u, options.fill_with_bots ? 1u : 0u);
            continue;
        }

        if (field_number == 21u && wire_type == 0u) {
            append_varint_field(output, 21u, options.server_region);
            continue;
        }

        if (field_number == 22u && wire_type == 0u) {
            saw_game_state = true;
            if (options.rewrite_runtime_fields) {
                append_varint_field(output, 22u, options.lobby_game_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 31u && wire_type == 0u) {
            append_varint_field(output, 31u, options.allow_spectating ? 1u : 0u);
            continue;
        }

        if (field_number == 30u && wire_type == 0u) {
            saw_match_id = true;
            if (options.rewrite_runtime_fields) {
                append_varint_field(output, 30u, options.match_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 36u && wire_type == 0u) {
            append_varint_field(output, 36u, options.bot_difficulty_radiant);
            continue;
        }

        if (field_number == 39u && wire_type == 2u) {
            append_bytes_field(output, 39u, options.pass_key);
            continue;
        }

        if (field_number == 57u && wire_type == 0u) {
            saw_lan = true;
            append_varint_field(output, 57u, options.lan ? 1u : 0u);
            continue;
        }

        if (field_number == 75u && wire_type == 0u) {
            append_varint_field(output, 75u, options.visibility);
            continue;
        }

        if (field_number == 87u && wire_type == 0u) {
            saw_game_start_time = true;
            if (options.rewrite_runtime_fields) {
                append_varint_field(output, 87u, options.game_start_time);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 93u && wire_type == 0u) {
            append_varint_field(output, 93u, options.bot_difficulty_dire);
            continue;
        }

        if (field_number == 94u && wire_type == 0u) {
            append_varint_field(output, 94u, options.bot_radiant);
            continue;
        }

        if (field_number == 95u && wire_type == 0u) {
            append_varint_field(output, 95u, options.bot_dire);
            continue;
        }

        if (field_number == 109u && wire_type == 2u) {
            saw_lan_host_ping_location = true;
            if (!options.lan_host_ping_location.empty())
                append_bytes_field(output, 109u, options.lan_host_ping_location);
            continue;
        }

        if (field_number == 120u && wire_type == 2u) {
            if (!options.rewrite_runtime_fields) {
                output.append(input.data() + field_offset, field_end - field_offset);
                continue;
            }

            std::string rewritten_member;
            if (!baseline_rewrite_dota_lobby_template_member_object(
                    std::string(input.data() + field.value_offset, field.value_size),
                    options.account_id,
                    options.steam_id,
                    options.owner_team,
                    options.owner_slot,
                    options.owner_hero_id,
                    false,
                    rewritten_member))
                return false;
            append_bytes_field(output, 120u, rewritten_member);
            continue;
        }

        if (field_number == 121u && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        if ((field_number == 122u || field_number == 123u) && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (options.rewrite_runtime_fields && !saw_lobby_id && options.lobby_id != 0)
        append_varint_field(output, 1u, options.lobby_id);
    if (options.rewrite_runtime_fields && !saw_state)
        append_varint_field(output, 4u, options.lobby_state);
    if (options.rewrite_runtime_fields && !saw_connect && !options.connect.empty())
        append_bytes_field(output, 5u, normalize_dota_practice_lobby_connect(options.connect));
    if (options.rewrite_runtime_fields && !saw_server_id && options.server_id != 0)
        append_fixed64_field(output, 6u, options.server_id);
    if (!saw_lan)
        append_varint_field(output, 57u, options.lan ? 1u : 0u);
    if (!saw_room_name)
        append_bytes_field(output, 16u, options.room_name);
    if (options.rewrite_runtime_fields && !saw_game_state)
        append_varint_field(output, 22u, options.lobby_game_state);
    if (options.rewrite_runtime_fields && !saw_match_id && options.match_id != 0)
        append_varint_field(output, 30u, options.match_id);
    if (!saw_lan_host_ping_location && !options.lan_host_ping_location.empty())
        append_bytes_field(output, 109u, options.lan_host_ping_location);
    if (options.rewrite_runtime_fields && !saw_game_start_time && options.game_start_time != 0)
        append_varint_field(output, 87u, options.game_start_time);

    return true;
}

bool baseline_rewrite_dota_lobby_template_object_2014(
    const std::string &input,
    const std::string &player_name,
    std::string &output)
{
    using namespace gbe::proto_wire;

    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 2u) {
            std::string rewritten_member;
            std::size_t member_offset = 0;
            while (member_offset < field.value_size) {
                Field member{};
                std::size_t member_field_offset = 0;
                std::size_t member_field_end = 0;
                if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()) + field.value_offset, field.value_size, member_offset, member, &member_field_offset, &member_field_end))
                    return false;

                if (member.number == 1u && member.wire_type == 2u) {
                    append_bytes_field(rewritten_member, 1u, player_name);
                    continue;
                }

                rewritten_member.append(input.data() + field.value_offset + member_field_offset, member_field_end - member_field_offset);
            }

            append_bytes_field(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool baseline_rewrite_dota_lobby_template_object_2015(
    const std::string &input,
    bool clear_existing_startup_data,
    std::uint32_t additional_startup_type_id,
    const std::string &additional_startup_payload,
    std::string &output)
{
    using namespace gbe::proto_wire;

    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;
        const std::uint32_t field_number = field.number;
        const std::uint32_t wire_type = field.wire_type;

        if (clear_existing_startup_data && field_number == 2u && wire_type == 2u) {
            std::uint64_t startup_type = 0;
            if (read_uint64_field(reinterpret_cast<const std::uint8_t *>(input.data()) + field.value_offset, field.value_size, 1u, startup_type)
                && startup_type == additional_startup_type_id)
                continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!additional_startup_payload.empty()) {
        std::string startup_message;
        append_varint_field(startup_message, 1u, additional_startup_type_id);
        append_bytes_field(startup_message, 2u, additional_startup_payload);
        append_bytes_field(output, 2u, startup_message);
    }

    return true;
}

bool baseline_rewrite_dota_lobby_template_object_2016(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output,
    gbe::dota_gc_wire::DotaLobbyTemplateObject2016RewriteDebug *debug = nullptr)
{
    using namespace gbe::proto_wire;

    if (debug) {
        debug->first_member_input.clear();
        debug->first_member_output.clear();
    }

    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 2u) {
            const std::string member_input(input.data() + field.value_offset, field.value_size);
            std::string rewritten_member;
            if (!baseline_rewrite_dota_server_static_lobby_member_object(
                    member_input,
                    old_account_id_varint,
                    account_id,
                    steam_id,
                    rewritten_member))
                return false;

            if (debug && debug->first_member_input.empty()) {
                debug->first_member_input = member_input;
                debug->first_member_output = rewritten_member;
            }

            append_bytes_field(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool test_basic_wire_and_parsers()
{
    using namespace gbe::proto_wire;

    bool ok = true;
    std::vector<std::uint8_t> varuint{0xac, 0x02};
    std::size_t offset = 0;
    std::uint64_t value = 0;
    std::size_t raw_size = 0;
    ok &= expect_true(read_varuint(varuint, offset, value, &raw_size), "read varuint");
    std::size_t baseline_offset = 0;
    std::uint64_t baseline_value = 0;
    std::size_t baseline_raw_begin = 0;
    std::size_t baseline_raw_end = 0;
    ok &= expect_true(baseline_read_varuint64(varuint.data(), varuint.size(), baseline_offset, baseline_value, &baseline_raw_begin, &baseline_raw_end), "baseline read varuint");
    ok &= expect_eq_u64(value, baseline_value, "read varuint baseline value match");
    ok &= expect_eq_size(offset, baseline_offset, "read varuint baseline offset match");
    ok &= expect_eq_size(raw_size, baseline_raw_end - baseline_raw_begin, "read varuint baseline raw size match");
    ok &= expect_eq_u64(value, 300u, "varuint value");
    ok &= expect_eq_size(raw_size, 2u, "varuint raw size");
    ok &= expect_eq_size(offset, 2u, "varuint offset");

    const std::vector<std::uint8_t> mixed{
        0x08, 0x01,
        0x11, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x1a, 0x03, 0x61, 0x62, 0x63,
        0x25, 0x0d, 0x0c, 0x0b, 0x0a,
    };
    const std::vector<Field> fields = parse_fields(mixed);
    ok &= expect_eq_size(fields.size(), 4u, "field count");
    if (fields.size() == 4u) {
        ok &= expect_eq_u64(fields[0].number, 1u, "field 1 number");
        ok &= expect_eq_u64(fields[0].wire_type, 0u, "field 1 wire");
        ok &= expect_eq_u64(fields[1].number, 2u, "field 2 number");
        ok &= expect_eq_u64(fields[1].wire_type, 1u, "field 2 wire");
        ok &= expect_eq_u64(fields[2].number, 3u, "field 3 number");
        ok &= expect_eq_u64(fields[2].wire_type, 2u, "field 3 wire");
        ok &= expect_eq_u64(fields[2].value_size, 3u, "field 3 size");
        ok &= expect_eq_u64(fields[3].number, 4u, "field 4 number");
        ok &= expect_eq_u64(fields[3].wire_type, 5u, "field 4 wire");
    }

    ok &= expect_eq_u64(read_little_endian(mixed, 3u, 8u), 0x0102030405060708ull, "fixed64 value");
    ok &= expect_eq_string(hex_prefix(mixed, 13u, 3u, 8u), "0x616263", "hex prefix full");
    ok &= expect_eq_string(hex_prefix(mixed, 0u, mixed.size(), 4u), "0x08011108...", "hex prefix truncated");
    ok &= expect_eq_string(format_hex_prefix(mixed.data(), mixed.size(), 4u), "08 01 11 08 ...", "format hex prefix");
    ok &= expect_eq_string(format_hex_prefix(nullptr, 0u, 4u), std::string(), "format hex prefix null");
    ok &= expect_eq_string(format_hex(mixed.data(), mixed.size()), "08 01 11 08 07 06 05 04 03 02 01 1a 03 61 62 63 25 0d 0c 0b 0a", "format hex");
    ok &= expect_eq_string(format_hex(nullptr, 0u), std::string(), "format hex null");
    ok &= expect_eq_string(format_ipv4(0x01020304u), "1.2.3.4", "format ipv4");
    ok &= expect_eq_string(sanitize_proto_log_string(std::string({ 'A', '\x01', 'B', '\x7f', 'C' })), "A.B.C", "sanitize proto log string");
    ok &= expect_eq_u64(parse_uint64_or_zero("12345"), 12345u, "parse uint64 valid");
    ok &= expect_eq_u64(parse_uint64_or_zero("12x"), 0u, "parse uint64 invalid");
    ok &= expect_eq_u64(parse_uint32_or_zero("4294967295"), 4294967295u, "parse uint32 max");
    ok &= expect_eq_u64(parse_uint32_or_zero("4294967296"), 0u, "parse uint32 overflow");
    ok &= expect_true(dota_string_is_unsigned_integer("12345"), "dota string unsigned integer");
    ok &= expect_true(!dota_string_is_unsigned_integer("12a"), "dota string unsigned integer mixed");
    ok &= expect_true(dota_is_readable_custom_game_name("mygame"), "dota readable custom game name");
    ok &= expect_true(!dota_is_readable_custom_game_name("dota"), "dota readable custom game name dota");
    ok &= expect_true(dota_is_rank_type_supported(1u), "dota supported rank type 1");
    ok &= expect_true(dota_is_rank_type_supported(101u), "dota supported rank type 101");
    ok &= expect_true(!dota_is_rank_type_supported(0u), "dota unsupported rank type 0");
    ok &= expect_true(!dota_is_rank_type_supported(102u), "dota unsupported rank type 102");
    ok &= expect_true(dota_is_dire_team(1u), "dota dire team bad guys");
    ok &= expect_true(dota_is_dire_team(3u), "dota dire team alternate");
    ok &= expect_true(!dota_is_dire_team(0u), "dota dire team good guys");
    ok &= expect_true(!dota_is_dire_team(4u), "dota dire team player pool");
    ok &= expect_true(!dota8053_indicates_load_failure(0u, std::string()), "dota 8053 empty text success");
    ok &= expect_true(!dota8053_indicates_load_failure(1u, "ok"), "dota 8053 success code");
    ok &= expect_true(dota8053_indicates_load_failure(2u, "failed"), "dota 8053 non-success code");
    ok &= expect_true(dota8053_indicates_load_failure(1u, "#GameUI_Disconnect_Test"), "dota 8053 disconnect text");
    ok &= expect_true(is_dota_practice_lobby_prelaunch_state(0u, 0u, 0u, std::string()), "dota prelaunch empty state");
    ok &= expect_true(!is_dota_practice_lobby_prelaunch_state(1u, 0u, 0u, std::string()), "dota prelaunch server id set");
    ok &= expect_true(!is_dota_practice_lobby_prelaunch_state(0u, 0u, 0u, "127.0.0.1:27015"), "dota prelaunch connect set");
    std::string dota7070_body;
    append_varint_field(dota7070_body, 1u, 2u);
    Dota7070ReadyUpRequest dota7070_request = parse_dota7070_ready_up_request(reinterpret_cast<const std::uint8_t *>(dota7070_body.data()), dota7070_body.size());
    ok &= expect_true(dota7070_request.has_ready_state, "dota 7070 has ready state");
    ok &= expect_eq_u64(dota7070_request.ready_state, 2u, "dota 7070 ready state");
    dota7070_request = parse_dota7070_ready_up_request(nullptr, 0u);
    ok &= expect_true(!dota7070_request.has_ready_state, "dota 7070 empty has no ready state");
    const std::string truncated_varint_field({ '\x08', '\x80' });
    dota7070_request = parse_dota7070_ready_up_request(reinterpret_cast<const std::uint8_t *>(truncated_varint_field.data()), truncated_varint_field.size());
    ok &= expect_true(!dota7070_request.has_ready_state, "dota 7070 truncated varint has no ready state");
    const std::string unknown_wire_type_field({ '\x0b' });
    dota7070_request = parse_dota7070_ready_up_request(reinterpret_cast<const std::uint8_t *>(unknown_wire_type_field.data()), unknown_wire_type_field.size());
    ok &= expect_true(!dota7070_request.has_ready_state, "dota 7070 unknown wire type has no ready state");
    std::string dota7070_duplicate_body;
    append_varint_field(dota7070_duplicate_body, 1u, 2u);
    append_varint_field(dota7070_duplicate_body, 1u, 3u);
    dota7070_request = parse_dota7070_ready_up_request(reinterpret_cast<const std::uint8_t *>(dota7070_duplicate_body.data()), dota7070_duplicate_body.size());
    ok &= expect_true(dota7070_request.has_ready_state, "dota 7070 duplicate has ready state");
    ok &= expect_eq_u64(dota7070_request.ready_state, 2u, "dota 7070 duplicate keeps first ready state");
    std::string dota8052_body;
    append_varint_field(dota8052_body, 1u, 9003u);
    append_varint_field(dota8052_body, 2u, 4500u);
    append_varint_field(dota8052_body, 4u, 12345u);
    Dota8052StartedLoadingRequest dota8052_request = parse_dota8052_started_loading_request(reinterpret_cast<const std::uint8_t *>(dota8052_body.data()), dota8052_body.size());
    ok &= expect_true(dota8052_request.has_lobby_id, "dota 8052 has lobby id");
    ok &= expect_eq_u64(dota8052_request.lobby_id, 9003u, "dota 8052 lobby id");
    ok &= expect_true(dota8052_request.has_custom_game_id, "dota 8052 has custom game id");
    ok &= expect_eq_u64(dota8052_request.custom_game_id, 4500u, "dota 8052 custom game id");
    ok &= expect_true(dota8052_request.has_start_time, "dota 8052 has start time");
    ok &= expect_eq_u64(dota8052_request.start_time, 12345u, "dota 8052 start time");
    dota8052_request = parse_dota8052_started_loading_request(nullptr, 0u);
    ok &= expect_true(!dota8052_request.has_lobby_id, "dota 8052 empty has no lobby id");
    dota8052_request = parse_dota8052_started_loading_request(reinterpret_cast<const std::uint8_t *>(truncated_varint_field.data()), truncated_varint_field.size());
    ok &= expect_true(!dota8052_request.has_lobby_id, "dota 8052 truncated varint has no lobby id");
    dota8052_request = parse_dota8052_started_loading_request(reinterpret_cast<const std::uint8_t *>(unknown_wire_type_field.data()), unknown_wire_type_field.size());
    ok &= expect_true(!dota8052_request.has_lobby_id, "dota 8052 unknown wire type has no lobby id");
    std::string dota8052_duplicate_body;
    append_varint_field(dota8052_duplicate_body, 1u, 9003u);
    append_varint_field(dota8052_duplicate_body, 1u, 9004u);
    append_varint_field(dota8052_duplicate_body, 2u, 4500u);
    append_varint_field(dota8052_duplicate_body, 4u, 12345u);
    dota8052_request = parse_dota8052_started_loading_request(reinterpret_cast<const std::uint8_t *>(dota8052_duplicate_body.data()), dota8052_duplicate_body.size());
    ok &= expect_true(dota8052_request.has_lobby_id, "dota 8052 duplicate has lobby id");
    ok &= expect_eq_u64(dota8052_request.lobby_id, 9003u, "dota 8052 duplicate keeps first lobby id");
    std::string dota8053_body;
    append_varint_field(dota8053_body, 1u, 9004u);
    append_varint_field(dota8053_body, 2u, 33u);
    append_varint_field(dota8053_body, 3u, 2u);
    append_bytes_field(dota8053_body, 4u, std::string({ 'O', 'K', '\x01' }));
    append_varint_field(dota8053_body, 5u, 4u);
    Dota8053FinishedLoadingRequest dota8053_request = parse_dota8053_finished_loading_request(reinterpret_cast<const std::uint8_t *>(dota8053_body.data()), dota8053_body.size());
    ok &= expect_true(dota8053_request.has_lobby_id, "dota 8053 has lobby id");
    ok &= expect_eq_u64(dota8053_request.lobby_id, 9004u, "dota 8053 request lobby id");
    ok &= expect_true(dota8053_request.has_loading_duration, "dota 8053 has loading duration");
    ok &= expect_eq_u64(dota8053_request.loading_duration, 33u, "dota 8053 request loading duration");
    ok &= expect_true(dota8053_request.has_result_code, "dota 8053 has result code");
    ok &= expect_eq_u64(dota8053_request.result_code, 2u, "dota 8053 request result code");
    ok &= expect_true(dota8053_request.has_result_text, "dota 8053 has result text");
    ok &= expect_eq_string(dota8053_request.result_text, "OK.", "dota 8053 request sanitized result text");
    ok &= expect_true(dota8053_request.has_signon_states, "dota 8053 has signon states");
    ok &= expect_eq_u64(dota8053_request.signon_states, 4u, "dota 8053 request signon states");
    dota8053_request = parse_dota8053_finished_loading_request(nullptr, 0u);
    ok &= expect_true(!dota8053_request.has_lobby_id, "dota 8053 empty has no lobby id");
    dota8053_request = parse_dota8053_finished_loading_request(reinterpret_cast<const std::uint8_t *>(truncated_varint_field.data()), truncated_varint_field.size());
    ok &= expect_true(!dota8053_request.has_lobby_id, "dota 8053 truncated varint has no lobby id");
    dota8053_request = parse_dota8053_finished_loading_request(reinterpret_cast<const std::uint8_t *>(unknown_wire_type_field.data()), unknown_wire_type_field.size());
    ok &= expect_true(!dota8053_request.has_lobby_id, "dota 8053 unknown wire type has no lobby id");
    std::string dota8053_duplicate_body;
    append_varint_field(dota8053_duplicate_body, 1u, 9004u);
    append_varint_field(dota8053_duplicate_body, 1u, 9005u);
    append_varint_field(dota8053_duplicate_body, 3u, 1u);
    append_bytes_field(dota8053_duplicate_body, 4u, "first");
    append_bytes_field(dota8053_duplicate_body, 4u, "second");
    dota8053_request = parse_dota8053_finished_loading_request(reinterpret_cast<const std::uint8_t *>(dota8053_duplicate_body.data()), dota8053_duplicate_body.size());
    ok &= expect_true(dota8053_request.has_lobby_id, "dota 8053 duplicate has lobby id");
    ok &= expect_eq_u64(dota8053_request.lobby_id, 9004u, "dota 8053 duplicate keeps first lobby id");
    ok &= expect_eq_string(dota8053_request.result_text, "first", "dota 8053 duplicate keeps first result text");
    Dota8053Result dota8053_result = parse_dota8053_result(reinterpret_cast<const std::uint8_t *>(dota8053_body.data()), dota8053_body.size());
    ok &= expect_eq_u64(dota8053_result.lobby_id, 9004u, "dota 8053 lobby id");
    ok &= expect_eq_u64(dota8053_result.loading_duration, 33u, "dota 8053 loading duration");
    ok &= expect_eq_u64(dota8053_result.result_code, 2u, "dota 8053 result code");
    ok &= expect_eq_string(dota8053_result.result_text, "OK.", "dota 8053 sanitized result text");
    ok &= expect_eq_u64(dota8053_result.signon_states, 4u, "dota 8053 signon states");
    dota8053_result = parse_dota8053_result(nullptr, 0u);
    ok &= expect_eq_u64(dota8053_result.lobby_id, 0u, "dota 8053 null lobby id");
    ok &= expect_eq_string(dota8053_result.result_text, std::string(), "dota 8053 null result text");
    std::string decoded_hex;
    ok &= expect_true(decode_hex_string("0a FF 10", decoded_hex), "decode hex string");
    ok &= expect_eq_string(hex_string(decoded_hex), "0aff10", "decode hex string bytes");
    ok &= expect_true(!decode_hex_string("abc", decoded_hex), "decode odd hex string");
    ok &= expect_true(!decode_hex_string("zz", decoded_hex), "decode invalid hex string");
    std::string overwrite_bytes = std::string({ 'a', 'b', 'a', 'b' });
    ok &= expect_true(find_and_overwrite_bytes(overwrite_bytes, std::vector<std::uint8_t>{ 'a', 'b' }, std::vector<std::uint8_t>{ 'x', 'y' }), "overwrite bytes");
    ok &= expect_eq_string(overwrite_bytes, "xyxy", "overwrite bytes result");
    ok &= expect_true(!find_and_overwrite_bytes(overwrite_bytes, std::vector<std::uint8_t>{ 'x' }, std::vector<std::uint8_t>{ '1', '2' }), "overwrite bytes size mismatch");
    std::string overwrite_string = "abcabc";
    ok &= expect_true(find_and_overwrite_string(overwrite_string, "ab", "xy"), "overwrite string");
    ok &= expect_eq_string(overwrite_string, "xycxyc", "overwrite string result");
    std::vector<std::uint8_t> expected_varuint;
    ok &= expect_true(encode_varuint_with_expected_size(300u, 2u, expected_varuint), "encode varuint expected size");
    ok &= expect_eq_size(expected_varuint.size(), 2u, "encode varuint expected size bytes");
    if (expected_varuint.size() == 2u) {
        ok &= expect_eq_u64(expected_varuint[0], 0xacu, "encode varuint expected first byte");
        ok &= expect_eq_u64(expected_varuint[1], 0x02u, "encode varuint expected second byte");
    }
    ok &= expect_true(!encode_varuint_with_expected_size(300u, 1u, expected_varuint), "encode varuint expected size mismatch");
    const std::vector<std::uint8_t> copied_bytes = vector_from_bytes(mixed.data() + 13u, 3u);
    ok &= expect_eq_size(copied_bytes.size(), 3u, "vector from bytes size");
    if (copied_bytes.size() == 3u)
        ok &= expect_eq_string(std::string(copied_bytes.begin(), copied_bytes.end()), "abc", "vector from bytes value");
    ok &= expect_eq_size(count_byte_pattern_matches("ababab", std::vector<std::uint8_t>{ 'a', 'b' }), 3u, "count byte pattern matches");
    ok &= expect_eq_size(count_byte_pattern_matches("aaaa", std::vector<std::uint8_t>{ 'a', 'a' }), 2u, "count byte pattern non-overlap matches");
    ok &= expect_eq_size(count_byte_pattern_matches("aaaa", std::vector<std::uint8_t>{}), 0u, "count byte pattern empty needle");
    ok &= expect_eq_string(normalize_dota_practice_lobby_connect("  1.2.3.4:27015 5.6.7.8:27016"), "1.2.3.4:27015", "normalize dota connect");
    ok &= expect_eq_string(normalize_dota_practice_lobby_connect_pair(" 1.2.3.4:27015 "), "1.2.3.4:27015 1.2.3.4:27015", "normalize dota connect pair duplicate");
    ok &= expect_eq_string(normalize_dota_practice_lobby_connect_pair("1.2.3.4:27015 5.6.7.8:27016"), "1.2.3.4:27015 5.6.7.8:27016", "normalize dota connect pair");
    ok &= expect_eq_string(format_dota_practice_lobby_connect_pair(" 1.2.3.4:27015 "), "1.2.3.4:27015 1.2.3.4:27015", "format dota connect pair");
    ok &= expect_eq_string(format_dota_practice_lobby_loopback_connect(), "127.0.0.1:27015", "format dota loopback connect");
    ok &= expect_eq_string(format_dota_practice_lobby_connect_from_endpoint(nullptr), "127.0.0.1:27015", "format dota connect null endpoint");
    ok &= expect_eq_string(format_dota_practice_lobby_endpoint_from_ip(0x01020304u, 0u), "1.2.3.4:27015", "format dota endpoint from ip default port");
    ok &= expect_eq_string(format_dota_practice_lobby_endpoint_from_ip(0x01020304u, 1234u), "1.2.3.4:1234", "format dota endpoint from ip port");
    ok &= expect_eq_string(format_dota_practice_lobby_connect_from_ips(0x01020304u, 0x05060708u, 27016u), "1.2.3.4:27016 5.6.7.8:27016", "format dota connect from ips");
    ok &= expect_eq_string(format_dota_practice_lobby_connect_from_ip(0u), "127.0.0.1:27015", "format dota connect from zero ip");
    ok &= expect_eq_u64(parse_dota_practice_lobby_connect_ipv4("1.2.3.4:27015"), 0x01020304u, "parse dota connect ipv4");
    ok &= expect_eq_u64(parse_dota_practice_lobby_connect_ipv4("1.2.3.4:0"), 0u, "parse dota connect ipv4 invalid port");
    ok &= expect_true(gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("", "1.2.3.4:27015"), "prefer connect when current empty");
    ok &= expect_true(gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("127.0.0.1:27015", "1.2.3.4:27015"), "prefer connect over loopback");
    ok &= expect_true(!gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("1.2.3.4:27015", "1.2.3.4:27015"), "skip identical connect update");
    ok &= expect_true(!gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("1.2.3.4:27015", ""), "skip empty connect update");
    ok &= expect_true(!gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("1.2.3.4:27015", "bad-connect"), "skip invalid connect update");
    ok &= expect_true(!gbe::dota_gc_wire::should_prefer_dota_lobby_connect_update("1.2.3.4:27015", "5.6.7.8:27015"), "skip replacing non-loopback connect");
    ok &= expect_eq_u64(gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(123u, 1u, 0u), 123u, "startup account lobby state 1");
    ok &= expect_eq_u64(gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(123u, 2u, 0u), 123u, "startup account lobby state 2");
    ok &= expect_eq_u64(gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(0u, 1u, 0u), 0u, "startup account zero account");
    ok &= expect_eq_u64(gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(123u, 1u, 1u), 0u, "startup account game state blocks");
    ok &= expect_eq_u64(gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(123u, 3u, 0u), 0u, "startup account lobby state blocks");
    ok &= expect_eq_string(get_dota_practice_lobby_first_connect_endpoint("1.2.3.4:27015 5.6.7.8:27016"), "1.2.3.4:27015", "get dota first connect endpoint");
    ok &= expect_eq_string(select_dota_arcade_connect_endpoint_for_local_player("1.2.3.4:27015", 10u, 10u), "127.0.0.1:27015", "select dota arcade local owner endpoint");
    ok &= expect_eq_string(select_dota_arcade_connect_endpoint_for_local_player("1.2.3.4:27015", 10u, 11u), "1.2.3.4:27015", "select dota arcade remote endpoint");

    Field field{};
    ok &= expect_true(find_field(mixed.data(), mixed.size(), 3u, field), "find field 3");
    ok &= expect_eq_u64(field.value_offset, 13u, "found field value offset");
    ok &= expect_eq_u64(field.value_size, 3u, "found field value size");
    std::string field_bytes;
    ok &= expect_true(read_field_bytes(mixed.data(), mixed.size(), field, field_bytes), "read field bytes");
    ok &= expect_eq_string(field_bytes, "abc", "field bytes value");

    std::string packed_payload;
    append_varuint(packed_payload, 1u);
    append_varuint(packed_payload, 300u);
    std::string packed_message;
    append_bytes_field(packed_message, 9u, packed_payload);
    ok &= expect_true(find_field(reinterpret_cast<const std::uint8_t *>(packed_message.data()), packed_message.size(), 9u, field), "find packed field");
    std::vector<std::uint32_t> packed_values;
    ok &= expect_true(extract_packed_uint32_field(reinterpret_cast<const std::uint8_t *>(packed_message.data()), packed_message.size(), field, packed_values), "extract packed uint32 field");
    ok &= expect_eq_size(packed_values.size(), 2u, "packed uint32 size");
    if (packed_values.size() == 2u) {
        ok &= expect_eq_u64(packed_values[0], 1u, "packed uint32 first value");
        ok &= expect_eq_u64(packed_values[1], 300u, "packed uint32 second value");
    }

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 1u, field), "find field 1");
    ok &= expect_true(read_field_uint64(mixed.data(), mixed.size(), field, value), "read field uint64 varint");
    ok &= expect_eq_u64(value, 1u, "field uint64 varint value");
    ok &= expect_true(read_uint64_field(mixed.data(), mixed.size(), 1u, value), "read uint64 field by number");
    ok &= expect_eq_u64(value, 1u, "read uint64 field by number value");

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 2u, field), "find field 2");
    ok &= expect_true(read_field_uint64(mixed.data(), mixed.size(), field, value), "read field uint64 fixed64");
    ok &= expect_eq_u64(value, 0x0102030405060708ull, "field uint64 fixed64 value");

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 4u, field), "find field 4");
    std::uint32_t value32 = 0;
    ok &= expect_true(read_field_uint32(mixed.data(), mixed.size(), field, value32), "read field uint32 fixed32");
    ok &= expect_eq_u64(value32, 0x0a0b0c0du, "field uint32 fixed32 value");
    ok &= expect_true(read_uint32_field(mixed.data(), mixed.size(), 4u, value32), "read uint32 field by number");
    ok &= expect_eq_u64(value32, 0x0a0b0c0du, "read uint32 field by number value");
    ok &= expect_true(read_bytes_field(mixed.data(), mixed.size(), 3u, field_bytes), "read bytes field by number");
    ok &= expect_eq_string(field_bytes, "abc", "read bytes field by number value");

    std::string nested_member;
    append_bytes_field(nested_member, 1u, "old");
    append_varint_field(nested_member, 2u, 7u);
    std::string nested_object;
    append_bytes_field(nested_object, 1u, nested_member);
    append_varint_field(nested_object, 9u, 5u);
    std::string rewritten_nested_object;
    std::size_t parent_offset = 0;
    while (parent_offset < nested_object.size()) {
        Field parent{};
        std::size_t parent_field_offset = 0;
        std::size_t parent_field_end = 0;
        ok &= expect_true(read_next_field(reinterpret_cast<const std::uint8_t *>(nested_object.data()), nested_object.size(), parent_offset, parent, &parent_field_offset, &parent_field_end), "nested parent read field");
        if (parent.number == 1u && parent.wire_type == 2u) {
            std::string rewritten_member;
            std::size_t member_offset = 0;
            while (member_offset < parent.value_size) {
                Field member{};
                std::size_t member_field_offset = 0;
                std::size_t member_field_end = 0;
                ok &= expect_true(read_next_field(reinterpret_cast<const std::uint8_t *>(nested_object.data()) + parent.value_offset, parent.value_size, member_offset, member, &member_field_offset, &member_field_end), "nested member read field");
                if (member.number == 1u && member.wire_type == 2u) {
                    append_bytes_field(rewritten_member, 1u, "new");
                    continue;
                }
                rewritten_member.append(nested_object.data() + parent.value_offset + member_field_offset, member_field_end - member_field_offset);
            }
            append_bytes_field(rewritten_nested_object, 1u, rewritten_member);
            continue;
        }
        rewritten_nested_object.append(nested_object.data() + parent_field_offset, parent_field_end - parent_field_offset);
    }
    std::string rewritten_member_bytes;
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(rewritten_nested_object.data()), rewritten_nested_object.size(), 1u, rewritten_member_bytes), "nested rewritten parent bytes");
    std::string rewritten_member_name;
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(rewritten_member_bytes.data()), rewritten_member_bytes.size(), 1u, rewritten_member_name), "nested rewritten member bytes");
    ok &= expect_eq_string(rewritten_member_name, "new", "nested rewritten member value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(rewritten_member_bytes.data()), rewritten_member_bytes.size(), 2u, value), "nested preserved member varint");
    ok &= expect_eq_u64(value, 7u, "nested preserved member varint value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(rewritten_nested_object.data()), rewritten_nested_object.size(), 9u, value), "nested preserved parent varint");
    ok &= expect_eq_u64(value, 5u, "nested preserved parent varint value");

    DotaEmptyRequestShape empty_shape = parse_dota_empty_request_shape(nullptr, 0u);
    ok &= expect_true(empty_shape.valid, "empty request null valid");
    ok &= expect_eq_u64(empty_shape.field_count, 0u, "empty request null field count");
    empty_shape = parse_dota_empty_request_shape(mixed.data(), mixed.size());
    ok &= expect_true(empty_shape.valid, "empty request mixed valid");
    ok &= expect_eq_u64(empty_shape.field_count, 4u, "empty request mixed field count");
    const std::string truncated_length_field = std::string({ '\x0a', '\x05', 'a' });
    empty_shape = parse_dota_empty_request_shape(reinterpret_cast<const std::uint8_t *>(truncated_length_field.data()), truncated_length_field.size());
    ok &= expect_true(!empty_shape.valid, "empty request malformed invalid");
    ok &= expect_eq_u64(empty_shape.field_count, 0u, "empty request malformed field count");

    std::string rank_request;
    append_varint_field(rank_request, 2u, 7u);
    append_varint_field(rank_request, 1u, 101u);
    DotaRankRequestShape rank_shape = parse_dota_rank_request_shape(reinterpret_cast<const std::uint8_t *>(rank_request.data()), rank_request.size());
    ok &= expect_true(rank_shape.valid, "rank request valid");
    ok &= expect_eq_u64(rank_shape.field_count, 2u, "rank request field count");
    ok &= expect_true(rank_shape.has_rank_type, "rank request has rank type");
    ok &= expect_eq_u64(rank_shape.rank_type, 101u, "rank request rank type");
    rank_shape = parse_dota_rank_request_shape(nullptr, 0u);
    ok &= expect_true(rank_shape.valid, "rank request null valid");
    ok &= expect_true(!rank_shape.has_rank_type, "rank request null missing rank type");
    rank_shape = parse_dota_rank_request_shape(reinterpret_cast<const std::uint8_t *>(truncated_length_field.data()), truncated_length_field.size());
    ok &= expect_true(!rank_shape.valid, "rank request malformed invalid");
    ok &= expect_true(!rank_shape.has_rank_type, "rank request malformed missing rank type");

    std::string dota7034_connected;
    append_varint_field(dota7034_connected, 1u, 0x010203040506ull);
    append_varint_field(dota7034_connected, 2u, 99u);
    std::string dota7034_leaver_state;
    append_varint_field(dota7034_leaver_state, 1u, 7u);
    append_varint_field(dota7034_leaver_state, 2u, 8u);
    std::string dota7034_disconnected;
    append_varint_field(dota7034_disconnected, 1u, 0x090807060504ull);
    append_bytes_field(dota7034_disconnected, 3u, dota7034_leaver_state);
    std::string dota7034_draft;
    append_varint_field(dota7034_draft, 1u, 0x0a0b0c0dull);
    append_varint_field(dota7034_draft, 2u, 3u);
    append_varint_field(dota7034_draft, 3u, 4u);
    std::string dota7034_body;
    append_bytes_field(dota7034_body, 1u, dota7034_connected);
    append_varint_field(dota7034_body, 2u, 5u);
    append_varint_field(dota7034_body, 8u, 6u);
    append_varint_field(dota7034_body, 6u, 1u);
    append_bytes_field(dota7034_body, 7u, dota7034_disconnected);
    append_varint_field(dota7034_body, 11u, 12u);
    append_varint_field(dota7034_body, 12u, 13u);
    append_varint_field(dota7034_body, 14u, 14u);
    append_varint_field(dota7034_body, 15u, 15u);
    append_bytes_field(dota7034_body, 16u, dota7034_draft);
    Dota7034RequestShape dota7034_shape = parse_dota7034_request_shape(reinterpret_cast<const std::uint8_t *>(dota7034_body.data()), dota7034_body.size());
    ok &= expect_true(dota7034_shape.has_game_state, "7034 shape has game state");
    ok &= expect_eq_u64(dota7034_shape.game_state, 5u, "7034 shape game state");
    ok &= expect_true(dota7034_shape.has_send_reason, "7034 shape has send reason");
    ok &= expect_eq_u64(dota7034_shape.send_reason, 6u, "7034 shape send reason");
    ok &= expect_true(dota7034_shape.has_first_blood_happened, "7034 shape has first blood");
    ok &= expect_eq_u64(dota7034_shape.first_blood_happened, 1u, "7034 shape first blood");
    ok &= expect_eq_u64(dota7034_shape.radiant_kills, 12u, "7034 shape radiant kills");
    ok &= expect_eq_u64(dota7034_shape.dire_kills, 13u, "7034 shape dire kills");
    ok &= expect_eq_u64(dota7034_shape.radiant_lead, 14u, "7034 shape radiant lead");
    ok &= expect_eq_u64(dota7034_shape.building_state, 15u, "7034 shape building state");
    ok &= expect_true(dota7034_shape.has_connected_player, "7034 shape has connected player");
    ok &= expect_eq_size(dota7034_shape.connected_players.size(), 1u, "7034 shape connected count");
    if (dota7034_shape.connected_players.size() == 1u) {
        ok &= expect_true(dota7034_shape.connected_players[0].has_steam_id, "7034 connected has steam id");
        ok &= expect_eq_u64(dota7034_shape.connected_players[0].steam_id, 0x010203040506ull, "7034 connected steam id");
        ok &= expect_true(dota7034_shape.connected_players[0].has_hero_id, "7034 connected has hero id");
        ok &= expect_eq_u64(dota7034_shape.connected_players[0].hero_id, 99u, "7034 connected hero id");
    }
    ok &= expect_true(dota7034_shape.has_disconnected_player, "7034 shape has disconnected player");
    ok &= expect_eq_size(dota7034_shape.disconnected_players.size(), 1u, "7034 shape disconnected count");
    if (dota7034_shape.disconnected_players.size() == 1u) {
        ok &= expect_true(dota7034_shape.disconnected_players[0].has_steam_id, "7034 disconnected has steam id");
        ok &= expect_eq_u64(dota7034_shape.disconnected_players[0].steam_id, 0x090807060504ull, "7034 disconnected steam id");
        ok &= expect_true(dota7034_shape.disconnected_players[0].has_lobby_state, "7034 disconnected has lobby state");
        ok &= expect_eq_u64(dota7034_shape.disconnected_players[0].lobby_state, 7u, "7034 disconnected lobby state");
        ok &= expect_true(dota7034_shape.disconnected_players[0].has_game_state, "7034 disconnected has game state");
        ok &= expect_eq_u64(dota7034_shape.disconnected_players[0].game_state, 8u, "7034 disconnected game state");
    }
    ok &= expect_true(dota7034_shape.has_draft, "7034 shape has draft");
    ok &= expect_true(dota7034_shape.has_draft_steam_id, "7034 shape has draft steam id");
    ok &= expect_eq_u64(dota7034_shape.draft_steam_id, 0x0a0b0c0dull, "7034 shape draft steam id");
    ok &= expect_true(dota7034_shape.has_draft_team, "7034 shape has draft team");
    ok &= expect_eq_u64(dota7034_shape.draft_team, 3u, "7034 shape draft team");
    ok &= expect_true(dota7034_shape.has_draft_team_slot, "7034 shape has draft team slot");
    ok &= expect_eq_u64(dota7034_shape.draft_team_slot, 4u, "7034 shape draft team slot");
    dota7034_shape = parse_dota7034_request_shape(nullptr, 0u);
    ok &= expect_true(!dota7034_shape.has_connected_player, "7034 null has no connected player");
    ok &= expect_true(!dota7034_shape.has_draft, "7034 null has no draft");
    Dota7034RuntimeRequest dota7034_runtime = parse_dota7034_runtime_request(nullptr, 0u);
    ok &= expect_true(!dota7034_runtime.has_game_state, "7034 runtime empty has no game state");
    ok &= expect_true(!dota7034_runtime.has_connected_player, "7034 runtime empty has no connected player");
    dota7034_runtime = parse_dota7034_runtime_request(reinterpret_cast<const std::uint8_t *>(truncated_varint_field.data()), truncated_varint_field.size());
    ok &= expect_true(!dota7034_runtime.has_game_state, "7034 runtime truncated varint has no game state");
    dota7034_runtime = parse_dota7034_runtime_request(reinterpret_cast<const std::uint8_t *>(unknown_wire_type_field.data()), unknown_wire_type_field.size());
    ok &= expect_true(!dota7034_runtime.has_game_state, "7034 runtime unknown wire type has no game state");
    const std::string truncated_length_field_7034({ '\x0a', '\x05', 'a' });
    dota7034_runtime = parse_dota7034_runtime_request(reinterpret_cast<const std::uint8_t *>(truncated_length_field_7034.data()), truncated_length_field_7034.size());
    ok &= expect_true(!dota7034_runtime.has_connected_player, "7034 runtime truncated connected player ignored");
    std::string dota7034_duplicate_body;
    append_bytes_field(dota7034_duplicate_body, 1u, dota7034_connected);
    append_bytes_field(dota7034_duplicate_body, 1u, dota7034_connected);
    append_varint_field(dota7034_duplicate_body, 2u, 5u);
    append_varint_field(dota7034_duplicate_body, 2u, 6u);
    dota7034_runtime = parse_dota7034_runtime_request(reinterpret_cast<const std::uint8_t *>(dota7034_duplicate_body.data()), dota7034_duplicate_body.size());
    ok &= expect_true(dota7034_runtime.has_connected_player, "7034 runtime duplicate has connected player");
    ok &= expect_eq_size(dota7034_runtime.connected_players.size(), 2u, "7034 runtime duplicate keeps repeated connected players");
    ok &= expect_true(dota7034_runtime.has_game_state, "7034 runtime duplicate has game state");
    ok &= expect_eq_u64(dota7034_runtime.game_state, 5u, "7034 runtime duplicate keeps first game state");

    std::string set_team_slot_body;
    append_varint_field(set_team_slot_body, 1u, 1u);
    append_varint_field(set_team_slot_body, 2u, 8u);
    append_varint_field(set_team_slot_body, 3u, 4u);
    DotaPracticeLobbySetTeamSlotRequest set_team_slot_request{};
    ok &= expect_true(parse_dota_practice_lobby_set_team_slot_body(reinterpret_cast<const std::uint8_t *>(set_team_slot_body.data()), set_team_slot_body.size(), set_team_slot_request), "parse set team slot body");
    ok &= expect_true(set_team_slot_request.has_team, "set team slot has team");
    ok &= expect_eq_u64(set_team_slot_request.team, 1u, "set team slot team");
    ok &= expect_true(set_team_slot_request.has_slot, "set team slot has slot");
    ok &= expect_eq_u64(set_team_slot_request.slot, 8u, "set team slot slot");
    ok &= expect_true(set_team_slot_request.has_bot_difficulty, "set team slot has bot difficulty");
    ok &= expect_eq_u64(set_team_slot_request.bot_difficulty, 4u, "set team slot bot difficulty");
    ok &= expect_true(!parse_dota_practice_lobby_set_team_slot_body(nullptr, 0u, set_team_slot_request), "parse empty set team slot body");

    std::string kick_body;
    append_varint_field(kick_body, 3u, 42u);
    DotaPracticeLobbyKickRequest kick_request{};
    ok &= expect_true(parse_dota_practice_lobby_kick_body(reinterpret_cast<const std::uint8_t *>(kick_body.data()), kick_body.size(), kick_request), "parse kick body");
    ok &= expect_true(kick_request.has_account_id, "kick body has account id");
    ok &= expect_eq_u64(kick_request.account_id, 42u, "kick body account id");
    ok &= expect_true(!parse_dota_practice_lobby_kick_body(nullptr, 0u, kick_request), "parse empty kick body");
    ok &= expect_true(!kick_request.has_account_id, "empty kick body resets account id");

    std::string join_body;
    append_varint_field(join_body, 1u, 9001u);
    append_bytes_field(join_body, 3u, "pass");
    DotaPracticeLobbyJoinRequest join_request{};
    ok &= expect_true(parse_dota_practice_lobby_join_body(reinterpret_cast<const std::uint8_t *>(join_body.data()), join_body.size(), join_request), "parse join body");
    ok &= expect_true(join_request.has_lobby_id, "join body has lobby id");
    ok &= expect_eq_u64(join_request.lobby_id, 9001u, "join body lobby id");
    ok &= expect_true(join_request.has_pass_key, "join body has pass key");
    ok &= expect_eq_string(join_request.pass_key, "pass", "join body pass key");
    ok &= expect_true(!parse_dota_practice_lobby_join_body(nullptr, 0u, join_request), "parse empty join body");
    ok &= expect_true(!join_request.has_lobby_id, "empty join body resets lobby id");

    std::string invite_body;
    append_varint_field(invite_body, 1u, 0x0102u);
    append_varint_field(invite_body, 2u, 77u);
    DotaInviteToLobbyRequest invite_request{};
    ok &= expect_true(parse_dota_invite_to_lobby_body(reinterpret_cast<const std::uint8_t *>(invite_body.data()), invite_body.size(), invite_request), "parse invite body");
    ok &= expect_true(invite_request.has_steam_id, "invite body has steam id");
    ok &= expect_eq_u64(invite_request.steam_id, 0x0102u, "invite body steam id");
    ok &= expect_true(invite_request.has_client_version, "invite body has client version");
    ok &= expect_eq_u64(invite_request.client_version, 77u, "invite body client version");
    ok &= expect_true(!parse_dota_invite_to_lobby_body(nullptr, 0u, invite_request), "parse empty invite body");

    std::string invite_response_body;
    append_varint_field(invite_response_body, 1u, 9002u);
    append_varint_field(invite_response_body, 2u, 1u);
    append_varint_field(invite_response_body, 3u, 78u);
    append_varint_field(invite_response_body, 6u, 0x01020304u);
    append_varint_field(invite_response_body, 7u, 123u);
    DotaLobbyInviteResponseRequest invite_response_request{};
    ok &= expect_true(parse_dota_lobby_invite_response_body(reinterpret_cast<const std::uint8_t *>(invite_response_body.data()), invite_response_body.size(), invite_response_request), "parse invite response body");
    ok &= expect_true(invite_response_request.has_lobby_id, "invite response has lobby id");
    ok &= expect_eq_u64(invite_response_request.lobby_id, 9002u, "invite response lobby id");
    ok &= expect_true(invite_response_request.has_accept, "invite response has accept");
    ok &= expect_true(invite_response_request.accept, "invite response accept");
    ok &= expect_true(invite_response_request.has_client_version, "invite response has client version");
    ok &= expect_eq_u64(invite_response_request.client_version, 78u, "invite response client version");
    ok &= expect_true(invite_response_request.has_custom_game_crc, "invite response has custom crc");
    ok &= expect_eq_u64(invite_response_request.custom_game_crc, 0x01020304u, "invite response custom crc");
    ok &= expect_true(invite_response_request.has_custom_game_timestamp, "invite response has custom timestamp");
    ok &= expect_eq_u64(invite_response_request.custom_game_timestamp, 123u, "invite response custom timestamp");
    ok &= expect_true(!parse_dota_lobby_invite_response_body(nullptr, 0u, invite_response_request), "parse empty invite response body");

    std::string join_broadcast_body;
    append_varint_field(join_broadcast_body, 1u, 5u);
    append_bytes_field(join_broadcast_body, 2u, "desc");
    append_bytes_field(join_broadcast_body, 3u, "US");
    append_bytes_field(join_broadcast_body, 4u, "en");
    DotaPracticeLobbyBroadcastChannelRequest broadcast_request{};
    ok &= expect_true(parse_dota_practice_lobby_join_broadcast_channel_body(reinterpret_cast<const std::uint8_t *>(join_broadcast_body.data()), join_broadcast_body.size(), broadcast_request), "parse join broadcast body");
    ok &= expect_true(broadcast_request.has_channel, "join broadcast has channel");
    ok &= expect_eq_u64(broadcast_request.channel, 5u, "join broadcast channel");
    ok &= expect_eq_string(broadcast_request.description, "desc", "join broadcast description");
    ok &= expect_eq_string(broadcast_request.country_code, "US", "join broadcast country");
    ok &= expect_eq_string(broadcast_request.language_code, "en", "join broadcast language");

    std::string update_broadcast_body;
    append_varint_field(update_broadcast_body, 1u, 6u);
    append_bytes_field(update_broadcast_body, 2u, "GB");
    append_bytes_field(update_broadcast_body, 3u, "updated");
    append_bytes_field(update_broadcast_body, 4u, "fr");
    ok &= expect_true(parse_dota_lobby_update_broadcast_channel_info_body(reinterpret_cast<const std::uint8_t *>(update_broadcast_body.data()), update_broadcast_body.size(), broadcast_request), "parse update broadcast body");
    ok &= expect_true(broadcast_request.has_channel, "update broadcast has channel");
    ok &= expect_eq_u64(broadcast_request.channel, 6u, "update broadcast channel");
    ok &= expect_eq_string(broadcast_request.country_code, "GB", "update broadcast country");
    ok &= expect_eq_string(broadcast_request.description, "updated", "update broadcast description");
    ok &= expect_eq_string(broadcast_request.language_code, "fr", "update broadcast language");

    std::string close_broadcast_body;
    append_varint_field(close_broadcast_body, 1u, 7u);
    ok &= expect_true(parse_dota_practice_lobby_close_broadcast_channel_body(reinterpret_cast<const std::uint8_t *>(close_broadcast_body.data()), close_broadcast_body.size(), broadcast_request), "parse close broadcast body");
    ok &= expect_true(broadcast_request.has_channel, "close broadcast has channel");
    ok &= expect_eq_u64(broadcast_request.channel, 7u, "close broadcast channel");
    ok &= expect_true(!parse_dota_practice_lobby_close_broadcast_channel_body(nullptr, 0u, broadcast_request), "parse empty close broadcast body");
    ok &= expect_true(!broadcast_request.has_channel, "empty close broadcast resets channel");

    std::string lobby_details_body;
    append_varint_field(lobby_details_body, 1u, 9003u);
    append_bytes_field(lobby_details_body, 2u, "Room");
    append_varint_field(lobby_details_body, 4u, 3u);
    append_varint_field(lobby_details_body, 25u, 1u);
    append_bytes_field(lobby_details_body, 48u, "lan");
    append_varint_field(lobby_details_body, 5u, 2u);
    append_varint_field(lobby_details_body, 9u, 1u);
    append_varint_field(lobby_details_body, 10u, 1u);
    append_varint_field(lobby_details_body, 11u, 1u);
    append_varint_field(lobby_details_body, 13u, 1u);
    append_bytes_field(lobby_details_body, 15u, "key");
    append_varint_field(lobby_details_body, 33u, 1u);
    append_varint_field(lobby_details_body, 43u, 2u);
    append_varint_field(lobby_details_body, 44u, 0x11u);
    append_varint_field(lobby_details_body, 45u, 0x22u);
    append_bytes_field(lobby_details_body, 26u, "mode");
    append_bytes_field(lobby_details_body, 27u, "map");
    append_varint_field(lobby_details_body, 28u, 4u);
    append_varint_field(lobby_details_body, 29u, 0x010203u);
    append_varint_field(lobby_details_body, 30u, 2u);
    append_varint_field(lobby_details_body, 31u, 10u);
    append_varint_field(lobby_details_body, 34u, 0x01020304u);
    append_fixed32_field(lobby_details_body, 37u, 0x0a0b0c0du);
    append_varint_field(lobby_details_body, 47u, 1u);
    DotaPracticeLobbyDetailsRequest lobby_details_request{};
    ok &= expect_true(parse_dota_practice_lobby_set_details_body(reinterpret_cast<const std::uint8_t *>(lobby_details_body.data()), lobby_details_body.size(), lobby_details_request), "parse lobby details body");
    ok &= expect_true(lobby_details_request.has_lobby_id, "lobby details has lobby id");
    ok &= expect_eq_u64(lobby_details_request.lobby_id, 9003u, "lobby details lobby id");
    ok &= expect_eq_string(lobby_details_request.room_name, "Room", "lobby details room name");
    ok &= expect_eq_u64(lobby_details_request.server_region, 3u, "lobby details server region");
    ok &= expect_true(lobby_details_request.lan, "lobby details lan");
    ok &= expect_eq_string(lobby_details_request.lan_host_ping_location, "lan", "lobby details lan host ping");
    ok &= expect_eq_u64(lobby_details_request.game_mode, 2u, "lobby details game mode");
    ok &= expect_eq_u64(lobby_details_request.bot_difficulty_radiant, 1u, "lobby details bot radiant difficulty");
    ok &= expect_true(lobby_details_request.allow_cheats, "lobby details allow cheats");
    ok &= expect_true(lobby_details_request.fill_with_bots, "lobby details fill bots");
    ok &= expect_true(lobby_details_request.allow_spectating, "lobby details allow spectating");
    ok &= expect_eq_string(lobby_details_request.pass_key, "key", "lobby details pass key");
    ok &= expect_eq_u64(lobby_details_request.visibility, 1u, "lobby details visibility");
    ok &= expect_eq_u64(lobby_details_request.bot_difficulty_dire, 2u, "lobby details bot dire difficulty");
    ok &= expect_eq_u64(lobby_details_request.bot_radiant, 0x11u, "lobby details bot radiant");
    ok &= expect_eq_u64(lobby_details_request.bot_dire, 0x22u, "lobby details bot dire");
    ok &= expect_eq_string(lobby_details_request.custom_game_mode, "mode", "lobby details custom mode");
    ok &= expect_eq_string(lobby_details_request.custom_map_name, "map", "lobby details custom map");
    ok &= expect_eq_u64(lobby_details_request.custom_difficulty, 4u, "lobby details custom difficulty");
    ok &= expect_eq_u64(lobby_details_request.custom_game_id, 0x010203u, "lobby details custom game id");
    ok &= expect_eq_u64(lobby_details_request.custom_min_players, 2u, "lobby details custom min players");
    ok &= expect_eq_u64(lobby_details_request.custom_max_players, 10u, "lobby details custom max players");
    ok &= expect_eq_u64(lobby_details_request.custom_game_crc, 0x01020304u, "lobby details custom crc");
    ok &= expect_eq_u64(lobby_details_request.custom_game_timestamp, 0x0a0b0c0du, "lobby details custom timestamp");
    ok &= expect_true(lobby_details_request.custom_game_penalties, "lobby details custom penalties");
    ok &= expect_true(!parse_dota_practice_lobby_set_details_body(nullptr, 0u, lobby_details_request), "parse empty lobby details body");
    ok &= expect_true(!lobby_details_request.has_lobby_id, "empty lobby details resets lobby id");

    std::string create_body;
    append_bytes_field(create_body, 5u, "create-key");
    append_bytes_field(create_body, 7u, lobby_details_body);
    DotaPracticeLobbyCreateRequest create_request{};
    ok &= expect_true(parse_dota_practice_lobby_create_body(reinterpret_cast<const std::uint8_t *>(create_body.data()), create_body.size(), create_request), "parse lobby create body");
    ok &= expect_true(create_request.has_pass_key, "lobby create has pass key");
    ok &= expect_eq_string(create_request.pass_key, "create-key", "lobby create pass key");
    ok &= expect_true(create_request.has_lobby_details, "lobby create has details");
    ok &= expect_eq_u64(create_request.lobby_details.lobby_id, 9003u, "lobby create nested lobby id");
    ok &= expect_eq_string(create_request.lobby_details.room_name, "Room", "lobby create nested room name");
    ok &= expect_true(!parse_dota_practice_lobby_create_body(nullptr, 0u, create_request), "parse empty lobby create body");
    ok &= expect_true(!create_request.has_lobby_details, "empty lobby create resets details");

    std::string join_chat_body;
    append_bytes_field(join_chat_body, 2u, "Lobby");
    append_varint_field(join_chat_body, 4u, 2u);
    DotaJoinChatChannelRequest join_chat_request{};
    ok &= expect_true(parse_dota_join_chat_channel_body(reinterpret_cast<const std::uint8_t *>(join_chat_body.data()), join_chat_body.size(), join_chat_request), "parse join chat body");
    ok &= expect_true(join_chat_request.has_channel_name, "join chat has channel name");
    ok &= expect_eq_string(join_chat_request.channel_name, "Lobby", "join chat channel name");
    ok &= expect_true(join_chat_request.has_channel_type, "join chat has channel type");
    ok &= expect_eq_u64(join_chat_request.channel_type, 2u, "join chat channel type");
    ok &= expect_true(!parse_dota_join_chat_channel_body(nullptr, 0u, join_chat_request), "parse empty join chat body");

    std::string leave_chat_body;
    append_varint_field(leave_chat_body, 1u, 0x010203u);
    DotaLeaveChatChannelRequest leave_chat_request{};
    ok &= expect_true(parse_dota_leave_chat_channel_body(reinterpret_cast<const std::uint8_t *>(leave_chat_body.data()), leave_chat_body.size(), leave_chat_request), "parse leave chat body");
    ok &= expect_true(leave_chat_request.has_channel_id, "leave chat has channel id");
    ok &= expect_eq_u64(leave_chat_request.channel_id, 0x010203u, "leave chat channel id");
    ok &= expect_true(!parse_dota_leave_chat_channel_body(nullptr, 0u, leave_chat_request), "parse empty leave chat body");
    ok &= expect_true(!leave_chat_request.has_channel_id, "empty leave chat resets channel id");

    std::string chat_message_body;
    append_varint_field(chat_message_body, 1u, 44u);
    append_varint_field(chat_message_body, 2u, 0x010204u);
    append_bytes_field(chat_message_body, 3u, "Persona");
    append_bytes_field(chat_message_body, 4u, "Hello");
    DotaChatMessageRequest chat_message_request{};
    ok &= expect_true(parse_dota_chat_message_body(reinterpret_cast<const std::uint8_t *>(chat_message_body.data()), chat_message_body.size(), chat_message_request), "parse chat message body");
    ok &= expect_true(chat_message_request.has_account_id, "chat message has account id");
    ok &= expect_eq_u64(chat_message_request.account_id, 44u, "chat message account id");
    ok &= expect_true(chat_message_request.has_channel_id, "chat message has channel id");
    ok &= expect_eq_u64(chat_message_request.channel_id, 0x010204u, "chat message channel id");
    ok &= expect_true(chat_message_request.has_persona_name, "chat message has persona");
    ok &= expect_eq_string(chat_message_request.persona_name, "Persona", "chat message persona");
    ok &= expect_true(chat_message_request.has_text, "chat message has text");
    ok &= expect_eq_string(chat_message_request.text, "Hello", "chat message text");
    ok &= expect_true(!parse_dota_chat_message_body(nullptr, 0u, chat_message_request), "parse empty chat message body");
    ok &= expect_true(!chat_message_request.has_text, "empty chat message resets text");

    return ok;
}

bool test_dota_gc_router_response_helpers()
{
    using namespace gbe::proto_wire;

    bool ok = true;
    const std::uint32_t inner_emsg = 26u;
    const std::uint32_t outer_emsg = 5453u;
    const std::uint32_t app_id = 570u;
    const std::uint64_t steam_id = 0x0102030405060708ull;

    std::string inner_body;
    append_varint_field(inner_body, 1u, 123u);
    append_bytes_field(inner_body, 2u, "payload-body");
    std::string inner_payload;
    append_little_endian32(inner_payload, gbe::gc_message::with_proto_mask(inner_emsg));
    append_little_endian32(inner_payload, 0u);
    inner_payload.append(inner_body);

    gbe::dota_gc_router::DotaGcOutboundMessage direct{};
    ok &= expect_true(
        gbe::dota_gc_router::build_outbound_message(inner_emsg, inner_payload, false, nullptr, steam_id, outer_emsg, app_id, direct),
        "router direct outbound builds");
    ok &= expect_eq_u64(direct.emsg, gbe::gc_message::with_proto_mask(inner_emsg), "router direct emsg");
    ok &= expect_eq_string(direct.payload, inner_payload, "router direct payload preserved");

    gbe::dota_gc_router::DotaGcOutboundMessage missing_session{};
    ok &= expect_true(
        !gbe::dota_gc_router::build_outbound_message(inner_emsg, inner_payload, true, nullptr, steam_id, outer_emsg, app_id, missing_session),
        "router wrapped rejects missing session");

    std::string session_field_raw;
    append_varint_field(session_field_raw, 1u, 42u);
    append_bytes_field(session_field_raw, 2u, "session");

    gbe::dota_gc_router::DotaGcOutboundMessage wrapped_a{};
    gbe::dota_gc_router::DotaGcOutboundMessage wrapped_b{};
    ok &= expect_true(
        gbe::dota_gc_router::build_outbound_message(inner_emsg, inner_payload, true, &session_field_raw, steam_id, outer_emsg, app_id, wrapped_a),
        "router wrapped outbound builds");
    ok &= expect_true(
        gbe::dota_gc_router::build_outbound_message(inner_emsg, inner_payload, true, &session_field_raw, steam_id, outer_emsg, app_id, wrapped_b),
        "router wrapped outbound rebuilds");
    ok &= expect_eq_u64(wrapped_a.emsg, gbe::gc_message::with_proto_mask(outer_emsg), "router wrapped outer emsg");
    ok &= expect_eq_string(wrapped_a.payload, wrapped_b.payload, "router wrapped output stable");

    ok &= expect_true(wrapped_a.payload.size() >= 8u, "router wrapped packet has prefix");

    std::uint32_t outer_raw_emsg = 0;
    std::uint32_t outer_header_size = 0;
    if (wrapped_a.payload.size() >= 8u) {
        std::memcpy(&outer_raw_emsg, wrapped_a.payload.data(), sizeof(outer_raw_emsg));
        std::memcpy(&outer_header_size, wrapped_a.payload.data() + sizeof(outer_raw_emsg), sizeof(outer_header_size));
    }
    ok &= expect_eq_u64(gbe::gc_message::without_proto_mask(outer_raw_emsg), outer_emsg, "router wrapped packet raw emsg");
    ok &= expect_true(8u + outer_header_size <= wrapped_a.payload.size(), "router wrapped packet header bounds");

    const bool has_outer_body = 8u + outer_header_size <= wrapped_a.payload.size();
    const std::uint8_t *outer_body = has_outer_body ? reinterpret_cast<const std::uint8_t *>(wrapped_a.payload.data()) + 8u + outer_header_size : nullptr;
    const std::size_t outer_body_size = has_outer_body ? wrapped_a.payload.size() - 8u - outer_header_size : 0u;

    std::string replay_payload;
    ok &= expect_true(
        read_bytes_field(outer_body, outer_body_size, 3u, replay_payload),
        "router wrapped replay payload field");
    ok &= expect_eq_size(replay_payload.size(), inner_payload.size(), "router wrapped inner payload size");
    ok &= expect_eq_string(replay_payload, inner_payload, "router wrapped inner payload preserved");

    std::uint64_t parsed_app_id = 0;
    std::uint64_t parsed_inner_emsg = 0;
    ok &= expect_true(
        read_uint64_field(outer_body, outer_body_size, 1u, parsed_app_id),
        "router wrapped app id field");
    ok &= expect_eq_u64(parsed_app_id, app_id, "router wrapped app id value");
    ok &= expect_true(
        read_uint64_field(outer_body, outer_body_size, 2u, parsed_inner_emsg),
        "router wrapped inner emsg field");
    ok &= expect_eq_u64(parsed_inner_emsg, gbe::gc_message::with_proto_mask(inner_emsg), "router wrapped inner emsg value");

    const std::uint8_t *outer_header = reinterpret_cast<const std::uint8_t *>(wrapped_a.payload.data()) + 8u;
    std::uint64_t parsed_steam_id = 0;
    ok &= expect_true(
        read_uint64_field(outer_header, outer_header_size, 1u, parsed_steam_id),
        "router wrapped steam id header field");
    ok &= expect_eq_u64(parsed_steam_id, steam_id, "router wrapped steam id header value");

    return ok;
}

bool test_dota_gc_router_wrapped_custom_game_lifecycle_requests()
{
    using namespace gbe::proto_wire;

    bool ok = true;
    const std::string session_field_raw("custom-game-session");
    const std::uint64_t request_job_id = 0x0102030405060708ull;
    const std::uint32_t custom_game_lifecycle_emsgs[] = {7070u, 8052u, 8053u};

    for (const std::uint32_t inner_emsg : custom_game_lifecycle_emsgs) {
        std::string inner_header;
        append_fixed64_field(inner_header, 10u, request_job_id);

        std::string inner_body;
        append_varint_field(inner_body, 1u, 99u);

        std::string inner_payload;
        append_little_endian32(inner_payload, gbe::gc_message::with_proto_mask(inner_emsg));
        append_little_endian32(inner_payload, static_cast<std::uint32_t>(inner_header.size()));
        inner_payload.append(inner_header);
        inner_payload.append(inner_body);

        std::string outer_header;
        append_bytes_field(outer_header, 2u, session_field_raw);

        std::string outer_body;
        append_varint_field(outer_body, 1u, 570u);
        append_varint_field(outer_body, 2u, gbe::gc_message::with_proto_mask(inner_emsg));
        append_bytes_field(outer_body, 3u, inner_payload);

        std::string wrapped_request;
        append_little_endian32(wrapped_request, gbe::gc_message::with_proto_mask(GBE_kEMsgClientToGC));
        append_little_endian32(wrapped_request, static_cast<std::uint32_t>(outer_header.size()));
        wrapped_request.append(outer_header);
        wrapped_request.append(outer_body);

        gbe::dota_gc_router::DotaGcRequestContext context{};
        ok &= expect_true(
            gbe::dota_gc_router::extract_wrapped_post_login_request(
                wrapped_request.data(),
                static_cast<std::uint32_t>(wrapped_request.size()),
                GBE_kEMsgClientToGC,
                context),
            "extract wrapped custom game lifecycle request");
        ok &= expect_true(context.valid, "wrapped custom game lifecycle context valid");
        ok &= expect_true(context.wrapped, "wrapped custom game lifecycle context wrapped");
        ok &= expect_eq_u64(context.inner_emsg, inner_emsg, "wrapped custom game lifecycle emsg");
        ok &= expect_eq_string(context.outer_session_field_raw, session_field_raw, "wrapped custom game lifecycle session");
        ok &= expect_true(context.has_request_job, "wrapped custom game lifecycle has request job");
        ok &= expect_eq_u64(context.request_job_id, request_job_id, "wrapped custom game lifecycle request job");
        ok &= expect_eq_string(context.body, inner_body, "wrapped custom game lifecycle body");
        ok &= expect_true(
            gbe::gc_message::is_supported_dota_wrapped_post_login_request(context.inner_emsg),
            "wrapped custom game lifecycle reaches post login gate");
    }

    return ok;
}

bool test_dota_lobby_state_helpers()
{
    bool ok = true;

    GBE_LocalLobby local{};
    local.active = true;
    local.generation = 0x0101ull;
    local.lobby_id = 0x0102ull;
    local.generic_lobby_id = 0x0304ull;
    local.room_name = "room";
    local.game_mode = 2u;
    local.server_region = 3u;
    local.state = 1u;
    local.game_state = 0u;
    local.server_id = 0x0506ull;
    local.match_id = 0x0708ull;
    local.owner_steam_id = 0x090aull;
    local.owner_account_id = 11u;
    local.owner_name = "owner";
    local.connect = "10.0.0.1:27015 10.0.0.2:27016";
    local.custom_game.game_id = 12345ull;
    local.has_cache_version = true;
    local.cache_version = 99ull;
    local.cache_service_list = { 1u, 2u };

    GBE_SharedDotaLobbyState shared{};
    shared.state = 4u;
    shared.game_state = 3u;
    shared.server_id = 0x1111ull;
    gbe::dota_lobby_state::publish_local_lobby_to_shared(local, false, shared);
    ok &= expect_true(shared.valid, "lobby state publish client valid");
    ok &= expect_eq_u64(shared.generation, local.generation, "lobby state publish generation");
    ok &= expect_eq_u64(shared.lobby_id, local.lobby_id, "lobby state publish lobby id");
    ok &= expect_eq_string(shared.room_name, local.room_name, "lobby state publish room name");
    ok &= expect_eq_u64(shared.state, 4u, "lobby state client preserves higher state");
    ok &= expect_eq_u64(shared.game_state, 3u, "lobby state client preserves higher game state");
    ok &= expect_eq_u64(shared.server_id, local.server_id, "lobby state client accepts nonzero server id");
    ok &= expect_eq_size(shared.cache_service_list.size(), 2u, "lobby state publish cache service list");

    shared.owner_hero_id = 57u;
    local.owner_hero_id = 0u;
    gbe::dota_lobby_state::publish_local_lobby_to_shared(local, false, shared);
    ok &= expect_eq_u64(shared.owner_hero_id, 57u, "lobby state client preserves known owner hero for the same active lobby owner");

    local.lobby_id = 0x0b0cull;
    gbe::dota_lobby_state::publish_local_lobby_to_shared(local, false, shared);
    ok &= expect_eq_u64(shared.owner_hero_id, 0u, "lobby state client clears owner hero when switching lobbies");

    local.lobby_id = 0x0102ull;
    shared.lobby_id = local.lobby_id;
    shared.owner_steam_id = local.owner_steam_id;
    local.owner_hero_id = 71u;
    gbe::dota_lobby_state::publish_local_lobby_to_shared(local, false, shared);
    ok &= expect_eq_u64(shared.owner_hero_id, 71u, "lobby state client publishes a confirmed owner hero");

    local.state = 2u;
    local.game_state = 2u;
    local.server_id = 0x2222ull;
    gbe::dota_lobby_state::publish_local_lobby_to_shared(local, true, shared);
    ok &= expect_eq_u64(shared.state, 2u, "lobby state server overwrites state");
    ok &= expect_eq_u64(shared.game_state, 2u, "lobby state server overwrites game state");
    ok &= expect_eq_u64(shared.server_id, 0x2222ull, "lobby state server overwrites server id");

    GBE_LocalLobby restored{};
    gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, restored);
    ok &= expect_true(restored.active, "lobby state restore active");
    ok &= expect_eq_u64(restored.generation, local.generation, "lobby state restore generation");
    ok &= expect_eq_u64(restored.lobby_id, local.lobby_id, "lobby state restore lobby id");
    ok &= expect_eq_u64(restored.generic_lobby_id, local.generic_lobby_id, "lobby state restore generic lobby id");
    ok &= expect_eq_string(restored.room_name, local.room_name, "lobby state restore room name");
    ok &= expect_eq_u64(restored.state, local.state, "lobby state restore state");
    ok &= expect_eq_u64(restored.game_state, local.game_state, "lobby state restore game state");
    ok &= expect_eq_u64(restored.match_id, local.match_id, "lobby state restore match id");
    ok &= expect_eq_u64(restored.server_id, local.server_id, "lobby state restore server id");
    ok &= expect_eq_u64(restored.owner_steam_id, local.owner_steam_id, "lobby state restore owner steam id");
    ok &= expect_eq_u64(restored.owner_account_id, local.owner_account_id, "lobby state restore owner account id");
    ok &= expect_eq_string(restored.owner_name, local.owner_name, "lobby state restore owner name");
    ok &= expect_eq_string(restored.connect, "10.0.0.1:27015", "lobby state restore normalized connect");
    ok &= expect_eq_u64(restored.custom_game.game_id, local.custom_game.game_id, "lobby state restore custom game id");
    ok &= expect_true(restored.has_cache_version, "lobby state restore cache version present");
    ok &= expect_eq_u64(restored.cache_version, local.cache_version, "lobby state restore cache version");
    ok &= expect_eq_size(restored.cache_service_list.size(), 2u, "lobby state restore cache service list");

    shared.match_id = 0ull;
    shared.server_id = 0x3333ull;
    gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, true, false, restored);
    ok &= expect_eq_u64(restored.server_id, 0ull, "lobby state restore clears server without match");

    shared.custom_game = GBE_DotaCustomGameDetails{};
    shared.custom_game.map_name = "custom_map_without_id";
    shared.state = 4u;
    shared.game_state = 2u;
    gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, false, true, restored);
    ok &= expect_eq_u64(restored.state, 2u, "lobby state restore normalizes custom readyup run state");

    gbe::dota_lobby_state::adopt_shared_lobby_to_local(shared, true, false, restored);
    ok &= expect_eq_u64(restored.state, 4u, "lobby state restore preserves readyup state without normalization");

    GBE_DotaReconnectContext reconnect{};
    auto reconnect_source = gbe::dota_reconnect::source_from_local_lobby(local);
    ok &= expect_true(
        gbe::dota_reconnect::build_context(reconnect_source, reconnect) == gbe::dota_reconnect::RejectReason::None,
        "lobby state reconnect builds");
    ok &= expect_eq_u64(reconnect.generation, local.generation, "lobby state reconnect generation");
    ok &= expect_eq_u64(reconnect.server_id, local.server_id, "lobby state reconnect server id");
    ok &= expect_eq_u64(reconnect.lobby_state, local.state, "lobby state reconnect lobby state");
    ok &= expect_eq_u64(reconnect.game_state, local.game_state, "lobby state reconnect game state");
    ok &= expect_eq_u64(reconnect.custom_game_id, local.custom_game.game_id, "lobby state reconnect custom id");
    ok &= expect_eq_u64(reconnect.owner_steam_id, local.owner_steam_id, "lobby state reconnect owner");
    ok &= expect_eq_string(reconnect.connect, "10.0.0.1:27015", "lobby state reconnect first endpoint");

    local.server_id = 0ull;
    reconnect_source = gbe::dota_reconnect::source_from_local_lobby(local);
    ok &= expect_true(
        gbe::dota_reconnect::build_context(reconnect_source, reconnect) != gbe::dota_reconnect::RejectReason::None,
        "lobby state reconnect rejects missing server");

    gbe::proto_wire::DotaPracticeLobbyCreateRequest create_request{};
    create_request.has_pass_key = true;
    create_request.pass_key = "top-level-pass";
    create_request.has_lobby_details = true;
    create_request.lobby_details.has_room_name = true;
    create_request.lobby_details.room_name = "created-room";
    create_request.lobby_details.has_server_region = true;
    create_request.lobby_details.server_region = 12u;
    create_request.lobby_details.has_lan = true;
    create_request.lobby_details.lan = false;
    create_request.lobby_details.has_fill_with_bots = true;
    create_request.lobby_details.fill_with_bots = false;
    create_request.lobby_details.has_pass_key = true;
    create_request.lobby_details.pass_key = "details-pass";
    create_request.lobby_details.has_custom_game_id = true;
    create_request.lobby_details.custom_game_id = 777ull;
    create_request.lobby_details.has_custom_map_name = true;
    create_request.lobby_details.custom_map_name = "custom-map";
    const gbe::dota_lobby_state::CreateLobbyPlan create_plan = gbe::dota_lobby_state::compose_create_lobby_plan(
        create_request,
        0x5555ull,
        0x6666ull,
        77u,
        "creator",
        0u,
        1u);
    ok &= expect_true(create_plan.lobby.active, "create lobby plan active");
    ok &= expect_eq_u64(create_plan.lobby.lobby_id, 0x5555ull, "create lobby plan lobby id");
    ok &= expect_eq_u64(create_plan.lobby.owner_steam_id, 0x6666ull, "create lobby plan owner steam id");
    ok &= expect_eq_u64(create_plan.lobby.owner_account_id, 77u, "create lobby plan owner account id");
    ok &= expect_eq_string(create_plan.lobby.owner_name, "creator", "create lobby plan owner name");
    ok &= expect_eq_string(create_plan.lobby.room_name, "created-room", "create lobby plan room name");
    ok &= expect_eq_u64(create_plan.lobby.server_region, 12u, "create lobby plan server region");
    ok &= expect_true(!create_plan.lobby.lan, "create lobby plan lan override");
    ok &= expect_true(!create_plan.lobby.fill_with_bots, "create lobby plan fill bots override");
    ok &= expect_eq_string(create_plan.lobby.pass_key, "details-pass", "create lobby plan details pass key wins");
    ok &= expect_eq_u64(create_plan.lobby.custom_game.game_id, 777ull, "create lobby plan custom game id");
    ok &= expect_eq_string(create_plan.lobby.custom_game.map_name, "custom-map", "create lobby plan custom map");
    ok &= expect_true(create_plan.custom_game_create, "create lobby plan custom flag");
    const gbe::dota_lobby_state::CreateLobbyStateApplyPlan create_state_apply_plan = gbe::dota_lobby_state::compose_create_lobby_state_apply_plan(create_plan, true);
    ok &= expect_eq_u64(create_state_apply_plan.lobby.lobby_id, create_plan.lobby.lobby_id, "create state apply lobby id");
    ok &= expect_true(create_state_apply_plan.normalize_custom_game_details, "create state apply normalizes details");
    ok &= expect_true(create_state_apply_plan.normalize_arcade_member_slots, "create state apply normalizes arcade slots");
    ok &= expect_true(create_state_apply_plan.clear_reconnect_context, "create state apply clears reconnect context");
    ok &= expect_true(create_state_apply_plan.set_reconnect_eligible, "create state apply sets reconnect eligible");
    ok &= expect_true(create_state_apply_plan.log_arcade_isolation, "create state apply logs arcade isolation");
    ok &= expect_eq_size(create_plan.lobby.members.size(), 1u, "create lobby plan owner member count");
    if (!create_plan.lobby.members.empty()) {
        ok &= expect_eq_u64(create_plan.lobby.members[0].steam_id, 0x6666ull, "create lobby plan owner member steam id");
        ok &= expect_eq_u64(create_plan.lobby.members[0].account_id, 77u, "create lobby plan owner member account id");
        ok &= expect_eq_u64(create_plan.lobby.members[0].team, 0u, "create lobby plan owner member team");
        ok &= expect_eq_u64(create_plan.lobby.members[0].slot, 1u, "create lobby plan owner member slot");
        ok &= expect_true(!create_plan.lobby.members[0].connected, "create lobby plan owner member disconnected");
    }

    create_request.lobby_details.has_pass_key = false;
    const gbe::dota_lobby_state::CreateLobbyPlan fallback_pass_plan = gbe::dota_lobby_state::compose_create_lobby_plan(
        create_request,
        0x5556ull,
        0x6667ull,
        78u,
        "creator2",
        0u,
        1u);
    ok &= expect_eq_string(fallback_pass_plan.lobby.pass_key, "top-level-pass", "create lobby plan top-level pass fallback");

    gbe::proto_wire::DotaPracticeLobbyCreateRequest plain_create_request{};
    const gbe::dota_lobby_state::CreateLobbyPlan plain_create_plan = gbe::dota_lobby_state::compose_create_lobby_plan(
        plain_create_request,
        0x5557ull,
        0x6668ull,
        79u,
        "creator3",
        0u,
        1u);
    const gbe::dota_lobby_state::CreateLobbyStateApplyPlan plain_state_apply_plan = gbe::dota_lobby_state::compose_create_lobby_state_apply_plan(plain_create_plan, false);
    ok &= expect_true(!plain_state_apply_plan.normalize_custom_game_details, "plain create state apply skips details normalize");
    ok &= expect_true(!plain_state_apply_plan.normalize_arcade_member_slots, "plain create state apply skips arcade slots");
    ok &= expect_true(!plain_state_apply_plan.clear_reconnect_context, "plain create state apply keeps reconnect context");
    ok &= expect_true(!plain_state_apply_plan.set_reconnect_eligible, "plain create state apply keeps reconnect eligibility");
    ok &= expect_true(!plain_state_apply_plan.log_arcade_isolation, "plain create state apply skips isolation log");

    GBE_LocalLobby previous_lobby{};
    previous_lobby.active = true;
    previous_lobby.lobby_id = 0x7000ull;
    previous_lobby.match_id = 0x8000ull;
    previous_lobby.custom_game.game_id = 0ull;
    previous_lobby.state = 2u;
    previous_lobby.game_state = 1u;
    previous_lobby.owner_team = 0u;
    previous_lobby.owner_slot = 1u;
    GBE_DotaCustomGameDetails requested_custom_game{};
    requested_custom_game.game_id = 0x9000ull;
    const gbe::dota_lobby_state::CreateLobbyResetPlan reset_plan = gbe::dota_lobby_state::compose_create_lobby_reset_plan(previous_lobby, requested_custom_game);
    ok &= expect_true(reset_plan.reset_gc_memory, "create reset plan resets GC memory");
    ok &= expect_true(reset_plan.reset_reason == "7038_create", "create reset plan reset reason");
    ok &= expect_true(reset_plan.reset_leave_generic_lobby, "create reset plan leaves generic lobby");
    ok &= expect_true(reset_plan.reset_clear_queued_messages, "create reset plan clears queued messages");
    ok &= expect_true(reset_plan.custom_game_create, "create reset plan custom game create");
    ok &= expect_true(reset_plan.unsubscribe_previous_practice_lobby, "create reset plan unsubscribes previous practice lobby");
    ok &= expect_eq_u64(reset_plan.previous_lobby_id, previous_lobby.lobby_id, "create reset plan previous lobby id");
    ok &= expect_eq_u64(reset_plan.previous_match_id, previous_lobby.match_id, "create reset plan previous match id");
    ok &= expect_eq_u64(reset_plan.previous_state, previous_lobby.state, "create reset plan previous state");
    ok &= expect_eq_u64(reset_plan.previous_game_state, previous_lobby.game_state, "create reset plan previous game state");
    previous_lobby.custom_game.game_id = 0x9000ull;
    const gbe::dota_lobby_state::CreateLobbyResetPlan custom_previous_reset_plan = gbe::dota_lobby_state::compose_create_lobby_reset_plan(previous_lobby, requested_custom_game);
    ok &= expect_true(!custom_previous_reset_plan.unsubscribe_previous_practice_lobby, "create reset plan keeps previous custom lobby");

    GBE_LocalLobby matched_lobby{};
    matched_lobby.active = true;
    matched_lobby.lobby_id = 0xabcull;
    matched_lobby.generic_lobby_id = 0xdefull;
    matched_lobby.room_name = "matched-room";
    matched_lobby.game_mode = 22u;
    matched_lobby.server_region = 33u;
    matched_lobby.pass_key = "matched-pass";
    matched_lobby.custom_game.game_id = 444ull;
    matched_lobby.state = 2u;
    matched_lobby.game_state = 1u;
    matched_lobby.match_id = 555ull;
    matched_lobby.server_id = 666ull;
    matched_lobby.connect = "127.0.0.1:27015";
    matched_lobby.owner_steam_id = 700ull;
    matched_lobby.owner_account_id = 70u;
    matched_lobby.owner_name = "matched-owner";
    matched_lobby.owner_team = 0u;
    matched_lobby.owner_slot = 1u;
    matched_lobby.members.push_back(GBE_DotaLobbyMemberState{700ull, 70u, 0u, 1u, 0u, true, 0u});
    matched_lobby.members.push_back(GBE_DotaLobbyMemberState{800ull, 80u, 4u, 2u, 0u, true, 0u});
    const gbe::dota_lobby_state::JoinLobbyMergePlan join_plan = gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        GBE_LocalLobby{},
        true,
        matched_lobby.lobby_id,
        false,
        std::string(),
        true,
        matched_lobby,
        800ull,
        80u,
        "local-player",
        0u,
        4u);
    ok &= expect_eq_u64(join_plan.lobby.lobby_id, matched_lobby.lobby_id, "join merge lobby id");
    ok &= expect_eq_u64(join_plan.lobby.generic_lobby_id, matched_lobby.generic_lobby_id, "join merge generic lobby id");
    ok &= expect_eq_string(join_plan.lobby.room_name, matched_lobby.room_name, "join merge room name");
    ok &= expect_eq_u64(join_plan.lobby.custom_game.game_id, matched_lobby.custom_game.game_id, "join merge custom game id");
    ok &= expect_eq_u64(join_plan.lobby.owner_steam_id, matched_lobby.owner_steam_id, "join merge owner steam id");
    ok &= expect_true(join_plan.seen_local_in_generic_lobby, "join merge sees local member");
    ok &= expect_eq_u64(join_plan.local_member.steam_id, 800ull, "join merge local member steam id");
    ok &= expect_eq_u64(join_plan.local_member.account_id, 80u, "join merge local member account id");
    ok &= expect_eq_u64(join_plan.local_member.team, 0u, "join merge local member normalized team");
    ok &= expect_true(join_plan.lobby.members.size() >= 2u, "join merge member count");

    const gbe::dota_lobby_state::JoinLobbyMergePlan fallback_join_plan = gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        GBE_LocalLobby{},
        true,
        0x1234ull,
        true,
        std::string("fallback-pass"),
        false,
        GBE_LocalLobby{},
        900ull,
        90u,
        "fallback-owner",
        0u,
        4u);
    ok &= expect_true(fallback_join_plan.lobby.active, "join merge fallback active");
    ok &= expect_eq_u64(fallback_join_plan.lobby.lobby_id, 0x1234ull, "join merge fallback lobby id");
    ok &= expect_eq_u64(fallback_join_plan.lobby.owner_steam_id, 900ull, "join merge fallback owner steam id");
    ok &= expect_eq_string(fallback_join_plan.lobby.owner_name, "fallback-owner", "join merge fallback owner name");
    ok &= expect_eq_string(fallback_join_plan.lobby.pass_key, "fallback-pass", "join merge fallback request pass key");
    ok &= expect_eq_u64(fallback_join_plan.lobby.game_mode, 2u, "join merge fallback game mode");
    ok &= expect_eq_u64(fallback_join_plan.lobby.server_region, 15u, "join merge fallback server region");
    ok &= expect_true(fallback_join_plan.lobby.allow_spectating, "join merge fallback spectating");

    GBE_LocalLobby seen_lobby = fallback_join_plan.lobby;
    seen_lobby.seen_local_in_generic_lobby = true;
    const gbe::dota_lobby_state::JoinLobbyMergePlan preserved_seen_join_plan = gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        seen_lobby,
        false,
        0ull,
        false,
        std::string(),
        false,
        GBE_LocalLobby{},
        900ull,
        90u,
        "fallback-owner",
        0u,
        4u);
    ok &= expect_true(preserved_seen_join_plan.seen_local_in_generic_lobby, "join merge preserves seen local flag");
    ok &= expect_true(preserved_seen_join_plan.lobby.seen_local_in_generic_lobby, "join merge preserves lobby seen local flag");

    GBE_LocalLobby launch_lobby{};
    launch_lobby.active = true;
    launch_lobby.lobby_id = 0x2222ull;
    const gbe::dota_lobby_state::LaunchInitPlan launch_init_plan = gbe::dota_lobby_state::compose_launch_init_plan(
        launch_lobby,
        0x3333ull,
        0x4444ull,
        "192.168.1.10:27015",
        123456u,
        1u);
    ok &= expect_eq_u64(launch_init_plan.lobby.match_id, 0x3333ull, "launch init match id");
    ok &= expect_eq_u64(launch_init_plan.lobby.server_id, 0x4444ull, "launch init server id");
    ok &= expect_eq_string(launch_init_plan.lobby.connect, "192.168.1.10:27015", "launch init connect");
    ok &= expect_eq_u64(launch_init_plan.lobby.game_start_time, 123456u, "launch init game start time");
    ok &= expect_eq_u64(launch_init_plan.lobby.launch_phase, 1u, "launch init requested phase");

    GBE_LocalLobby custom_launch_lobby = launch_init_plan.lobby;
    custom_launch_lobby.custom_game.game_id = 0x5555ull;
    custom_launch_lobby.state = 0u;
    custom_launch_lobby.game_state = 9u;
    const gbe::dota_lobby_state::CustomGameLaunchSetupPlan custom_launch_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_plan(custom_launch_lobby, 2u);
    ok &= expect_eq_u64(custom_launch_plan.readyup_lobby.state, 4u, "custom launch readyup state");
    ok &= expect_eq_u64(custom_launch_plan.readyup_lobby.game_state, 0u, "custom launch readyup game state");
    ok &= expect_eq_u64(custom_launch_plan.readyup_lobby.launch_phase, 1u, "custom launch readyup preserves phase");
    ok &= expect_eq_u64(custom_launch_plan.serversetup_lobby.state, 1u, "custom launch setup state");
    ok &= expect_eq_u64(custom_launch_plan.serversetup_lobby.game_state, 0u, "custom launch setup game state");
    ok &= expect_eq_u64(custom_launch_plan.serversetup_lobby.launch_phase, 1u, "custom launch setup preserves phase");
    ok &= expect_eq_u64(custom_launch_plan.synced_launch_phase, 2u, "custom launch synced phase");

    GBE_LocalLobby run_lobby = custom_launch_plan.serversetup_lobby;
    run_lobby.launch_phase = 2u;
    const gbe::dota_lobby_state::LaunchRunPlan run_plan = gbe::dota_lobby_state::compose_launch_run_plan(run_lobby, 2u, 3u, 0u);
    ok &= expect_true(run_plan.can_advance, "launch run plan can advance");
    ok &= expect_eq_u64(run_plan.launch_phase, 3u, "launch run plan phase");
    ok &= expect_eq_u64(run_plan.next_state, 2u, "launch run plan next state");
    ok &= expect_eq_u64(run_plan.next_game_state, 0u, "launch run plan next game state");

    run_lobby.match_id = 0ull;
    const gbe::dota_lobby_state::LaunchRunPlan blocked_run_plan = gbe::dota_lobby_state::compose_launch_run_plan(run_lobby, 2u, 3u, 0u);
    ok &= expect_true(!blocked_run_plan.can_advance, "launch run plan blocks missing setup sync");

    GBE_LocalLobby queued_lobby = custom_launch_plan.serversetup_lobby;
    queued_lobby.launch_phase = 1u;
    const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan setup_apply_plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
        queued_lobby,
        1u,
        0u,
        true,
        2u,
        3u);
    ok &= expect_eq_u64(setup_apply_plan.state, 1u, "queued setup apply state");
    ok &= expect_eq_u64(setup_apply_plan.game_state, 0u, "queued setup apply game state");
    ok &= expect_eq_u64(setup_apply_plan.launch_phase, 2u, "queued setup apply phase");

    const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan run_apply_plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
        queued_lobby,
        2u,
        0u,
        true,
        2u,
        3u);
    ok &= expect_eq_u64(run_apply_plan.state, 2u, "queued run apply state");
    ok &= expect_eq_u64(run_apply_plan.game_state, 0u, "queued run apply game state");
    ok &= expect_eq_u64(run_apply_plan.launch_phase, 3u, "queued run apply phase");

    queued_lobby.state = 2u;
    queued_lobby.game_state = 2u;
    queued_lobby.launch_phase = 3u;
    const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan monotonic_apply_plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
        queued_lobby,
        2u,
        0u,
        true,
        2u,
        3u);
    ok &= expect_true(monotonic_apply_plan.preserved_game_state, "queued apply preserves game state flag");
    ok &= expect_eq_u64(monotonic_apply_plan.game_state, 2u, "queued apply preserves game state value");
    ok &= expect_eq_u64(monotonic_apply_plan.launch_phase, 3u, "queued apply preserves launch phase");

    const gbe::dota_lobby_state::PracticeLobbyLaunchEventPlan practice_launch_events = gbe::dota_lobby_state::compose_practice_lobby_launch_event_plan(26u);
    ok &= expect_true(practice_launch_events.initial_details.send, "practice launch sends initial details");
    ok &= expect_eq_u64(practice_launch_events.initial_details.emsg, 26u, "practice launch details emsg");
    ok &= expect_eq_string(practice_launch_events.initial_details.reason, "7041_initial_26", "practice launch details reason");
    ok &= expect_true(practice_launch_events.initial_details.apply_lobby_state, "practice launch details applies state");
    ok &= expect_eq_u64(practice_launch_events.initial_details.lobby_state, 1u, "practice launch details state");
    ok &= expect_eq_u64(practice_launch_events.initial_details.lobby_game_state, 0u, "practice launch details game state");
    ok &= expect_eq_u64(practice_launch_events.initial_details.lobby_source, gbe::dota_lobby_state::LaunchDetailsLobbySourceCurrent, "practice launch details source");
    ok &= expect_true(practice_launch_events.steam_auth_ack.queue, "practice launch queues steam auth ack");
    ok &= expect_eq_string(practice_launch_events.steam_auth_ack.reason, "7041_serversetup", "practice launch steam auth reason");
    ok &= expect_true(practice_launch_events.presence.update, "practice launch updates presence");
    ok &= expect_eq_string(practice_launch_events.presence.status, "#DOTA_RP_INIT", "practice launch presence status");
    ok &= expect_eq_string(practice_launch_events.presence.lobby_state, "SERVERSETUP", "practice launch presence lobby state");
    ok &= expect_true(!practice_launch_events.presence.include_party, "practice launch presence excludes party");
    ok &= expect_true(practice_launch_events.presence.include_lobby, "practice launch persona includes lobby");
    ok &= expect_eq_string(practice_launch_events.presence.persona_reason, "7041_launch_init", "practice launch persona reason");

    const gbe::dota_lobby_state::LaunchPresenceEvent custom_init_presence = gbe::dota_lobby_state::compose_launch_serversetup_presence_event("7041_custom_game_launch_init");
    ok &= expect_true(custom_init_presence.update, "custom launch init updates presence");
    ok &= expect_eq_string(custom_init_presence.persona_reason, "7041_custom_game_launch_init", "custom launch init persona reason");

    const gbe::dota_lobby_state::CustomGameLaunchSetupEventPlan custom_launch_events = gbe::dota_lobby_state::compose_custom_game_launch_setup_event_plan(26u);
    ok &= expect_eq_size(custom_launch_events.details_events.size(), 2u, "custom launch details event count");
    if (custom_launch_events.details_events.size() == 2u) {
        ok &= expect_eq_string(custom_launch_events.details_events[0].reason, "7041_custom_game_readyup", "custom launch readyup reason");
        ok &= expect_true(!custom_launch_events.details_events[0].apply_lobby_state, "custom launch readyup no apply state");
        ok &= expect_eq_u64(custom_launch_events.details_events[0].lobby_state, 4u, "custom launch readyup state");
        ok &= expect_eq_u64(custom_launch_events.details_events[0].lobby_source, gbe::dota_lobby_state::LaunchDetailsLobbySourceReadyUp, "custom launch readyup source");
        ok &= expect_eq_string(custom_launch_events.details_events[1].reason, "7041_custom_game_serversetup", "custom launch setup reason");
        ok &= expect_true(custom_launch_events.details_events[1].apply_lobby_state, "custom launch setup apply state");
        ok &= expect_eq_u64(custom_launch_events.details_events[1].lobby_state, 1u, "custom launch setup event state");
        ok &= expect_eq_u64(custom_launch_events.details_events[1].lobby_source, gbe::dota_lobby_state::LaunchDetailsLobbySourceServerSetup, "custom launch setup source");
    }
    ok &= expect_eq_string(custom_launch_events.mark_phase_reason, "7041_custom_game_serversetup_synced", "custom launch phase reason");
    ok &= expect_true(custom_launch_events.steam_auth_ack.queue, "custom launch queues steam auth ack");
    ok &= expect_eq_string(custom_launch_events.steam_auth_ack.reason, "7041_custom_game_serversetup", "custom launch steam auth reason");

    return ok;
}

bool test_patch_and_rewrite_helpers()
{
    using namespace gbe::proto_wire;

    bool ok = true;
    const std::vector<std::uint8_t> mixed{
        0x08, 0x01,
        0x11, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x1a, 0x03, 0x61, 0x62, 0x63,
        0x25, 0x0d, 0x0c, 0x0b, 0x0a,
    };

    Field field{};
    std::size_t offset = 0;
    std::size_t field_offset = 0;
    std::size_t field_end = 0;
    ok &= expect_true(read_next_field(mixed.data(), mixed.size(), offset, field, &field_offset, &field_end), "read next field");
    std::size_t baseline_next_offset = 0;
    std::uint32_t baseline_field_number = 0;
    std::uint32_t baseline_wire_type = 0;
    std::size_t baseline_field_offset = 0;
    std::size_t baseline_value_offset = 0;
    std::size_t baseline_value_size = 0;
    std::size_t baseline_field_end = 0;
    ok &= expect_true(baseline_read_next_proto_field(
                          mixed.data(),
                          mixed.size(),
                          baseline_next_offset,
                          baseline_field_number,
                          baseline_wire_type,
                          baseline_field_offset,
                          baseline_value_offset,
                          baseline_value_size,
                          baseline_field_end),
                      "baseline read next field");
    ok &= expect_eq_u64(field.number, baseline_field_number, "read next field baseline number match");
    ok &= expect_eq_u64(field.wire_type, baseline_wire_type, "read next field baseline wire match");
    ok &= expect_eq_size(field_offset, baseline_field_offset, "read next field baseline offset match");
    ok &= expect_eq_size(field.value_offset, baseline_value_offset, "read next field baseline value offset match");
    ok &= expect_eq_size(field.value_size, baseline_value_size, "read next field baseline value size match");
    ok &= expect_eq_size(field_end, baseline_field_end, "read next field baseline end match");
    ok &= expect_eq_size(offset, baseline_next_offset, "read next field baseline next offset match");
    ok &= expect_eq_u64(field_offset, 0u, "next field offset");
    ok &= expect_eq_u64(field_end, 2u, "next field end");

    std::string encoded;
    append_varuint(encoded, 300u);
    ok &= expect_eq_string(hex_string(encoded), "ac02", "append varuint");

    encoded.clear();
    append_varint_field(encoded, 1u, 1u);
    append_fixed64_field(encoded, 2u, 0x0102030405060708ull);
    append_bytes_field(encoded, 3u, "abc");
    append_fixed32_field(encoded, 4u, 0x0a0b0c0du);
    ok &= expect_eq_string(hex_string(encoded), "08011108070605040302011a03616263250d0c0b0a", "append fields");

    std::string rewritten;
    bool rewrote = false;
    ok &= expect_true(rewrite_varint_fields(encoded, std::vector<std::uint32_t>{1u, 4u}, 9u, rewritten, &rewrote), "rewrite varint fields");
    std::string baseline_rewritten;
    bool baseline_rewrote = false;
    ok &= expect_true(baseline_rewrite_varint_fields(encoded, std::vector<std::uint32_t>{1u, 4u}, 9u, baseline_rewritten, &baseline_rewrote), "baseline rewrite varint fields");
    ok &= expect_true(rewrote == baseline_rewrote, "rewrite varint fields baseline rewrote match");
    ok &= expect_eq_string(rewritten, baseline_rewritten, "rewrite varint fields baseline bytes match");
    ok &= expect_true(rewrote, "rewrite varint fields changed");
    ok &= expect_eq_string(hex_string(rewritten), "08091108070605040302011a03616263250d0c0b0a", "rewrite varint fields bytes");

    std::string recursive_nested;
    append_varint_field(recursive_nested, 1u, 5u);
    append_varint_field(recursive_nested, 2u, 6u);
    std::string recursive_input;
    append_varint_field(recursive_input, 1u, 5u);
    append_bytes_field(recursive_input, 9u, recursive_nested);
    append_varint_field(recursive_input, 3u, 5u);
    std::string old_varint_5;
    append_varuint(old_varint_5, 5u);
    std::size_t recursive_replacements = 0;
    ok &= expect_true(rewrite_varint_bytes_recursive(
                          recursive_input,
                          std::vector<std::uint8_t>(old_varint_5.begin(), old_varint_5.end()),
                          1u,
                          42u,
                          rewritten,
                          recursive_replacements),
                      "rewrite varint bytes recursive");
    std::size_t baseline_recursive_replacements = 0;
    ok &= expect_true(baseline_rewrite_varint_bytes_recursive(
                          recursive_input,
                          std::vector<std::uint8_t>(old_varint_5.begin(), old_varint_5.end()),
                          1u,
                          42u,
                          baseline_rewritten,
                          baseline_recursive_replacements),
                      "baseline rewrite varint bytes recursive");
    ok &= expect_eq_size(recursive_replacements, baseline_recursive_replacements, "rewrite varint bytes recursive baseline count match");
    ok &= expect_eq_string(rewritten, baseline_rewritten, "rewrite varint bytes recursive baseline bytes match");
    ok &= expect_eq_size(recursive_replacements, 2u, "rewrite varint bytes recursive count");
    ok &= expect_eq_string(hex_string(rewritten), "082a4a04082a10061805", "rewrite varint bytes recursive bytes");

    std::vector<std::uint8_t> old_lobby_id_varint;
    ok &= expect_true(encode_varuint_with_expected_size(300u, 2u, old_lobby_id_varint), "old lobby id varint size");
    std::string old_steam_id_fixed64_raw;
    append_little_endian64(old_steam_id_fixed64_raw, 0x0102030405060708ull);
    const std::vector<std::uint8_t> old_steam_id_fixed64(old_steam_id_fixed64_raw.begin(), old_steam_id_fixed64_raw.end());
    std::string template_identifiers_message;
    template_identifiers_message.append(old_lobby_id_varint.begin(), old_lobby_id_varint.end());
    template_identifiers_message.append(old_steam_id_fixed64_raw);
    template_identifiers_message.append(old_lobby_id_varint.begin(), old_lobby_id_varint.end());
    std::string baseline_template_identifiers_message = template_identifiers_message;
    std::size_t baseline_lobby_id_match_count = 0;
    std::size_t baseline_steam_id_fixed64_match_count = 0;
    const bool baseline_template_identifiers_result = baseline_patch_template_identifiers(
        baseline_template_identifiers_message,
        old_lobby_id_varint,
        301u,
        true,
        old_steam_id_fixed64,
        0x1112131415161718ull,
        true,
        baseline_lobby_id_match_count,
        baseline_steam_id_fixed64_match_count);
    PatchTemplateIdentifierResult patch_identifier_result{};
    ok &= expect_true(patch_dota_lobby_template_identifiers(
                          template_identifiers_message,
                          old_lobby_id_varint,
                          301u,
                          true,
                          old_steam_id_fixed64,
                          0x1112131415161718ull,
                          true,
                          patch_identifier_result),
                      "patch template identifiers required");
    ok &= expect_true(baseline_template_identifiers_result, "baseline patch template identifiers required");
    ok &= expect_eq_string(template_identifiers_message, baseline_template_identifiers_message, "patch template identifiers baseline bytes match");
    ok &= expect_eq_size(patch_identifier_result.lobby_id_match_count, baseline_lobby_id_match_count, "patch template identifiers baseline lobby count match");
    ok &= expect_eq_size(patch_identifier_result.steam_id_fixed64_match_count, baseline_steam_id_fixed64_match_count, "patch template identifiers baseline steam count match");
    ok &= expect_eq_size(patch_identifier_result.lobby_id_match_count, 2u, "patch template identifiers lobby count");
    ok &= expect_eq_size(patch_identifier_result.steam_id_fixed64_match_count, 1u, "patch template identifiers steam count");
    ok &= expect_eq_string(hex_string(template_identifiers_message), "ad021817161514131211ad02", "patch template identifiers bytes");

    template_identifiers_message.assign("missing");
    baseline_template_identifiers_message = template_identifiers_message;
    bool baseline_optional_template_result = baseline_patch_template_identifiers(
        baseline_template_identifiers_message,
        old_lobby_id_varint,
        301u,
        false,
        old_steam_id_fixed64,
        0x1112131415161718ull,
        false,
        baseline_lobby_id_match_count,
        baseline_steam_id_fixed64_match_count);
    ok &= expect_true(patch_dota_lobby_template_identifiers(
                          template_identifiers_message,
                          old_lobby_id_varint,
                          301u,
                          false,
                          old_steam_id_fixed64,
                          0x1112131415161718ull,
                          false,
                          patch_identifier_result),
                      "patch template identifiers optional missing");
    ok &= expect_true(baseline_optional_template_result, "baseline patch template identifiers optional missing");
    ok &= expect_eq_string(template_identifiers_message, baseline_template_identifiers_message, "patch template identifiers optional baseline bytes match");
    ok &= expect_eq_size(patch_identifier_result.lobby_id_match_count, baseline_lobby_id_match_count, "patch template identifiers optional baseline lobby count match");
    ok &= expect_eq_size(patch_identifier_result.steam_id_fixed64_match_count, baseline_steam_id_fixed64_match_count, "patch template identifiers optional baseline steam count match");
    ok &= expect_eq_size(patch_identifier_result.lobby_id_match_count, 0u, "patch template identifiers optional lobby count");
    ok &= expect_eq_size(patch_identifier_result.steam_id_fixed64_match_count, 0u, "patch template identifiers optional steam count");
    ok &= expect_eq_string(template_identifiers_message, "missing", "patch template identifiers optional bytes");
    baseline_template_identifiers_message = template_identifiers_message;
    baseline_optional_template_result = baseline_patch_template_identifiers(
        baseline_template_identifiers_message,
        old_lobby_id_varint,
        301u,
        true,
        old_steam_id_fixed64,
        0x1112131415161718ull,
        true,
        baseline_lobby_id_match_count,
        baseline_steam_id_fixed64_match_count);
    const bool required_missing_template_result = patch_dota_lobby_template_identifiers(
                           template_identifiers_message,
                           old_lobby_id_varint,
                           301u,
                          true,
                           old_steam_id_fixed64,
                           0x1112131415161718ull,
                           true,
                           patch_identifier_result);
    ok &= expect_true(!required_missing_template_result, "patch template identifiers required missing");
    ok &= expect_true(required_missing_template_result == baseline_optional_template_result, "patch template identifiers required missing baseline return match");

    std::string fixed32_message = std::string({ 'x', '\x04', '\x03', '\x02', '\x01', 'y', '\x04', '\x03', '\x02', '\x01' });
    std::size_t fixed32_match_count = 0;
    ok &= expect_true(patch_fixed32_template_value(
                          fixed32_message,
                          std::vector<std::uint8_t>{ 0x04u, 0x03u, 0x02u, 0x01u },
                          0x0a0b0c0du,
                          fixed32_match_count),
                      "patch fixed32 template value");
    ok &= expect_eq_size(fixed32_match_count, 2u, "patch fixed32 template count");
    ok &= expect_eq_string(hex_string(fixed32_message), "780d0c0b0a790d0c0b0a", "patch fixed32 template bytes");
    ok &= expect_true(patch_fixed32_template_value(fixed32_message, std::vector<std::uint8_t>{ 0xffu }, 1u, fixed32_match_count), "patch fixed32 template missing");
    ok &= expect_eq_size(fixed32_match_count, 0u, "patch fixed32 template missing count");

    std::string varint_template_message;
    append_varuint(varint_template_message, 300u);
    append_varuint(varint_template_message, 300u);
    std::size_t varint_match_count = 0;
    bool varint_size_ok = false;
    ok &= expect_true(patch_varint_template_value(
                          varint_template_message,
                          std::vector<std::uint8_t>{ 0xacu, 0x02u },
                          301u,
                          varint_match_count,
                          varint_size_ok),
                      "patch varint template value");
    ok &= expect_true(varint_size_ok, "patch varint template size ok");
    ok &= expect_eq_size(varint_match_count, 2u, "patch varint template count");
    ok &= expect_eq_string(hex_string(varint_template_message), "ad02ad02", "patch varint template bytes");
    ok &= expect_true(!patch_varint_template_value(varint_template_message, std::vector<std::uint8_t>{ 0x01u }, 300u, varint_match_count, varint_size_ok), "patch varint template size mismatch");
    ok &= expect_true(!varint_size_ok, "patch varint template size mismatch flag");
    ok &= expect_true(!patch_varint_template_value(varint_template_message, std::vector<std::uint8_t>{ 0xffu, 0x02u }, 301u, varint_match_count, varint_size_ok), "patch varint template missing");
    ok &= expect_true(varint_size_ok, "patch varint template missing size ok");
    ok &= expect_eq_size(varint_match_count, 0u, "patch varint template missing count");

    std::string member_template;
    append_fixed64_field(member_template, 1u, 0x0102030405060708ull);
    append_varint_field(member_template, 55u, 11u);
    append_varint_field(member_template, 2u, 12u);
    append_varint_field(member_template, 3u, 13u);
    append_varint_field(member_template, 7u, 14u);
    append_fixed32_field(member_template, 16u, 15u);
    append_varint_field(member_template, 28u, 16u);
    append_bytes_field(member_template, 99u, "keep");
    std::string rewritten_member;
    std::string baseline_member;
    bool baseline_result = baseline_rewrite_dota_lobby_template_member_object(
        member_template,
        42u,
        0x1112131415161718ull,
        2u,
        3u,
        77u,
        true,
        baseline_member);
    bool migrated_result = false;
    ok &= expect_true(gbe::dota_gc_wire::rewrite_dota_lobby_template_member_object(
                          member_template,
                          42u,
                          0x1112131415161718ull,
                          2u,
                          3u,
                          77u,
                          true,
                          rewritten_member),
                      "rewrite dota lobby member object");
    migrated_result = true;
    ok &= expect_true(baseline_result == migrated_result, "rewrite dota lobby member baseline return matches");
    ok &= expect_eq_string(rewritten_member, baseline_member, "rewrite dota lobby member baseline bytes match");
    ok &= expect_eq_string(hex_string(rewritten_member), "091817161514131211b8032a104d18023803850100000000e001009a06046b656570", "rewrite dota lobby member object bytes");
    std::uint64_t rewritten_u64 = 0;
    std::uint32_t rewritten_u32 = 0;
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 1u, rewritten_u64), "rewrite member steam field");
    ok &= expect_eq_u64(rewritten_u64, 0x1112131415161718ull, "rewrite member steam value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 55u, rewritten_u64), "rewrite member account field 55");
    ok &= expect_eq_u64(rewritten_u64, 42u, "rewrite member account field 55 value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 2u, rewritten_u32), "rewrite member hero field");
    ok &= expect_eq_u64(rewritten_u32, 77u, "rewrite member hero value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 3u, rewritten_u32), "rewrite member team field");
    ok &= expect_eq_u64(rewritten_u32, 2u, "rewrite member team value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 7u, rewritten_u32), "rewrite member slot field");
    ok &= expect_eq_u64(rewritten_u32, 3u, "rewrite member slot value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 16u, rewritten_u32), "rewrite member leaver status field");
    ok &= expect_eq_u64(rewritten_u32, 0u, "rewrite member leaver status value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 28u, rewritten_u32), "rewrite member leaver actions field");
    ok &= expect_eq_u64(rewritten_u32, 0u, "rewrite member leaver actions value");
    std::string preserved_member_bytes;
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 99u, preserved_member_bytes), "rewrite member preserves unknown bytes");
    ok &= expect_eq_string(preserved_member_bytes, "keep", "rewrite member preserved unknown value");

    std::string sparse_member;
    append_varint_field(sparse_member, 1u, 5u);
    baseline_result = baseline_rewrite_dota_lobby_template_member_object(
        sparse_member,
        42u,
        0x1112131415161718ull,
        4u,
        5u,
        0u,
        true,
        baseline_member);
    migrated_result = false;
    ok &= expect_true(gbe::dota_gc_wire::rewrite_dota_lobby_template_member_object(
                          sparse_member,
                          42u,
                          0x1112131415161718ull,
                          4u,
                          5u,
                          0u,
                          true,
                          rewritten_member),
                      "rewrite sparse dota lobby member object");
    migrated_result = true;
    ok &= expect_true(baseline_result == migrated_result, "rewrite sparse dota lobby member baseline return matches");
    ok &= expect_eq_string(rewritten_member, baseline_member, "rewrite sparse dota lobby member baseline bytes match");
    ok &= expect_eq_string(hex_string(rewritten_member), "082a18043805850100000000e00100", "rewrite sparse member appended bytes");
    ok &= expect_true(!read_uint32_field(reinterpret_cast<const std::uint8_t *>(rewritten_member.data()), rewritten_member.size(), 2u, rewritten_u32), "rewrite sparse member skips zero hero");
    const std::string malformed_member({ '\x0a', '\x05', 'x' });
    baseline_result = baseline_rewrite_dota_lobby_template_member_object(
        malformed_member,
        42u,
        0x1112131415161718ull,
        4u,
        5u,
        0u,
        false,
        baseline_member);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_member_object(
        malformed_member,
        42u,
        0x1112131415161718ull,
        4u,
        5u,
        0u,
        false,
        rewritten_member);
    ok &= expect_true(!migrated_result, "rewrite malformed dota lobby member object fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed dota lobby member baseline return matches");

    std::string server_static_member;
    append_fixed64_field(server_static_member, 1u, 0x0102030405060708ull);
    append_varint_field(server_static_member, 11u, 0u);
    append_varint_field(server_static_member, 3u, 99u);
    std::string server_static_rewritten;
    std::string baseline_server_static;
    baseline_result = baseline_rewrite_dota_server_static_lobby_member_object(
        server_static_member,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        baseline_server_static);
    migrated_result = false;
    ok &= expect_true(gbe::dota_gc_wire::rewrite_dota_server_static_lobby_member_object(
                          server_static_member,
                          std::vector<std::uint8_t>{ 0x05u },
                          42u,
                          0x1112131415161718ull,
                          server_static_rewritten),
                      "rewrite dota server static member object");
    migrated_result = true;
    ok &= expect_true(baseline_result == migrated_result, "rewrite server static baseline return matches");
    ok &= expect_eq_string(server_static_rewritten, baseline_server_static, "rewrite server static baseline bytes match");
    ok &= expect_eq_string(hex_string(server_static_rewritten), "09181716151413121158011863", "rewrite server static member bytes");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(server_static_rewritten.data()), server_static_rewritten.size(), 1u, rewritten_u64), "rewrite server static steam field");
    ok &= expect_eq_u64(rewritten_u64, 0x1112131415161718ull, "rewrite server static steam value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(server_static_rewritten.data()), server_static_rewritten.size(), 11u, rewritten_u32), "rewrite server static plus field");
    ok &= expect_eq_u64(rewritten_u32, 1u, "rewrite server static plus value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(server_static_rewritten.data()), server_static_rewritten.size(), 3u, rewritten_u32), "rewrite server static preserves rank field");
    ok &= expect_eq_u64(rewritten_u32, 99u, "rewrite server static preserves rank value");

    std::string server_static_sparse;
    append_varint_field(server_static_sparse, 1u, 5u);
    baseline_result = baseline_rewrite_dota_server_static_lobby_member_object(
        server_static_sparse,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        baseline_server_static);
    migrated_result = false;
    ok &= expect_true(gbe::dota_gc_wire::rewrite_dota_server_static_lobby_member_object(
                          server_static_sparse,
                          std::vector<std::uint8_t>{ 0x05u },
                          42u,
                          0x1112131415161718ull,
                          server_static_rewritten),
                      "rewrite sparse dota server static member object");
    migrated_result = true;
    ok &= expect_true(baseline_result == migrated_result, "rewrite sparse server static baseline return matches");
    ok &= expect_eq_string(server_static_rewritten, baseline_server_static, "rewrite sparse server static baseline bytes match");
    ok &= expect_eq_string(hex_string(server_static_rewritten), "082a091817161514131211", "rewrite sparse server static member appended steam");
    baseline_result = baseline_rewrite_dota_server_static_lobby_member_object(
        malformed_member,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        baseline_server_static);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_server_static_lobby_member_object(
        malformed_member,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        server_static_rewritten);
    ok &= expect_true(!migrated_result, "rewrite malformed dota server static member object fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed server static baseline return matches");

    gbe::dota_gc_wire::DotaLobbyTemplateObject2004RewriteOptions object2004_options{};
    object2004_options.account_id = 42u;
    object2004_options.steam_id = 0x1112131415161718ull;
    object2004_options.lobby_id = 9001u;
    object2004_options.rewrite_runtime_fields = true;
    object2004_options.lobby_state = 2u;
    object2004_options.lobby_game_state = 3u;
    object2004_options.server_id = 0x2122232425262728ull;
    object2004_options.match_id = 9002u;
    object2004_options.game_start_time = 77u;
    object2004_options.connect = "  1.2.3.4:27015 5.6.7.8:27016";
    object2004_options.room_name = "Room";
    object2004_options.game_mode = 9u;
    object2004_options.server_region = 10u;
    object2004_options.lan = true;
    object2004_options.lan_host_ping_location = "lan";
    object2004_options.allow_cheats = true;
    object2004_options.fill_with_bots = true;
    object2004_options.allow_spectating = true;
    object2004_options.visibility = 4u;
    object2004_options.bot_difficulty_radiant = 5u;
    object2004_options.bot_difficulty_dire = 6u;
    object2004_options.bot_radiant = 7u;
    object2004_options.bot_dire = 8u;
    object2004_options.owner_team = 1u;
    object2004_options.owner_slot = 2u;
    object2004_options.owner_hero_id = 99u;
    object2004_options.pass_key = "pass";

    std::string object2004_member;
    append_fixed64_field(object2004_member, 1u, 0x0102030405060708ull);
    append_varint_field(object2004_member, 55u, 11u);
    append_varint_field(object2004_member, 2u, 12u);
    append_varint_field(object2004_member, 3u, 13u);
    append_varint_field(object2004_member, 7u, 14u);
    std::string object2004_input;
    append_varint_field(object2004_input, 1u, 1u);
    append_varint_field(object2004_input, 3u, 1u);
    append_varint_field(object2004_input, 4u, 1u);
    append_bytes_field(object2004_input, 5u, "old-connect");
    append_fixed64_field(object2004_input, 6u, 0x0101010101010101ull);
    append_fixed64_field(object2004_input, 11u, 0x0202020202020202ull);
    append_varint_field(object2004_input, 13u, 0u);
    append_varint_field(object2004_input, 14u, 0u);
    append_bytes_field(object2004_input, 16u, "OldRoom");
    append_varint_field(object2004_input, 21u, 1u);
    append_varint_field(object2004_input, 22u, 1u);
    append_varint_field(object2004_input, 30u, 1u);
    append_varint_field(object2004_input, 31u, 0u);
    append_varint_field(object2004_input, 36u, 1u);
    append_bytes_field(object2004_input, 39u, "old-pass");
    append_varint_field(object2004_input, 57u, 0u);
    append_varint_field(object2004_input, 75u, 1u);
    append_varint_field(object2004_input, 87u, 1u);
    append_varint_field(object2004_input, 93u, 1u);
    append_varint_field(object2004_input, 94u, 1u);
    append_varint_field(object2004_input, 95u, 1u);
    append_bytes_field(object2004_input, 109u, "old-lan");
    append_bytes_field(object2004_input, 120u, object2004_member);
    append_varint_field(object2004_input, 121u, 123u);
    append_varint_field(object2004_input, 122u, 124u);
    append_bytes_field(object2004_input, 200u, "keep");
    std::string object2004_baseline;
    std::string object2004_rewritten;
    baseline_result = baseline_rewrite_dota_lobby_template_object_2004(object2004_input, object2004_options, object2004_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2004(object2004_input, object2004_options, object2004_rewritten);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2004 runtime");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2004 runtime baseline return matches");
    ok &= expect_eq_string(object2004_rewritten, object2004_baseline, "rewrite 2004 runtime baseline bytes match");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(object2004_rewritten.data()), object2004_rewritten.size(), 1u, rewritten_u64), "rewrite 2004 lobby id field");
    ok &= expect_eq_u64(rewritten_u64, 9001u, "rewrite 2004 lobby id value");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(object2004_rewritten.data()), object2004_rewritten.size(), 5u, preserved_member_bytes), "rewrite 2004 connect field");
    ok &= expect_eq_string(preserved_member_bytes, "1.2.3.4:27015", "rewrite 2004 normalized connect");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(object2004_rewritten.data()), object2004_rewritten.size(), 120u, preserved_member_bytes), "rewrite 2004 member field");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 55u, rewritten_u64), "rewrite 2004 member account");
    ok &= expect_eq_u64(rewritten_u64, 42u, "rewrite 2004 member account value");

    object2004_options.rewrite_runtime_fields = false;
    baseline_result = baseline_rewrite_dota_lobby_template_object_2004(object2004_input, object2004_options, object2004_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2004(object2004_input, object2004_options, object2004_rewritten);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2004 static");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2004 static baseline return matches");
    ok &= expect_eq_string(object2004_rewritten, object2004_baseline, "rewrite 2004 static baseline bytes match");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(object2004_rewritten.data()), object2004_rewritten.size(), 1u, rewritten_u64), "rewrite 2004 static lobby id field");
    ok &= expect_eq_u64(rewritten_u64, 1u, "rewrite 2004 static preserves lobby id");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(object2004_rewritten.data()), object2004_rewritten.size(), 120u, preserved_member_bytes), "rewrite 2004 static member field");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 55u, rewritten_u64), "rewrite 2004 static member account");
    ok &= expect_eq_u64(rewritten_u64, 11u, "rewrite 2004 static preserves member account");

    baseline_result = baseline_rewrite_dota_lobby_template_object_2004(malformed_member, object2004_options, object2004_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2004(malformed_member, object2004_options, object2004_rewritten);
    ok &= expect_true(!migrated_result, "rewrite malformed dota lobby template object 2004 fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed 2004 baseline return matches");

    std::string object2014_member;
    append_bytes_field(object2014_member, 1u, "OldName");
    append_varint_field(object2014_member, 2u, 99u);
    append_bytes_field(object2014_member, 9u, "keep");
    std::string object2014_input;
    append_bytes_field(object2014_input, 1u, object2014_member);
    append_varint_field(object2014_input, 4u, 12u);
    std::string object2014_baseline;
    std::string object2014_rewritten;
    baseline_result = baseline_rewrite_dota_lobby_template_object_2014(object2014_input, "NewName", object2014_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2014(object2014_input, "NewName", object2014_rewritten);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2014");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2014 baseline return matches");
    ok &= expect_eq_string(object2014_rewritten, object2014_baseline, "rewrite 2014 baseline bytes match");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(object2014_rewritten.data()), object2014_rewritten.size(), 1u, preserved_member_bytes), "rewrite 2014 member field");
    std::string rewritten_member_bytes;
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 1u, rewritten_member_bytes), "rewrite 2014 member name field");
    ok &= expect_eq_string(rewritten_member_bytes, "NewName", "rewrite 2014 member name value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 2u, rewritten_u64), "rewrite 2014 preserves member varint");
    ok &= expect_eq_u64(rewritten_u64, 99u, "rewrite 2014 preserves member varint value");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 9u, rewritten_member_bytes), "rewrite 2014 preserves unknown bytes");
    ok &= expect_eq_string(rewritten_member_bytes, "keep", "rewrite 2014 preserves unknown value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(object2014_rewritten.data()), object2014_rewritten.size(), 4u, rewritten_u64), "rewrite 2014 preserves outer field");
    ok &= expect_eq_u64(rewritten_u64, 12u, "rewrite 2014 preserves outer field value");

    baseline_result = baseline_rewrite_dota_lobby_template_object_2014(malformed_member, "NewName", object2014_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2014(malformed_member, "NewName", object2014_rewritten);
    ok &= expect_true(!migrated_result, "rewrite malformed dota lobby template object 2014 fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed 2014 baseline return matches");

    const std::uint32_t additional_startup_type_id = 8869u;
    std::string old_startup_message;
    append_varint_field(old_startup_message, 1u, additional_startup_type_id);
    append_bytes_field(old_startup_message, 2u, "old-startup");
    std::string other_startup_message;
    append_varint_field(other_startup_message, 1u, 777u);
    append_bytes_field(other_startup_message, 2u, "keep-startup");
    std::string object2015_input;
    append_varint_field(object2015_input, 1u, 5u);
    append_bytes_field(object2015_input, 2u, old_startup_message);
    append_bytes_field(object2015_input, 2u, other_startup_message);
    append_bytes_field(object2015_input, 9u, "keep");
    std::string object2015_baseline;
    std::string object2015_rewritten;
    baseline_result = baseline_rewrite_dota_lobby_template_object_2015(object2015_input, true, additional_startup_type_id, "new-startup", object2015_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2015(object2015_input, true, additional_startup_type_id, "new-startup", object2015_rewritten);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2015 clear and append");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2015 clear baseline return matches");
    ok &= expect_eq_string(object2015_rewritten, object2015_baseline, "rewrite 2015 clear baseline bytes match");
    ok &= expect_eq_string(format_field_layout_summary(object2015_rewritten), "1:0:1,2:2:17,9:2:4,2:2:16", "rewrite 2015 clear output layout");

    baseline_result = baseline_rewrite_dota_lobby_template_object_2015(object2015_input, false, additional_startup_type_id, std::string(), object2015_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2015(object2015_input, false, additional_startup_type_id, std::string(), object2015_rewritten);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2015 preserve no append");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2015 preserve baseline return matches");
    ok &= expect_eq_string(object2015_rewritten, object2015_baseline, "rewrite 2015 preserve baseline bytes match");
    ok &= expect_eq_string(object2015_rewritten, object2015_input, "rewrite 2015 preserve equals input");

    baseline_result = baseline_rewrite_dota_lobby_template_object_2015(malformed_member, true, additional_startup_type_id, "new-startup", object2015_baseline);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2015(malformed_member, true, additional_startup_type_id, "new-startup", object2015_rewritten);
    ok &= expect_true(!migrated_result, "rewrite malformed dota lobby template object 2015 fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed 2015 baseline return matches");

    std::string object2016_member;
    append_fixed64_field(object2016_member, 1u, 0x0102030405060708ull);
    append_varint_field(object2016_member, 1u, 5u);
    append_varint_field(object2016_member, 11u, 0u);
    append_bytes_field(object2016_member, 9u, "keep");
    std::string object2016_input;
    append_bytes_field(object2016_input, 1u, object2016_member);
    append_varint_field(object2016_input, 4u, 12u);
    std::string object2016_baseline;
    std::string object2016_rewritten;
    gbe::dota_gc_wire::DotaLobbyTemplateObject2016RewriteDebug baseline_debug{};
    gbe::dota_gc_wire::DotaLobbyTemplateObject2016RewriteDebug migrated_debug{};
    baseline_result = baseline_rewrite_dota_lobby_template_object_2016(
        object2016_input,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        object2016_baseline,
        &baseline_debug);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2016(
        object2016_input,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        object2016_rewritten,
        &migrated_debug);
    ok &= expect_true(migrated_result, "rewrite dota lobby template object 2016");
    ok &= expect_true(baseline_result == migrated_result, "rewrite 2016 baseline return matches");
    ok &= expect_eq_string(object2016_rewritten, object2016_baseline, "rewrite 2016 baseline bytes match");
    ok &= expect_eq_string(migrated_debug.first_member_input, baseline_debug.first_member_input, "rewrite 2016 debug input baseline match");
    ok &= expect_eq_string(migrated_debug.first_member_output, baseline_debug.first_member_output, "rewrite 2016 debug output baseline match");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(object2016_rewritten.data()), object2016_rewritten.size(), 1u, preserved_member_bytes), "rewrite 2016 member field");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 1u, rewritten_u64), "rewrite 2016 member steam/account field");
    ok &= expect_eq_u64(rewritten_u64, 0x1112131415161718ull, "rewrite 2016 member steam value");
    ok &= expect_true(read_uint32_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 11u, rewritten_u32), "rewrite 2016 plus field");
    ok &= expect_eq_u64(rewritten_u32, 1u, "rewrite 2016 plus value");
    ok &= expect_true(read_bytes_field(reinterpret_cast<const std::uint8_t *>(preserved_member_bytes.data()), preserved_member_bytes.size(), 9u, rewritten_member_bytes), "rewrite 2016 preserves member bytes");
    ok &= expect_eq_string(rewritten_member_bytes, "keep", "rewrite 2016 preserves member bytes value");
    ok &= expect_true(read_uint64_field(reinterpret_cast<const std::uint8_t *>(object2016_rewritten.data()), object2016_rewritten.size(), 4u, rewritten_u64), "rewrite 2016 preserves outer field");
    ok &= expect_eq_u64(rewritten_u64, 12u, "rewrite 2016 preserves outer field value");

    baseline_result = baseline_rewrite_dota_lobby_template_object_2016(
        malformed_member,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        object2016_baseline,
        &baseline_debug);
    migrated_result = gbe::dota_gc_wire::rewrite_dota_lobby_template_object_2016(
        malformed_member,
        std::vector<std::uint8_t>{ 0x05u },
        42u,
        0x1112131415161718ull,
        object2016_rewritten,
        &migrated_debug);
    ok &= expect_true(!migrated_result, "rewrite malformed dota lobby template object 2016 fails");
    ok &= expect_true(baseline_result == migrated_result, "rewrite malformed 2016 baseline return matches");

    std::vector<std::uint8_t> old_launch_lobby_id;
    std::vector<std::uint8_t> old_launch_match_id;
    std::vector<std::uint8_t> old_launch_game_time;
    ok &= expect_true(encode_varuint_with_expected_size(300u, 2u, old_launch_lobby_id), "old launch lobby id varint");
    ok &= expect_true(encode_varuint_with_expected_size(400u, 2u, old_launch_match_id), "old launch match id varint");
    ok &= expect_true(encode_varuint_with_expected_size(10u, 1u, old_launch_game_time), "old launch game time varint");
    std::string old_launch_steam_raw;
    append_little_endian64(old_launch_steam_raw, 0x0102030405060708ull);
    std::string old_launch_server_raw;
    append_little_endian64(old_launch_server_raw, 0x2122232425262728ull);
    std::string launch_template_message;
    launch_template_message.append(old_launch_steam_raw);
    launch_template_message.append(old_launch_lobby_id.begin(), old_launch_lobby_id.end());
    launch_template_message.append(old_launch_match_id.begin(), old_launch_match_id.end());
    launch_template_message.append(old_launch_server_raw);
    launch_template_message.append(old_launch_game_time.begin(), old_launch_game_time.end());
    launch_template_message.append("1.1");
    DotaPracticeLobbyLaunchTemplatePatchResult launch_patch_result{};
    ok &= expect_true(patch_dota_practice_lobby_launch_template(
                          launch_template_message,
                          std::vector<std::uint8_t>(old_launch_steam_raw.begin(), old_launch_steam_raw.end()),
                          0x1112131415161718ull,
                          old_launch_lobby_id,
                          301u,
                          old_launch_match_id,
                          401u,
                          true,
                          std::vector<std::uint8_t>(old_launch_server_raw.begin(), old_launch_server_raw.end()),
                          0x3132333435363738ull,
                          true,
                          old_launch_game_time,
                          11u,
                          true,
                          "1.1",
                          "2.2",
                          launch_patch_result),
                      "patch practice lobby launch template");
    ok &= expect_eq_size(launch_patch_result.steam_id_fixed64_match_count, 1u, "launch patch steam count");
    ok &= expect_true(launch_patch_result.lobby_id_size_ok, "launch patch lobby size ok");
    ok &= expect_eq_size(launch_patch_result.lobby_id_match_count, 1u, "launch patch lobby count");
    ok &= expect_true(launch_patch_result.match_id_size_ok, "launch patch match size ok");
    ok &= expect_eq_size(launch_patch_result.match_id_match_count, 1u, "launch patch match count");
    ok &= expect_eq_size(launch_patch_result.server_id_fixed64_match_count, 1u, "launch patch server count");
    ok &= expect_true(launch_patch_result.game_start_time_size_ok, "launch patch game time size ok");
    ok &= expect_eq_size(launch_patch_result.game_start_time_match_count, 1u, "launch patch game time count");
    ok &= expect_true(launch_patch_result.connect_size_ok, "launch patch connect size ok");
    ok &= expect_eq_size(launch_patch_result.connect_match_count, 1u, "launch patch connect count");
    ok &= expect_eq_string(hex_string(launch_template_message), "1817161514131211ad02910338373635343332310b322e32", "launch patch bytes");
    ok &= expect_eq_u64(read_little_endian(std::vector<std::uint8_t>(launch_template_message.begin(), launch_template_message.begin() + 8), 0u, 8u), 0x1112131415161718ull, "launch patch steam id parseable");
    std::size_t launch_value_offset = 8u;
    std::uint64_t launch_lobby_id_value = 0;
    ok &= expect_true(read_varuint(reinterpret_cast<const std::uint8_t *>(launch_template_message.data()), launch_template_message.size(), launch_value_offset, launch_lobby_id_value), "launch patch lobby id parseable");
    ok &= expect_eq_u64(launch_lobby_id_value, 301u, "launch patch lobby id value");
    std::uint64_t launch_match_id_value = 0;
    ok &= expect_true(read_varuint(reinterpret_cast<const std::uint8_t *>(launch_template_message.data()), launch_template_message.size(), launch_value_offset, launch_match_id_value), "launch patch match id parseable");
    ok &= expect_eq_u64(launch_match_id_value, 401u, "launch patch match id value");
    ok &= expect_eq_u64(read_little_endian(std::vector<std::uint8_t>(launch_template_message.begin() + launch_value_offset, launch_template_message.begin() + launch_value_offset + 8), 0u, 8u), 0x3132333435363738ull, "launch patch server id parseable");
    launch_value_offset += 8u;
    std::uint64_t launch_game_time_value = 0;
    ok &= expect_true(read_varuint(reinterpret_cast<const std::uint8_t *>(launch_template_message.data()), launch_template_message.size(), launch_value_offset, launch_game_time_value), "launch patch game time parseable");
    ok &= expect_eq_u64(launch_game_time_value, 11u, "launch patch game time value");
    ok &= expect_eq_string(launch_template_message.substr(launch_value_offset), "2.2", "launch patch connect value");

    launch_template_message.assign(old_launch_lobby_id.begin(), old_launch_lobby_id.end());
    ok &= expect_true(patch_dota_practice_lobby_launch_template(
                          launch_template_message,
                          std::vector<std::uint8_t>{ 0xffu },
                          0u,
                          std::vector<std::uint8_t>{ 0x01u },
                          300u,
                          old_launch_match_id,
                          401u,
                          false,
                          std::vector<std::uint8_t>{},
                          0u,
                          false,
                          old_launch_game_time,
                          11u,
                          true,
                          "1.1",
                          "longer",
                          launch_patch_result),
                      "patch practice lobby launch template size mismatch");
    ok &= expect_true(!launch_patch_result.lobby_id_size_ok, "launch patch lobby size mismatch");
    ok &= expect_true(!launch_patch_result.connect_size_ok, "launch patch connect size mismatch");

    std::string peripheral_old_steam_raw;
    append_little_endian64(peripheral_old_steam_raw, 0x0102030405060708ull);
    std::string peripheral_old_persona_steam_raw;
    append_little_endian64(peripheral_old_persona_steam_raw, 0x1111111111111111ull);
    std::string peripheral_old_server_raw;
    append_little_endian64(peripheral_old_server_raw, 0x2122232425262728ull);
    std::string peripheral_message;
    peripheral_message.append(peripheral_old_persona_steam_raw);
    peripheral_message.append(peripheral_old_server_raw);
    peripheral_message.append("12345");
    peripheral_message.append("abcde");
    DotaPracticeLobbyPeripheralTemplatePatchResult peripheral_patch_result{};
    ok &= expect_true(patch_dota_practice_lobby_peripheral_template(
                          peripheral_message,
                          std::vector<std::uint8_t>(peripheral_old_steam_raw.begin(), peripheral_old_steam_raw.end()),
                          std::vector<std::uint8_t>(peripheral_old_persona_steam_raw.begin(), peripheral_old_persona_steam_raw.end()),
                          0x3132333435363738ull,
                          true,
                          std::vector<std::uint8_t>(peripheral_old_server_raw.begin(), peripheral_old_server_raw.end()),
                          0x4142434445464748ull,
                          std::vector<std::string>{ "12345", "22222" },
                          67890u,
                          peripheral_patch_result),
                      "patch peripheral template");
    ok &= expect_eq_size(peripheral_patch_result.steam_id_fixed64_match_count, 1u, "patch peripheral steam count");
    ok &= expect_eq_size(peripheral_patch_result.server_id_fixed64_match_count, 1u, "patch peripheral server count");
    ok &= expect_eq_size(peripheral_patch_result.lobby_id_text_match_count, 1u, "patch peripheral lobby text count");
    ok &= expect_eq_string(hex_string(peripheral_message), "3837363534333231484746454443424136373839306162636465", "patch peripheral bytes");
    ok &= expect_eq_u64(read_little_endian(std::vector<std::uint8_t>(peripheral_message.begin(), peripheral_message.begin() + 8), 0u, 8u), 0x3132333435363738ull, "patch peripheral steam id parseable");
    ok &= expect_eq_u64(read_little_endian(std::vector<std::uint8_t>(peripheral_message.begin() + 8, peripheral_message.begin() + 16), 0u, 8u), 0x4142434445464748ull, "patch peripheral server id parseable");
    ok &= expect_eq_string(peripheral_message.substr(16u, 5u), "67890", "patch peripheral lobby id text value");
    ok &= expect_true(!patch_dota_practice_lobby_peripheral_template(
                          peripheral_message,
                          std::vector<std::uint8_t>{ 0xffu },
                          std::vector<std::uint8_t>{ 0xfeu },
                          0u,
                          false,
                          std::vector<std::uint8_t>{},
                          0u,
                          std::vector<std::string>{},
                          0u,
                          peripheral_patch_result),
                      "patch peripheral missing steam fails");

    rewritten.clear();
    rewrote = false;
    ok &= expect_true(rewrite_varint_fields(encoded, std::vector<std::uint32_t>{9u}, 7u, rewritten, &rewrote), "rewrite varint fields miss");
    baseline_rewritten.clear();
    baseline_rewrote = false;
    ok &= expect_true(baseline_rewrite_varint_fields(encoded, std::vector<std::uint32_t>{9u}, 7u, baseline_rewritten, &baseline_rewrote), "baseline rewrite varint fields miss");
    ok &= expect_true(rewrote == baseline_rewrote, "rewrite varint fields miss baseline rewrote match");
    ok &= expect_eq_string(rewritten, baseline_rewritten, "rewrite varint fields miss baseline bytes match");
    ok &= expect_true(!rewrote, "rewrite varint fields miss changed");
    ok &= expect_eq_string(hex_string(rewritten), hex_string(encoded), "rewrite varint fields miss bytes");

    std::string object_2002;
    append_varint_field(object_2002, 1u, 5u);
    append_varint_field(object_2002, 72u, 3u);
    append_varint_field(object_2002, 18u, 9u);
    ok &= expect_true(rewrite_dota_account_bound_object_data(object_2002, 2002, 42u, rewritten), "rewrite account bound object 2002");
    ok &= expect_eq_string(hex_string(rewritten), "082ac004e05d900100", "rewrite account bound object 2002 bytes");

    std::string object_2012;
    append_varint_field(object_2012, 1u, 7u);
    append_varint_field(object_2012, 5u, 99u);
    ok &= expect_true(rewrite_dota_account_bound_object_data(object_2012, 2012, 42u, rewritten), "rewrite account bound object 2012");
    ok &= expect_eq_string(hex_string(rewritten), "082a1080c280d60518012001280030003d80d8db70410000000000000000", "rewrite account bound object 2012 bytes");

    return ok;
}

bool test_summary_helpers()
{
    using namespace gbe::proto_wire;

    bool ok = true;
    std::string encoded;
    append_varint_field(encoded, 1u, 1u);
    append_fixed64_field(encoded, 2u, 0x0102030405060708ull);
    append_bytes_field(encoded, 3u, "abc");
    append_fixed32_field(encoded, 4u, 0x0a0b0c0du);

    std::string repeated_varints;
    append_varint_field(repeated_varints, 121u, 1u);
    append_varint_field(repeated_varints, 122u, 2u);
    append_varint_field(repeated_varints, 121u, 3u);
    ok &= expect_eq_string(summarize_repeated_varint_field(repeated_varints, 121u), "1,3", "summarize repeated varint field");
    ok &= expect_eq_string(summarize_repeated_varint_field(repeated_varints, 132u), "-", "summarize missing repeated varint field");

    std::string repeated_bytes;
    append_bytes_field(repeated_bytes, 17u, "a");
    append_varint_field(repeated_bytes, 17u, 1u);
    append_bytes_field(repeated_bytes, 17u, "bc");
    append_bytes_field(repeated_bytes, 1u, "d");
    ok &= expect_eq_u64(count_repeated_bytes_field(repeated_bytes, 17u), 2u, "count repeated bytes field");
    ok &= expect_eq_u64(count_repeated_bytes_field(repeated_bytes, 1u), 1u, "count repeated bytes field other");
    ok &= expect_eq_u64(count_repeated_bytes_field(repeated_bytes, 9u), 0u, "count missing repeated bytes field");
    ok &= expect_eq_string(format_field_layout_summary(repeated_bytes), "17:2:1,17:0:1,17:2:2,1:2:1", "format field layout summary");
    ok &= expect_eq_string(format_field_layout_summary(std::string()), "", "format empty field layout summary");
    ok &= expect_eq_string(
        format_top_level_field_summary(reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()),
        "1:0:1=1,2:1:8=72623859790382856,3:2:3,4:5:4=168496141",
        "format top level field summary");
    ok &= expect_eq_string(format_top_level_field_summary(nullptr, 0u), "empty", "format null top level field summary");
    ok &= expect_eq_string(format_top_level_field_summary(reinterpret_cast<const std::uint8_t *>(std::string().data()), 0u), "empty", "format empty top level field summary");

    std::string lobby_member_state;
    append_varint_field(lobby_member_state, 1u, 101u);
    append_varint_field(lobby_member_state, 55u, 202u);
    append_varint_field(lobby_member_state, 2u, 303u);
    append_varint_field(lobby_member_state, 3u, 4u);
    append_varint_field(lobby_member_state, 7u, 5u);
    append_varint_field(lobby_member_state, 16u, 6u);
    append_varint_field(lobby_member_state, 28u, 7u);
    ok &= expect_eq_string(
        format_dota_lobby_member_state_summary(lobby_member_state),
        "steam_id=101 account_id=202 hero_id=303 team=4 slot=5 leaver_status=6 leaver_actions=7 flags[steam_id=1 account_id=1 hero_id=1 team=1 slot=1 leaver_status=1 leaver_actions=1]",
        "format dota lobby member state summary");

    std::string lobby_aux;
    append_varint_field(lobby_aux, 121u, 1u);
    append_varint_field(lobby_aux, 121u, 2u);
    append_varint_field(lobby_aux, 122u, 3u);
    append_varint_field(lobby_aux, 124u, 4u);
    append_varint_field(lobby_aux, 132u, 5u);
    ok &= expect_eq_string(
        format_dota_lobby_aux_field_summary(lobby_aux),
        "121=[1,2] 122=[3] 123=[-] 124=[4] 132=[5]",
        "format dota lobby aux field summary");

    std::string static_lobby_member;
    append_bytes_field(static_lobby_member, 1u, "Player");
    append_varint_field(static_lobby_member, 2u, 404u);
    ok &= expect_eq_string(
        format_dota_static_lobby_member_summary(static_lobby_member),
        "name=Player party_id=404 flags[name=1 party_id=1]",
        "format dota static lobby member summary");

    std::string server_static_lobby_member;
    append_varint_field(server_static_lobby_member, 1u, 505u);
    append_varint_field(server_static_lobby_member, 3u, 606u);
    append_varint_field(server_static_lobby_member, 7u, 707u);
    append_varint_field(server_static_lobby_member, 12u, 808u);
    ok &= expect_eq_string(
        format_dota_server_static_lobby_member_summary(server_static_lobby_member),
        "steam_id=505 rank_tier=606 coach_rating=707 favorite_team_packed_lo=808 flags[steam_id=1 rank_tier=1 coach_rating=1 favorite_team_packed=1]",
        "format dota server static lobby member summary");

    std::string leaver_state;
    append_varint_field(leaver_state, 1u, 10u);
    append_varint_field(leaver_state, 2u, 20u);
    append_varint_field(leaver_state, 3u, 30u);
    append_varint_field(leaver_state, 4u, 40u);
    append_varint_field(leaver_state, 5u, 50u);
    append_varint_field(leaver_state, 6u, 60u);
    ok &= expect_eq_string(
        format_dota7034_leaver_state_summary(leaver_state),
        "lobby_state=10 game_state=20 leaver_detected=30 first_blood=40 discard=50 mass_disconnect=60",
        "format dota7034 leaver state summary");

    std::string player;
    append_varint_field(player, 1u, 99u);
    append_varint_field(player, 2u, 7u);
    append_bytes_field(player, 3u, leaver_state);
    append_varint_field(player, 4u, 8u);
    ok &= expect_eq_string(
        format_dota7034_player_summary(player),
        "steam_id=99 hero_id=7 disconnect_reason=8 leaver_state{lobby_state=10 game_state=20 leaver_detected=30 first_blood=40 discard=50 mass_disconnect=60}",
        "format dota7034 player summary");

    std::string draft;
    append_varint_field(draft, 1u, 123u);
    append_varint_field(draft, 2u, 4u);
    append_varint_field(draft, 3u, 2u);
    ok &= expect_eq_string(
        format_dota7034_draft_summary(draft),
        "steam_id=123 team=4 team_slot=2",
        "format dota7034 draft summary");

    std::string summary;
    append_varint_field(summary, 2u, 11u);
    append_varint_field(summary, 8u, 22u);
    append_varint_field(summary, 11u, 33u);
    append_varint_field(summary, 12u, 44u);
    append_varint_field(summary, 14u, 55u);
    append_varint_field(summary, 15u, 66u);
    append_bytes_field(summary, 1u, player);
    append_bytes_field(summary, 7u, player);
    append_bytes_field(summary, 16u, draft);
    ok &= expect_eq_string(
        format_dota7034_summary(reinterpret_cast<const std::uint8_t *>(summary.data()), summary.size()),
        "game_state=11 send_reason=22 connected=1 disconnected=1 drafts=1 radiant_kills=33 dire_kills=44 radiant_lead=55 building_state=66 connected0{steam_id=99 hero_id=7 disconnect_reason=8 leaver_state{lobby_state=10 game_state=20 leaver_detected=30 first_blood=40 discard=50 mass_disconnect=60}} disconnected0{steam_id=99 hero_id=7 disconnect_reason=8 leaver_state{lobby_state=10 game_state=20 leaver_detected=30 first_blood=40 discard=50 mass_disconnect=60}} draft0{steam_id=123 team=4 team_slot=2}",
        "format dota7034 summary");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_basic_wire_and_parsers();
    ok &= test_dota_gc_router_response_helpers();
    ok &= test_dota_gc_router_wrapped_custom_game_lifecycle_requests();
    ok &= test_dota_lobby_state_helpers();
    ok &= test_patch_and_rewrite_helpers();
    ok &= test_summary_helpers();

    if (!ok)
        return 1;

    std::cout << "gbe_proto_wire_test passed" << std::endl;
    return 0;
}

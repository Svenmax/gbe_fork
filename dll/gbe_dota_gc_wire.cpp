#include "gbe_dota_gc_wire.h"

#include "gbe_proto_wire.h"

namespace gbe::dota_gc_wire {

bool should_prefer_dota_lobby_connect_update(
    const std::string &current_connect,
    const std::string &candidate_connect)
{
    if (candidate_connect.empty() || current_connect == candidate_connect)
        return false;

    if (current_connect.empty())
        return true;

    const std::uint32_t candidate_ip = proto_wire::parse_dota_practice_lobby_connect_ipv4(candidate_connect);
    if (candidate_ip == 0u)
        return false;

    if (current_connect == proto_wire::format_dota_practice_lobby_loopback_connect())
        return true;

    return false;
}

std::uint32_t get_dota_practice_lobby_startup_account_id_for_state(
    std::uint32_t account_id,
    std::uint32_t lobby_state,
    std::uint32_t lobby_game_state)
{
    if (account_id == 0u)
        return 0u;

    if (lobby_state == 1u && lobby_game_state == 0u)
        return account_id;

    if (lobby_state == 2u && lobby_game_state == 0u)
        return account_id;

    return 0u;
}

bool rewrite_dota_lobby_template_member_object(
    const std::string &input,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool force_connected_leaver_state,
    std::string &output)
{
    output.clear();
    bool saw_team = false;
    bool saw_slot = false;
    bool saw_hero_id = false;
    bool saw_leaver_status = false;
    bool saw_leaver_actions = false;

    std::size_t offset = 0;
    while (offset < input.size()) {
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u) {
            if (field.wire_type == 1u) {
                proto_wire::append_fixed64_field(output, 1u, steam_id);
                continue;
            }

            if (field.wire_type == 0u) {
                proto_wire::append_varint_field(output, 1u, account_id);
                continue;
            }
        }

        if (field.number == 55u && field.wire_type == 0u) {
            proto_wire::append_varint_field(output, 55u, account_id);
            continue;
        }

        if (field.number == 2u && field.wire_type == 0u) {
            saw_hero_id = true;
            if (owner_hero_id != 0u)
                proto_wire::append_varint_field(output, 2u, owner_hero_id);
            continue;
        }

        if (field.number == 3u && field.wire_type == 0u) {
            saw_team = true;
            proto_wire::append_varint_field(output, 3u, owner_team);
            continue;
        }

        if (field.number == 7u && field.wire_type == 0u) {
            saw_slot = true;
            proto_wire::append_varint_field(output, 7u, owner_slot);
            continue;
        }

        if (field.number == 16u && field.wire_type == 5u) {
            saw_leaver_status = true;
            if (force_connected_leaver_state) {
                proto_wire::append_fixed32_field(output, 16u, 0u);
                continue;
            }
        }

        if (field.number == 28u && field.wire_type == 0u) {
            saw_leaver_actions = true;
            if (force_connected_leaver_state) {
                proto_wire::append_varint_field(output, 28u, 0u);
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_team)
        proto_wire::append_varint_field(output, 3u, owner_team);

    if (!saw_slot)
        proto_wire::append_varint_field(output, 7u, owner_slot);

    if (!saw_hero_id && owner_hero_id != 0u)
        proto_wire::append_varint_field(output, 2u, owner_hero_id);

    if (force_connected_leaver_state && !saw_leaver_status)
        proto_wire::append_fixed32_field(output, 16u, 0u);

    if (force_connected_leaver_state && !saw_leaver_actions)
        proto_wire::append_varint_field(output, 28u, 0u);

    return true;
}

bool rewrite_dota_server_static_lobby_member_object(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output)
{
    std::string account_rewritten_input = input;
    if (account_id != 0) {
        std::string rewritten_account_fields;
        std::size_t replacement_count = 0;
        if (!proto_wire::rewrite_varint_bytes_recursive(
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
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(account_rewritten_input.data()), account_rewritten_input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 1u) {
            saw_steam_id = true;
            proto_wire::append_fixed64_field(output, 1u, steam_id);
            continue;
        }

        if (field.number == 11u && field.wire_type == 0u) {
            proto_wire::append_varint_field(output, 11u, 1u);
            continue;
        }

        output.append(account_rewritten_input.data() + field_offset, field_end - field_offset);
    }

    if (saw_steam_id)
        return true;

    proto_wire::append_fixed64_field(output, 1u, steam_id);
    return true;
}

bool rewrite_dota_lobby_template_object_2004(
    const std::string &input,
    const DotaLobbyTemplateObject2004RewriteOptions &options,
    std::string &output)
{
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
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;
        const std::uint32_t field_number = static_cast<std::uint32_t>(field.number);
        const std::uint32_t wire_type = static_cast<std::uint32_t>(field.wire_type);

        if (field_number == 1u && wire_type == 0u) {
            saw_lobby_id = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_varint_field(output, 1u, options.lobby_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 3u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 3u, options.game_mode);
            continue;
        }

        if (field_number == 4u && wire_type == 0u) {
            saw_state = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_varint_field(output, 4u, options.lobby_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 5u && wire_type == 2u) {
            saw_connect = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_bytes_field(output, 5u, proto_wire::normalize_dota_practice_lobby_connect(options.connect));
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 6u && wire_type == 1u) {
            saw_server_id = true;
            if (options.rewrite_runtime_fields) {
                if (options.server_id != 0)
                    proto_wire::append_fixed64_field(output, 6u, options.server_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 13u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 13u, options.allow_cheats ? 1u : 0u);
            continue;
        }

        if (field_number == 16u && wire_type == 2u) {
            saw_room_name = true;
            proto_wire::append_bytes_field(output, 16u, options.room_name);
            continue;
        }

        if (field_number == 11u && wire_type == 1u) {
            proto_wire::append_fixed64_field(output, 11u, options.steam_id);
            continue;
        }

        if (field_number == 14u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 14u, options.fill_with_bots ? 1u : 0u);
            continue;
        }

        if (field_number == 21u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 21u, options.server_region);
            continue;
        }

        if (field_number == 22u && wire_type == 0u) {
            saw_game_state = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_varint_field(output, 22u, options.lobby_game_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 31u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 31u, options.allow_spectating ? 1u : 0u);
            continue;
        }

        if (field_number == 30u && wire_type == 0u) {
            saw_match_id = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_varint_field(output, 30u, options.match_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 36u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 36u, options.bot_difficulty_radiant);
            continue;
        }

        if (field_number == 39u && wire_type == 2u) {
            proto_wire::append_bytes_field(output, 39u, options.pass_key);
            continue;
        }

        if (field_number == 57u && wire_type == 0u) {
            saw_lan = true;
            proto_wire::append_varint_field(output, 57u, options.lan ? 1u : 0u);
            continue;
        }

        if (field_number == 75u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 75u, options.visibility);
            continue;
        }

        if (field_number == 87u && wire_type == 0u) {
            saw_game_start_time = true;
            if (options.rewrite_runtime_fields) {
                proto_wire::append_varint_field(output, 87u, options.game_start_time);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 93u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 93u, options.bot_difficulty_dire);
            continue;
        }

        if (field_number == 94u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 94u, options.bot_radiant);
            continue;
        }

        if (field_number == 95u && wire_type == 0u) {
            proto_wire::append_varint_field(output, 95u, options.bot_dire);
            continue;
        }

        if (field_number == 109u && wire_type == 2u) {
            saw_lan_host_ping_location = true;
            if (!options.lan_host_ping_location.empty())
                proto_wire::append_bytes_field(output, 109u, options.lan_host_ping_location);
            continue;
        }

        if (field_number == 120u && wire_type == 2u) {
            if (!options.rewrite_runtime_fields) {
                output.append(input.data() + field_offset, field_end - field_offset);
                continue;
            }

            std::string rewritten_member;
            if (!rewrite_dota_lobby_template_member_object(
                    std::string(input.data() + field.value_offset, field.value_size),
                    options.account_id,
                    options.steam_id,
                    options.owner_team,
                    options.owner_slot,
                    options.owner_hero_id,
                    false,
                    rewritten_member))
                return false;
            proto_wire::append_bytes_field(output, 120u, rewritten_member);
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
        proto_wire::append_varint_field(output, 1u, options.lobby_id);
    if (options.rewrite_runtime_fields && !saw_state)
        proto_wire::append_varint_field(output, 4u, options.lobby_state);
    if (options.rewrite_runtime_fields && !saw_connect && !options.connect.empty())
        proto_wire::append_bytes_field(output, 5u, proto_wire::normalize_dota_practice_lobby_connect(options.connect));
    if (options.rewrite_runtime_fields && !saw_server_id && options.server_id != 0)
        proto_wire::append_fixed64_field(output, 6u, options.server_id);
    if (!saw_lan)
        proto_wire::append_varint_field(output, 57u, options.lan ? 1u : 0u);
    if (!saw_room_name)
        proto_wire::append_bytes_field(output, 16u, options.room_name);
    if (options.rewrite_runtime_fields && !saw_game_state)
        proto_wire::append_varint_field(output, 22u, options.lobby_game_state);
    if (options.rewrite_runtime_fields && !saw_match_id && options.match_id != 0)
        proto_wire::append_varint_field(output, 30u, options.match_id);
    if (!saw_lan_host_ping_location && !options.lan_host_ping_location.empty())
        proto_wire::append_bytes_field(output, 109u, options.lan_host_ping_location);
    if (options.rewrite_runtime_fields && !saw_game_start_time && options.game_start_time != 0)
        proto_wire::append_varint_field(output, 87u, options.game_start_time);

    return true;
}

bool rewrite_dota_lobby_template_object_2014(
    const std::string &input,
    const std::string &player_name,
    std::string &output)
{
    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 2u) {
            std::string rewritten_member;
            std::size_t member_offset = 0;
            while (member_offset < field.value_size) {
                proto_wire::Field member{};
                std::size_t member_field_offset = 0;
                std::size_t member_field_end = 0;
                if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()) + field.value_offset, field.value_size, member_offset, member, &member_field_offset, &member_field_end))
                    return false;

                if (member.number == 1u && member.wire_type == 2u) {
                    proto_wire::append_bytes_field(rewritten_member, 1u, player_name);
                    continue;
                }

                rewritten_member.append(input.data() + field.value_offset + member_field_offset, member_field_end - member_field_offset);
            }

            proto_wire::append_bytes_field(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

bool rewrite_dota_lobby_template_object_2015(
    const std::string &input,
    bool clear_existing_startup_data,
    std::uint32_t additional_startup_type_id,
    const std::string &additional_startup_payload,
    std::string &output)
{
    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;
        const std::uint32_t field_number = static_cast<std::uint32_t>(field.number);
        const std::uint32_t wire_type = static_cast<std::uint32_t>(field.wire_type);

        if (clear_existing_startup_data && field_number == 2u && wire_type == 2u) {
            std::uint64_t startup_type = 0;
            if (proto_wire::read_uint64_field(reinterpret_cast<const std::uint8_t *>(input.data()) + field.value_offset, field.value_size, 1u, startup_type)
                && startup_type == additional_startup_type_id)
                continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!additional_startup_payload.empty()) {
        std::string startup_message;
        proto_wire::append_varint_field(startup_message, 1u, additional_startup_type_id);
        proto_wire::append_bytes_field(startup_message, 2u, additional_startup_payload);
        proto_wire::append_bytes_field(output, 2u, startup_message);
    }

    return true;
}

bool rewrite_dota_lobby_template_object_2016(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output,
    DotaLobbyTemplateObject2016RewriteDebug *debug)
{
    if (debug) {
        debug->first_member_input.clear();
        debug->first_member_output.clear();
    }

    output.clear();

    std::size_t offset = 0;
    while (offset < input.size()) {
        proto_wire::Field field{};
        std::size_t field_offset = 0;
        std::size_t field_end = 0;
        if (!proto_wire::read_next_field(reinterpret_cast<const std::uint8_t *>(input.data()), input.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number == 1u && field.wire_type == 2u) {
            const std::string member_input(input.data() + field.value_offset, field.value_size);
            std::string rewritten_member;
            if (!rewrite_dota_server_static_lobby_member_object(
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

            proto_wire::append_bytes_field(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

} // namespace gbe::dota_gc_wire

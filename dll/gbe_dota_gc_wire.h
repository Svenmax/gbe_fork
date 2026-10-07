#ifndef GBE_DOTA_GC_WIRE_H
#define GBE_DOTA_GC_WIRE_H

#include <cstdint>
#include <string>
#include <vector>

namespace gbe::dota_gc_wire {

bool should_prefer_dota_lobby_connect_update(
    const std::string &current_connect,
    const std::string &candidate_connect);

std::uint32_t get_dota_practice_lobby_startup_account_id_for_state(
    std::uint32_t account_id,
    std::uint32_t lobby_state,
    std::uint32_t lobby_game_state);

bool rewrite_dota_lobby_template_member_object(
    const std::string &input,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool force_connected_leaver_state,
    std::string &output);

bool rewrite_dota_server_static_lobby_member_object(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output);

struct DotaLobbyTemplateObject2004RewriteOptions {
    std::uint32_t account_id = 0;
    std::uint64_t steam_id = 0;
    std::uint64_t lobby_id = 0;
    bool rewrite_runtime_fields = false;
    std::uint32_t lobby_state = 0;
    std::uint32_t lobby_game_state = 0;
    std::uint64_t server_id = 0;
    std::uint64_t match_id = 0;
    std::uint32_t game_start_time = 0;
    std::string connect;
    std::string room_name;
    std::uint32_t game_mode = 0;
    std::uint32_t server_region = 0;
    bool lan = false;
    std::string lan_host_ping_location;
    bool allow_cheats = false;
    bool fill_with_bots = false;
    bool allow_spectating = false;
    std::uint32_t visibility = 0;
    std::uint32_t bot_difficulty_radiant = 0;
    std::uint32_t bot_difficulty_dire = 0;
    std::uint64_t bot_radiant = 0;
    std::uint64_t bot_dire = 0;
    std::uint32_t owner_team = 0;
    std::uint32_t owner_slot = 0;
    std::uint32_t owner_hero_id = 0;
    std::string pass_key;
};

bool rewrite_dota_lobby_template_object_2004(
    const std::string &input,
    const DotaLobbyTemplateObject2004RewriteOptions &options,
    std::string &output);

bool rewrite_dota_lobby_template_object_2014(
    const std::string &input,
    const std::string &player_name,
    std::string &output);

bool rewrite_dota_lobby_template_object_2015(
    const std::string &input,
    bool clear_existing_startup_data,
    std::uint32_t additional_startup_type_id,
    const std::string &additional_startup_payload,
    std::string &output);

struct DotaLobbyTemplateObject2016RewriteDebug {
    std::string first_member_input;
    std::string first_member_output;
};

bool rewrite_dota_lobby_template_object_2016(
    const std::string &input,
    const std::vector<std::uint8_t> &old_account_id_varint,
    std::uint32_t account_id,
    std::uint64_t steam_id,
    std::string &output,
    DotaLobbyTemplateObject2016RewriteDebug *debug = nullptr);

} // namespace gbe::dota_gc_wire

#endif // GBE_DOTA_GC_WIRE_H

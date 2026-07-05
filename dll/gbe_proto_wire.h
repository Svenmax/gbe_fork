#ifndef GBE_PROTO_WIRE_H
#define GBE_PROTO_WIRE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::proto_wire {

struct Field {
    std::uint32_t number{};
    std::uint32_t wire_type{};
    std::size_t value_offset{};
    std::size_t value_size{};
};

struct DotaEmptyRequestShape {
    bool valid{};
    std::uint32_t field_count{};
};

struct DotaRankRequestShape {
    bool valid{};
    std::uint32_t field_count{};
    std::uint32_t rank_type{};
    bool has_rank_type{};
};

struct Dota8053Result {
    std::uint64_t lobby_id{};
    std::uint64_t loading_duration{};
    std::uint64_t result_code{};
    std::uint64_t signon_states{};
    std::string result_text;
};

struct Dota7070ReadyUpRequest {
    std::uint32_t ready_state{};
    bool has_ready_state{};
};

struct Dota8052StartedLoadingRequest {
    std::uint64_t lobby_id{};
    std::uint64_t custom_game_id{};
    std::uint64_t start_time{};
    bool has_lobby_id{};
    bool has_custom_game_id{};
    bool has_start_time{};
};

struct Dota8053FinishedLoadingRequest {
    std::uint64_t lobby_id{};
    std::uint64_t loading_duration{};
    std::uint64_t result_code{};
    std::uint64_t signon_states{};
    std::string result_text;
    bool has_lobby_id{};
    bool has_loading_duration{};
    bool has_result_code{};
    bool has_result_text{};
    bool has_signon_states{};
};

struct Dota7034ConnectedPlayer {
    std::uint64_t steam_id{};
    std::uint32_t hero_id{};
    bool has_steam_id{};
    bool has_hero_id{};
};

struct Dota7034DisconnectedPlayer {
    std::uint64_t steam_id{};
    std::uint32_t lobby_state{};
    std::uint32_t game_state{};
    bool has_steam_id{};
    bool has_lobby_state{};
    bool has_game_state{};
};

struct Dota7034RequestShape {
    std::uint32_t game_state{};
    std::uint32_t send_reason{};
    std::uint32_t first_blood_happened{};
    std::uint32_t radiant_kills{};
    std::uint32_t dire_kills{};
    std::uint32_t radiant_lead{};
    std::uint32_t building_state{};
    std::uint64_t draft_steam_id{};
    std::uint32_t draft_team{};
    std::uint32_t draft_team_slot{};
    std::vector<Dota7034ConnectedPlayer> connected_players;
    std::vector<Dota7034DisconnectedPlayer> disconnected_players;
    bool has_game_state{};
    bool has_send_reason{};
    bool has_first_blood_happened{};
    bool has_radiant_kills{};
    bool has_dire_kills{};
    bool has_radiant_lead{};
    bool has_building_state{};
    bool has_connected_player{};
    bool has_draft{};
    bool has_draft_steam_id{};
    bool has_draft_team{};
    bool has_draft_team_slot{};
    bool has_disconnected_player{};
};

struct Dota7034RuntimeRequest {
    std::uint32_t game_state{};
    std::uint32_t send_reason{};
    std::uint32_t first_blood_happened{};
    std::uint32_t radiant_kills{};
    std::uint32_t dire_kills{};
    std::uint32_t radiant_lead{};
    std::uint32_t building_state{};
    std::uint64_t draft_steam_id{};
    std::uint32_t draft_team{};
    std::uint32_t draft_team_slot{};
    std::vector<Dota7034ConnectedPlayer> connected_players;
    std::vector<Dota7034DisconnectedPlayer> disconnected_players;
    bool has_game_state{};
    bool has_send_reason{};
    bool has_first_blood_happened{};
    bool has_radiant_kills{};
    bool has_dire_kills{};
    bool has_radiant_lead{};
    bool has_building_state{};
    bool has_connected_player{};
    bool has_draft{};
    bool has_draft_steam_id{};
    bool has_draft_team{};
    bool has_draft_team_slot{};
    bool has_disconnected_player{};
};

struct DotaPracticeLobbySetTeamSlotRequest {
    bool has_team{};
    std::uint32_t team{};
    bool has_slot{};
    std::uint32_t slot{};
    bool has_bot_difficulty{};
    std::uint32_t bot_difficulty{};
};

struct DotaPracticeLobbyKickRequest {
    bool has_account_id{};
    std::uint32_t account_id{};
};

struct DotaPracticeLobbyJoinRequest {
    bool has_lobby_id{};
    std::uint64_t lobby_id{};
    bool has_pass_key{};
    std::string pass_key;
};

struct DotaInviteToLobbyRequest {
    bool has_steam_id{};
    std::uint64_t steam_id{};
    bool has_client_version{};
    std::uint32_t client_version{};
};

struct DotaLobbyInviteResponseRequest {
    bool has_lobby_id{};
    std::uint64_t lobby_id{};
    bool has_accept{};
    bool accept{};
    bool has_client_version{};
    std::uint32_t client_version{};
    bool has_custom_game_crc{};
    std::uint64_t custom_game_crc{};
    bool has_custom_game_timestamp{};
    std::uint32_t custom_game_timestamp{};
};

struct DotaPracticeLobbyBroadcastChannelRequest {
    bool has_channel{};
    std::uint32_t channel{};
    bool has_country_code{};
    std::string country_code;
    bool has_description{};
    std::string description;
    bool has_language_code{};
    std::string language_code;
};

struct DotaPracticeLobbyDetailsRequest {
    bool has_lobby_id{};
    std::uint64_t lobby_id{};
    bool has_room_name{};
    std::string room_name;
    bool has_server_region{};
    std::uint32_t server_region{};
    bool has_lan{};
    bool lan{};
    bool has_lan_host_ping_location{};
    std::string lan_host_ping_location;
    bool has_game_mode{};
    std::uint32_t game_mode{};
    bool has_bot_difficulty_radiant{};
    std::uint32_t bot_difficulty_radiant{};
    bool has_allow_cheats{};
    bool allow_cheats{};
    bool has_fill_with_bots{};
    bool fill_with_bots{};
    bool has_allow_spectating{};
    bool allow_spectating{};
    bool has_pass_key{};
    std::string pass_key;
    bool has_visibility{};
    std::uint32_t visibility{};
    bool has_bot_difficulty_dire{};
    std::uint32_t bot_difficulty_dire{};
    bool has_bot_radiant{};
    std::uint64_t bot_radiant{};
    bool has_bot_dire{};
    std::uint64_t bot_dire{};
    bool has_custom_game_mode{};
    std::string custom_game_mode;
    bool has_custom_map_name{};
    std::string custom_map_name;
    bool has_custom_difficulty{};
    std::uint32_t custom_difficulty{};
    bool has_custom_game_id{};
    std::uint64_t custom_game_id{};
    bool has_custom_min_players{};
    std::uint32_t custom_min_players{};
    bool has_custom_max_players{};
    std::uint32_t custom_max_players{};
    bool has_custom_game_crc{};
    std::uint64_t custom_game_crc{};
    bool has_custom_game_timestamp{};
    std::uint32_t custom_game_timestamp{};
    bool has_custom_game_penalties{};
    bool custom_game_penalties{};
};

struct DotaPracticeLobbyCreateRequest {
    bool has_pass_key{};
    std::string pass_key;
    bool has_lobby_details{};
    DotaPracticeLobbyDetailsRequest lobby_details;
};

struct DotaJoinChatChannelRequest {
    bool has_channel_name{};
    std::string channel_name;
    bool has_channel_type{};
    std::uint32_t channel_type{};
};

struct DotaLeaveChatChannelRequest {
    bool has_channel_id{};
    std::uint64_t channel_id{};
};

struct DotaChatMessageRequest {
    bool has_account_id{};
    std::uint32_t account_id{};
    bool has_channel_id{};
    std::uint64_t channel_id{};
    bool has_persona_name{};
    std::string persona_name;
    bool has_text{};
    std::string text;
};

struct PatchTemplateIdentifierResult {
    std::size_t lobby_id_match_count{};
    std::size_t steam_id_fixed64_match_count{};
};

// Practice lobby launch template patch points:
// - SteamID fixed64: required owner/local SteamID replacement
// - lobby_id varint: required when require_lobby_id is set by caller
// - match_id varint: optional size-checked replacement
// - server_id fixed64: optional replacement guarded by patch_server_id
// - game_start_time varint: optional size-checked replacement
// - connect string: optional same-length endpoint replacement
struct DotaPracticeLobbyLaunchTemplatePatchResult {
    std::size_t steam_id_fixed64_match_count{};
    bool lobby_id_size_ok{};
    std::size_t lobby_id_match_count{};
    bool match_id_size_ok{};
    std::size_t match_id_match_count{};
    std::size_t server_id_fixed64_match_count{};
    bool game_start_time_size_ok{};
    std::size_t game_start_time_match_count{};
    bool connect_size_ok{};
    std::size_t connect_match_count{};
};

// Practice lobby peripheral/persona patch points:
// - SteamID fixed64: required replacement for owner/persona identity
// - server_id fixed64: optional replacement guarded by patch_server_id
// - lobby_id text: optional same-length decimal text replacement
struct DotaPracticeLobbyPeripheralTemplatePatchResult {
    std::size_t steam_id_fixed64_match_count{};
    std::size_t server_id_fixed64_match_count{};
    std::size_t lobby_id_text_match_count{};
};

bool read_varuint(const std::vector<std::uint8_t> &bytes, std::size_t &offset, std::uint64_t &value, std::size_t *raw_size = nullptr);
bool read_varuint(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint64_t &value, std::size_t *raw_begin = nullptr, std::size_t *raw_end = nullptr);
std::uint64_t read_little_endian(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size);
std::vector<Field> parse_fields(const std::vector<std::uint8_t> &bytes);
bool read_next_field(const std::uint8_t *data, std::size_t size, std::size_t &offset, Field &field, std::size_t *field_offset = nullptr, std::size_t *field_end = nullptr);
bool find_field(const std::uint8_t *data, std::size_t size, std::uint32_t wanted_field, Field &field);
bool read_field_uint64(const std::uint8_t *data, std::size_t size, const Field &field, std::uint64_t &value);
bool read_field_uint32(const std::uint8_t *data, std::size_t size, const Field &field, std::uint32_t &value);
bool read_field_bytes(const std::uint8_t *data, std::size_t size, const Field &field, std::string &value);
bool read_uint64_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::uint64_t &value);
bool read_uint32_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::uint32_t &value);
bool read_bytes_field(const std::uint8_t *data, std::size_t size, std::uint32_t field_number, std::string &value);
bool extract_packed_uint32_field(const std::uint8_t *data, std::size_t size, const Field &field, std::vector<std::uint32_t> &values);
DotaEmptyRequestShape parse_dota_empty_request_shape(const std::uint8_t *data, std::size_t size);
DotaRankRequestShape parse_dota_rank_request_shape(const std::uint8_t *data, std::size_t size);
Dota7070ReadyUpRequest parse_dota7070_ready_up_request(const std::uint8_t *data, std::size_t size);
Dota8052StartedLoadingRequest parse_dota8052_started_loading_request(const std::uint8_t *data, std::size_t size);
Dota8053FinishedLoadingRequest parse_dota8053_finished_loading_request(const std::uint8_t *data, std::size_t size);
Dota8053Result parse_dota8053_result(const std::uint8_t *data, std::size_t size);
Dota7034RequestShape parse_dota7034_request_shape(const std::uint8_t *data, std::size_t size);
Dota7034RuntimeRequest parse_dota7034_runtime_request(const std::uint8_t *data, std::size_t size);
bool parse_dota_practice_lobby_set_team_slot_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbySetTeamSlotRequest &request);
bool parse_dota_practice_lobby_kick_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyKickRequest &request);
bool parse_dota_practice_lobby_join_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyJoinRequest &request);
bool parse_dota_invite_to_lobby_body(const std::uint8_t *data, std::size_t size, DotaInviteToLobbyRequest &request);
bool parse_dota_lobby_invite_response_body(const std::uint8_t *data, std::size_t size, DotaLobbyInviteResponseRequest &request);
bool parse_dota_practice_lobby_join_broadcast_channel_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request);
bool parse_dota_lobby_update_broadcast_channel_info_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request);
bool parse_dota_practice_lobby_close_broadcast_channel_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyBroadcastChannelRequest &request);
bool parse_dota_practice_lobby_set_details_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyDetailsRequest &request);
bool parse_dota_practice_lobby_create_body(const std::uint8_t *data, std::size_t size, DotaPracticeLobbyCreateRequest &request);
bool parse_dota_join_chat_channel_body(const std::uint8_t *data, std::size_t size, DotaJoinChatChannelRequest &request);
bool parse_dota_leave_chat_channel_body(const std::uint8_t *data, std::size_t size, DotaLeaveChatChannelRequest &request);
bool parse_dota_chat_message_body(const std::uint8_t *data, std::size_t size, DotaChatMessageRequest &request);
void append_varuint(std::string &buffer, std::uint64_t value);
void append_little_endian32(std::string &buffer, std::uint32_t value);
void append_little_endian64(std::string &buffer, std::uint64_t value);
void append_varint_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value);
void append_fixed64_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value);
void append_bytes_field(std::string &buffer, std::uint32_t field_number, const std::string &value);
void append_fixed32_field(std::string &buffer, std::uint32_t field_number, std::uint32_t value);
bool rewrite_varint_fields(const std::string &input, const std::vector<std::uint32_t> &field_numbers, std::uint64_t value, std::string &output, bool *rewrote = nullptr);
bool rewrite_varint_bytes_recursive(const std::string &input, const std::vector<std::uint8_t> &old_encoded, std::uint32_t target_field_number, std::uint64_t new_value, std::string &output, std::size_t &replacement_count);
bool rewrite_dota_account_bound_object_data(const std::string &input, int type_id, std::uint32_t account_id, std::string &output);
bool patch_dota_lobby_template_identifiers(std::string &message, const std::vector<std::uint8_t> &old_lobby_id_varint, std::uint64_t lobby_id, bool require_lobby_id, const std::vector<std::uint8_t> &old_steam_id_fixed64, std::uint64_t steam_id, bool require_steam_id_fixed64, PatchTemplateIdentifierResult &result);
bool patch_fixed32_template_value(std::string &message, const std::vector<std::uint8_t> &old_fixed32, std::uint32_t value, std::size_t &match_count);
bool patch_varint_template_value(std::string &message, const std::vector<std::uint8_t> &old_varint, std::uint64_t value, std::size_t &match_count, bool &size_ok);
bool patch_dota_practice_lobby_launch_template(std::string &message, const std::vector<std::uint8_t> &old_steam_id_fixed64, std::uint64_t steam_id, const std::vector<std::uint8_t> &old_lobby_id_varint, std::uint64_t lobby_id, const std::vector<std::uint8_t> &old_match_id_varint, std::uint64_t match_id, bool patch_server_id, const std::vector<std::uint8_t> &old_server_id_fixed64, std::uint64_t server_id, bool patch_game_start_time, const std::vector<std::uint8_t> &old_game_start_time_varint, std::uint32_t game_start_time, bool patch_connect, const std::string &old_connect, const std::string &connect, DotaPracticeLobbyLaunchTemplatePatchResult &result);
bool patch_dota_practice_lobby_peripheral_template(std::string &message, const std::vector<std::uint8_t> &old_steam_id_fixed64, const std::vector<std::uint8_t> &old_persona_steam_id_fixed64, std::uint64_t steam_id, bool patch_server_id, const std::vector<std::uint8_t> &old_server_id_fixed64, std::uint64_t server_id, const std::vector<std::string> &old_lobby_id_texts, std::uint64_t lobby_id, DotaPracticeLobbyPeripheralTemplatePatchResult &result);
std::string summarize_repeated_varint_field(const std::string &input, std::uint32_t target_field);
std::uint32_t count_repeated_bytes_field(const std::string &input, std::uint32_t target_field);
std::string format_field_layout_summary(const std::string &input);
std::string format_top_level_field_summary(const std::uint8_t *data, std::size_t size);
std::string format_dota_lobby_member_state_summary(const std::string &input);
std::string format_dota_lobby_aux_field_summary(const std::string &input);
std::string format_dota_static_lobby_member_summary(const std::string &input);
std::string format_dota_server_static_lobby_member_summary(const std::string &input);
std::string format_dota7034_leaver_state_summary(const std::string &input);
std::string format_dota7034_player_summary(const std::string &input);
std::string format_dota7034_draft_summary(const std::string &input);
std::string format_dota7034_summary(const std::uint8_t *data, std::size_t size);
std::uint64_t fnv1a64(const std::vector<std::uint8_t> &bytes);
std::string hex_prefix(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size, std::size_t max_bytes);
std::string format_hex_prefix(const std::uint8_t *data, std::size_t size, std::size_t max_bytes);
std::string format_hex(const std::uint8_t *data, std::size_t size);
std::string format_ipv4(std::uint32_t value);
std::string sanitize_proto_log_string(const std::string &value);
std::uint64_t parse_uint64_or_zero(const char *text);
std::uint32_t parse_uint32_or_zero(const char *text);
bool dota_string_is_unsigned_integer(const std::string &value);
bool dota_is_readable_custom_game_name(const std::string &value);
bool dota_is_rank_type_supported(std::uint32_t rank_type);
bool dota_is_dire_team(std::uint32_t team);
bool dota8053_indicates_load_failure(std::uint64_t result_code, const std::string &result_text);
bool is_dota_practice_lobby_prelaunch_state(std::uint64_t server_id, std::uint64_t match_id, std::uint32_t game_start_time, const std::string &connect);
bool decode_hex_string(const char *hex, std::string &decoded);
bool find_and_overwrite_bytes(std::string &buffer, const std::vector<std::uint8_t> &needle, const std::vector<std::uint8_t> &replacement);
bool find_and_overwrite_string(std::string &buffer, const std::string &needle, const std::string &replacement);
bool encode_varuint_with_expected_size(std::uint64_t value, std::size_t expected_size, std::vector<std::uint8_t> &encoded);
std::vector<std::uint8_t> vector_from_bytes(const std::uint8_t *data, std::size_t size);
std::size_t count_byte_pattern_matches(const std::string &buffer, const std::vector<std::uint8_t> &needle);
std::string normalize_dota_practice_lobby_connect(const std::string &connect);
std::string normalize_dota_practice_lobby_connect_pair(const std::string &connect);
std::string format_dota_practice_lobby_connect_pair(const std::string &endpoint);
std::string format_dota_practice_lobby_loopback_connect();
std::string format_dota_practice_lobby_connect_from_endpoint(const char *endpoint);
std::string format_dota_practice_lobby_endpoint_from_ip(std::uint32_t ip, std::uint32_t port);
std::string format_dota_practice_lobby_connect_from_ips(std::uint32_t public_ip, std::uint32_t private_ip, std::uint32_t port);
std::string format_dota_practice_lobby_connect_from_ip(std::uint32_t ip);
std::uint32_t parse_dota_practice_lobby_connect_ipv4(const std::string &connect);
std::string get_dota_practice_lobby_first_connect_endpoint(const std::string &connect);
std::string select_dota_arcade_connect_endpoint_for_local_player(const std::string &connect, std::uint64_t local_steam_id, std::uint64_t owner_steam_id);

} // namespace gbe::proto_wire

#endif // GBE_PROTO_WIRE_H

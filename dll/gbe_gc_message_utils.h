#ifndef GBE_GC_MESSAGE_UTILS_H
#define GBE_GC_MESSAGE_UTILS_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace gbe::gc_message {

constexpr std::uint32_t kProtoMask = 0x80000000u;

struct DotaChatMember {
    std::uint64_t steam_id{};
    std::string persona_name;
};

struct Dota7034Player {
    std::uint64_t steam_id{};
    std::uint32_t hero_id{};
    std::uint32_t team{};
    std::uint32_t slot{};
    std::uint32_t lobby_state{};
    std::uint32_t game_state{};
    bool include_draft{};
};

struct Dota7034ExtraState {
    bool has_first_blood_happened{};
    std::uint32_t first_blood_happened{};
    bool has_send_reason{};
    std::uint32_t send_reason{};
    bool has_radiant_kills{};
    std::uint32_t radiant_kills{};
    bool has_dire_kills{};
    std::uint32_t dire_kills{};
    bool has_radiant_lead{};
    std::uint32_t radiant_lead{};
    bool has_building_state{};
    std::uint32_t building_state{};
};

struct DotaLobbyMemberObjectState {
    std::uint64_t steam_id{};
    std::uint32_t team{};
    std::uint32_t slot{};
    std::uint32_t hero_id{};
    bool connected{};
    std::uint32_t leaver_status{};
};

struct DotaStaticLobbyMember {
    std::uint64_t steam_id{};
    std::uint32_t account_id{};
    bool connected{};
};

struct DotaCustomGameDetails {
    std::string mode;
    std::string map_name;
    std::uint32_t difficulty{};
    std::uint64_t game_id{};
    std::uint32_t min_players{};
    std::uint32_t max_players{};
    std::uint64_t crc{};
    std::uint32_t timestamp{};
    bool penalties{};
};

struct DotaLobbyObject2004Options {
    std::uint64_t steam_id{};
    std::uint64_t lobby_id{};
    std::uint32_t lobby_state{};
    std::uint32_t lobby_game_state{};
    std::uint64_t server_id{};
    std::uint64_t match_id{};
    std::uint32_t game_start_time{};
    std::uint32_t elapsed_game_time{};
    std::string connect;
    std::string room_name;
    std::uint32_t game_mode{};
    std::uint32_t server_region{};
    bool lan{};
    std::string lan_host_ping_location;
    bool allow_cheats{};
    bool fill_with_bots{};
    bool allow_spectating{};
    std::uint32_t visibility{};
    std::uint32_t bot_difficulty_radiant{};
    std::uint32_t bot_difficulty_dire{};
    std::uint64_t bot_radiant{};
    std::uint64_t bot_dire{};
    bool has_broadcast_channel{};
    std::uint32_t broadcast_channel_id{};
    std::string broadcast_country_code;
    std::string broadcast_description;
    std::string broadcast_language_code;
    std::string pass_key;
    const DotaCustomGameDetails *custom_game{};
};

struct DotaPracticeLobbyObjects {
    std::string object_2015;
    std::string object_2016;
    std::string object_2004;
    std::string object_2014;
};

struct DotaPracticeLobbyObjectOptions {
    std::uint32_t extra_startup_account_id{};
    std::uint64_t steam_id{};
    std::uint32_t game_mode{};
    bool is_custom_game{};
    std::string player_name;
    DotaLobbyObject2004Options object_2004_options;
    std::vector<DotaLobbyMemberObjectState> lobby_members;
    std::vector<DotaStaticLobbyMember> static_lobby_members;
};

struct DotaJoinableCustomGameMode {
    std::uint64_t custom_game_id{};
    std::uint32_t member_count{};
};

struct DotaJoinableCustomLobby {
    std::uint64_t lobby_id{};
    std::uint64_t custom_game_id{};
    std::string display_name;
    std::uint32_t member_count{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    std::string map_name;
    std::uint32_t max_players{};
    std::uint32_t server_region{};
    bool has_pass_key{};
    std::string lan_host_ping_location;
    std::uint32_t created_time{};
    std::uint32_t custom_game_timestamp{};
    std::uint64_t custom_game_crc{};
    std::uint32_t min_players{};
    bool penalties{};
};

struct DotaSourceTVPlayer {
    std::uint32_t account_id{};
    std::uint32_t hero_id{};
    std::uint32_t slot{};
    std::uint32_t team{};
};

struct DotaSourceTVGame {
    std::uint32_t start_time{};
    std::uint64_t server_id{};
    std::uint64_t lobby_id{};
    std::uint32_t game_time{};
    std::uint32_t game_mode{};
    std::uint64_t match_id{};
    std::vector<DotaSourceTVPlayer> players;
};

bool has_proto_mask(std::uint32_t emsg);
std::uint32_t without_proto_mask(std::uint32_t emsg);
std::uint32_t with_proto_mask(std::uint32_t emsg);
bool should_trace_dota_proto_boundary(std::uint32_t emsg);
bool is_supported_dota_wrapped_post_login_request(std::uint32_t inner_emsg);

bool build_dota_zero_header_payload(std::uint32_t emsg, const std::string &body, std::string &message);
bool build_dota_job_reply_payload(std::uint32_t emsg, std::uint64_t request_job_id, const std::string &body, std::string &message);
bool build_dota_job_reply_or_zero_header_payload(std::uint32_t emsg, bool has_request_job, std::uint64_t request_job_id, const std::string &body, std::string &message);
bool build_wrapped_dota_replay_message(std::uint32_t outer_emsg, std::uint32_t app_id, const std::string &inner_payload, const std::string &outer_session_field_raw, std::uint64_t steam_id, std::string &message);
bool build_dota_varint_response_payload(std::uint32_t emsg, std::uint32_t field_number, std::uint64_t value, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_fixed32_response_payload(std::uint32_t emsg, std::uint32_t field_number, std::uint32_t value, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_bytes_response_payload(std::uint32_t emsg, std::uint32_t field_number, const std::string &value, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_lobby_additional_startup_account_data_payload(std::uint32_t account_id, std::string &payload);
bool build_dota_server_lobby_object_2015(std::size_t member_count, std::uint32_t extra_startup_account_id, std::string &object_2015);
void build_dota_server_static_lobby_object_2016(std::uint32_t account_id, std::uint64_t steam_id, std::uint32_t game_mode, const std::vector<DotaStaticLobbyMember> &members, std::string &object_2016, bool is_custom_game = false);
std::string build_dota_lobby_team_details_payload(bool is_home_team);
void build_dota_static_lobby_object_2014(const std::string &player_name, std::size_t member_count, std::string &object_2014);
void build_dota_lobby_member_object_2004(const DotaLobbyMemberObjectState &member, std::uint32_t lobby_state, std::uint32_t lobby_game_state, std::string &member_state);
void build_dota_lobby_object_2004(const DotaLobbyObject2004Options &options, const std::vector<DotaLobbyMemberObjectState> &members, std::string &object_2004);
bool build_dota_practice_lobby_objects(const DotaPracticeLobbyObjectOptions &options, DotaPracticeLobbyObjects &objects);
bool build_dota_top_custom_games_list_payload(const std::vector<std::uint64_t> &mod_ids, std::string &message, std::size_t &game_count);
bool build_dota_practice_lobby_cache_subscribed_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2004, const std::string &object_2015, const std::string &object_2014, const std::string &object_2016, std::string &message);
bool build_dota_practice_lobby_details_update_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2014, const std::string &object_2015, const std::string &object_2004, const std::string &object_2016, bool include_server_lobby_placeholder, std::string &message);
bool build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(std::uint64_t lobby_id, const std::string &object_2014, const std::string &object_2016, const std::string &object_2015, const std::string &object_2004, std::string &message);
std::size_t build_dota_joinable_custom_game_modes_body(const std::vector<DotaJoinableCustomGameMode> &modes, std::string &body);
std::size_t build_dota_joinable_custom_lobbies_body(const std::vector<DotaJoinableCustomLobby> &lobbies, std::uint64_t requested_custom_game_id, std::string &body);
void build_dota_find_top_source_tv_games_body(const DotaSourceTVGame *game, std::string &body);
void build_dota_find_top_source_tv_games_empty_body(std::string &body);
void build_dota_spectate_friend_game_response_body(std::uint64_t server_steam_id, std::string &body);
void build_dota_watch_game_pending_response_body(std::string &body);
void build_dota_watch_game_ready_response_body(std::uint32_t source_tv_addr, std::uint32_t source_tv_port, std::uint64_t watch_server_steam_id, std::uint64_t secret_code, std::string &body);
void build_dota_claim_event_action_response_body(std::uint32_t action_id, std::string &body);
void build_dota_claim_event_action_using_item_response_body(std::uint32_t action_id, std::string &body);

bool build_dota_practice_lobby_response_payload(std::uint64_t request_job_id, bool has_request_job, std::string &message);
bool build_dota_practice_lobby_join_response_payload(bool has_request_job, std::uint64_t request_job_id, std::uint32_t result, std::string &message);
bool build_dota_invitation_created_payload(std::uint64_t group_id, std::uint64_t steam_id, bool user_offline, std::string &message);
bool build_dota_lobby_invite_cache_subscribed_payload(std::uint64_t lobby_id, std::uint64_t inviter_steam_id, std::uint64_t invitee_steam_id, const std::string &inviter_name, const std::vector<std::pair<std::uint64_t, std::string>> &members, std::string &message, std::uint64_t *invite_gid_out = nullptr, std::uint64_t *cache_version_out = nullptr);
bool build_dota_other_joined_channel_payload(std::uint64_t channel_id, const std::string &persona_name, std::uint64_t steam_id, std::string &message);
bool build_dota_other_left_channel_payload(std::uint64_t channel_id, std::uint64_t steam_id, std::string &message);
std::string build_dota_7034_leaver_state_payload(std::uint32_t lobby_state, std::uint32_t game_state);
bool build_dota_7034_connected_players_response_payload(const std::vector<Dota7034Player> &connected_players, const std::vector<Dota7034Player> &disconnected_players, const std::vector<Dota7034Player> &draft_players, std::uint32_t game_state, const Dota7034ExtraState &extra_state, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_join_chat_channel_response_payload(std::uint64_t channel_id, const std::string &channel_name, const std::vector<DotaChatMember> &members, std::uint32_t channel_type, std::string &message);
bool build_dota_post_game_join_chat_channel_response_payload(std::uint64_t steam_id, std::uint64_t channel_id, const std::string &channel_name, const std::string &player_name, std::string &message);
bool build_dota_chat_message_payload(const std::string &request_body, std::uint64_t channel_id, std::uint32_t account_id, const std::string &persona_name, std::string &message);
bool build_dota_so_owner_cache_unsubscribed_payload(std::uint32_t owner_type, std::uint64_t owner_id, std::string &message);
bool build_dota_lobby_cache_unsubscribed_payload(std::uint64_t lobby_id, std::string &message);
bool build_dota_lobby_cache_subscribed_up_to_date_payload(std::uint64_t lobby_id, std::string &message);
bool build_dota_lobby_cache_subscribed_up_to_date_payload(std::uint64_t lobby_id, bool has_version, std::uint64_t version, bool has_service_id, std::uint32_t service_id, const std::vector<std::uint32_t> &service_list, bool has_sync_version, std::uint64_t sync_version, std::string &message);
bool build_dota_practice_lobby_kicked_popup_payload(std::string &message);
bool build_dota_destroy_lobby_response_payload(std::uint64_t request_job_id, std::string &message);
bool build_dota_ready_up_status_payload(bool has_request_job, std::uint64_t request_job_id, std::uint64_t lobby_id, std::uint32_t state, std::uint32_t local_ready_state, std::string &message);
bool build_dota_7428_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_4524_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_7388_minimal_response_payload(std::uint32_t event_id, std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_2582_lookup_account_name_response_payload(std::uint32_t account_id, const std::string &account_name, bool has_request_job, std::uint64_t request_job_id, std::string &message);
void build_dota_unlock_item_style_response_body(std::uint64_t item_id, std::uint32_t style_index, std::string &body);
void build_dota_set_item_style_response_body(std::string &body);
bool build_dota_7504_response_payload(std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_8096_response_payload(std::uint32_t account_id, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_game_match_sign_out_response_payload(std::uint64_t match_id, std::uint32_t duration, std::uint32_t signout_time, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_8880_response_payload(bool request_valid, bool has_rank_type, bool supported_rank_type, bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_give_tip_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_rank_request_response_payload(bool has_request_job, std::uint64_t request_job_id, std::string &message);
bool build_dota_submit_player_report_response_v2_payload(const std::uint8_t *request_body, std::size_t request_body_size, bool has_request_job, std::uint64_t request_job_id, std::string &message);
void build_dota_store_purchase_init_response_body(std::uint64_t transaction_id, std::string &body);
void build_dota_crate_items_response_body(const std::vector<std::uint32_t> &item_defs, std::string &body);
void build_dota_use_item_response_body(bool items_granted, std::string &body);
void build_dota_unlock_crate_response_body(const std::vector<std::uint32_t> &granted_defs, std::string &body);
void build_dota_unpack_bundle_response_body(const std::vector<std::uint32_t> &granted_defs, std::string &body);
void build_dota_add_socket_response_body(std::uint32_t result, std::uint64_t subject_item_id, std::uint32_t socket_attr_def, std::string &body);
bool build_dota_7451_batch_player_resources_response_payload(const std::vector<std::uint32_t> &account_ids, bool has_request_job, std::uint64_t request_job_id, std::string &message);
std::uint64_t generate_dota_so_change_version(std::uint64_t lobby_id, std::uint64_t owner_id);
std::uint64_t generate_dota_lobby_invite_gid(std::uint64_t lobby_id, std::uint64_t invitee_steam_id, std::uint64_t &cache_version);
bool build_dota_remove_lobby_invite_payload(std::uint64_t lobby_id, std::uint64_t owner_steam_id, std::string &message);
std::string build_dota_practice_lobby_list_entry_body(std::uint64_t lobby_id, std::uint32_t account_id, const std::string &player_name, const std::string &room_name, std::uint32_t game_mode, std::uint32_t server_region, bool requires_pass_key, std::uint32_t player_count, std::uint32_t max_player_count, const std::string &lan_host_ping_location);
bool build_dota_lobby_list_response_payload(const std::vector<std::string> &entries, std::string &message);
bool build_dota_lobby_list_response_payload(const std::string &entry, bool include_entry, std::string &message);
bool build_dota_friend_practice_lobby_list_response_payload(const std::vector<std::string> &entries, std::string &message);
bool build_dota_friend_practice_lobby_list_response_payload(const std::string &entry, bool include_entry, std::string &message);
bool build_dota_custom_game_info_response_payload(std::uint64_t custom_game_id, bool has_request_job, std::uint64_t request_job_id, std::string &message);
std::string build_dota_custom_lobby_list_entry_body(std::uint64_t lobby_id, std::uint32_t account_id, const std::string &player_name, bool requires_pass_key, const std::string &lan_host_ping_location);
bool build_dota_custom_lobby_list_response_payload(std::uint64_t request_list_job_id, const std::vector<std::string> &entries, std::string &message);

} // namespace gbe::gc_message

#endif // GBE_GC_MESSAGE_UTILS_H

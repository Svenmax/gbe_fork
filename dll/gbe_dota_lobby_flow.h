#ifndef GBE_DOTA_LOBBY_FLOW_H
#define GBE_DOTA_LOBBY_FLOW_H

#include "gbe_dota_lobby_publish.h"
#include "gbe_dota_lobby_snapshot.h"
#include "gbe_dota_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gbe::dota_lobby_flow {

bool lobby_members_equal(
    const std::vector<GBE_DotaLobbyMemberState> &left,
    const std::vector<GBE_DotaLobbyMemberState> &right);

bool lobby_members_contain_steam_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id);

bool find_lobby_member_index(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::size_t &index);

std::uint64_t find_lobby_member_steam_id_by_account_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id);

bool clear_lobby_member_by_account_id(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id);

void upsert_lobby_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_DotaLobbyMemberState &member);

std::vector<GBE_DotaLobbyMemberState> find_joined_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    const std::vector<GBE_DotaLobbyMemberState> &current_members);

std::vector<GBE_DotaLobbyMemberState> filter_nonzero_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members);

void count_remote_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count);

bool should_hold_lan_launch_for_remote_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count);

bool should_use_client_peer_for_launch_state_push(
    bool source_is_server,
    bool client_peer_available);

bool is_valid_launch_state_push_target(
    bool target_available,
    bool target_is_server,
    bool target_is_dota_profile);

std::string resolve_chat_member_display_name(
    std::uint64_t member_steam_id,
    std::uint64_t local_steam_id,
    const std::string &local_name,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::string &generic_member_name,
    const std::string &friend_name,
    const std::string &fallback_name);

std::vector<GBE_DotaChatMemberState> compose_join_chat_channel_members(
    std::uint64_t local_steam_id,
    const std::string &local_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::vector<GBE_DotaChatMemberState> &resolved_remote_names);

bool should_use_current_practice_lobby_payload_for_details_update(
    std::uint64_t match_id,
    bool lan,
    std::size_t member_count,
    std::uint64_t custom_game_id);

bool should_use_current_practice_lobby_payload_for_cache_subscribed(
    bool launch_started,
    bool lan,
    std::size_t member_count);

GBE_DotaAuthoritativeLobbyPayloadData compose_authoritative_lobby_payload_data(
    bool preserve_server_id,
    bool launch_started,
    std::uint64_t current_server_id,
    std::uint64_t server_candidate_id,
    const std::string &formatted_connect);

void apply_lobby_member_team_slot_update(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t account_id,
    bool has_team,
    std::uint32_t team,
    bool has_slot,
    std::uint32_t slot,
    std::uint32_t default_team,
    bool connected);

bool set_lobby_member_connected(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t account_id,
    bool connected,
    bool has_custom_game,
    bool should_mark_leaver,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);

bool set_lobby_member_hero(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t hero_id);

void preserve_generic_snapshot_member_runtime(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members);

void update_generic_lobby_member_snapshot_state(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::vector<GBE_DotaLobbyMemberState> &members,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);

void merge_existing_lobby_members_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool generic_members_empty,
    std::uint64_t local_steam_id,
    std::uint64_t owner_steam_id,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members);

void upsert_owner_member_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    bool owner_in_generic_members,
    bool generic_members_empty,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected);

GBE_DotaLobbyMemberState adopt_lobby_owner_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t new_owner_steam_id,
    std::uint32_t default_owner_account_id,
    std::uint32_t default_owner_team,
    bool default_owner_connected,
    std::uint64_t &owner_steam_id,
    std::uint32_t &owner_account_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::uint32_t &owner_hero_id,
    bool &owner_connected);

void preserve_lobby_owner_transfer_slots(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    std::uint64_t previous_owner_steam_id,
    std::uint64_t new_owner_steam_id);

std::vector<GBE_DotaLobbyMemberState> compose_lobby_members(
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected,
    std::uint32_t player_pool_team,
    const std::vector<GBE_DotaLobbyMemberState> &members);

std::uint32_t select_arcade_lobby_member_slot(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t owner_slot);

bool normalize_arcade_lobby_member_slot(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);

bool normalize_arcade_lobby_member_slots(
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t good_guys_team);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_FLOW_H

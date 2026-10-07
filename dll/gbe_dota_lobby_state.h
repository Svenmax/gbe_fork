#ifndef GBE_DOTA_LOBBY_STATE_H
#define GBE_DOTA_LOBBY_STATE_H

#include "gbe_dota_types.h"
#include "dll/gbe_dota_reconnect_shared.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::proto_wire {
struct DotaPracticeLobbyCreateRequest;
}

// Shared Dota lobby state DTOs and pure transition plans used by GC handlers.

struct GBE_LocalLobby
{
    bool active{};
    std::uint64_t lobby_id{};
    std::uint64_t generic_lobby_id{};
    bool has_chat_channel{};
    std::uint64_t chat_channel_id{};
    std::string chat_channel_name;
    std::uint32_t chat_channel_type{};
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
    GBE_DotaCustomGameDetails custom_game;
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint64_t match_id{};
    std::uint64_t server_id{};
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    std::string connect;
    std::uint32_t game_start_time{};
    std::uint32_t owner_team{};
    std::uint32_t owner_slot{};
    std::uint32_t owner_hero_id{};
    bool owner_connected{};
    std::vector<GBE_DotaLobbyMemberState> members;
    std::uint32_t launch_phase{};
    bool launch_4511_seen{};
    bool launch_steam_auth_ack_queued{};
    std::uint32_t launch_steam_auth_ticket_crc{};
    std::uint32_t launch_steam_auth_message_sequence{};
    bool has_broadcast_channel{};
    std::uint32_t broadcast_channel_id{};
    std::string broadcast_country_code;
    std::string broadcast_description;
    std::string broadcast_language_code;
    std::string pass_key;
    std::uint64_t tv_secret_code{};
    std::uint32_t tv_port{};
    bool has_cache_version{};
    std::uint64_t cache_version{};
    bool has_cache_service_id{};
    std::uint32_t cache_service_id{};
    std::vector<std::uint32_t> cache_service_list;
    bool has_cache_sync_version{};
    std::uint64_t cache_sync_version{};
    bool abandon_postgame_active{};
    std::uint64_t abandon_pre_postgame_chat_channel_id{};
    bool pending_leave_after_7040{};
    std::uint64_t pending_leave_lobby_id{};
    bool seen_local_in_generic_lobby{};
    bool kicked_suppressed_logged{};
    bool waiting_join_confirmation_logged{};
    bool owner_adoption_suppressed_logged{};
    bool ignored_early_arcade_launch{};
    std::chrono::high_resolution_clock::time_point created{};
};

struct GBE_SharedDotaLobbyState {
    bool valid{};
    bool active{};
    std::uint64_t lobby_id{};
    std::uint64_t generic_lobby_id{};
    bool has_chat_channel{};
    std::uint64_t chat_channel_id{};
    std::string chat_channel_name;
    std::uint32_t chat_channel_type{};
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
    GBE_DotaCustomGameDetails custom_game;
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint64_t match_id{};
    std::uint64_t server_id{};
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    std::string connect;
    std::uint32_t game_start_time{};
    std::uint32_t owner_team{};
    std::uint32_t owner_slot{};
    std::uint32_t owner_hero_id{};
    bool owner_connected{};
    std::vector<GBE_DotaLobbyMemberState> members;
    std::uint32_t launch_phase{};
    bool launch_4511_seen{};
    bool has_broadcast_channel{};
    std::uint32_t broadcast_channel_id{};
    std::string broadcast_country_code;
    std::string broadcast_description;
    std::string broadcast_language_code;
    std::string pass_key;
    bool has_cache_version{};
    std::uint64_t cache_version{};
    bool has_cache_service_id{};
    std::uint32_t cache_service_id{};
    std::vector<std::uint32_t> cache_service_list;
    bool has_cache_sync_version{};
    std::uint64_t cache_sync_version{};
};

namespace gbe::dota_lobby_state {

struct CreateLobbyPlan {
    GBE_LocalLobby lobby;
    bool custom_game_create{};
};

struct CreateLobbyResetPlan {
    bool custom_game_create{};
    bool unsubscribe_previous_practice_lobby{};
    std::uint64_t previous_lobby_id{};
    std::uint64_t previous_match_id{};
    std::uint64_t previous_custom_game_id{};
    std::uint32_t previous_state{};
    std::uint32_t previous_game_state{};
    std::uint32_t previous_owner_team{};
    std::uint32_t previous_owner_slot{};
};

struct JoinLobbyMergePlan {
    GBE_LocalLobby lobby;
    GBE_DotaLobbyMemberState local_member;
    bool seen_local_in_generic_lobby{};
};

struct LaunchInitPlan {
    GBE_LocalLobby lobby;
};

struct CustomGameLaunchSetupPlan {
    GBE_LocalLobby readyup_lobby;
    GBE_LocalLobby serversetup_lobby;
    std::uint32_t synced_launch_phase{};
};

struct LaunchRunPlan {
    bool can_advance{};
    std::uint32_t launch_phase{};
    std::uint32_t next_state{};
    std::uint32_t next_game_state{};
};

struct QueuedLobbyStateApplyPlan {
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint32_t launch_phase{};
    bool preserved_game_state{};
};

enum LaunchDetailsLobbySource : std::uint32_t {
    LaunchDetailsLobbySourceCurrent = 0u,
    LaunchDetailsLobbySourceReadyUp = 1u,
    LaunchDetailsLobbySourceServerSetup = 2u,
};

struct LaunchDetailsEvent {
    bool send{};
    std::uint32_t emsg{};
    std::string reason;
    bool apply_lobby_state{};
    std::uint32_t lobby_state{};
    std::uint32_t lobby_game_state{};
    std::uint32_t lobby_source{};
};

struct LaunchPresenceEvent {
    bool update{};
    std::string status;
    std::string lobby_state;
    bool include_party{};
    bool include_lobby{};
    std::string persona_reason;
};

struct LaunchSteamAuthAckEvent {
    bool queue{};
    std::string reason;
};

struct PracticeLobbyLaunchEventPlan {
    LaunchDetailsEvent initial_details;
    LaunchSteamAuthAckEvent steam_auth_ack;
    LaunchPresenceEvent presence;
};

struct CustomGameLaunchSetupEventPlan {
    std::vector<LaunchDetailsEvent> details_events;
    std::string mark_phase_reason;
    LaunchSteamAuthAckEvent steam_auth_ack;
};

void publish_local_lobby_to_shared(const GBE_LocalLobby &local, bool is_server, GBE_SharedDotaLobbyState &shared);
void adopt_shared_lobby_to_local(
    const GBE_SharedDotaLobbyState &shared,
    bool clear_server_id_without_match,
    bool normalize_custom_readyup_run_state,
    GBE_LocalLobby &local);
bool build_reconnect_context(const GBE_LocalLobby &local, GBE_DotaReconnectContext &context);
CreateLobbyPlan compose_create_lobby_plan(
    const proto_wire::DotaPracticeLobbyCreateRequest &request,
    std::uint64_t lobby_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    const std::string &owner_name,
    std::uint32_t owner_team,
    std::uint32_t owner_slot);
CreateLobbyResetPlan compose_create_lobby_reset_plan(
    const GBE_LocalLobby &previous_lobby,
    const GBE_DotaCustomGameDetails &requested_custom_game);
JoinLobbyMergePlan compose_join_lobby_merge_plan(
    const GBE_LocalLobby &current_lobby,
    bool has_request_lobby_id,
    std::uint64_t request_lobby_id,
    bool matched_generic_lobby,
    const GBE_LocalLobby &matched_lobby,
    std::uint64_t local_steam_id,
    std::uint32_t local_account_id,
    const std::string &local_name,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);
LaunchInitPlan compose_launch_init_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint64_t match_id,
    std::uint64_t server_id,
    const std::string &connect,
    std::uint32_t game_start_time,
    std::uint32_t requested_launch_phase);
CustomGameLaunchSetupPlan compose_custom_game_launch_setup_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t synced_launch_phase);
bool has_launch_server_setup_sync(const GBE_LocalLobby &current_lobby);
LaunchRunPlan compose_launch_run_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    std::uint32_t next_game_state);
QueuedLobbyStateApplyPlan compose_queued_lobby_state_apply_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t queued_state,
    std::uint32_t queued_game_state,
    bool preserve_monotonic_game_state,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase);
LaunchPresenceEvent compose_launch_serversetup_presence_event(const std::string &persona_reason);
PracticeLobbyLaunchEventPlan compose_practice_lobby_launch_event_plan(std::uint32_t details_update_emsg);
CustomGameLaunchSetupEventPlan compose_custom_game_launch_setup_event_plan(std::uint32_t details_update_emsg);

} // namespace gbe::dota_lobby_state

#endif // GBE_DOTA_LOBBY_STATE_H

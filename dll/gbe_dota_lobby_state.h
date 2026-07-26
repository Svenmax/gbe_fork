#ifndef GBE_DOTA_LOBBY_STATE_H
#define GBE_DOTA_LOBBY_STATE_H

#include "gbe_dota_types.h"
#include "gbe_dota_diagnostic_event.h"
#include "dll/gbe_dota_reconnect_shared.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::proto_wire {
struct DotaPracticeLobbyCreateRequest;
struct DotaPracticeLobbyDetailsRequest;
}

// Shared Dota lobby state DTOs and pure transition plans used by GC handlers.

struct GBE_LocalLobby
{
    bool active{};
    std::uint64_t generation{};
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
    bool postgame_chat_tombstone_active{};
    std::uint64_t postgame_chat_tombstone_channel_id{};
    std::uint64_t postgame_chat_tombstone_generation{};
    bool pending_leave_after_7040{};
    std::uint64_t pending_leave_lobby_id{};
    bool seen_local_in_generic_lobby{};
    bool kicked_suppressed_logged{};
    bool waiting_join_confirmation_logged{};
    bool owner_adoption_suppressed_logged{};
    bool ignored_early_arcade_launch{};
    std::uint64_t generic_launch_runtime_generation{};
    std::uint64_t generic_runtime_identity_generation{};
    std::chrono::high_resolution_clock::time_point created{};
};

struct GBE_SharedDotaLobbyState {
    bool valid{};
    bool active{};
    std::uint64_t generation{};
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

struct CreateLobbyStateApplyPlan {
    GBE_LocalLobby lobby;
    bool normalize_custom_game_details{};
    bool normalize_arcade_member_slots{};
    bool clear_reconnect_context{};
    bool set_reconnect_eligible{};
    bool log_arcade_isolation{};
};

struct CreateLobbyResetPlan {
    bool reset_gc_memory{};
    std::string reset_reason;
    bool reset_leave_generic_lobby{};
    bool reset_clear_queued_messages{};
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

struct GenericLobbyStateCapturePlan {
    bool apply_state{};
    std::uint32_t state{};
    bool apply_game_state{};
    std::uint32_t game_state{};
    bool ignored_stale_state{};
};

struct GenericLobbyRuntimeIdentityCapturePlan {
    bool apply_room_name{};
    std::string room_name;
    bool apply_match_id{};
    std::uint64_t match_id{};
    bool preserve_existing_lan_runtime{};
    bool apply_server_id{};
    std::uint64_t server_id{};
    bool apply_connect{};
    std::string connect;
    bool apply_game_start_time{};
    std::uint32_t game_start_time{};
};

struct GenericLobbyOptionsCapturePlan {
    bool apply_allow_cheats{};
    bool allow_cheats{};
    bool apply_fill_with_bots{};
    bool fill_with_bots{};
    bool apply_allow_spectating{};
    bool allow_spectating{};
    bool apply_visibility{};
    std::uint32_t visibility{};
    bool apply_bot_difficulty_radiant{};
    std::uint32_t bot_difficulty_radiant{};
    bool apply_bot_difficulty_dire{};
    std::uint32_t bot_difficulty_dire{};
    bool apply_bot_radiant{};
    std::uint64_t bot_radiant{};
    bool apply_bot_dire{};
    std::uint64_t bot_dire{};
};

struct GenericLobbyCustomGameCapturePlan {
    bool apply_mode{};
    std::string mode;
    bool apply_map_name{};
    std::string map_name;
    bool apply_difficulty{};
    std::uint32_t difficulty{};
    bool apply_game_id{};
    std::uint64_t game_id{};
    bool apply_min_players{};
    std::uint32_t min_players{};
    bool apply_max_players{};
    std::uint32_t max_players{};
    bool apply_crc{};
    std::uint64_t crc{};
    bool apply_timestamp{};
    std::uint32_t timestamp{};
    bool apply_penalties{};
    bool penalties{};
};

struct GenericLobbyCaptureInput {
    bool has_state{};
    std::uint32_t state{};
    bool has_game_state{};
    std::uint32_t game_state{};
    std::string room_name;
    std::string match_id_raw;
    std::string server_id_raw;
    std::string connect;
    std::string game_start_time_raw;
    std::string allow_cheats_raw;
    std::string fill_with_bots_raw;
    std::string allow_spectating_raw;
    std::string visibility_raw;
    std::string bot_difficulty_radiant_raw;
    std::string bot_difficulty_dire_raw;
    std::string bot_radiant_raw;
    std::string bot_dire_raw;
    std::string custom_game_mode;
    std::string custom_map_name;
    std::string custom_difficulty_raw;
    std::string custom_game_id_raw;
    std::string custom_min_players_raw;
    std::string custom_max_players_raw;
    std::string custom_game_crc_raw;
    std::string custom_game_timestamp_raw;
    std::string custom_game_penalties_raw;
};

struct GenericLobbyCapturePlan {
    GenericLobbyStateCapturePlan state;
    GenericLobbyRuntimeIdentityCapturePlan runtime_identity;
    GenericLobbyOptionsCapturePlan options;
    GenericLobbyCustomGameCapturePlan custom_game;
};

enum class SharedLobbyRestoreSource {
    SharedSnapshot,
    LocalGenericCapture,
    ReadyupRegression,
};

struct SourceAwareSharedRuntimeRestorePlan {
    bool apply_state{};
    std::uint32_t state{};
    SharedLobbyRestoreSource state_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_game_state{};
    std::uint32_t game_state{};
    SharedLobbyRestoreSource game_state_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_launch_phase{};
    std::uint32_t launch_phase{};
    SharedLobbyRestoreSource launch_phase_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_room_name{};
    std::string room_name;
    SharedLobbyRestoreSource room_name_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_connect{};
    std::string connect;
    SharedLobbyRestoreSource connect_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_match_id{};
    std::uint64_t match_id{};
    SharedLobbyRestoreSource match_id_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_server_id{};
    std::uint64_t server_id{};
    SharedLobbyRestoreSource server_id_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool apply_game_start_time{};
    std::uint32_t game_start_time{};
    SharedLobbyRestoreSource game_start_time_source{SharedLobbyRestoreSource::SharedSnapshot};
    bool ignored_readyup_regression{};
};

struct SharedLobbyOptionsRestorePlan {
    bool apply_game_mode{};
    std::uint32_t game_mode{};
    bool apply_server_region{};
    std::uint32_t server_region{};
    bool apply_lan{};
    bool lan{};
    bool apply_lan_host_ping_location{};
    std::string lan_host_ping_location;
    bool apply_allow_cheats{};
    bool allow_cheats{};
    bool apply_fill_with_bots{};
    bool fill_with_bots{};
    bool apply_allow_spectating{};
    bool allow_spectating{};
    bool apply_pass_key{};
    std::string pass_key;
    bool apply_visibility{};
    std::uint32_t visibility{};
    bool apply_bot_difficulty_radiant{};
    std::uint32_t bot_difficulty_radiant{};
    bool apply_bot_difficulty_dire{};
    std::uint32_t bot_difficulty_dire{};
    bool apply_bot_radiant{};
    std::uint64_t bot_radiant{};
    bool apply_bot_dire{};
    std::uint64_t bot_dire{};
};

struct SharedLobbyCacheRestorePlan {
    bool apply_cache_version{};
    bool has_cache_version{};
    std::uint64_t cache_version{};
    bool apply_cache_service_id{};
    bool has_cache_service_id{};
    std::uint32_t cache_service_id{};
    bool apply_cache_service_list{};
    std::vector<std::uint32_t> cache_service_list;
    bool apply_cache_sync_version{};
    bool has_cache_sync_version{};
    std::uint64_t cache_sync_version{};
};

struct SteamAuthAckLaunchPlan {
    std::uint32_t ticket_crc{};
    std::uint32_t message_sequence{};
    bool mark_ack_queued{};
};

struct PostGameLobbyStateApplyPlan {
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint64_t chat_channel_id{};
    std::string chat_channel_name;
    std::uint64_t abandon_pre_postgame_chat_channel_id{};
};

struct LaunchLifecycleTransitionDecision {
    bool apply_lobby_state{};
    std::uint32_t next_state{};
    std::uint32_t next_game_state{};
    bool mark_launch_phase{};
    std::uint32_t launch_phase{};
    bool publish_shared_state{};
    bool send_details_update{};
    bool queue_runtime_lobby_update{};
    double runtime_update_delay{};
    std::string reason;
};

struct ReconnectEligibilityDecision {
    bool source_valid{};
    bool active{};
    bool started{};
    bool has_server_id{};
    bool has_connect{};
    bool custom_game{};
    bool owner_connected{};
    bool launch_run_or_later{};
    bool launch_loaded{};
    bool context_eligible{};
};

struct ReconnectInterceptionDecision {
    bool has_context{};
    bool remote_matches_server{};
    bool state_ready{};
    bool has_connect{};
    bool arcade_context{};
    bool local_is_owner{};
    bool reconnect_eligible{};
    bool p2p_rendezvous_candidate{};
    bool can_post_connection_state{};
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

// owner_hero_id authority helpers (single preserve / apply rules for host path).
bool should_preserve_known_owner_hero_on_publish(const GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared);
bool should_preserve_known_owner_hero_on_adopt(const GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared);
bool should_peer_restore_owner_hero_from_client(
    bool server_role,
    const GBE_LocalLobby &server_local,
    const GBE_LocalLobby &client_local);
// Apply non-zero hero to local owner. Returns true when owner_hero_id changed.
bool apply_owner_hero_id(GBE_LocalLobby &local, std::uint32_t hero_id);
// Adopt shared hero when known (non-zero) and different; never clears a known local with 0.
bool apply_owner_hero_from_shared(GBE_LocalLobby &local, const GBE_SharedDotaLobbyState &shared);

// Host one-shot keys: generation + identity only (no generation-less bypass).
bool host_showcase_equip_key_matches(
    std::uint64_t key_generation,
    std::uint64_t key_lobby_id,
    std::uint64_t key_owner_steam_id,
    std::uint32_t key_owner_hero_id,
    std::uint64_t current_generation,
    std::uint64_t lobby_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_hero_id);
bool host_wearable_refresh_key_matches(
    std::uint64_t key_generation,
    std::uint64_t key_steam_id,
    std::uint32_t key_hero_id,
    std::uint64_t current_generation,
    std::uint64_t steam_id,
    std::uint32_t hero_id);

void publish_local_lobby_to_shared(const GBE_LocalLobby &local, bool is_server, GBE_SharedDotaLobbyState &shared);
void adopt_shared_lobby_to_local(
    const GBE_SharedDotaLobbyState &shared,
    bool clear_server_id_without_match,
    bool normalize_custom_readyup_run_state,
    GBE_LocalLobby &local);
bool is_active_lobby_owned_by_local_user(
    const GBE_LocalLobby &lobby,
    std::uint64_t lobby_id,
    std::uint64_t local_steam_id);
ReconnectEligibilityDecision compute_reconnect_eligibility_decision(
    bool source_valid,
    bool active,
    std::uint32_t lobby_state,
    std::uint32_t game_state,
    std::uint64_t server_id,
    bool has_connect,
    std::uint64_t custom_game_id,
    bool owner_connected,
    std::uint32_t launch_phase);
ReconnectInterceptionDecision compute_reconnect_interception_decision(
    const GBE_DotaReconnectContext &context,
    bool has_context,
    std::uint64_t local_steam_id,
    std::uint64_t remote_steam_id,
    bool reconnect_eligible);

// Pure abandon-current-game decision (emsg 7035). Reads a captured request
// context; returns all derived decision flags needed by the abandon handler.
// Kept pure so the handler only executes the decision's side effects in the
// documented order without interleaving boolean derivation.
struct DotaAbandonRequestContext {
    bool wrapped{};
    bool has_wrapped_session{};
    bool is_server{};
    bool owner_connected{};
    bool has_custom_game_details{};
    std::uint64_t lobby_id{};
    std::uint32_t lobby_state{};
    std::uint32_t game_state{};
    std::uint64_t server_id{};
    std::uint32_t launch_phase{};
    std::uint64_t pre_postgame_chat_channel_id{};
};

struct AbandonDecision {
    std::uint64_t lobby_id{};
    std::uint32_t lobby_state{};
    std::uint32_t lobby_game_state{};
    std::uint64_t pre_postgame_chat_channel_id{};
    std::uint32_t abandon_game_state_threshold{};  // 1 for wrapped/client, 2 for direct server
    bool treat_as_current_game_disconnect{};
    bool ready_for_abandon_teardown{};
    bool arcade_launch_failed_before_connect{};
    bool queue_cache_unsubscribed{};
    bool set_pending_reset_after_cache_unsubscribed{};
    bool discard_queued_launch_messages{};
    bool suppress_abandoned_lobby{};
    bool queue_postgame_teardown{};
    bool require_wrapped_session{};
    bool suppress_previous_chat_channel{};
    bool push_postgame_cache_unsubscribed{};
    bool push_postgame_join{};
};

bool build_dota_abandon_request_context(
    const GBE_LocalLobby &lobby,
    bool wrapped,
    bool has_wrapped_session,
    bool is_server,
    DotaAbandonRequestContext &context);

AbandonDecision compute_abandon_decision(const DotaAbandonRequestContext &context);

AbandonDecision compute_abandon_decision(
    const GBE_LocalLobby &lobby,
    bool wrapped,
    bool is_server);

struct TeardownRetrievalDecision {
    bool finalize_abandon_after_7014{};
    bool finalize_normal_signout_after_25{};
    bool reset_after_cache_unsubscribed{};
};

struct PostgameObservationDecision {
    bool skip_for_host_client{};
    bool skip_for_arcade_active_match{};
    bool run_player_cleanup{};
};

struct RuntimeResetDecision {
    bool preserve_reconnect_context{};
};

TeardownRetrievalDecision compute_teardown_retrieval_decision(
    bool is_dota_profile,
    bool pending_abandon_after_7014,
    bool pending_normal_signout_after_25,
    bool pending_reset_after_cache_unsubscribed,
    std::uint32_t retrieved_emsg,
    bool retrieved_other_left_matches_abandon_channel);
PostgameObservationDecision compute_postgame_observation_decision(
    bool is_server,
    bool host_has_active_server_gc,
    bool arcade_active_match,
    std::uint32_t previous_state,
    std::uint32_t current_state,
    std::uint64_t lobby_id);
RuntimeResetDecision compute_runtime_reset_decision(dota_diagnostic::Reason reason);

CreateLobbyPlan compose_create_lobby_plan(
    const proto_wire::DotaPracticeLobbyCreateRequest &request,
    std::uint64_t lobby_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    const std::string &owner_name,
    std::uint32_t owner_team,
    std::uint32_t owner_slot);
CreateLobbyStateApplyPlan compose_create_lobby_state_apply_plan(
    const CreateLobbyPlan &create_plan,
    bool has_lobby_details);
CreateLobbyResetPlan compose_create_lobby_reset_plan(
    const GBE_LocalLobby &previous_lobby,
    const GBE_DotaCustomGameDetails &requested_custom_game);
JoinLobbyMergePlan compose_join_lobby_merge_plan(
    const GBE_LocalLobby &current_lobby,
    bool has_request_lobby_id,
    std::uint64_t request_lobby_id,
    bool has_request_pass_key,
    const std::string &request_pass_key,
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
bool advance_launch_phase(GBE_LocalLobby &lobby, std::uint32_t phase);
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
void apply_queued_lobby_state_apply_plan(
    GBE_LocalLobby &lobby,
    const QueuedLobbyStateApplyPlan &plan);
GenericLobbyStateCapturePlan compose_generic_lobby_state_capture_plan(
    const GBE_LocalLobby &current_lobby,
    bool has_generic_state,
    std::uint32_t generic_state,
    bool has_generic_game_state,
    std::uint32_t generic_game_state,
    std::uint32_t setup_synced_launch_phase);
void apply_generic_lobby_state_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyStateCapturePlan &plan);
GenericLobbyRuntimeIdentityCapturePlan compose_generic_lobby_runtime_identity_capture_plan(
    const GBE_LocalLobby &current_lobby,
    const std::string &generic_room_name,
    const std::string &generic_match_id_raw,
    const std::string &generic_server_id_raw,
    const std::string &generic_connect,
    const std::string &generic_game_start_time_raw);
void apply_generic_lobby_runtime_identity_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyRuntimeIdentityCapturePlan &plan);
GenericLobbyOptionsCapturePlan compose_generic_lobby_options_capture_plan(
    const std::string &generic_allow_cheats_raw,
    const std::string &generic_fill_with_bots_raw,
    const std::string &generic_allow_spectating_raw,
    const std::string &generic_visibility_raw,
    const std::string &generic_bot_difficulty_radiant_raw,
    const std::string &generic_bot_difficulty_dire_raw,
    const std::string &generic_bot_radiant_raw,
    const std::string &generic_bot_dire_raw);
void apply_generic_lobby_options_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyOptionsCapturePlan &plan);
GenericLobbyCustomGameCapturePlan compose_generic_lobby_custom_game_capture_plan(
    const std::string &generic_custom_game_mode,
    const std::string &generic_custom_map_name,
    const std::string &generic_custom_difficulty_raw,
    const std::string &generic_custom_game_id_raw,
    const std::string &generic_custom_min_players_raw,
    const std::string &generic_custom_max_players_raw,
    const std::string &generic_custom_game_crc_raw,
    const std::string &generic_custom_game_timestamp_raw,
    const std::string &generic_custom_game_penalties_raw);
void apply_generic_lobby_custom_game_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyCustomGameCapturePlan &plan);
GenericLobbyCapturePlan compose_generic_lobby_capture_plan(
    const GBE_LocalLobby &current_lobby,
    const GenericLobbyCaptureInput &input,
    std::uint32_t setup_synced_launch_phase);
void apply_generic_lobby_capture_plan(
    GBE_LocalLobby &lobby,
    const GenericLobbyCapturePlan &plan);
SourceAwareSharedRuntimeRestorePlan compose_source_aware_shared_runtime_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby,
    std::uint32_t run_queued_launch_phase);
bool apply_source_aware_shared_runtime_restore_plan(
    GBE_LocalLobby &lobby,
    const SourceAwareSharedRuntimeRestorePlan &plan);
SharedLobbyOptionsRestorePlan compose_shared_lobby_options_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby);
bool apply_shared_lobby_options_restore_plan(
    GBE_LocalLobby &lobby,
    const SharedLobbyOptionsRestorePlan &plan);
SharedLobbyCacheRestorePlan compose_shared_lobby_cache_restore_plan(
    const GBE_LocalLobby &current_lobby,
    const GBE_SharedDotaLobbyState &shared_lobby);
bool apply_shared_lobby_cache_restore_plan(
    GBE_LocalLobby &lobby,
    const SharedLobbyCacheRestorePlan &plan);
bool apply_cache_subscription_metadata(
    GBE_LocalLobby &lobby,
    bool has_cache_version,
    std::uint64_t cache_version,
    bool has_cache_service_id,
    std::uint32_t cache_service_id,
    const std::vector<std::uint32_t> &cache_service_list,
    bool has_cache_sync_version,
    std::uint64_t cache_sync_version);
SteamAuthAckLaunchPlan compose_steam_auth_ack_launch_plan(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t derived_ticket_crc);
void apply_steam_auth_ack_launch_plan(
    GBE_LocalLobby &lobby,
    const SteamAuthAckLaunchPlan &plan);
bool mark_launch_4511_seen(GBE_LocalLobby &lobby);
bool restore_launch_4511_seen(
    GBE_LocalLobby &lobby,
    bool launch_4511_seen);
bool restore_lobby_owner_connected(
    GBE_LocalLobby &lobby,
    bool shared_owner_connected);
bool apply_lobby_owner_connected(
    GBE_LocalLobby &lobby,
    bool owner_connected);
bool restore_lobby_owner_team(
    GBE_LocalLobby &lobby,
    std::uint32_t shared_owner_team);
bool apply_lobby_owner_team(
    GBE_LocalLobby &lobby,
    std::uint32_t owner_team);
bool restore_lobby_owner_slot(
    GBE_LocalLobby &lobby,
    std::uint32_t shared_owner_slot);
bool restore_lobby_members(
    GBE_LocalLobby &lobby,
    const std::vector<GBE_DotaLobbyMemberState> &shared_members);
bool apply_lobby_owner_slot(
    GBE_LocalLobby &lobby,
    std::uint32_t owner_slot);
bool apply_lobby_bot_difficulty_for_team(
    GBE_LocalLobby &lobby,
    std::uint32_t team,
    std::uint32_t bot_difficulty);
bool restore_lobby_custom_game(
    GBE_LocalLobby &lobby,
    const GBE_DotaCustomGameDetails &shared_custom_game);
bool apply_custom_game_loading_metadata(
    GBE_LocalLobby &lobby,
    std::uint64_t custom_game_id,
    std::uint32_t game_start_time);
bool restore_lobby_generation(
    GBE_LocalLobby &lobby,
    std::uint64_t shared_generation);
bool apply_lobby_generation(
    GBE_LocalLobby &lobby,
    std::uint64_t generation);
bool restore_lobby_generic_lobby_id(
    GBE_LocalLobby &lobby,
    std::uint64_t shared_generic_lobby_id);
bool apply_lobby_generic_lobby_id(
    GBE_LocalLobby &lobby,
    std::uint64_t generic_lobby_id);
bool apply_source_tv_metadata(
    GBE_LocalLobby &lobby,
    std::uint64_t tv_secret_code,
    std::uint32_t tv_port);
bool apply_runtime_metadata(
    GBE_LocalLobby &lobby,
    const std::string &connect,
    std::uint64_t server_id);
bool apply_lobby_server_id(
    GBE_LocalLobby &lobby,
    std::uint64_t server_id);
void apply_lobby_details_update(
    GBE_LocalLobby &lobby,
    const proto_wire::DotaPracticeLobbyDetailsRequest &details);
bool apply_runtime_connect(
    GBE_LocalLobby &lobby,
    const std::string &connect);
bool apply_chat_channel(
    GBE_LocalLobby &lobby,
    std::uint64_t channel_id,
    const std::string &channel_name,
    std::uint32_t channel_type);
bool clear_chat_channel(GBE_LocalLobby &lobby);
bool apply_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id,
    const std::string &country_code,
    const std::string &description,
    const std::string &language_code);
bool patch_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id,
    bool has_country_code,
    const std::string &country_code,
    bool has_description,
    const std::string &description,
    bool has_language_code,
    const std::string &language_code);
bool clear_broadcast_channel(
    GBE_LocalLobby &lobby,
    std::uint32_t channel_id);
void apply_lifecycle_lobby_state(
    GBE_LocalLobby &lobby,
    std::uint32_t state,
    std::uint32_t game_state);
void apply_postgame_lobby_state_plan(
    GBE_LocalLobby &lobby,
    const PostGameLobbyStateApplyPlan &plan);
bool postgame_chat_tombstone_matches(
    const GBE_LocalLobby &lobby,
    std::uint64_t channel_id);
void clear_postgame_chat_tombstone(GBE_LocalLobby &lobby);
LaunchLifecycleTransitionDecision compute_custom_game_ready_up_transition(
    const GBE_LocalLobby &current_lobby,
    std::uint32_t ready_state,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason);
LaunchLifecycleTransitionDecision compute_custom_game_started_loading_transition(
    const GBE_LocalLobby &current_lobby,
    bool matching_lobby,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason);
LaunchLifecycleTransitionDecision compute_custom_game_finished_loading_transition(
    const GBE_LocalLobby &current_lobby,
    bool matching_lobby,
    bool load_failed,
    std::uint32_t run_queued_launch_phase,
    std::uint32_t loaded_launch_phase,
    const std::string &reason);
LaunchLifecycleTransitionDecision compute_runtime_game_state_transition(
    const GBE_LocalLobby &current_lobby,
    bool custom_game_launch,
    bool has_request_game_state,
    std::uint32_t request_game_state,
    std::uint32_t run_queued_launch_phase,
    const std::string &reason);
LaunchLifecycleTransitionDecision compute_launch_poll_transition(
    const GBE_LocalLobby &current_lobby,
    const std::string &reason);
LaunchPresenceEvent compose_launch_serversetup_presence_event(const std::string &persona_reason);
PracticeLobbyLaunchEventPlan compose_practice_lobby_launch_event_plan(std::uint32_t details_update_emsg);
CustomGameLaunchSetupEventPlan compose_custom_game_launch_setup_event_plan(std::uint32_t details_update_emsg);

} // namespace gbe::dota_lobby_state

#endif // GBE_DOTA_LOBBY_STATE_H

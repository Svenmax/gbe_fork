/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef __INCLUDED_STEAM_GAME_COORDINATOR_H__
#define __INCLUDED_STEAM_GAME_COORDINATOR_H__

#include "base.h"
#include "econ_item.h"
#include "gbe_dota_types.h"
#include "gbe_dota_reconnect_shared.h"

namespace gbe::proto_wire {
struct Dota7034DisconnectedPlayer;
struct Dota7034RequestShape;
}

class Steam_User_Items;
class Steam_GameServer_Items;
struct GCMsgHdr_t;
struct GCMsgHdrEx_t;
struct ProtoBufMsgHeader_t;
class CMsgProtoBufHeader;

class Steam_Game_Coordinator :
public ISteamGameCoordinator
{
    static constexpr const auto items_user_file = "items.json";
    static constexpr const auto gc_config_file = "gc.json";
    constexpr const static uint32 protobuf_mask = 0x80000000;

    class Settings *settings{};
    class Networking *network{};
    class Local_Storage *local_storage{};
    class SteamCallBacks *callbacks{};
    class RunEveryRunCB *run_every_runcb{};
    bool is_server{};

    struct GC_Message
    {
        uint32 msg_type{};
        std::string msg_body;
        std::chrono::high_resolution_clock::time_point created{};
        double post_in{};
        uint64 sequence{};
        bool apply_lobby_state{};
        uint32 lobby_state{};
        uint32 lobby_game_state{};
    };

    std::vector<GC_Message> pending_messages;
    std::queue<GC_Message> incoming_messages;
    uint64 pending_message_sequence{};

    enum GC_Profile
    {
        GC_PROFILE_INVALID = 0,
        GC_PROFILE_TF2,
        GC_PROFILE_DOTA2,
        //GC_PROFILE_PORTAL2,
    };

    int gc_version{};
    GC_Profile gc_profile{};
    bool is_portal2{};
    bool gc_initialized{};
    bool delay_init{};
    bool welcome_received{};
    std::chrono::high_resolution_clock::time_point welcome_time{};
    bool GBE_dota_login_sync_sent{};
    bool GBE_dota_host_showcase_equip_pushed{};
    bool GBE_dota_private_lobby_snapshot_replayed{};
    uint32 GBE_last_dota_launch_state_pushed_game_state{};
    bool GBE_pending_reset_after_cache_unsubscribed{};
    uint64 GBE_pending_reset_after_cache_unsubscribed_lobby_id{};
    uint64 GBE_suppressed_dota_abandon_lobby_id{};
    bool GBE_pending_dota_abandon_finalize_after_7014{};
    uint64 GBE_pending_dota_abandon_finalize_lobby_id{};
    std::string GBE_last_dota_launch_persona_signature;
    std::string GBE_last_dota_direct_connect_callback_signature;
    std::chrono::high_resolution_clock::time_point GBE_last_lobby_poll_time{};

    struct GBE_LocalLobby
    {
        bool active{};
        uint64 lobby_id{};
        uint64 generic_lobby_id{};
        bool has_chat_channel{};
        uint64 chat_channel_id{};
        std::string chat_channel_name;
        uint32 chat_channel_type{};
        std::string room_name;
        uint32 game_mode{};
        uint32 server_region{};
        bool lan{};
        std::string lan_host_ping_location;
        bool allow_cheats{};
        bool fill_with_bots{};
        bool allow_spectating{};
        uint32 visibility{};
        uint32 bot_difficulty_radiant{};
        uint32 bot_difficulty_dire{};
        uint64 bot_radiant{};
        uint64 bot_dire{};
        GBE_DotaCustomGameDetails custom_game;
        uint32 state{};
        uint32 game_state{};
        uint64 match_id{};
        uint64 server_id{};
        uint64 owner_steam_id{};
        uint32 owner_account_id{};
        std::string owner_name;
        std::string connect;
        uint32 game_start_time{};
        uint32 owner_team{};
        uint32 owner_slot{};
        uint32 owner_hero_id{};
        bool owner_connected{};
        std::vector<GBE_DotaLobbyMemberState> members;
        uint32 launch_phase{};
        bool launch_4511_seen{};
        bool launch_steam_auth_ack_queued{};
        uint32 launch_steam_auth_ticket_crc{};
        uint32 launch_steam_auth_message_sequence{};
        bool has_broadcast_channel{};
        uint32 broadcast_channel_id{};
        std::string broadcast_country_code;
        std::string broadcast_description;
        std::string broadcast_language_code;
        std::string pass_key;
        uint64 tv_secret_code{};
        uint32 tv_port{};
        bool has_cache_version{};
        uint64 cache_version{};
        bool has_cache_service_id{};
        uint32 cache_service_id{};
        std::vector<uint32> cache_service_list;
        bool has_cache_sync_version{};
        uint64 cache_sync_version{};
        bool abandon_postgame_active{};
        uint64 abandon_pre_postgame_chat_channel_id{};
        bool pending_leave_after_7040{};
        uint64 pending_leave_lobby_id{};
        bool seen_local_in_generic_lobby{};
        bool kicked_suppressed_logged{};
        bool waiting_join_confirmation_logged{};
        bool owner_adoption_suppressed_logged{};
        bool ignored_early_arcade_launch{};
        std::chrono::high_resolution_clock::time_point created{};
    };

    GBE_LocalLobby GBE_local_lobby{};

    std::vector<Econ_Item> items;
    bool items_loaded{};

    struct RequestInventory
    {
        std::chrono::high_resolution_clock::time_point created{};
        CSteamID steam_id{};
        SteamAPICall_t steam_api_call{};
        bool is_gc{};
    };

    std::map<CSteamID, std::vector<Econ_Item>> all_user_items;
    std::vector<RequestInventory> pending_items_requests;

    bool gc_enabled();
    Steam_User_Items *client_items();
    Steam_GameServer_Items *server_items();
    void parse_gc_config();
    bool is_welcome_message(const GC_Message &message);
    void GBE_ApplyQueuedLobbyState(const GC_Message &message);
    bool GBE_ShouldSuppressDotaAbandonedLobby(uint64 lobby_id) const;
    void GBE_MarkDotaAbandonedLobbySuppressed(uint64 lobby_id, const char *reason);
    void GBE_ClearDotaAbandonedLobbySuppression(uint64 lobby_id, const char *reason);
    bool GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(uint32 masked_emsg) const;
    bool GBE_SetDotaLobbyMemberConnected(uint64 steam_id, bool connected);
    bool GBE_SetDotaLobbyMemberRuntimeState(uint64 steam_id, bool connected, uint32 hero_id, bool has_hero_id);
    bool GBE_ShouldHoldDotaLanLaunchForRemoteMembers(uint32 next_game_state, uint32 *remote_count_out, uint32 *connected_remote_count_out) const;
    void GBE_DiscardQueuedDotaLaunchMessagesForAbandon(const char *reason);
    void push_incoming(uint32 msg_type, const std::string &message, double delay = 0.1, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0);
    void push_incoming_now(uint32 msg_type, const std::string &message, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0);

    std::string build_msg_header(JobID_t target_job = k_GIDNil, JobID_t source_job = k_GIDNil);
    GCMsgHdrEx_t parse_msg_header(const char *&p);
    std::string build_protomsg_header(uint32 msg_type, JobID_t target_job = k_GIDNil, JobID_t source_job = k_GIDNil);
    template <class T>
    std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, T, bool> parse_protomsg(const void *input, uint32 input_size);
    bool GBE_PatchDotaLoginCacheSubscribedInventory(std::string &message);
    uint64 item_id_local_to_network(uint64 item_id);
    uint64 item_id_network_to_local(uint64 item_id);
    std::string item_to_gcstruct(const Econ_Item &item, CSteamID steam_id);
    std::string item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id);

    void handle_set_item_pos(const void *input, uint32 input_size);
    void handle_delete_item(const void *input, uint32 input_size);
    void handle_motd_request(const void *input, uint32 input_size);
    void handle_respawn(const void *input, uint32 input_size);
    void handle_set_item_style(const void *input, uint32 input_size);
    void handle_adjust_equip_state(const void *input, uint32 input_size);
    void handle_set_multiple_item_pos(const void *input, uint32 input_size);
    void GBE_PushDotaLoginSyncMessages();
    void GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    bool GBE_ShouldTrackDotaPracticeLobbyLateSteamChain() const;
    void GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *status, const char *lobby_state, bool include_party, bool include_lobby = true);
    void GBE_ClearDotaPracticeLobbyLaunchRichPresence();
    void GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(const char *status, const char *lobby_state, bool include_party, bool include_lobby, const char *reason);
    void GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(const char *reason);
    void GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(const char *reason);
    void GBE_FinalizeDotaAbandonAfterOtherLeftChannel(uint64 consumed_lobby_id, const char *reason);
    void GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(uint64 consumed_lobby_id, const char *reason);
    bool GBE_QueueDotaPostGameTeardown(const char *reason, bool wrapped, const std::string *outer_session_field_raw, bool suppress_previous_chat_channel, bool push_cache_unsubscribed, bool push_postgame_join);
    bool GBE_CaptureCurrentDotaLobbyState(const char *reason, GBE_LocalLobby &snapshot, bool restore_shared = true);
    bool GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(const char *reason, const std::vector<GBE_DotaLobbyMemberState> &previous_members, uint64 previous_owner_steam_id, GBE_LocalLobby &snapshot);
    void GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason);
    bool GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id = false);
    bool GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id = false);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    void GBE_PushDotaLaunchStateToClientPeer(const char *reason);
    bool GBE_TryQueueDotaPrelaunch021(const char *note, uint32 trigger_emsg, uint64 source_job);
    bool GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(const char *note, uint32 trigger_emsg, uint64 source_job, uint32 next_state, uint32 next_game_state, double delay = 0.0);
    void GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(uint64 owner_steam_id, uint64 source_job);
    void GBE_HandleDotaDirect7034DisconnectedPlayers(const std::vector<gbe::proto_wire::Dota7034DisconnectedPlayer> &disconnected_players, uint64 source_job);
    void GBE_HandleDotaDirect7034RuntimeUpdates(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RequestShape &request_shape, bool custom_game_launch, uint64 source_job, bool &queued_runtime_lobby_update);
    bool GBE_HandleDotaDirect7034Response(uint32 request_emsg, const gbe::proto_wire::Dota7034RequestShape &request_shape, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    void GBE_HandleDotaDirect7034WaitForPlayers(uint32 request_emsg, const uint8 *body, size_t body_size, bool custom_game_launch, bool request_advances_to_hero_selection, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTime(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RequestShape &request_shape, bool custom_game_launch, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTimeFallback(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RequestShape &request_shape, bool custom_game_launch, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTimePreserve(uint32 request_emsg, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034LaunchPoll(uint32 request_emsg, uint64 source_job, bool &queued_runtime_lobby_update);
    bool GBE_HandleDotaDirect7034Request(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HasDotaLaunchServerSetupSync() const;
    void GBE_MarkDotaLaunchPhase(uint32 phase, const char *reason);
    bool GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(const char *reason, uint64 request_job_id);
    bool GBE_TryAdvanceDotaLaunchToRun(const char *note, uint32 trigger_emsg, uint64 source_job, const char *reason, uint32 next_game_state = 0u);
    void GBE_RecordDotaLobbyCacheSubscriptionState(const std::string &message, const char *reason);
    void GBE_PublishSharedDotaLobbyState(const char *reason);
    bool GBE_MaybeNotifyDotaPracticeLobbyMembersChanged(const char *reason);
    bool GBE_MaybeHandleDotaPracticeLobbyKicked(const char *reason);
    bool GBE_AdoptDotaGenericLobbyOwnerIfNeeded(const char *reason);
    bool GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_LocalLobby &lobby);
    void GBE_PublishDotaPracticeLobbyLocalMemberData(const char *reason);
    void GBE_PublishDotaPracticeLobbyMetadata(const char *reason);
    bool GBE_FindDotaGenericLobbyByDotaLobbyId(uint64 dota_lobby_id, CSteamID &generic_lobby_id, GBE_LocalLobby *lobby_snapshot, const char *reason);
    std::vector<GBE_LocalLobby> GBE_GetDotaGenericLobbySnapshots(const char *reason);
    void GBE_RestoreSharedDotaLobbyState(const char *reason);
    void ResetGCMemory(const char *reason = nullptr, bool leave_generic_lobby = true, bool clear_queued_messages = true);
    void GBE_LeaveGenericLobby();
    void GBE_SyncSettingsLobbyFromGenericLobby(const char *reason);
    bool GBE_SyncGenericLobbyGameServer(const char *reason);
    bool GBE_TrySyncDotaLobbyServerIdFromGameServer(const char *reason);
    uint64 GBE_GetDotaLobbyOwnerSteamId() const;
    uint32 GBE_GetDotaLobbyOwnerAccountId() const;
    std::string GBE_GetDotaLobbyOwnerName() const;
    bool GBE_HandleDotaJoinChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaAbandonCurrentGameRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaGameMatchSignOutRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_PushDotaResponse(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0, std::string *out_wrapped_message = nullptr);
    bool GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason);
    bool GBE_SendDotaPracticeLobbyLaunchMessage(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0);
    bool GBE_SendDotaCustomGameLaunchSetupFlow(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaCustomLobbyListRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyListRequest(bool has_request_job, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaFriendPracticeLobbyListRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response = true);
    bool GBE_HandleDotaInviteToLobbyRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyInviteResponseRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyLaunchRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyKickRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaChatMessageRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaFriendLobbyInviteMessage(Common_Message *msg);
    bool GBE_HandleDotaNetworkLobbyInviteMessage(Common_Message *msg);
    bool GBE_HandleDotaNetworkChatMessage(Common_Message *msg);
    bool GBE_HandleDotaLeaveChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaAddSocketRequest(const uint8 *body, size_t body_size, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaDirectPostLoginRequest(uint32 unMsgType, const void *pubData, uint32 cubData);
    bool GBE_HandleDotaWrappedPostLoginRequest(const void *pubData, uint32 cubData);
    bool handle_dota_client_message(uint32 unMsgType, const void *pubData, uint32 cubData);

    void callback_client_welcome();
    void callback_server_welcome();
    void callback_items_received(CSteamID steam_id, const std::vector<Econ_Item> &items);
    void callback_items_removed(CSteamID steam_id);
    void callback_item_updated(CSteamID steam_id, const Econ_Item &item);
    void callback_item_deleted(CSteamID steam_id, uint64 item_id);
    void callback_respawn_request(CSteamID steam_id);

    void network_callback_inventory_request(Common_Message *msg);
    void network_callback_inventory_response(Common_Message *msg);
    void network_callback_item_update(Common_Message *msg);
    void network_callback_item_deletion(Common_Message *msg);
    void network_callback_respawn_request(Common_Message *msg);
    void network_callback(Common_Message *msg);
    void RunCallbacks();

    static void steam_network_callback(void *object, Common_Message *msg);
    static void steam_run_every_runcb(void *object);

public:
    Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, bool is_server);
    ~Steam_Game_Coordinator();

    void initialize_gc();
    void shutdown_gc();
    void on_appid_changed(uint32 appid);
    bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64 local_steam_id, GBE_DotaReconnectContext *out);
    void GBE_MaybePrimeDotaServerWelcomeFromCache(const char *reason);
    std::string GBE_GetDotaJoinableCustomLobbiesHTTPJSON(uint64 requested_custom_game_id);

    // Returns true if the server GC has an active lobby matching the given lobby_id.
    // Used by client GC to detect HOST scenario and avoid running PLAYER PostGame cleanup.
    bool GBE_HasActiveServerLobby(uint64 lobby_id) const {
        // Must be active, same lobby, AND this server GC must be the lobby owner.
        // On a PLAYER machine, the server GC may sync the same lobby_id from generic
        // lobby metadata, but it is NOT the owner -- only the HOST's server GC is.
        if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id != lobby_id) return false;
        uint64 local_sid = settings ? settings->get_local_steam_id().ConvertToUint64() : 0;
        return local_sid != 0 && GBE_local_lobby.owner_steam_id == local_sid;
    }

    const std::vector<Econ_Item> &get_items() { return items; }
    const std::map<CSteamID, std::vector<Econ_Item>> &get_all_user_items() { return all_user_items; }
    const bool has_items_for_user(CSteamID steam_id) { return (all_user_items.count(steam_id) != 0); }
    const std::vector<Econ_Item> &get_items_for_user(CSteamID steam_id) { return all_user_items.at(steam_id); }

    // Public accessor for cross-GC item serialization (server GC reads client GC items)
    std::string serialize_item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id) { return item_to_gcprotobuf(item, steam_id); }

    // Public accessor for cross-GC message injection (client GC pushes to server GC)
    void push_incoming_message(uint32 msg_type, const std::string &message) { push_incoming_now(msg_type, message); }

    const std::vector<Econ_Item> &load_items_from_file();
    void save_items_to_file();
    const Econ_Item *set_item_pos(uint64 item_id, uint32 inv_pos, bool is_gc, bool save = true);
    bool delete_item(uint64 item_id, bool is_gc);

    void request_user_items(CSteamID steam_id, SteamAPICall_t api_call, bool is_gc);
    SteamAPICall_t find_items_request(CSteamID steam_id);
    void remove_user_items(CSteamID steam_id);

    void on_client_connected(CSteamID steam_id);
    void on_client_disconnected(CSteamID steam_id);

    // sends a message to the Game Coordinator
    EGCResults SendMessage_( uint32 unMsgType, const void *pubData, uint32 cubData );

    // returns true if there is a message waiting from the game coordinator
    bool IsMessageAvailable( uint32 *pcubMsgSize );

    // fills the provided buffer with the first message in the queue and returns k_EGCResultOK or 
    // returns k_EGCResultNoMessage if there is no message waiting. pcubMsgSize is filled with the message size.
    // If the provided buffer is not large enough to fit the entire message, k_EGCResultBufferTooSmall is returned
    // and the message remains at the head of the queue.
    EGCResults RetrieveMessage( uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize );

};

#endif // __INCLUDED_STEAM_GAME_COORDINATOR_H__

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
    bool GBE_dota_private_lobby_snapshot_replayed{};
    bool GBE_pending_reset_after_cache_unsubscribed{};
    uint64 GBE_pending_reset_after_cache_unsubscribed_lobby_id{};

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
        bool has_broadcast_channel{};
        uint32 broadcast_channel_id{};
        std::string broadcast_country_code;
        std::string broadcast_description;
        std::string broadcast_language_code;
        std::string pass_key;
        bool has_cache_version{};
        uint64 cache_version{};
        bool has_cache_service_id{};
        uint32 cache_service_id{};
        std::vector<uint32> cache_service_list;
        bool has_cache_sync_version{};
        uint64 cache_sync_version{};
        bool abandon_postgame_active{};
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
    bool GBE_FinalizeDotaPracticeLobbyLateSteamChain(const char *reason);
    bool GBE_QueueDotaPracticeLobbyLaunchPeripheralOnce(uint32 stage_bit, uint32 emsg, const char *template_hex, bool patch_server_id, const char *note);
    void GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *status, const char *lobby_state, bool include_party, bool include_lobby = true);
    void GBE_ClearDotaPracticeLobbyLaunchRichPresence();
    void GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(const char *reason);
    bool GBE_CaptureCurrentDotaLobbyState(const char *reason, GBE_LocalLobby &snapshot, bool restore_shared = true);
    void GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const std::string &player_name, std::string &message);
    bool GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message);
    bool GBE_TryQueueDotaPrelaunch021(const char *note, uint32 trigger_emsg, uint64 source_job);
    bool GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(const char *note, uint32 trigger_emsg, uint64 source_job, uint32 next_state, uint32 next_game_state);
    void GBE_RecordDotaLobbyCacheSubscriptionState(const std::string &message, const char *reason);
    void GBE_PublishSharedDotaLobbyState(const char *reason);
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
    bool GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason);
    bool GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyLaunchRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLeaveChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
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
    void GBE_MaybePrimeDotaServerWelcomeFromCache(const char *reason);

    const std::vector<Econ_Item> &get_items() { return items; }
    const std::map<CSteamID, std::vector<Econ_Item>> &get_all_user_items() { return all_user_items; }
    const bool has_items_for_user(CSteamID steam_id) { return (all_user_items.count(steam_id) != 0); }
    const std::vector<Econ_Item> &get_items_for_user(CSteamID steam_id) { return all_user_items.at(steam_id); }

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

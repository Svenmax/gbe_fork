/* Handler-level test stubs for offline testing of gbe_dota_*_handlers.cpp
 *
 * This file provides minimal type definitions and a recording Steam_Game_Coordinator
 * class that allows handler TUs to compile and execute without the full Steam SDK
 * dependency chain. Side-effectful coordinator methods (push_incoming_now,
 * save_items_to_file, callback_item_updated) are captured by an ActionRecorder
 * instead of executing real I/O.
 *
 * Strategy:
 * - Include real lightweight GBE headers for type definitions (lobby state, types)
 * - Provide stubs for Steam SDK types (CSteamID, Settings, Networking, etc.)
 * - Provide stubs for protobuf message types (reuse pb_stubs/ from payload_helpers_test)
 * - Define a Steam_Game_Coordinator class with recording side effects
 * - Do NOT define GBE_GC_DebugLog (it's stubbed in free_func_stubs.cpp)
 *
 * This harness is the regression-protection foundation for Phase 3.1.7-3.1.10
 * logic refactors. Each domain's smoke test drives a representative handler
 * and asserts on the recorded action sequence.
 */

#ifndef GBE_DOTA_HANDLER_TEST_STUBS_H
#define GBE_DOTA_HANDLER_TEST_STUBS_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

// =====================================================================
// Include real lightweight GBE headers for type definitions
// =====================================================================

// Action model header defines the canonical GBE_DotaActionType enum used by
// RecordedAction below. It is dependency-free (only std headers).
#include "dll/gbe_dota_action_model.h"

#include "dll/gbe_dota_types.h"
// gbe_dota_lobby_state.h transitively includes gbe_dota_reconnect_shared.h
// which provides GBE_DotaReconnectContext, GBE_SharedDotaLobbyState, etc.
#include "dll/gbe_dota_lobby_state.h"

// =====================================================================
// Steam SDK type aliases
// =====================================================================

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

// Prevent the real sdk/steam/steamtypes.h from being included after our stub
// typedefs are in place. gbe_dota_gc_internal.h includes <sdk/steam/steamtypes.h>
// for self-containment in production builds; in the test environment we provide
// equivalent typedefs above. Defining the guard here makes any subsequent
// #include <sdk/steam/steamtypes.h> a no-op so the two declarations don't
// conflict (typedef vs using).
#define STEAMTYPES_H

using PublishedFileId_t = uint64_t;
using SteamAPICall_t = uint64_t;
using AppId_t = uint32_t;
using SteamItemInstanceID_t = uint64_t;
using UGCHandle_t = uint64_t;
using JobID_t = uint64_t;

static const PublishedFileId_t k_PublishedFileIdInvalid = 0;
static const JobID_t k_GIDNil = 0;

enum EUniverse : uint32 { k_EUniverseInvalid = 0, k_EUniversePublic = 1 };
enum EAccountType : uint32 {
    k_EAccountTypeInvalid = 0,
    k_EAccountTypeIndividual = 1,
    k_EAccountTypeGameServer = 3,
    k_EAccountTypeClan = 7,
    k_EAccountTypeChat = 8,
    k_EAccountTypeConsoleUser = 9,
};

static const uint32 k_unSteamUserDefaultInstance = 1;
static const uint32 k_unSteamAccountInstanceMask = 0x000FFFFF;
static const uint32 k_EChatInstanceFlagLobby = (k_unSteamAccountInstanceMask + 1) >> 2;

// Enums needed by Econ_Item and Mod_entry
enum EItemQuality : uint32 { k_EItemQuality_Any = 0 };
enum EWorkshopFileType : uint32 { k_EWorkshopFileTypeCommunity = 0 };
enum ERemoteStoragePublishedFileVisibility : uint32 { k_ERemoteStoragePublishedFileVisibilityPublic = 0 };

// =====================================================================
// CSteamID stub
// =====================================================================

class CSteamID
{
public:
    CSteamID() : m_steamID(0) {}
    CSteamID(uint64_t steamID) : m_steamID(steamID) {}
    CSteamID(uint32_t unAccountID, uint32_t unAccountInstance, uint32_t unUniverse, uint32_t unAccountType)
    {
        InstancedSet(unAccountID, unAccountInstance, unUniverse, unAccountType);
    }

    uint64_t ConvertToUint64() const { return m_steamID; }
    operator uint64_t() const { return m_steamID; }
    uint32_t GetAccountID() const { return static_cast<uint32_t>(m_steamID & 0xFFFFFFFF); }

    bool IsValid() const { return m_steamID != 0; }
    bool IsLobby() const { return GetAccountType() == k_EAccountTypeChat && (GetAccountInstance() & k_EChatInstanceFlagLobby) != 0; }
    bool BIndividualAccount() const { return GetAccountType() == k_EAccountTypeIndividual || GetAccountType() == k_EAccountTypeConsoleUser; }

    bool operator==(const CSteamID &other) const { return m_steamID == other.m_steamID; }
    bool operator!=(const CSteamID &other) const { return m_steamID != other.m_steamID; }
    bool operator<(const CSteamID &other) const { return m_steamID < other.m_steamID; }

private:
    void InstancedSet(uint32_t account_id, uint32_t instance, uint32_t universe, uint32_t account_type)
    {
        m_steamID = static_cast<uint64_t>(account_id)
            | (static_cast<uint64_t>(instance & 0x000FFFFF) << 32)
            | (static_cast<uint64_t>(account_type & 0x0F) << 52)
            | (static_cast<uint64_t>(universe & 0xFF) << 56);
    }

    uint32_t GetAccountInstance() const { return static_cast<uint32_t>((m_steamID >> 32) & 0x000FFFFF); }
    uint32_t GetAccountType() const { return static_cast<uint32_t>((m_steamID >> 52) & 0x0F); }

    uint64_t m_steamID;
};

static const CSteamID k_steamIDNil;

// =====================================================================
// Econ_Item stub (matching real definition in dll/econ_item.h)
// =====================================================================

struct Econ_Item_Attribute
{
    enum Attribute_Type
    {
        ATTR_TYPE_DEFAULT = 0,
        ATTR_TYPE_FLOAT,
        ATTR_TYPE_INT,
        ATTR_TYPE_STRING,
    };
    uint32 def{};
    float value{};
    std::string value_bytes;
    Attribute_Type type{};
};

struct Econ_Item
{
    uint64 id{};
    uint32 def{};
    uint32 level{};
    EItemQuality quality{};
    uint32 inv_pos{};
    uint32 quantity{};
    uint8 flags{};
    uint8 origin{};
    std::string custom_name;
    std::string custom_desc;
    bool in_use{};
    uint64 original_id{};
    uint8 style{};
    std::map<uint16, uint16> equip_states;
    std::vector<Econ_Item_Attribute> attributes;
};

// =====================================================================
// Mod_entry stub (minimal, matching settings.h)
// =====================================================================

struct Mod_entry
{
    PublishedFileId_t id{};
    std::string title{};
    std::string path{};
    std::string previewURL{};
    EWorkshopFileType fileType{};
    std::string description{};
    uint64 steamIDOwner{};
    uint32 timeCreated{};
    uint32 timeUpdated{};
    uint32 timeAddedToUserList{};
    ERemoteStoragePublishedFileVisibility visibility{};
    bool banned{};
    bool acceptedForUse{};
    bool tagsTruncated{};
    std::string tags{};
    UGCHandle_t handleFile{};
    UGCHandle_t handlePreviewFile{};
    std::string primaryFileName{};
    int32 primaryFileSize{};
    std::string previewFileName{};
    int32 previewFileSize{};
    uint64 total_files_sizes{};
    std::string min_game_branch{};
    std::string max_game_branch{};
    std::string metadata{};
    std::string workshopItemURL{};
    uint32 votesUp{};
    uint32 votesDown{};
    float score{};
    uint32 numChildren{};
};

// =====================================================================
// Settings stub
// =====================================================================

class Settings
{
public:
    Settings() : m_local_steam_id(12345) {}

    CSteamID get_local_steam_id() const { return m_local_steam_id; }
    void set_local_steam_id(CSteamID sid) { m_local_steam_id = sid; }

    CSteamID get_lobby() const { return CSteamID(); }
    void set_lobby(const CSteamID &lobby_id) { (void)lobby_id; }

    const std::string &get_local_name() const { return m_name; }
    const std::string &get_language() const { return m_language; }

    const std::vector<PublishedFileId_t> &modSet() const { return m_mods; }
    bool has_mod(PublishedFileId_t mod_id) const { (void)mod_id; return false; }
    bool isModInstalled(PublishedFileId_t mod_id) const { (void)mod_id; return false; }
    Mod_entry getMod(PublishedFileId_t mod_id) const { (void)mod_id; return Mod_entry{}; }

    CSteamID m_local_steam_id;
    std::string m_name;
    std::string m_language;
    std::vector<PublishedFileId_t> m_mods;
};

// =====================================================================
// Action Recorder - captures side effects in order
// =====================================================================

struct RecordedAction
{
    // Type uses the canonical GBE_DotaActionType from dll/gbe_dota_action_model.h
    // so test assertions reference the same names the action-list contract uses.
    GBE_DotaActionType type{};
    uint32 msg_type{};           // PushIncomingNow / PushIncoming / ServerGcForward
    std::string msg_body;        // PushIncomingNow / PushIncoming (truncated to first 64 bytes for assertions)
    uint64 steam_id{};           // CallbackItemUpdated / ServerGcForward
    uint64 item_id{};            // CallbackItemUpdated
    std::string reason;          // LobbySnapshotRefresh
    int callback_id{};           // CallbackResult
    uint64 source_id{};          // NetworkBroadcast

    const char *type_name() const
    {
        switch (type) {
            case GBE_DotaActionType::PushIncomingNow:       return "PushIncomingNow";
            case GBE_DotaActionType::PushIncoming:          return "PushIncoming";
            case GBE_DotaActionType::SaveItemsToFile:       return "SaveItemsToFile";
            case GBE_DotaActionType::CallbackItemUpdated:   return "CallbackItemUpdated";
            case GBE_DotaActionType::ServerGcForward:       return "ServerGcForward";
            case GBE_DotaActionType::NetworkBroadcast:      return "NetworkBroadcast";
            case GBE_DotaActionType::LobbySnapshotRefresh:  return "LobbySnapshotRefresh";
        }
        return "Unknown";
    }
};

class ActionRecorder
{
public:
    std::vector<RecordedAction> actions;

    void clear() { actions.clear(); }

    void record_push_incoming_now(uint32 msg_type, const std::string &msg_body)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncomingNow;
        a.msg_type = msg_type;
        a.msg_body = msg_body.size() > 64 ? msg_body.substr(0, 64) : msg_body;
        actions.push_back(std::move(a));
    }

    void record_push_incoming(uint32 msg_type, const std::string &msg_body)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncoming;
        a.msg_type = msg_type;
        a.msg_body = msg_body.size() > 64 ? msg_body.substr(0, 64) : msg_body;
        actions.push_back(std::move(a));
    }

    void record_save_items()
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::SaveItemsToFile;
        actions.push_back(std::move(a));
    }

    void record_callback_item_updated(uint64 steam_id, uint64 item_id)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::CallbackItemUpdated;
        a.steam_id = steam_id;
        a.item_id = item_id;
        actions.push_back(std::move(a));
    }

    void record_server_gc_forward(uint32 msg_type)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::ServerGcForward;
        a.msg_type = msg_type;
        actions.push_back(std::move(a));
    }

    void record_network_broadcast()
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::NetworkBroadcast;
        actions.push_back(std::move(a));
    }

    void record_network_broadcast(uint64 source_id)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::NetworkBroadcast;
        a.source_id = source_id;
        actions.push_back(std::move(a));
    }

    void record_lobby_snapshot_refresh(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LobbySnapshotRefresh;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    size_t count_type(GBE_DotaActionType t) const
    {
        size_t count = 0;
        for (const auto &a : actions)
            if (a.type == t) ++count;
        return count;
    }

    // Returns the sequence of action type names for easy assertion.
    std::string action_sequence() const
    {
        std::string seq;
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) seq += " -> ";
            seq += actions[i].type_name();
            if (actions[i].type == GBE_DotaActionType::PushIncomingNow ||
                actions[i].type == GBE_DotaActionType::PushIncoming ||
                actions[i].type == GBE_DotaActionType::ServerGcForward) {
                seq += "(emsg=";
                seq += std::to_string(actions[i].msg_type & ~0x80000000u);
                seq += ")";
            }
        }
        return seq;
    }
};

// Global action recorder (set by the test before calling handlers).
// Using a pointer so the test can create/destroy recorders per test case.
inline ActionRecorder *g_action_recorder = nullptr;

class SteamCallBacks
{
public:
    void addCBResult(int iCallback, void *result, unsigned int size, double timeout)
    {
        (void)result; (void)size; (void)timeout;
        if (g_action_recorder) {
            RecordedAction a;
            a.type = GBE_DotaActionType::PushIncoming;
            a.callback_id = iCallback;
            g_action_recorder->actions.push_back(std::move(a));
        }
    }
};

// =====================================================================
// Networking stub
// =====================================================================

class GameServer_Items_Messages;

class Steam_Messages
{
public:
    enum Types
    {
        FRIEND_CHAT = 1,
    };

    void set_type(Types type) { m_type = type; }
    Types type() const { return m_type; }
    void set_message(const std::string &message) { m_message = message; }
    const std::string &message() const { return m_message; }

private:
    Types m_type{};
    std::string m_message;
};

class Common_Message
{
public:
    void set_allocated_gameserver_items_messages(GameServer_Items_Messages *msg);
    void set_allocated_steam_messages(Steam_Messages *msg) { m_steam_messages.reset(msg); }
    void set_source_id(uint64_t id) { m_source_id = id; }
    uint64 source_id() const { return m_source_id; }
    bool has_steam_messages() const { return static_cast<bool>(m_steam_messages); }
    const Steam_Messages &steam_messages() const { return *m_steam_messages; }

private:
    uint64 m_source_id{};
    std::unique_ptr<Steam_Messages> m_steam_messages;
};

class Networking
{
public:
    Networking() {}
    bool sendToAllGameservers(Common_Message *msg, bool reliable)
    {
        // The equip handler passes a stack-allocated Common_Message, so we
        // must NOT delete it. The inner GameServer_Items_Messages is already
        // freed by Common_Message::set_allocated_gameserver_items_messages.
        (void)msg; (void)reliable;
        if (g_action_recorder)
            g_action_recorder->record_network_broadcast();
        return true;
    }

    bool sendToAll(Common_Message *msg, bool reliable)
    {
        (void)reliable;
        if (g_action_recorder)
            g_action_recorder->record_network_broadcast(msg ? msg->source_id() : 0u);
        return true;
    }
};

// =====================================================================
// GameServer_Items_Messages protobuf stub
// =====================================================================

class GameServer_Items_Messages
{
public:
    enum Type
    {
        Response_Inventory = 0,
    };

    class InventoryResponse
    {
    public:
        class Item
        {
        public:
            void set_id(uint64_t v) { (void)v; }
            void set_def(uint32_t v) { (void)v; }
            void set_level(uint32_t v) { (void)v; }
            void set_quality(int32_t v) { (void)v; }
            void set_inv_pos(uint32_t v) { (void)v; }
            void set_quantity(uint32_t v) { (void)v; }
            void set_flags(uint8_t v) { (void)v; }
            void set_origin(uint8_t v) { (void)v; }
            void set_original_id(uint64_t v) { (void)v; }
            void set_in_use(bool v) { (void)v; }
            void set_style(uint8_t v) { (void)v; }

            class EquippedState
            {
            public:
                void set_class_id(uint16_t v) { (void)v; }
                void set_slot_id(uint16_t v) { (void)v; }
            };

            class Attribute
            {
            public:
                void set_def(uint32_t v) { (void)v; }
                void set_value(float v) { (void)v; }
                void set_value_bytes(const std::string &v) { (void)v; }
            };

            EquippedState *add_equip_states() { return &m_es; }
            Attribute *add_attributes() { return &m_attr; }

            EquippedState m_es;
            Attribute m_attr;
        };

        void set_steam_api_call(uint64_t v) { (void)v; }
        Item *add_items() { return &m_item; }

        Item m_item;
    };

    void set_type(Type t) { (void)t; }
    void set_is_gc(bool v) { (void)v; }
    void set_allocated_inventory_response(InventoryResponse *resp) { delete resp; }
};

inline void Common_Message::set_allocated_gameserver_items_messages(GameServer_Items_Messages *msg) { delete msg; }

// =====================================================================
// Steam_Client stub
// =====================================================================

class Steam_Game_Coordinator; // forward declaration
class Steam_Matchmaking;
class Steam_Friends;

class Steam_Client
{
public:
    Steam_Client() {}
    Steam_Matchmaking *steam_matchmaking{};
    Steam_Friends *steam_friends{};
    Steam_Game_Coordinator *steam_game_coordinator{};
    Steam_Game_Coordinator *steam_gameserver_game_coordinator{};
};

Steam_Client *get_steam_client();

// Test-only: the static Steam_Client instance returned by get_steam_client().
// Exposed so smoke tests can wire a server GC into it (set
// steam_gameserver_game_coordinator) to exercise the server-GC-forward path.
extern Steam_Client g_test_steam_client;

// =====================================================================
// GC message types
// =====================================================================

struct GC_Message
{
    uint32_t msg_type{};
    std::string msg_body;
    std::chrono::high_resolution_clock::time_point created{};
    double post_in{};
    uint64_t sequence{};
    bool apply_lobby_state{};
    uint32_t lobby_state{};
    uint32_t lobby_game_state{};
};

struct GCMessageAvailable_t
{
    enum { k_iCallback = 1701 };
    uint32 m_nMessageSize{};
};

// =====================================================================
// Protobuf namespace stubs (supplement pb_stubs/)
// =====================================================================

namespace google { namespace protobuf {
    class Message {};
    class MessageLite {};
}}

// =====================================================================
// ESOMsg stub (not in pb_stubs/gcsystemmsgs.pb.h which is empty)
// =====================================================================

namespace gamecoordinator { namespace tf2 {

enum ESOMsg {
    k_ESOMsg_Create = 21,
    k_ESOMsg_Update = 22,
    k_ESOMsg_Destroy = 24,
    k_ESOMsg_UpdateMultiple = 26,
    k_ESOMsg_CacheSubscribed = 27,
    k_ESOMsg_CacheUnsubscribed = 28,
};

}} // namespace gamecoordinator::tf2

// =====================================================================
// Steam_Game_Coordinator stub with recording side effects
// =====================================================================

class Steam_Game_Coordinator
{
public:
    // Enum needed by handlers
    enum GC_Profile
    {
        GC_PROFILE_INVALID = 0,
        GC_PROFILE_TF2,
        GC_PROFILE_DOTA2,
    };

    static constexpr uint32 protobuf_mask = 0x80000000;

    // Member state accessed by handlers
    Settings *settings{};
    Networking *network{};
    void *local_storage{};       // opaque, not used by handler tests
    SteamCallBacks *callbacks{};
    bool is_server{};

    GBE_LocalLobby GBE_local_lobby{};
    std::vector<Econ_Item> items;
    std::map<CSteamID, std::vector<Econ_Item>> all_user_items;
    std::queue<GC_Message> incoming_messages;
    bool items_loaded{};
    bool gc_initialized{true};
    bool GBE_pending_dota_abandon_finalize_after_7014{};
    uint64 GBE_pending_dota_abandon_finalize_lobby_id{};

    GC_Profile gc_profile{};
    bool GBE_dota_private_lobby_snapshot_replayed{};

    // --- Recording side-effect methods ---

    void push_incoming_now(uint32 msg_type, const std::string &message,
                           bool apply_lobby_state = false,
                           uint32 lobby_state = 0,
                           uint32 lobby_game_state = 0)
    {
        (void)apply_lobby_state; (void)lobby_state; (void)lobby_game_state;
        if (g_action_recorder)
            g_action_recorder->record_push_incoming_now(msg_type, message);
    }

    void push_incoming(uint32 msg_type, const std::string &message,
                       double delay = 0.1,
                       bool apply_lobby_state = false,
                       uint32 lobby_state = 0,
                       uint32 lobby_game_state = 0)
    {
        (void)delay; (void)apply_lobby_state; (void)lobby_state; (void)lobby_game_state;
        if (g_action_recorder)
            g_action_recorder->record_push_incoming(msg_type, message);
    }

    bool GBE_PushDotaResponse(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0, std::string *out_wrapped_message = nullptr)
    {
        (void)wrapped; (void)outer_session_field_raw; (void)reason;
        push_incoming_now(inner_emsg | protobuf_mask, inner_message, apply_lobby_state, lobby_state, lobby_game_state);
        if (out_wrapped_message)
            *out_wrapped_message = inner_message;
        return true;
    }

    void save_items_to_file()
    {
        if (g_action_recorder)
            g_action_recorder->record_save_items();
    }

    std::string build_protomsg_header(uint32 msg_type,
                                      JobID_t target_job = k_GIDNil,
                                      JobID_t source_job = k_GIDNil)
    {
        (void)msg_type; (void)target_job; (void)source_job;
        // Return a minimal placeholder so AppendToString can chain.
        return std::string(8, '\0');
    }

    std::string item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id)
    {
        (void)item; (void)steam_id;
        // Return a minimal placeholder so CMsgSOSingleObject.set_object_data gets
        // non-empty data. The smoke test only asserts on action sequence, not
        // payload bytes, so the exact content doesn't matter.
        return std::string(4, '\x01');
    }

    void callback_item_updated(CSteamID steam_id, const Econ_Item &item)
    {
        if (g_action_recorder)
            g_action_recorder->record_callback_item_updated(
                steam_id.ConvertToUint64(), item.id);
    }

    // Public accessor used by handler code for cross-GC message injection.
    // In tests, this records the server GC forward.
    void push_incoming_message(uint32 msg_type, const std::string &message)
    {
        if (g_action_recorder)
            g_action_recorder->record_server_gc_forward(msg_type);
        (void)message;
    }

    // Inline helper from real header - used by inventory handlers to check
    // if the server GC owns a lobby. In tests, return false to skip the
    // server-GC-forward code path unless the test explicitly enables it.
    bool GBE_HasActiveServerLobby(uint64 lobby_id) const
    {
        (void)lobby_id;
        return m_test_has_active_server_lobby;
    }

    void callback_items_received(CSteamID, const std::vector<Econ_Item> &) {}
    void callback_items_removed(CSteamID) {}
    void callback_item_deleted(CSteamID, uint64) {}
    void callback_respawn_request(CSteamID) {}
    void callback_client_welcome() {}
    void callback_server_welcome() {}

    // Member function used by equip handler - stub records the call.
    void GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_snapshot_refresh(reason);
    }

    bool GBE_HasDotaLaunchServerSetupSync() const { return false; }
    bool GBE_TryAdvanceDotaLaunchToRun(const char *, uint32, uint64, const char *, uint32 = 0u) { return false; }
    void GBE_PublishSharedDotaLobbyState(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_snapshot_refresh(reason);
    }
    bool GBE_TrySyncDotaLobbyServerIdFromGameServer(const char *) { return false; }
    bool GBE_SendDotaPracticeLobbyDetailsUpdate(bool, const std::string *, const char *) { return true; }
    bool GBE_CaptureCurrentDotaLobbyState(const char *, GBE_LocalLobby &snapshot, bool = true) { snapshot = GBE_local_lobby; return GBE_local_lobby.active; }
    void GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *, const char *, bool, bool = true) {}
    void GBE_LeaveGenericLobby() { GBE_local_lobby = GBE_LocalLobby{}; }
    bool GBE_MaybeHandleDotaPracticeLobbyKicked(const char *) { return false; }
    uint64 GBE_GetDotaLobbyOwnerSteamId() const { return GBE_local_lobby.owner_steam_id; }
    const std::vector<Econ_Item> &get_items() { return items; }
    std::string serialize_item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id) { return item_to_gcprotobuf(item, steam_id); }

    // --- Test-only controls ---
    void test_set_active_server_lobby(bool v) { m_test_has_active_server_lobby = v; }

    // --- Handler declarations (defined in handler .cpp via test_wrapper) ---
    // Inventory domain
    bool GBE_HandleDotaUnlockItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaSetItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaEquipItemsRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);

    // Misc domain
    bool GBE_HandleDotaMinimalVarintSuccessRequest(uint32 request_emsg, uint32 response_emsg, const char *log_note, const char *push_note, bool has_source_job, uint64 source_job);
    bool GBE_HandleDota7427NotificationsRequest(bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaUploadRateRequest(bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaProfileCardRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaLookupAccountNameRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaEmoticonDataRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaConductScorecardRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaCoachingSummaryRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaRankRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaLaunchAdvanceOrConsume(uint32 request_emsg, const char *advance_reason, const char *advance_phase, const char *consume_note, uint64 source_job, size_t body_size);
    bool GBE_HandleDota8870LaunchMarkerRequest(uint32 request_emsg, uint64 source_job);
    bool GBE_HandleDotaLanServerAvailableRequest(uint32 request_emsg, const uint8 *body, size_t body_size, uint64 source_job);
    bool GBE_HandleDotaBatchPlayerResourcesRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaCacheSubscriptionRefreshRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaLeaverDetectedRequest(const uint8 *body, size_t body_size, uint64 source_job);
    bool GBE_HandleDotaSignOutPermissionRequest(bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaSubmitPlayerReportV2Request(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);

    // Chat domain
    bool GBE_HandleDotaJoinChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaChatMessageRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaNetworkChatMessage(Common_Message *msg);
    bool GBE_HandleDotaLeaveChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);

    // Other handlers declared in the real header but not defined in the
    // inventory TU - we don't need them here. If a future domain test
    // compiles a different handler .cpp, add the corresponding declarations
    // to a domain-specific stub extension.

private:
    bool m_test_has_active_server_lobby = false;
};

// =====================================================================
// Steam_Friends stub (needed by some handler TUs)
// =====================================================================

class Steam_Friends
{
public:
    const char *GetFriendPersonaName(const CSteamID &steam_id) { (void)steam_id; return ""; }
};

// =====================================================================
// Steam_Matchmaking stub
// =====================================================================

class Steam_Matchmaking
{
public:
    const char *GetLobbyMemberData(const CSteamID &lobby, const CSteamID &member, const char *key)
    { (void)lobby; (void)member; (void)key; return ""; }
    void RefreshLobbyCallbacksForDota() {}
};

#endif // GBE_DOTA_HANDLER_TEST_STUBS_H

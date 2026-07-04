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

using PublishedFileId_t = uint64_t;
using SteamAPICall_t = uint64_t;
using AppId_t = uint32_t;
using SteamItemInstanceID_t = uint64_t;
using UGCHandle_t = uint64_t;
using JobID_t = uint64_t;

static const PublishedFileId_t k_PublishedFileIdInvalid = 0;
static const JobID_t k_GIDNil = 0;

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
    CSteamID(uint32_t unAccountID, uint32_t unAccountInstance, uint32_t unAccountType, uint32_t unUniverse)
        : m_steamID(0) { (void)unAccountID; (void)unAccountInstance; (void)unAccountType; (void)unUniverse; }

    uint64_t ConvertToUint64() const { return m_steamID; }
    operator uint64_t() const { return m_steamID; }
    uint32_t GetAccountID() const { return static_cast<uint32_t>(m_steamID & 0xFFFFFFFF); }

    bool IsValid() const { return m_steamID != 0; }
    bool IsLobby() const { return false; }
    bool BIndividualAccount() const { return IsValid(); }

    bool operator==(const CSteamID &other) const { return m_steamID == other.m_steamID; }
    bool operator!=(const CSteamID &other) const { return m_steamID != other.m_steamID; }
    bool operator<(const CSteamID &other) const { return m_steamID < other.m_steamID; }

private:
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

// =====================================================================
// Networking stub
// =====================================================================

class GameServer_Items_Messages;

class Common_Message
{
public:
    void set_allocated_gameserver_items_messages(GameServer_Items_Messages *msg);
    void set_source_id(uint64_t id) { (void)id; }
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

class Steam_Client
{
public:
    Steam_Client() {}
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
    void *callbacks{};           // opaque
    bool is_server{};

    GBE_LocalLobby GBE_local_lobby{};
    std::vector<Econ_Item> items;
    bool items_loaded{};

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

    // --- Test-only controls ---
    void test_set_active_server_lobby(bool v) { m_test_has_active_server_lobby = v; }

    // --- Handler declarations (defined in handler .cpp via test_wrapper) ---
    // Inventory domain
    bool GBE_HandleDotaUnlockItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaSetItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    bool GBE_HandleDotaEquipItemsRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);

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
};

#endif // GBE_DOTA_HANDLER_TEST_STUBS_H

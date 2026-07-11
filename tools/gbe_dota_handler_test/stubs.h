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
#include <cstring>

// =====================================================================
// Core type surface: real lightweight GBE headers
// =====================================================================

// Action model header defines the canonical GBE_DotaActionType enum used by
// RecordedAction below. It is dependency-free (only std headers).
#include "dll/gbe_dota_action_model.h"

#include "dll/gbe_dota_protocol_constants.h"
#include "dll/gbe_dota_custom_game.h"
#include "dll/gbe_dota_custom_game_lifecycle.h"
#include "dll/gbe_dota_connection_dedup_key.h"
#include "dll/gbe_dota_gc_router.h"
#include "dll/gbe_dota_handler_registry.h"
#include "dll/gbe_dota_lobby_generation.h"
#include "dll/gbe_dota_types.h"
#include "dll/gbe_proto_wire.h"
// gbe_dota_lobby_state.h transitively includes gbe_dota_reconnect_shared.h
// which provides GBE_DotaReconnectContext, GBE_SharedDotaLobbyState, etc.
#include "dll/gbe_dota_lobby_state.h"
#include "dll/gbe_dota_lobby_state_store.h"
#include "dll/gbe_dota_lobby_launch_flow.h"

namespace gbe::dota_lobby_flow {
GBE_DotaActionList player_postgame_cleanup_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    bool push_cache_unsubscribed,
    const char *reason);
GBE_DotaActionList normal_signout_finalize_action_list(
    std::uint64_t lobby_id,
    const std::string &client_response_25,
    bool push_client_cache_unsubscribed,
    const char *reason);
GBE_DotaActionList abandon_finalize_action_list(
    const char *reason);
}

gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore();
extern bool GBE_pending_reset_after_cache_unsubscribed;
extern uint64_t GBE_pending_reset_after_cache_unsubscribed_lobby_id;
extern bool GBE_pending_dota_normal_signout_finalize_after_25;
extern uint64_t GBE_pending_dota_normal_signout_finalize_lobby_id;
extern bool GBE_dota_host_showcase_equip_pushed;
extern uint64_t GBE_dota_host_local_wearable_refresh_generation;
extern uint64_t GBE_dota_host_local_wearable_refresh_steam_id;
extern uint32_t GBE_dota_host_local_wearable_refresh_hero_id;

// =====================================================================
// Core type surface: Steam SDK aliases and constants
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
enum ELobbyType : uint32 { k_ELobbyTypeInvisible = 0 };

// =====================================================================
// Core type surface: CSteamID stub
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
// Inventory model stubs: Econ_Item and Mod_entry
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
// Core service stubs: Settings
// =====================================================================

class Settings
{
public:
    Settings() : m_local_steam_id(12345) {}

    CSteamID get_local_steam_id() const { return m_local_steam_id; }
    void set_local_steam_id(CSteamID sid) { m_local_steam_id = sid; }

    CSteamID get_lobby() const { return m_lobby_id; }
    void set_lobby(const CSteamID &lobby_id) { m_lobby_id = lobby_id; }

    const std::string &get_local_name() const { return m_name; }
    const std::string &get_language() const { return m_language; }

    const std::vector<PublishedFileId_t> &modSet() const { return m_mods; }
    bool has_mod(PublishedFileId_t mod_id) const { return m_mod_entries.find(mod_id) != m_mod_entries.end(); }
    bool isModInstalled(PublishedFileId_t mod_id) const { return has_mod(mod_id); }
    Mod_entry getMod(PublishedFileId_t mod_id) const
    {
        auto it = m_mod_entries.find(mod_id);
        return it != m_mod_entries.end() ? it->second : Mod_entry{};
    }

    CSteamID m_local_steam_id;
    CSteamID m_lobby_id;
    std::string m_name;
    std::string m_language;
    std::vector<PublishedFileId_t> m_mods;
    std::map<PublishedFileId_t, Mod_entry> m_mod_entries;
};

// =====================================================================
// Core recorder: captures side effects and domain-specific observations
// =====================================================================

struct RecordedAction
{
    // Type uses the canonical GBE_DotaActionType from dll/gbe_dota_action_model.h
    // so test assertions reference the same names the action-list contract uses.
    GBE_DotaActionType type{};
    uint32 msg_type{};           // PushIncomingNow / PushIncoming / ServerGcForward
    std::string msg_body;        // PushIncomingNow / PushIncoming / ServerGcForward
    uint64 steam_id{};           // CallbackItemUpdated / ServerGcForward
    uint64 item_id{};            // CallbackItemUpdated
    std::string reason;          // Dota response / LobbySnapshotRefresh / ServerGcForward
    int callback_id{};           // CallbackResult
    uint64 source_id{};          // NetworkBroadcast
    bool server_gc{};            // PushIncoming
    bool wrapped{};              // Dota response wrapper flag
    std::string session_raw;     // Dota response outer session field
    std::string status;          // RichPresenceUpdate
    std::string lobby_state;     // RichPresenceUpdate
    JobID_t target_job{};        // build_protomsg_header target job
    JobID_t source_job{};        // build_protomsg_header source job
    bool server_gc_unsubscribe_first{};
    bool include_party{};        // RichPresenceUpdate
    bool include_lobby{};        // RichPresenceUpdate
    bool apply_lobby_state{};    // PushIncomingNow
    uint32 applied_lobby_state{};      // PushIncomingNow
    uint32 applied_lobby_game_state{}; // PushIncomingNow
    size_t server_gc_source_item_count{};
    double delay{};              // PushIncoming

    const char *type_name() const
    {
        switch (type) {
            case GBE_DotaActionType::PushIncomingNow:       return "PushIncomingNow";
            case GBE_DotaActionType::PushIncoming:          return "PushIncoming";
            case GBE_DotaActionType::SaveItemsToFile:       return "SaveItemsToFile";
            case GBE_DotaActionType::CallbackItemUpdated:   return "CallbackItemUpdated";
            case GBE_DotaActionType::ServerGcForward:       return "ServerGcForward";
            case GBE_DotaActionType::NetworkBroadcast:      return "NetworkBroadcast";
            case GBE_DotaActionType::GcMemoryReset:         return "GcMemoryReset";
            case GBE_DotaActionType::LobbySnapshotRefresh:  return "LobbySnapshotRefresh";
            case GBE_DotaActionType::GenericLobbyCreate:    return "GenericLobbyCreate";
            case GBE_DotaActionType::GenericLobbyLeave:     return "GenericLobbyLeave";
            case GBE_DotaActionType::GenericLobbyJoin:      return "GenericLobbyJoin";
            case GBE_DotaActionType::SettingsLobbyClear:    return "SettingsLobbyClear";
            case GBE_DotaActionType::RichPresenceUpdate:    return "RichPresenceUpdate";
            case GBE_DotaActionType::RichPresenceClear:     return "RichPresenceClear";
            case GBE_DotaActionType::LaunchPersonaState:    return "LaunchPersonaState";
            case GBE_DotaActionType::LobbyLocalMemberData:  return "LobbyLocalMemberData";
            case GBE_DotaActionType::LobbyMetadataPublish:  return "LobbyMetadataPublish";
            case GBE_DotaActionType::LaunchPeripheralReset: return "LaunchPeripheralReset";
            case GBE_DotaActionType::LobbyCacheSubscriptionRecord: return "LobbyCacheSubscriptionRecord";
            case GBE_DotaActionType::LaunchStateGameStateRecord: return "LaunchStateGameStateRecord";
            case GBE_DotaActionType::SettingsLobbySync:     return "SettingsLobbySync";
            case GBE_DotaActionType::DotaLobbyRuntimeClear: return "DotaLobbyRuntimeClear";
            case GBE_DotaActionType::AbandonedLobbySuppressed: return "AbandonedLobbySuppressed";
            case GBE_DotaActionType::LaunchMessagesDiscardedForAbandon: return "LaunchMessagesDiscardedForAbandon";
            case GBE_DotaActionType::PendingResetAfterCacheUnsubscribed: return "PendingResetAfterCacheUnsubscribed";
            case GBE_DotaActionType::PendingResetAfterCacheUnsubscribedClear: return "PendingResetAfterCacheUnsubscribedClear";
            case GBE_DotaActionType::PendingNormalSignoutFinalizeAfterCacheUnsubscribed: return "PendingNormalSignoutFinalizeAfterCacheUnsubscribed";
            case GBE_DotaActionType::LobbyStateApply: return "LobbyStateApply";
            case GBE_DotaActionType::LobbyMemberRuntimeUpdate: return "LobbyMemberRuntimeUpdate";
            case GBE_DotaActionType::LaunchPhaseMark: return "LaunchPhaseMark";
            case GBE_DotaActionType::SharedLobbyPublish: return "SharedLobbyPublish";
            case GBE_DotaActionType::PracticeLobbyDetailsUpdate: return "PracticeLobbyDetailsUpdate";
            case GBE_DotaActionType::RuntimeLobbyDetailsUpdate: return "RuntimeLobbyDetailsUpdate";
        }
        return "Unknown";
    }
};

struct RecordedRuntimeState
{
    uint64 steam_id{};
    bool connected{};
    uint32 hero_id{};
    bool has_hero_id{};
    size_t action_sequence_index{};
};

struct RecordedPracticeLobbyDetailsUpdate
{
    bool preserve_server_id{};
    std::string message_override;
    std::string reason;
    size_t action_sequence_index{};
};

struct RecordedLobbyKick
{
    uint64 lobby_id{};
    uint64 member_id{};
};

class ActionRecorder
{
public:
    std::vector<RecordedAction> actions;
    std::vector<RecordedRuntimeState> runtime_states;
    std::vector<RecordedPracticeLobbyDetailsUpdate> practice_lobby_details_updates;
    std::vector<RecordedLobbyKick> lobby_kicks;
    std::vector<std::string> lifecycle_events;

    void clear()
    {
        actions.clear();
        runtime_states.clear();
        practice_lobby_details_updates.clear();
        lobby_kicks.clear();
        lifecycle_events.clear();
    }

    void record_runtime_state(uint64 steam_id, bool connected, uint32 hero_id, bool has_hero_id)
    {
        RecordedRuntimeState state;
        state.steam_id = steam_id;
        state.connected = connected;
        state.hero_id = hero_id;
        state.has_hero_id = has_hero_id;
        state.action_sequence_index = actions.size();
        runtime_states.push_back(state);
        lifecycle_events.push_back("runtime_state");
    }

    void record_practice_lobby_details_update(bool preserve_server_id, const std::string *message_override, const char *reason)
    {
        RecordedPracticeLobbyDetailsUpdate update;
        update.preserve_server_id = preserve_server_id;
        if (message_override)
            update.message_override = *message_override;
        update.reason = reason ? reason : "";
        update.action_sequence_index = actions.size();
        practice_lobby_details_updates.push_back(std::move(update));
        lifecycle_events.push_back("details_update");
    }

    void record_lobby_kick(uint64 lobby_id, uint64 member_id)
    {
        RecordedLobbyKick kick;
        kick.lobby_id = lobby_id;
        kick.member_id = member_id;
        lobby_kicks.push_back(kick);
    }

    void record_push_incoming_now(uint32 msg_type,
                                  const std::string &msg_body,
                                  bool apply_lobby_state = false,
                                  uint32 lobby_state = 0,
                                  uint32 lobby_game_state = 0)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncomingNow;
        a.msg_type = msg_type;
        a.msg_body = msg_body;
        a.apply_lobby_state = apply_lobby_state;
        a.applied_lobby_state = lobby_state;
        a.applied_lobby_game_state = lobby_game_state;
        actions.push_back(std::move(a));
    }

    void record_push_incoming(uint32 msg_type, const std::string &msg_body)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncoming;
        a.msg_type = msg_type;
        a.msg_body = msg_body;
        actions.push_back(std::move(a));
    }

    void record_respawn_request(uint64 steam_id, double delay, bool server_gc)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncoming;
        a.msg_type = 1029u;
        a.steam_id = steam_id;
        a.reason = "host_local_wearable_refresh";
        a.delay = delay;
        a.server_gc = server_gc;
        actions.push_back(std::move(a));
    }

    void record_dota_response(uint32 msg_type,
                              const std::string &msg_body,
                              bool wrapped,
                              const std::string *outer_session_field_raw,
                              const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::PushIncomingNow;
        a.msg_type = msg_type;
        a.msg_body = msg_body;
        a.wrapped = wrapped;
        if (outer_session_field_raw)
            a.session_raw = *outer_session_field_raw;
        a.reason = reason ? reason : "";
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

    void record_server_gc_cache_forward(uint32 msg_type,
                                        const void *target_gc,
                                        uint64 steam_id,
                                        size_t source_item_count,
                                        bool unsubscribe_first,
                                        const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::ServerGcForward;
        a.msg_type = msg_type;
        a.steam_id = steam_id;
        a.server_gc_source_item_count = source_item_count;
        a.server_gc_unsubscribe_first = unsubscribe_first;
        a.reason = reason ? reason : "";
        a.item_id = target_gc ? 1u : 0u;
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

    void record_dota_lobby_runtime_clear(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::DotaLobbyRuntimeClear;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_gc_memory_reset(const char *reason, bool leave_generic_lobby, bool clear_queued_messages)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::GcMemoryReset;
        a.reason = reason ? reason : "";
        a.include_lobby = leave_generic_lobby;
        a.include_party = clear_queued_messages;
        actions.push_back(std::move(a));
    }

    void record_generic_lobby_leave(uint64 lobby_id)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::GenericLobbyLeave;
        a.item_id = lobby_id;
        actions.push_back(std::move(a));
    }

    void record_settings_lobby_clear(uint64 lobby_id)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::SettingsLobbyClear;
        a.item_id = lobby_id;
        actions.push_back(std::move(a));
    }

    void record_rich_presence_update(const char *status, const char *lobby_state, bool include_party, bool include_lobby)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::RichPresenceUpdate;
        a.status = status ? status : "";
        a.lobby_state = lobby_state ? lobby_state : "";
        a.include_party = include_party;
        a.include_lobby = include_lobby;
        actions.push_back(std::move(a));
    }

    void record_rich_presence_clear(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::RichPresenceClear;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_launch_persona_state(const char *status, const char *lobby_state, bool include_party, bool include_lobby, const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LaunchPersonaState;
        a.status = status ? status : "";
        a.lobby_state = lobby_state ? lobby_state : "";
        a.include_party = include_party;
        a.include_lobby = include_lobby;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_lobby_local_member_data(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LobbyLocalMemberData;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_lobby_metadata_publish(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LobbyMetadataPublish;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_launch_peripheral_reset(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LaunchPeripheralReset;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_lobby_cache_subscription_record(const std::string &message, const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LobbyCacheSubscriptionRecord;
        a.msg_body = message;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_settings_lobby_sync(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::SettingsLobbySync;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_abandoned_lobby_suppressed(uint64 lobby_id, const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::AbandonedLobbySuppressed;
        a.item_id = lobby_id;
        a.reason = reason ? reason : "";
        actions.push_back(std::move(a));
    }

    void record_launch_messages_discarded_for_abandon(const char *reason)
    {
        RecordedAction a;
        a.type = GBE_DotaActionType::LaunchMessagesDiscardedForAbandon;
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
// Core service stubs: Networking and outbound common messages
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

class Friend_Messages
{
public:
    enum Types
    {
        LOBBY_INVITE = 1,
    };

    void set_type(Types type) { m_type = type; }
    Types type() const { return m_type; }
    void set_lobby_id(uint64 id) { m_lobby_id = id; }
    uint64 lobby_id() const { return m_lobby_id; }

private:
    Types m_type{};
    uint64 m_lobby_id{};
};

class Common_Message
{
public:
    void set_allocated_gameserver_items_messages(GameServer_Items_Messages *msg);
    void set_allocated_steam_messages(Steam_Messages *msg) { m_steam_messages.reset(msg); }
    void set_allocated_friend_messages(Friend_Messages *msg) { m_friend_messages.reset(msg); }
    void set_source_id(uint64_t id) { m_source_id = id; }
    uint64 source_id() const { return m_source_id; }
    bool has_steam_messages() const { return static_cast<bool>(m_steam_messages); }
    const Steam_Messages &steam_messages() const { return *m_steam_messages; }
    bool has_friend_messages() const { return static_cast<bool>(m_friend_messages); }
    const Friend_Messages &friend_messages() const { return *m_friend_messages; }

private:
    uint64 m_source_id{};
    std::unique_ptr<Steam_Messages> m_steam_messages;
    std::unique_ptr<Friend_Messages> m_friend_messages;
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
            g_action_recorder->record_network_broadcast(msg ? msg->source_id() : 0u);
        return true;
    }

    bool sendToAll(Common_Message *msg, bool reliable)
    {
        (void)reliable;
        if (g_action_recorder)
            g_action_recorder->record_network_broadcast(msg ? msg->source_id() : 0u);
        return true;
    }

    uint32 getOwnIP() const { return 0x7f000001u; }
};

// =====================================================================
// Inventory protobuf stubs: GameServer_Items_Messages
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
// Core service stubs: Steam_Client singleton surface
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
// Core GC queue and callback types
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
    uint64_t lobby_id{};
    uint64_t generation{};
    bool validate_lobby_generation{};
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
// Inventory protobuf enum stubs
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
// Coordinator seam: shared state, side-effect methods, and domain hooks
// =====================================================================

class Steam_Game_Coordinator
{
    friend class gbe::dota_lifecycle::CoordinatorExecutor;

public:
    enum class GBE_DotaGenerationAdvanceResult : uint8 { Advanced, Exhausted };
    enum class GBE_DotaDeferredTaskStatus : uint8 { Current, Stale, Empty };
    struct GBE_DotaDeferredTaskSlot { uint64 lobby_id{}; uint64 generation{}; bool pending{}; };
    struct GBE_DotaDeferredTaskConsumeResult {
        GBE_DotaDeferredTaskStatus status{GBE_DotaDeferredTaskStatus::Empty};
        uint64 lobby_id{};
        uint64 generation{};
    };
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
    gbe::dota_lifecycle::CoordinatorExecutor production_lifecycle_executor{};
    gbe::dota_lifecycle::Executor *lifecycle_executor{&production_lifecycle_executor};
    bool is_server{};
    gbe::dota_handler_registry::View handler_registry{};

    static gbe::dota_handler_registry::View GBE_ProductionDotaHandlerRegistry();
    bool GBE_DispatchDotaPostLoginRequest(const gbe::dota_gc_router::DotaGcRequestContext &context);

    GBE_LocalLobby GBE_local_lobby{};
    std::vector<Econ_Item> items;
    std::map<CSteamID, std::vector<Econ_Item>> all_user_items;
    std::queue<GC_Message> incoming_messages;
    std::vector<GC_Message> pending_messages;
    bool items_loaded{};
    bool gc_initialized{true};
    bool GBE_dota_login_sync_sent{};
    bool GBE_pending_dota_abandon_finalize_after_7014{};
    uint64 GBE_pending_dota_abandon_finalize_lobby_id{};
    gbe::dota_lobby_generation::Counter GBE_dota_lobby_generation_counter;
    GBE_DotaDeferredTaskSlot GBE_pending_dota_abandon_finalize_slot;
    GBE_DotaDeferredTaskSlot GBE_pending_dota_normal_signout_finalize_slot;
    GBE_DotaDeferredTaskSlot GBE_pending_reset_after_cache_unsubscribed_slot;

    uint64 GBE_CurrentDotaLobbyGeneration() const { return GBE_dota_lobby_generation_counter.current().value; }
    GBE_DotaGenerationAdvanceResult GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary boundary, const char *)
    {
        return GBE_dota_lobby_generation_counter.advance(boundary).advanced ? GBE_DotaGenerationAdvanceResult::Advanced : GBE_DotaGenerationAdvanceResult::Exhausted;
    }
    GBE_DotaDeferredTaskConsumeResult GBE_ConsumeDotaDeferredTask(GBE_DotaDeferredTaskSlot &slot)
    {
        if (!slot.pending)
            return {};
        GBE_DotaDeferredTaskConsumeResult result;
        result.lobby_id = slot.lobby_id;
        result.generation = slot.generation;
        result.status = slot.lobby_id == GBE_local_lobby.lobby_id && slot.generation == GBE_CurrentDotaLobbyGeneration()
            ? GBE_DotaDeferredTaskStatus::Current : GBE_DotaDeferredTaskStatus::Stale;
        slot = {};
        return result;
    }

    bool GBE_HasPendingDotaAbandonFinalizeAfterOtherLeftChannel() const { return GBE_pending_dota_abandon_finalize_after_7014; }
    bool GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed() const { return GBE_pending_dota_normal_signout_finalize_after_25; }
    bool GBE_HasPendingResetAfterCacheUnsubscribed() const { return GBE_pending_reset_after_cache_unsubscribed; }
    gbe::dota_lobby_state::Store &GBE_SharedLobbyStore() const { return GBE_GetSharedDotaLobbyStateStore(); }
    bool GBE_HasSentDotaLoginSync() const { return GBE_dota_login_sync_sent; }
    void GBE_MarkDotaLoginSyncSent() { GBE_dota_login_sync_sent = true; }
    void GBE_ClearDotaLoginSyncSent() { GBE_dota_login_sync_sent = false; }
    bool GBE_HasPushedDotaHostShowcaseEquip() const { return GBE_dota_host_showcase_equip_pushed; }
    void GBE_MarkDotaHostShowcaseEquipPushed() { GBE_dota_host_showcase_equip_pushed = true; }
    void GBE_ClearDotaHostShowcaseEquipPushed() { GBE_dota_host_showcase_equip_pushed = false; }
    bool GBE_HasRefreshedDotaHostLocalWearables(uint64 steam_id, uint32 hero_id) const
    {
        return GBE_dota_host_local_wearable_refresh_generation == GBE_CurrentDotaLobbyGeneration() &&
            GBE_dota_host_local_wearable_refresh_steam_id == steam_id &&
            GBE_dota_host_local_wearable_refresh_hero_id == hero_id;
    }
    void GBE_MarkDotaHostLocalWearablesRefreshed(uint64 steam_id, uint32 hero_id)
    {
        GBE_dota_host_local_wearable_refresh_generation = GBE_CurrentDotaLobbyGeneration();
        GBE_dota_host_local_wearable_refresh_steam_id = steam_id;
        GBE_dota_host_local_wearable_refresh_hero_id = hero_id;
    }
    void GBE_ClearDotaHostLocalWearablesRefreshed()
    {
        GBE_dota_host_local_wearable_refresh_generation = 0;
        GBE_dota_host_local_wearable_refresh_steam_id = 0;
        GBE_dota_host_local_wearable_refresh_hero_id = 0;
    }
    bool GBE_HasReplayedDotaPrivateLobbySnapshot() const { return GBE_dota_private_lobby_snapshot_replayed; }
    void GBE_MarkDotaPrivateLobbySnapshotReplayed() { GBE_dota_private_lobby_snapshot_replayed = true; }
    void GBE_ClearDotaPrivateLobbySnapshotReplayed() { GBE_dota_private_lobby_snapshot_replayed = false; }
    uint32 GBE_GetLastDotaLaunchStatePushedGameState() const { return GBE_last_dota_launch_state_pushed_game_state; }
    void GBE_SetLastDotaLaunchStatePushedGameState(uint32 game_state) { GBE_last_dota_launch_state_pushed_game_state = game_state; }
    void GBE_ClearLastDotaLaunchStatePushedGameState() { GBE_last_dota_launch_state_pushed_game_state = 0; }
    void GBE_ClearDotaLobbyRuntimeState()
    {
        const uint64 generation = GBE_local_lobby.generation;
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_GetSharedDotaLobbyStateStore().compare_clear(generation);
        GBE_ClearLastDotaLaunchStatePushedGameState();
    }
    void GBE_ClearSettingsLobbyForDotaSignout()
    {
        const uint64 lobby_id = settings ? settings->get_lobby().ConvertToUint64() : 0ull;
        if (lobby_id != 0) {
            if (g_action_recorder)
                g_action_recorder->record_settings_lobby_clear(lobby_id);
            settings->set_lobby(k_steamIDNil);
        }
    }
    const std::string &GBE_GetLastDotaLaunchPersonaSignature() const { return GBE_last_dota_launch_persona_signature; }
    void GBE_SetLastDotaLaunchPersonaSignature(const std::string &signature) { GBE_last_dota_launch_persona_signature = signature; }
    void GBE_ClearLastDotaLaunchPersonaSignature() { GBE_last_dota_launch_persona_signature.clear(); }
    const gbe::dota_connection::DedupKey &GBE_GetLastDotaDirectConnectCallbackKey() const { return GBE_last_dota_direct_connect_callback_key; }
    void GBE_SetLastDotaDirectConnectCallbackKey(const gbe::dota_connection::DedupKey &key) { GBE_last_dota_direct_connect_callback_key = key; }
    void GBE_ClearLastDotaDirectConnectCallbackKey() { GBE_last_dota_direct_connect_callback_key = {}; }
    void GBE_SetPendingDotaAbandonFinalizeAfterOtherLeftChannel(uint64 lobby_id)
    {
        GBE_pending_dota_abandon_finalize_after_7014 = true;
        GBE_pending_dota_abandon_finalize_lobby_id = lobby_id;
        GBE_pending_dota_abandon_finalize_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
    }
    GBE_DotaDeferredTaskConsumeResult GBE_ConsumePendingDotaAbandonFinalizeAfterOtherLeftChannel()
    {
        const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_dota_abandon_finalize_slot);
        GBE_pending_dota_abandon_finalize_after_7014 = false;
        GBE_pending_dota_abandon_finalize_lobby_id = 0;
        return result;
    }
    void GBE_ClearPendingDotaAbandonFinalizeAfterOtherLeftChannel()
    {
        GBE_pending_dota_abandon_finalize_after_7014 = false;
        GBE_pending_dota_abandon_finalize_lobby_id = 0;
        GBE_pending_dota_abandon_finalize_slot = {};
    }
    void GBE_FinalizeDotaAbandonAfterOtherLeftChannel(uint64 consumed_lobby_id, const char *reason)
    {
        if (is_server || gc_profile != GC_PROFILE_DOTA2 || !GBE_local_lobby.abandon_postgame_active)
            return;
        if (GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.lobby_id != consumed_lobby_id)
            return;
        for (const GBE_DotaAction &action : gbe::dota_lobby_flow::abandon_finalize_action_list(reason)) {
            switch (action.type) {
                case GBE_DotaActionType::GcMemoryReset:
                    if (g_action_recorder)
                        g_action_recorder->record_gc_memory_reset(action.reason.c_str(), action.leave_generic_lobby, action.clear_queued_messages);
                    ResetGCMemory(action.reason.c_str(), action.leave_generic_lobby, action.clear_queued_messages);
                    break;
                default:
                    break;
            }
        }
    }
    void GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(uint64 lobby_id)
    {
        GBE_pending_dota_normal_signout_finalize_after_25 = true;
        GBE_pending_dota_normal_signout_finalize_lobby_id = lobby_id;
        GBE_pending_dota_normal_signout_finalize_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
    }
    GBE_DotaDeferredTaskConsumeResult GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed()
    {
        const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_dota_normal_signout_finalize_slot);
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        return result;
    }
    void GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(uint64 consumed_lobby_id, const char *reason)
    {
        if (gc_profile != GC_PROFILE_DOTA2)
            return;
        const uint64 shared_lobby_id = GBE_GetSharedDotaLobbyStateStore().snapshot().lobby_id;
        const uint64 lobby_id = consumed_lobby_id != 0 ? consumed_lobby_id : shared_lobby_id;
        for (const GBE_DotaAction &action : gbe::dota_lobby_flow::normal_signout_finalize_action_list(
                 lobby_id,
                 std::string(),
                 false,
                 reason ? reason : "unknown")) {
            switch (action.type) {
                case GBE_DotaActionType::SettingsLobbyClear:
                    GBE_ClearSettingsLobbyForDotaSignout();
                    break;
                case GBE_DotaActionType::LaunchPeripheralReset:
                    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
                    break;
                case GBE_DotaActionType::DotaLobbyRuntimeClear:
                    if (g_action_recorder)
                        g_action_recorder->record_dota_lobby_runtime_clear(action.reason.c_str());
                    GBE_ClearDotaLobbyRuntimeState();
                    break;
                case GBE_DotaActionType::RichPresenceClear:
                    GBE_ClearDotaPracticeLobbyLaunchRichPresence();
                    break;
                default:
                    break;
            }
        }
    }
    void GBE_ClearPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed()
    {
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        GBE_pending_dota_normal_signout_finalize_slot = {};
    }
    void GBE_SetPendingResetAfterCacheUnsubscribed(uint64 lobby_id)
    {
        GBE_pending_reset_after_cache_unsubscribed = true;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
        GBE_pending_reset_after_cache_unsubscribed_slot = {lobby_id, GBE_CurrentDotaLobbyGeneration(), true};
    }
    void GBE_ClearPendingResetAfterCacheUnsubscribed(uint64 retained_lobby_id = 0)
    {
        GBE_pending_reset_after_cache_unsubscribed = false;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = retained_lobby_id;
        GBE_pending_reset_after_cache_unsubscribed_slot = {};
    }
    GBE_DotaDeferredTaskConsumeResult GBE_ConsumePendingResetAfterCacheUnsubscribed()
    {
        const auto result = GBE_ConsumeDotaDeferredTask(GBE_pending_reset_after_cache_unsubscribed_slot);
        GBE_pending_reset_after_cache_unsubscribed = false;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
        return result;
    }

    GC_Profile gc_profile{};
    bool GBE_dota_private_lobby_snapshot_replayed{};
    uint32 GBE_last_dota_launch_state_pushed_game_state{};
    std::string GBE_last_dota_launch_persona_signature;
    gbe::dota_connection::DedupKey GBE_last_dota_direct_connect_callback_key;

    // --- Core recording side-effect methods ---

    void push_incoming_now(uint32 msg_type, const std::string &message,
                           bool apply_lobby_state = false,
                           uint32 lobby_state = 0,
                           uint32 lobby_game_state = 0)
    {
        if (g_action_recorder)
            g_action_recorder->record_push_incoming_now(msg_type, message, apply_lobby_state, lobby_state, lobby_game_state);
    }

    void push_incoming(uint32 msg_type, const std::string &message,
                       double delay = 0.1,
                       bool apply_lobby_state = false,
                       uint32 lobby_state = 0,
                       uint32 lobby_game_state = 0)
    {
        GC_Message queued;
        queued.msg_type = msg_type;
        queued.msg_body = message;
        queued.post_in = delay;
        queued.apply_lobby_state = apply_lobby_state;
        queued.lobby_state = lobby_state;
        queued.lobby_game_state = lobby_game_state;
        queued.lobby_id = GBE_local_lobby.lobby_id;
        queued.generation = GBE_CurrentDotaLobbyGeneration();
        queued.validate_lobby_generation = gc_profile == GC_PROFILE_DOTA2 && apply_lobby_state;
        pending_messages.push_back(queued);
        if (g_action_recorder)
            g_action_recorder->record_push_incoming(msg_type, message);
    }
    GBE_DotaDeferredTaskStatus test_deliver_next_pending_message()
    {
        if (pending_messages.empty())
            return GBE_DotaDeferredTaskStatus::Empty;
        const GC_Message queued = pending_messages.front();
        pending_messages.erase(pending_messages.begin());
        if (queued.validate_lobby_generation &&
                (queued.lobby_id != GBE_local_lobby.lobby_id || queued.generation != GBE_CurrentDotaLobbyGeneration()))
            return GBE_DotaDeferredTaskStatus::Stale;
        if (queued.apply_lobby_state) {
            GBE_local_lobby.state = queued.lobby_state;
            GBE_local_lobby.game_state = queued.lobby_game_state;
        }
        incoming_messages.push(queued);
        return GBE_DotaDeferredTaskStatus::Current;
    }

    bool GBE_PushDotaResponse(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state = false, uint32 lobby_state = 0, uint32 lobby_game_state = 0, std::string *out_wrapped_message = nullptr)
    {
        (void)apply_lobby_state; (void)lobby_state; (void)lobby_game_state;
        if (g_action_recorder)
            g_action_recorder->record_dota_response(inner_emsg | protobuf_mask, inner_message, wrapped, outer_session_field_raw, reason);
        if (out_wrapped_message)
            *out_wrapped_message = inner_message;
        return m_test_dota_response_result;
    }

    bool GBE_PushDotaCacheUnsubscribedResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason)
    {
        return GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, message, wrapped, outer_session_field_raw, reason);
    }

    bool GBE_PushDotaOtherLeftChannelResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason)
    {
        return GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, message, wrapped, outer_session_field_raw, reason);
    }

    bool GBE_PushDotaPracticeLobbyResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, std::string *out_wrapped_message = nullptr)
    {
        return GBE_PushDotaResponse(GBE_kDotaPracticeLobbyResponse, message, wrapped, outer_session_field_raw, reason, false, 0u, 0u, out_wrapped_message);
    }

    bool GBE_PushDotaJoinChatChannelResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason)
    {
        return GBE_PushDotaResponse(GBE_kDotaJoinChatChannelResponse, message, wrapped, outer_session_field_raw, reason);
    }

    void save_items_to_file()
    {
        if (g_action_recorder)
            g_action_recorder->record_save_items();
    }

    void GBE_SaveDotaItemsFromExecutor(const char *reason);
    void GBE_ForwardDotaEquipItemsToServerGC(
        Steam_Game_Coordinator *server_gc,
        const std::unordered_set<std::uint64_t> &modified_item_ids,
        const std::string &update_message,
        uint64 cache_version,
        bool unsubscribe_first,
        const char *cache_reason);
    void GBE_BroadcastDotaEquippedItemsToGameServers();
    void GBE_RefreshDotaEquipLobbySnapshot(const char *reason);

    std::string build_protomsg_header(uint32 msg_type,
                                      JobID_t target_job = k_GIDNil,
                                      JobID_t source_job = k_GIDNil)
    {
        // Return a minimal placeholder so AppendToString can chain.
        std::string header;
        header.append(reinterpret_cast<const char *>(&msg_type), sizeof(msg_type));
        header.append(reinterpret_cast<const char *>(&target_job), sizeof(target_job));
        header.append(reinterpret_cast<const char *>(&source_job), sizeof(source_job));
        return header;
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
        return m_test_has_active_server_lobby &&
            (m_test_active_server_lobby_id == 0ull || m_test_active_server_lobby_id == lobby_id);
    }
    bool GBE_HostHasActiveDotaServerLobby(uint64 lobby_id) const
    {
        if (lobby_id == 0)
            return false;
        Steam_Client *steam_client = get_steam_client();
        return steam_client &&
            steam_client->steam_gameserver_game_coordinator &&
            steam_client->steam_gameserver_game_coordinator->GBE_HasActiveServerLobby(lobby_id);
    }

    void callback_items_received(CSteamID, const std::vector<Econ_Item> &) {}
    void callback_items_removed(CSteamID) {}
    void callback_item_deleted(CSteamID, uint64) {}
    void callback_respawn_request(CSteamID steam_id, double delay = 0.1)
    {
        if (g_action_recorder)
            g_action_recorder->record_respawn_request(steam_id.ConvertToUint64(), delay, is_server);
    }
    void callback_client_welcome() {}
    void callback_server_welcome() {}

    // --- Inventory hooks ---

    // Member function used by equip handler - stub records the call.
    void GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_snapshot_refresh(reason);
    }

    // --- Match and launch hooks ---

    bool GBE_HasDotaLaunchServerSetupSync() const { return false; }
    bool GBE_TryAdvanceDotaLaunchToRun(const char *, uint32, uint64, const char *, uint32 = 0u) { return false; }
    void GBE_PublishSharedDotaLobbyState(const char *reason)
    {
        if (g_action_recorder) {
            g_action_recorder->record_lobby_snapshot_refresh(reason);
            g_action_recorder->lifecycle_events.push_back("shared_publish");
        }
    }
    bool GBE_TrySyncDotaLobbyServerIdFromGameServer(const char *) { return false; }
    bool GBE_SendDotaPracticeLobbyDetailsUpdate(bool preserve_server_id, const std::string *message_override, const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_practice_lobby_details_update(preserve_server_id, message_override, reason);
        return true;
    }
    bool GBE_CaptureCurrentDotaLobbyState(const char *, GBE_LocalLobby &snapshot, bool = true) { snapshot = GBE_local_lobby; return GBE_local_lobby.active; }
    bool GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(const char *, const std::vector<GBE_DotaLobbyMemberState> &, uint64, GBE_LocalLobby &snapshot)
    {
        if (m_test_has_next_lobby_capture) {
            GBE_local_lobby = m_test_next_lobby_capture;
            snapshot = GBE_local_lobby;
            m_test_next_lobby_capture = GBE_LocalLobby{};
            m_test_has_next_lobby_capture = false;
            return GBE_local_lobby.active;
        }
        snapshot = GBE_local_lobby;
        return GBE_local_lobby.active;
    }
    void GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *status, const char *lobby_state, bool include_party, bool include_lobby = true)
    {
        if (g_action_recorder)
            g_action_recorder->record_rich_presence_update(status, lobby_state, include_party, include_lobby);
    }
    void GBE_ResetDotaPracticeLobbyLaunchRichPresenceToServerSetup()
    {
        GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);
    }
    void GBE_ClearDotaPracticeLobbyLaunchRichPresence()
    {
        if (g_action_recorder)
            g_action_recorder->record_rich_presence_clear("clear_launch_rich_presence");
    }
    bool ResetGCMemory(
        const char *reason,
        bool = true,
        bool = true,
        gbe::dota_lobby_generation::Boundary generation_boundary = gbe::dota_lobby_generation::Boundary::Reset,
        bool generation_already_advanced = false)
    {
        if (!generation_already_advanced &&
            GBE_AdvanceDotaLobbyGeneration(generation_boundary, reason) == GBE_DotaGenerationAdvanceResult::Exhausted)
            return false;
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_local_lobby.generation = GBE_CurrentDotaLobbyGeneration();
        return true;
    }
    bool GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_LocalLobby &) { return false; }
    void GBE_PublishDotaPracticeLobbyLocalMemberData(const char *reason)
    {
        if (g_action_recorder) {
            g_action_recorder->record_lobby_local_member_data(reason);
            g_action_recorder->lifecycle_events.push_back("local_member_publish");
        }
    }
    void GBE_SyncSettingsLobbyFromGenericLobby(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_settings_lobby_sync(reason);
    }
    void GBE_PublishDotaPracticeLobbyMetadata(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_metadata_publish(reason);
    }
    bool GBE_PublishDotaPracticeLobbySetDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw);
    std::string GBE_GetDotaLobbyOwnerName() const { return GBE_local_lobby.owner_name.empty() ? std::string(settings ? settings->get_local_name() : "") : GBE_local_lobby.owner_name; }
    bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &, std::string &message) { message = "cache_subscribed"; return true; }
    bool GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(const GBE_LocalLobby &, const std::string &, std::string &message) { message = "cache_subscribed"; return true; }
    bool GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &, const std::string &, std::string &message, bool = false) { message = "details_update"; return true; }
    void GBE_RecordDotaLobbyCacheSubscriptionState(const std::string &message, const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_cache_subscription_record(message, reason);
    }
    std::vector<GBE_LocalLobby> GBE_GetDotaGenericLobbySnapshots(const char *) { return {}; }
    bool GBE_ShouldSuppressDotaAbandonedLobby(uint64) const { return false; }
    void GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_rich_presence_update("#DOTA_RP_PLAYING_AS", "RUN", false, true);
        (void)reason;
    }
    bool GBE_TestExecuteDotaLaunchStatePush(
        const gbe::dota_lobby_flow::LaunchStatePushPlan &plan,
        const GBE_LocalLobby &lobby,
        const std::string &response_24,
        const std::string &response_26,
        const char *reason)
    {
        if (plan.skip_reason != gbe::dota_lobby_flow::LaunchStatePushSkipReason::None)
            return false;
        for (const GBE_DotaAction &action : gbe::dota_lobby_flow::launch_state_push_action_list(plan)) {
            switch (action.type) {
                case GBE_DotaActionType::LobbyCacheSubscriptionRecord:
                    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, reason ? reason : "push_launch_state_to_client");
                    break;
                case GBE_DotaActionType::PushIncomingNow:
                    if (action.emsg == (GBE_kDotaCacheSubscribed | protobuf_mask)) {
                        push_incoming_now(action.emsg, response_24);
                    } else if (action.emsg == (GBE_kDotaPracticeLobbyDetailsUpdate | protobuf_mask)) {
                        push_incoming_now(action.emsg, response_26, true, lobby.state, lobby.game_state);
                    }
                    break;
                case GBE_DotaActionType::RichPresenceUpdate:
                    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "push_launch_state_to_client");
                    break;
                case GBE_DotaActionType::LaunchStateGameStateRecord:
                    GBE_SetLastDotaLaunchStatePushedGameState(lobby.game_state);
                    if (g_action_recorder) {
                        RecordedAction a;
                        a.type = GBE_DotaActionType::LaunchStateGameStateRecord;
                        a.item_id = lobby.game_state;
                        a.reason = reason ? reason : "";
                        g_action_recorder->actions.push_back(std::move(a));
                    }
                    break;
                default:
                    break;
            }
        }
        return true;
    }
    bool GBE_MaybeNotifyDotaPracticeLobbyMembersChanged(const char *reason)
    {
        if (is_server || gc_profile != GC_PROFILE_DOTA2)
            return false;
        if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
            return false;
        if (GBE_local_lobby.state > 2u)
            return false;

        const uint32 previous_state = GBE_local_lobby.state;
        GBE_LocalLobby lobby{};
        if (!GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(reason ? reason : "generic_lobby_members_changed", GBE_local_lobby.members, GBE_local_lobby.owner_steam_id, lobby))
            return false;

        const auto postgame_observation = gbe::dota_lobby_state::compute_postgame_observation_decision(
            is_server,
            GBE_HostHasActiveDotaServerLobby(GBE_local_lobby.lobby_id),
            gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
                GBE_local_lobby.match_id != 0ull &&
                GBE_local_lobby.game_state >= 2u &&
                GBE_local_lobby.launch_phase >= 3u,
            previous_state,
            GBE_local_lobby.state,
            GBE_local_lobby.lobby_id);
        if (postgame_observation.run_player_cleanup) {
            const uint64 cleaning_lobby_id = GBE_local_lobby.lobby_id;
            std::string response_26;
            if (GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, true))
                push_incoming_now(26u | protobuf_mask, response_26);
            const std::string response_25 = std::to_string(cleaning_lobby_id);
            for (const GBE_DotaAction &action : gbe::dota_lobby_flow::player_postgame_cleanup_action_list(
                     cleaning_lobby_id,
                     response_25,
                     true,
                     reason ? reason : "generic_lobby_members_changed")) {
                switch (action.type) {
                    case GBE_DotaActionType::RichPresenceClear:
                        GBE_ClearDotaPracticeLobbyLaunchRichPresence();
                        break;
                    case GBE_DotaActionType::LaunchPeripheralReset:
                        GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
                        break;
                    case GBE_DotaActionType::DotaLobbyRuntimeClear:
                        if (g_action_recorder)
                            g_action_recorder->record_dota_lobby_runtime_clear(action.reason.c_str());
                        GBE_ClearDotaLobbyRuntimeState();
                        break;
                    case GBE_DotaActionType::PushIncomingNow:
                        push_incoming_now(action.emsg, action.payload);
                        break;
                    case GBE_DotaActionType::SettingsLobbyClear:
                        GBE_ClearSettingsLobbyForDotaSignout();
                        break;
                    default:
                        break;
                }
            }
            return true;
        }

        if (postgame_observation.skip_for_host_client || postgame_observation.skip_for_arcade_active_match) {
            const bool arcade_runtime_member_change =
                gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
                GBE_local_lobby.match_id != 0ull &&
                GBE_local_lobby.launch_phase >= 3u &&
                GBE_local_lobby.state >= 2u;
            std::string response_26;
            if (postgame_observation.skip_for_host_client &&
                !arcade_runtime_member_change &&
                GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, true))
                push_incoming_now(26u | protobuf_mask, response_26);
            return true;
        }

        return previous_state != GBE_local_lobby.state;
    }
    bool GBE_FindDotaGenericLobbyByDotaLobbyId(uint64 lobby_id, CSteamID &generic_lobby_id, GBE_LocalLobby *matched_lobby, const char *)
    { generic_lobby_id = CSteamID(0xBEEF); if (matched_lobby) { *matched_lobby = GBE_local_lobby; matched_lobby->lobby_id = lobby_id; } return GBE_local_lobby.active && GBE_local_lobby.lobby_id == lobby_id; }
    void GBE_DiscardQueuedDotaLaunchMessagesForAbandon(const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_launch_messages_discarded_for_abandon(reason);
    }
    void GBE_MarkDotaAbandonedLobbySuppressed(uint64 lobby_id, const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_abandoned_lobby_suppressed(lobby_id, reason);
    }
    // --- Lobby/chat hooks ---

    bool GBE_QueueDotaPostGameTeardown(const char *, bool wrapped, const std::string *outer_session_field_raw, bool, bool, bool)
    {
        if (GBE_local_lobby.chat_channel_id != 0)
            GBE_PushDotaResponse(7014u, std::string("postgame_teardown"), wrapped, outer_session_field_raw, "postgame_teardown_7014");
        return true;
    }
    void GBE_ResetDotaPracticeLobbyLaunchPeripheralState()
    {
        if (g_action_recorder)
            g_action_recorder->record_launch_peripheral_reset("launch_peripheral_reset");
    }
    void GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(const char *status, const char *lobby_state, bool include_party, bool include_lobby, const char *reason)
    {
        if (g_action_recorder)
            g_action_recorder->record_launch_persona_state(status, lobby_state, include_party, include_lobby, reason);
    }
    bool GBE_SendDotaCustomGameLaunchSetupFlow(bool, const std::string *, bool, uint64) { return m_test_custom_game_launch_setup_flow_result; }
    void GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(const char *, uint64) {}
    void GBE_LeaveGenericLobby()
    {
        const uint64 lobby_id = GBE_local_lobby.lobby_id;
        if (g_action_recorder)
            g_action_recorder->record_generic_lobby_leave(lobby_id);
        GBE_local_lobby = GBE_LocalLobby{};
    }
    bool GBE_MaybeHandleDotaPracticeLobbyKicked(const char *) { return false; }
    bool GBE_SetDotaLobbyMemberRuntimeState(uint64 steam_id, bool connected, uint32 hero_id, bool has_hero_id)
    {
        if (g_action_recorder)
            g_action_recorder->record_runtime_state(steam_id, connected, hero_id, has_hero_id);
        if (!m_test_member_runtime_result)
            return false;
        if (steam_id == GBE_local_lobby.owner_steam_id) {
            GBE_local_lobby.owner_connected = connected;
            if (has_hero_id)
                GBE_local_lobby.owner_hero_id = hero_id;
        }
        return true;
    }
    bool GBE_ShouldHoldDotaLanLaunchForRemoteMembers(uint32 next_game_state, uint32 *remote_count_out, uint32 *connected_remote_count_out) const
    {
        (void)next_game_state;
        if (remote_count_out) *remote_count_out = 0u;
        if (connected_remote_count_out) *connected_remote_count_out = 0u;
        return false;
    }
    bool GBE_TryQueueDotaPrelaunch021(const char *note, uint32 trigger_emsg, uint64 source_job)
    {
        (void)trigger_emsg;
        push_incoming_now(21u | protobuf_mask, build_protomsg_header(21u, k_GIDNil, source_job) + std::string(note ? note : "prelaunch"));
        return true;
    }
    bool GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(const char *note, uint32 trigger_emsg, uint64 source_job, uint32 next_state, uint32 next_game_state, double delay = 0.0)
    {
        (void)trigger_emsg; (void)delay;
        GBE_local_lobby.state = next_state;
        GBE_local_lobby.game_state = next_game_state;
        if (g_action_recorder)
            g_action_recorder->lifecycle_events.push_back("runtime_update");
        push_incoming_now(26u | protobuf_mask, build_protomsg_header(26u, k_GIDNil, source_job) + std::string(note ? note : "runtime_update"));
        return m_test_runtime_update_result;
    }
    void GBE_MarkDotaLaunchPhase(uint32 phase, const char *, bool = true)
    {
        GBE_local_lobby.launch_phase = phase;
        if (g_action_recorder)
            g_action_recorder->lifecycle_events.push_back("launch_phase");
    }
    uint64 GBE_GetDotaLobbyOwnerSteamId() const { return GBE_local_lobby.owner_steam_id; }
    const std::vector<Econ_Item> &get_items() { return items; }
    void GBE_MirrorDotaEquippedItemsForUser(CSteamID steam_id, const std::vector<Econ_Item> &source_items, const char *)
    {
        std::vector<Econ_Item> mirrored_items;
        for (const Econ_Item &item : source_items) {
            if (!item.equip_states.empty())
                mirrored_items.push_back(item);
        }
        all_user_items[steam_id] = mirrored_items;
    }
    std::string serialize_item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id) { return item_to_gcprotobuf(item, steam_id); }

    // --- Test-only controls ---
    void test_set_active_server_lobby(bool v)
    {
        m_test_has_active_server_lobby = v;
        if (!v)
            m_test_active_server_lobby_id = 0ull;
    }
    void test_set_active_server_lobby_id(uint64 lobby_id)
    {
        m_test_has_active_server_lobby = lobby_id != 0ull;
        m_test_active_server_lobby_id = lobby_id;
    }
    void test_set_next_lobby_capture(const GBE_LocalLobby &lobby)
    {
        m_test_next_lobby_capture = lobby;
        m_test_has_next_lobby_capture = true;
    }
    void test_clear_next_lobby_capture()
    {
        m_test_next_lobby_capture = GBE_LocalLobby{};
        m_test_has_next_lobby_capture = false;
    }
    void test_set_custom_game_launch_setup_flow_result(bool v) { m_test_custom_game_launch_setup_flow_result = v; }
    void test_set_dota_response_result(bool v) { m_test_dota_response_result = v; }
    void test_set_member_runtime_result(bool v) { m_test_member_runtime_result = v; }
    void test_set_runtime_update_result(bool v) { m_test_runtime_update_result = v; }

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

    // Lobby domain
    bool GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyListRequest(bool has_request_job, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaCustomLobbyListRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaFriendPracticeLobbyListRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response = true);
    bool GBE_HandleDotaInviteToLobbyRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaLobbyInviteResponseRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaFriendLobbyInviteMessage(Common_Message *msg);
    bool GBE_HandleDotaNetworkLobbyInviteMessage(Common_Message *msg);
    bool GBE_HandleDotaAbandonCurrentGameRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaGameMatchSignOutRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyLaunchRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id);
    bool GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaPracticeLobbyKickRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw);
    bool GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw);

    // Match domain
    bool GBE_HandleDotaDirect7034Request(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    void GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(uint64 owner_steam_id, uint64 source_job);
    void GBE_HandleDotaDirect7034DisconnectedPlayers(const std::vector<gbe::proto_wire::Dota7034DisconnectedPlayer> &disconnected_players, uint64 source_job);
    void GBE_HandleDotaDirect7034RuntimeUpdates(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RuntimeRequest &request, bool has_source_job, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTime(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RuntimeRequest &request, bool has_source_job, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTimeFallback(uint32 request_emsg, const uint8 *body, size_t body_size, const gbe::proto_wire::Dota7034RuntimeRequest &request, bool has_source_job, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034StrategyTimePreserve(uint32 request_emsg, uint64 source_job, bool &queued_runtime_lobby_update);
    bool GBE_HandleDotaDirect7034Response(uint32 request_emsg, const gbe::proto_wire::Dota7034RequestShape &shape, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job);
    void GBE_HandleDotaDirect7034LaunchPoll(uint32 request_emsg, uint64 source_job, bool &queued_runtime_lobby_update);
    void GBE_HandleDotaDirect7034WaitForPlayers(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, bool has_teams, uint64 source_job, bool &queued_runtime_lobby_update);
    bool GBE_HandleDotaCustomGameLifecycleRequest(const gbe::dota_gc_router::DotaGcRequestContext &context);
    gbe::dota_lifecycle::ExecutionResult GBE_ExecuteDotaLifecycleActions(
        const GBE_DotaActionList &actions,
        const gbe::dota_lifecycle::ExecutionOptions &options = {});
    gbe::dota_lifecycle::ExecutionResult GBE_ExecuteDotaLifecycleEffects(
        const GBE_DotaActionList &actions,
        const gbe::dota_lifecycle::ExecutionOptions &options);
    bool GBE_ExecuteDotaCustomGameLifecycleTransition(const gbe::dota_custom_game_lifecycle::ExecutionContext &context);

    // Other handlers declared in the real header but not defined in the
    // inventory TU - we don't need them here. If a future domain test
    // compiles a different handler .cpp, add the corresponding declarations
    // to a domain-specific stub extension.

private:
    bool m_test_has_active_server_lobby = false;
    uint64 m_test_active_server_lobby_id = 0;
    bool m_test_has_next_lobby_capture = false;
    GBE_LocalLobby m_test_next_lobby_capture{};
    bool m_test_custom_game_launch_setup_flow_result = false;
    bool m_test_dota_response_result = true;
    bool m_test_member_runtime_result = true;
    bool m_test_runtime_update_result = true;
};

// =====================================================================
// Platform service stubs: Steam_Friends and Steam_Matchmaking
// =====================================================================

class Steam_Friends
{
public:
    const char *GetFriendPersonaName(const CSteamID &steam_id) { (void)steam_id; return ""; }
};

// =====================================================================
// =====================================================================

class Steam_Matchmaking
{
public:
    const char *GetLobbyMemberData(const CSteamID &lobby, const CSteamID &member, const char *key)
    { (void)lobby; (void)member; (void)key; return ""; }
    void RefreshLobbyCallbacksForDota() {}
    CSteamID CreateLobbyImmediate(ELobbyType type, int max_members)
    { (void)type; (void)max_members; return CSteamID(0xBEEF); }
    CSteamID FindLobbyByDotaLobbyIdForInvite(uint64 lobby_id)
    { (void)lobby_id; return CSteamID(0xBEEF); }
    CSteamID FindLobbyByDotaLobbyIdForInvite(uint64 lobby_id, const char *marker_key, const char *marker_value, const char *lobby_id_key)
    { (void)marker_key; (void)marker_value; (void)lobby_id_key; return FindLobbyByDotaLobbyIdForInvite(lobby_id); }
    void JoinLobby(CSteamID lobby_id) { (void)lobby_id; }
    bool SendLobbySnapshotToUserForDotaInvite(CSteamID lobby_id, CSteamID user_id)
    { (void)lobby_id; (void)user_id; return true; }
    bool InviteUserToLobby(CSteamID lobby_id, CSteamID user_id)
    { (void)lobby_id; (void)user_id; return true; }
    const char *GetLobbyData(CSteamID lobby_id, const char *key)
    { (void)lobby_id; (void)key; return ""; }
    std::vector<CSteamID> GetLobbyMemberListSnapshot(CSteamID lobby_id)
    { (void)lobby_id; return {}; }
    bool KickLobbyMemberForDota(CSteamID lobby_id, CSteamID member_id)
    {
        if (g_action_recorder)
            g_action_recorder->record_lobby_kick(lobby_id.ConvertToUint64(), member_id.ConvertToUint64());
        return true;
    }
};

#endif // GBE_DOTA_HANDLER_TEST_STUBS_H

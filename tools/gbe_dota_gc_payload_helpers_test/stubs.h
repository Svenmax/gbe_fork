/* Comprehensive test stubs for offline testing of gbe_dota_gc_payload_helpers.cpp
 *
 * This file provides minimal type definitions and stubs for the Steam SDK
 * types that payload_helpers.cpp depends on, allowing the TU to compile
 * without the full Steam SDK / steam_api(64).dll dependency chain.
 *
 * Strategy:
 * - Include real lightweight GBE headers that provide type definitions
 * - Provide stubs only for Steam SDK types (CSteamID, Settings, etc.)
 * - Provide stubs for protobuf message types
 * - Do NOT define GBE_GC_DebugLog (it's defined in the TU itself)
 */

#ifndef GBE_GC_PAYLOAD_HELPERS_TEST_STUBS_H
#define GBE_GC_PAYLOAD_HELPERS_TEST_STUBS_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <atomic>

// =====================================================================
// Include real lightweight GBE headers for type definitions
// These headers only depend on standard library types, not Steam SDK
// =====================================================================

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

static const PublishedFileId_t k_PublishedFileIdInvalid = 0;

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
    Settings() {}

    CSteamID get_lobby() const { return CSteamID(); }
    void set_lobby(const CSteamID &lobby_id) { (void)lobby_id; }

    const std::string &get_local_name() const { return m_name; }
    const std::string &get_language() const { return m_language; }

    const std::vector<PublishedFileId_t> &modSet() const { return m_mods; }
    bool has_mod(PublishedFileId_t mod_id) const { (void)mod_id; return false; }
    bool isModInstalled(PublishedFileId_t mod_id) const { (void)mod_id; return false; }
    Mod_entry getMod(PublishedFileId_t mod_id) const { (void)mod_id; return Mod_entry{}; }

    std::string m_name;
    std::string m_language;
    std::vector<PublishedFileId_t> m_mods;
};

// =====================================================================
// Steam_Game_Coordinator stub
// =====================================================================

class Steam_Game_Coordinator
{
public:
    void push_incoming_message(uint32_t msg_type, const std::string &msg) { (void)msg_type; (void)msg; }
    bool GBE_IsServerGC() const { return true; }
    std::string serialize_item_to_gcprotobuf(const Econ_Item &item, const CSteamID &steam_id)
        { (void)item; (void)steam_id; return {}; }
    bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64_t local_steam_id, GBE_DotaReconnectContext *out)
        { (void)local_steam_id; (void)out; return false; }
};

// =====================================================================
// Steam_Friends stub
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

// =====================================================================
// Steam_Client stub
// =====================================================================

class Steam_Client
{
public:
    Steam_Friends *steam_friends{};
    Steam_Game_Coordinator *steam_game_coordinator{};
    Steam_Matchmaking *steam_matchmaking{};

    Steam_Client() {}
};

inline Steam_Client *get_steam_client() { return nullptr; }

// =====================================================================
// GC message types
// =====================================================================

struct GC_Message
{
    uint32_t msg_type{};
    std::string msg_body;
    double created{};
    uint64_t sequence{};
    bool apply_lobby_state{};
    int lobby_state{};
    int lobby_game_state{};
    double post_in{};
};

// =====================================================================
// Protobuf namespace stubs
// =====================================================================

namespace google { namespace protobuf {
    class Message {};
    class MessageLite {};
}}

// Note: GBE_GC_DebugLog is NOT defined here.
// It is declared in gbe_dota_gc_internal.h and defined in the TU under test.

#endif // GBE_GC_PAYLOAD_HELPERS_TEST_STUBS_H

#ifndef GBE_DOTA_TYPES_H
#define GBE_DOTA_TYPES_H

#include <cstdint>
#include <string>

struct GBE_DotaCustomGameDetails
{
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

struct GBE_DotaLobbyMemberState
{
    std::uint64_t steam_id{};
    std::uint32_t account_id{};
    std::uint32_t team{};
    std::uint32_t slot{};
    std::uint32_t hero_id{};
    bool connected{};
    std::uint32_t leaver_status{};
};

struct GBE_DotaChatMemberState
{
    std::uint64_t steam_id{};
    std::string name;
};

struct GBE_DotaLobbyMemberPublishData
{
    std::uint32_t team{};
    std::uint32_t slot{};
    std::uint32_t hero_id{};
    bool connected{};
    std::string name;
};

struct GBE_DotaLobbyMetadataPublishData
{
    std::string room_name;
    std::string owner_name;
    std::uint64_t server_id{};
    std::string connect;
};

struct GBE_DotaLobbyOptionsPublishData
{
    std::string game_mode;
    std::string server_region;
    std::string lan_host_ping_location;
    std::string pass_key;
    std::string allow_cheats;
    std::string fill_with_bots;
    std::string allow_spectating;
    std::string visibility;
    std::string bot_difficulty_radiant;
    std::string bot_difficulty_dire;
    std::string bot_radiant;
    std::string bot_dire;
};

struct GBE_DotaLobbyScalarPublishData
{
    std::string dota_lobby_id;
    std::string owner_steam_id;
    std::string owner_account_id;
    std::string state;
    std::string game_state;
    std::string match_id;
    std::string game_start_time;
    std::string tv_secret_code;
    std::string tv_port;
};

struct GBE_DotaGenericLobbySnapshotScalarData
{
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
    std::uint32_t state{};
    std::uint32_t game_state{};
    std::uint64_t match_id{};
    std::uint64_t server_id{};
    std::string connect;
    std::uint32_t game_start_time{};
    std::string pass_key;
    std::uint64_t tv_secret_code{};
    std::uint32_t tv_port{};
};

struct GBE_DotaGenericLobbyMemberSnapshotInput
{
    std::uint64_t member_steam_id{};
    std::uint32_t member_account_id{};
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::uint32_t owner_team{};
    std::uint32_t owner_slot{};
    std::uint32_t owner_hero_id{};
    bool owner_connected{};
    std::string member_team_raw;
    std::string member_slot_raw;
    std::string member_hero_raw;
    std::string member_connected_raw;
    std::uint32_t player_pool_team{};
};

struct GBE_DotaLobbyOwnerSnapshotData
{
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    bool should_publish_local_owner{};
};

struct GBE_DotaLobbyOwnerPublishData
{
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    bool should_publish_local_owner{};
};

struct GBE_DotaLobbyMemberSnapshotData
{
    GBE_DotaLobbyMemberState member;
    std::uint32_t owner_team{};
    std::uint32_t owner_slot{};
    std::uint32_t owner_hero_id{};
    bool owner_connected{};
};

struct GBE_DotaJoinableCustomLobbyItemData
{
    std::uint32_t member_count{};
    std::uint32_t max_players{};
    std::uint32_t min_players{};
    std::uint32_t leader_account_id{};
    std::string leader_name;
    std::string room_name;
    std::string custom_map_name;
    std::uint32_t lobby_creation_time{};
};

struct GBE_DotaCustomGamePublishData
{
    std::string mode;
    std::string map_name;
    std::string difficulty;
    std::string game_id;
    std::string min_players;
    std::string max_players;
    std::string crc;
    std::string timestamp;
    std::string penalties;
};

struct GBE_DotaAuthoritativeLobbyPayloadData
{
    std::uint64_t server_id{};
    std::string connect;
};

#endif // GBE_DOTA_TYPES_H

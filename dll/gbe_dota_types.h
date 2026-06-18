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

#endif // GBE_DOTA_TYPES_H

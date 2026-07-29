#ifndef __INCLUDED_GBE_DOTA_CONNECTION_DEDUP_KEY_H__
#define __INCLUDED_GBE_DOTA_CONNECTION_DEDUP_KEY_H__

#include "gbe_dota_lobby_generation.h"

#include <cstdint>
#include <string>

namespace gbe::dota_connection {

struct DedupKey {
    gbe::dota_lobby_generation::Generation generation;
    std::uint64_t server_id{};
    std::string endpoint;
};

inline bool operator==(const DedupKey &lhs, const DedupKey &rhs)
{
    return lhs.generation == rhs.generation
        && lhs.server_id == rhs.server_id
        && lhs.endpoint == rhs.endpoint;
}

inline bool operator!=(const DedupKey &lhs, const DedupKey &rhs)
{
    return !(lhs == rhs);
}

} // namespace gbe::dota_connection

#endif // __INCLUDED_GBE_DOTA_CONNECTION_DEDUP_KEY_H__

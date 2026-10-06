#ifndef GBE_DOTA_CHAT_FLOW_H
#define GBE_DOTA_CHAT_FLOW_H

#include "gbe_dota_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gbe::dota_lobby_flow {

std::string resolve_chat_member_display_name(
    std::uint64_t member_steam_id,
    std::uint64_t local_steam_id,
    const std::string &local_name,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::string &generic_member_name,
    const std::string &friend_name,
    const std::string &fallback_name);

std::vector<GBE_DotaChatMemberState> compose_join_chat_channel_members(
    std::uint64_t local_steam_id,
    const std::string &local_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::vector<GBE_DotaChatMemberState> &resolved_remote_names);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_CHAT_FLOW_H

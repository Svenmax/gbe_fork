#include "gbe_dota_chat_flow.h"

namespace gbe::dota_lobby_flow {

std::string resolve_chat_member_display_name(
    std::uint64_t member_steam_id,
    std::uint64_t local_steam_id,
    const std::string &local_name,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::string &generic_member_name,
    const std::string &friend_name,
    const std::string &fallback_name)
{
    if (member_steam_id == local_steam_id)
        return local_name;
    if (member_steam_id == owner_steam_id && !owner_name.empty())
        return owner_name;
    if (!generic_member_name.empty())
        return generic_member_name;
    if (!friend_name.empty() && friend_name != "Unknown User")
        return friend_name;
    if (!fallback_name.empty())
        return fallback_name;
    return std::to_string(member_steam_id);
}

std::vector<GBE_DotaChatMemberState> compose_join_chat_channel_members(
    std::uint64_t local_steam_id,
    const std::string &local_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    std::uint64_t owner_steam_id,
    const std::string &owner_name,
    const std::vector<GBE_DotaChatMemberState> &resolved_remote_names)
{
    auto find_resolved_name = [&resolved_remote_names](std::uint64_t steam_id) -> std::string {
        for (const GBE_DotaChatMemberState &resolved : resolved_remote_names) {
            if (resolved.steam_id == steam_id)
                return resolved.name;
        }
        return std::string();
    };

    std::vector<GBE_DotaChatMemberState> chat_members;
    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == local_steam_id) {
            chat_members.push_back({ channel_member.steam_id, local_name });
            break;
        }
    }
    chat_members.push_back({ local_steam_id, local_name });

    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == local_steam_id)
            continue;
        chat_members.push_back({
            channel_member.steam_id,
            resolve_chat_member_display_name(
                channel_member.steam_id,
                local_steam_id,
                local_name,
                owner_steam_id,
                owner_name,
                find_resolved_name(channel_member.steam_id),
                std::string(),
                std::string()) });
    }

    return chat_members;
}

} // namespace gbe::dota_lobby_flow

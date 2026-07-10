#ifndef GBE_DOTA_HANDLER_REGISTRY_H
#define GBE_DOTA_HANDLER_REGISTRY_H

#include "gbe_dota_gc_router.h"

#include <cstddef>
#include <cstdint>
#include <string>

class Steam_Game_Coordinator;

namespace gbe::dota_handler_registry {

enum class RequestMode : std::uint8_t {
    None = 0u,
    Direct = 1u,
    Wrapped = 2u,
    DirectAndWrapped = 3u,
};

enum class SessionPolicy : std::uint8_t {
    Ignore,
    ForwardWrappedSession,
};

enum class LifecycleClass : std::uint8_t {
    None,
    LobbyRead,
    LobbyMutation,
    LobbyLifecycle,
};

enum class HandlerId : std::uint8_t {
    Unknown,
    JoinChatChannel,
    PracticeLobbyCreate,
    LobbyList,
    CustomLobbyList,
    FriendPracticeLobbyList,
    InviteToLobby,
    LobbyInviteResponse,
    PracticeLobbyJoin,
    PracticeLobbyLeave,
    PracticeLobbyLaunch,
    PracticeLobbySetDetails,
    PracticeLobbySetTeamSlot,
    PracticeLobbyKick,
    PracticeLobbyJoinBroadcastChannel,
    LobbyUpdateBroadcastChannelInfo,
    PracticeLobbyCloseBroadcastChannel,
    CustomGameReadyUp,
    CustomGameStartedLoading,
    CustomGameFinishedLoading,
    Notifications7427,
    UploadRate,
    Rank,
    ProfileCard,
    LookupAccountName,
    EmoticonData,
    ConductScorecard,
    CoachingSummary,
};

using Adapter = bool (*)(
    Steam_Game_Coordinator *coordinator,
    const dota_gc_router::DotaGcRequestContext &context,
    const std::string *outer_session_field_raw);

struct Entry {
    std::uint32_t message_id{};
    RequestMode modes{RequestMode::None};
    SessionPolicy session_policy{SessionPolicy::Ignore};
    LifecycleClass lifecycle{LifecycleClass::None};
    Adapter adapter{};
    HandlerId handler{HandlerId::Unknown};
    const char *fixture{};
};

struct View {
    const Entry *entries{};
    std::size_t size{};
};

constexpr bool supports_mode(RequestMode modes, dota_gc_router::DotaGcRequestPath path)
{
    const auto mask = static_cast<std::uint8_t>(modes);
    switch (path) {
    case dota_gc_router::DotaGcRequestPath::Direct:
        return (mask & static_cast<std::uint8_t>(RequestMode::Direct)) != 0u;
    case dota_gc_router::DotaGcRequestPath::Wrapped:
        return (mask & static_cast<std::uint8_t>(RequestMode::Wrapped)) != 0u;
    case dota_gc_router::DotaGcRequestPath::Unknown:
        return false;
    }
    return false;
}

constexpr bool forwards_wrapped_session(SessionPolicy policy)
{
    return policy == SessionPolicy::ForwardWrappedSession;
}

constexpr bool is_high_risk(LifecycleClass lifecycle)
{
    return lifecycle == LifecycleClass::LobbyMutation || lifecycle == LifecycleClass::LobbyLifecycle;
}

constexpr bool has_test_fixture(const Entry &entry)
{
    return entry.fixture != nullptr && entry.fixture[0] != '\0';
}

constexpr const Entry *find_entry(
    const Entry *entries,
    std::size_t entry_count,
    std::uint32_t message_id,
    dota_gc_router::DotaGcRequestPath path)
{
    for (std::size_t index = 0; index < entry_count; ++index) {
        const Entry &entry = entries[index];
        if (entry.message_id == message_id && supports_mode(entry.modes, path))
            return &entry;
    }
    return nullptr;
}

constexpr bool has_unique_message_ids_per_mode(const Entry *entries, std::size_t entry_count)
{
    for (std::size_t left = 0; left < entry_count; ++left) {
        for (std::size_t right = left + 1; right < entry_count; ++right) {
            if (entries[left].message_id != entries[right].message_id)
                continue;
            if (supports_mode(entries[left].modes, dota_gc_router::DotaGcRequestPath::Direct) &&
                supports_mode(entries[right].modes, dota_gc_router::DotaGcRequestPath::Direct))
                return false;
            if (supports_mode(entries[left].modes, dota_gc_router::DotaGcRequestPath::Wrapped) &&
                supports_mode(entries[right].modes, dota_gc_router::DotaGcRequestPath::Wrapped))
                return false;
        }
    }
    return true;
}

constexpr bool all_high_risk_entries_have_fixture(const Entry *entries, std::size_t entry_count)
{
    for (std::size_t index = 0; index < entry_count; ++index) {
        if (is_high_risk(entries[index].lifecycle) && !has_test_fixture(entries[index]))
            return false;
    }
    return true;
}

} // namespace gbe::dota_handler_registry

#endif

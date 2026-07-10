#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

#include "gbe_dota_protocol_constants.h"

#include <cstdint>

namespace gbe::dota_lifecycle_state_machine {

enum class State : std::uint8_t {
    Idle,
    Created,
    Joined,
    Setup,
    Loading,
    Loaded,
    Running,
    PostGame,
};

enum class EventKind : std::uint8_t {
    Create,
    Join,
    Setup,
    Loading,
    Loaded,
    Run,
    PostGame,
    Leave,
    Abandon,
    Reset,
};

enum class EventSource : std::uint8_t {
    Internal,
    Direct,
    Wrapped,
};

struct Event {
    EventKind kind{EventKind::Reset};
    EventSource source{EventSource::Internal};
    std::uint32_t message_id{};
};

struct EventMapping {
    bool mapped{};
    Event event{};
};

constexpr EventSource transport_source(bool wrapped)
{
    return wrapped ? EventSource::Wrapped : EventSource::Direct;
}

constexpr EventMapping event_from_message(std::uint32_t message_id, bool wrapped)
{
    const EventSource source = transport_source(wrapped);
    switch (message_id) {
        case GBE_kDotaPracticeLobbyCreate:
            return { true, { EventKind::Create, source, message_id } };
        case GBE_kDotaPracticeLobbyJoin:
            return { true, { EventKind::Join, source, message_id } };
        case GBE_kDotaPracticeLobbyLaunch:
            return { true, { EventKind::Setup, source, message_id } };
        case 8052u:
            return { true, { EventKind::Loading, source, message_id } };
        case 8053u:
            return { true, { EventKind::Loaded, source, message_id } };
        case 7070u:
            return { true, { EventKind::Run, source, message_id } };
        case GBE_kDotaGameMatchSignOut:
            return { true, { EventKind::PostGame, source, message_id } };
        case GBE_kDotaPracticeLobbyLeave:
            return { true, { EventKind::Leave, source, message_id } };
        case GBE_kDotaAbandonCurrentGame:
            return { true, { EventKind::Abandon, source, message_id } };
        default:
            return {};
    }
}

constexpr Event reset_event()
{
    return { EventKind::Reset, EventSource::Internal, 0u };
}

} // namespace gbe::dota_lifecycle_state_machine

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

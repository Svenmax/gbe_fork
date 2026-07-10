#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

#include "gbe_dota_protocol_constants.h"

#include <array>
#include <cstddef>
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

enum class DecisionStatus : std::uint8_t {
    Accepted,
    Rejected,
};

enum class DecisionReason : std::uint8_t {
    TransitionApplied,
    AlreadyInState,
    InvalidTransition,
};

enum class EffectKind : std::uint8_t {
    StateChanged,
};

struct Effect {
    EffectKind kind{EffectKind::StateChanged};
    State from{State::Idle};
    State to{State::Idle};
};

struct EffectList {
    std::array<Effect, 1> values{};
    std::size_t count{};

    constexpr bool empty() const
    {
        return count == 0u;
    }
};

struct TransitionResult {
    State state{State::Idle};
    EffectList effects{};
    DecisionReason reason{DecisionReason::InvalidTransition};
    DecisionStatus status{DecisionStatus::Rejected};

    constexpr bool accepted() const
    {
        return status == DecisionStatus::Accepted;
    }
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

constexpr TransitionResult accepted_transition(State from, State to)
{
    return {
        to,
        { { Effect{ EffectKind::StateChanged, from, to } }, 1u },
        DecisionReason::TransitionApplied,
        DecisionStatus::Accepted,
    };
}

constexpr TransitionResult rejected_transition(State state, DecisionReason reason)
{
    return { state, {}, reason, DecisionStatus::Rejected };
}

constexpr TransitionResult transition(State state, const Event &event)
{
    State target = state;
    bool valid = false;

    switch (event.kind) {
        case EventKind::Create:
            target = State::Created;
            valid = state == State::Idle;
            break;
        case EventKind::Join:
            target = State::Joined;
            valid = state == State::Idle;
            break;
        case EventKind::Setup:
            target = State::Setup;
            valid = state == State::Created || state == State::Joined;
            break;
        case EventKind::Loading:
            target = State::Loading;
            valid = state == State::Setup;
            break;
        case EventKind::Loaded:
            target = State::Loaded;
            valid = state == State::Loading;
            break;
        case EventKind::Run:
            target = State::Running;
            valid = state == State::Loaded;
            break;
        case EventKind::PostGame:
            target = State::PostGame;
            valid = state != State::Idle && state != State::PostGame;
            break;
        case EventKind::Leave:
        case EventKind::Abandon:
            target = State::PostGame;
            valid = state != State::Idle && state != State::PostGame;
            break;
        case EventKind::Reset:
            target = State::Idle;
            valid = state == State::PostGame;
            break;
    }

    if (valid)
        return accepted_transition(state, target);
    if (state == target)
        return rejected_transition(state, DecisionReason::AlreadyInState);
    return rejected_transition(state, DecisionReason::InvalidTransition);
}

} // namespace gbe::dota_lifecycle_state_machine

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

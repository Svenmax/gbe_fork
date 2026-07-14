#ifndef __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__
#define __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

#include "gbe_dota_protocol_constants.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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
    Count,
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
    Recover,
    Reconnect,
    RuntimePoll,
    Count,
};

inline constexpr std::array<State, static_cast<std::size_t>(State::Count)> all_states{{
    State::Idle,
    State::Created,
    State::Joined,
    State::Setup,
    State::Loading,
    State::Loaded,
    State::Running,
    State::PostGame,
}};

inline constexpr std::array<EventKind, static_cast<std::size_t>(EventKind::Count)> all_event_kinds{{
    EventKind::Create,
    EventKind::Join,
    EventKind::Setup,
    EventKind::Loading,
    EventKind::Loaded,
    EventKind::Run,
    EventKind::PostGame,
    EventKind::Leave,
    EventKind::Abandon,
    EventKind::Reset,
    EventKind::Recover,
    EventKind::Reconnect,
    EventKind::RuntimePoll,
}};

enum class EventSource : std::uint8_t {
    Internal,
    Direct,
    Wrapped,
};

struct Event {
    EventKind kind{EventKind::Reset};
    EventSource source{EventSource::Internal};
    std::uint32_t message_id{};
    std::uint64_t generation{};
    std::uint64_t server_id{};
    std::uint64_t endpoint_key{};
};

struct EventMapping {
    bool mapped{};
    Event event{};
};

enum class DecisionStatus : std::uint8_t {
    Accepted,
    Rejected,
};

enum class TransitionDisposition : std::uint8_t {
    Accepted,
    Rejected,
    Ignored,
};

enum class DecisionReason : std::uint8_t {
    TransitionApplied,
    AlreadyInState,
    InvalidTransition,
    StaleGeneration,
    GenerationExhausted,
    ReconnectQueued,
    ReconnectAlreadyQueued,
    TerminalState,
    RequestIgnored,
};

enum class EffectKind : std::uint8_t {
    StateChanged,
    GenerationAdvanced,
    ReconnectQueued,
    PracticeLobbyDetailsRequested,
    RuntimeMemberUpdateRequested,
    RuntimeGameStateUpdateRequested,
    // Named teardown path gates (C3–C6). Call sites check the path token for event+stage.
    TeardownLeaveInitiateRequested,
    TeardownLeaveFinalizeRequested,
    TeardownAbandonInitiateRequested,
    TeardownAbandonFinalizeRequested,
    TeardownPostGameInitiateRequested,
    TeardownPostGameFinalizeRequested,
    TeardownResetRequested,
    // Named custom-game gate (C5). 7070/8052/8053 check this token; compute_* + Execute stay in handler.
    CustomGameLifecycleActionsRequested,
};

struct Effect {
    EffectKind kind{EffectKind::StateChanged};
    State from{State::Idle};
    State to{State::Idle};
    std::uint64_t generation{};
};

struct EffectList {
    std::array<Effect, 2> values{};
    std::size_t count{};

    constexpr bool empty() const
    {
        return count == 0u;
    }

    constexpr bool contains(EffectKind kind) const
    {
        for (std::size_t index = 0u; index < count; ++index) {
            if (values[index].kind == kind)
                return true;
        }
        return false;
    }
};

struct ReconnectKey {
    std::uint64_t generation{};
    std::uint64_t server_id{};
    std::uint64_t endpoint_key{};
};

constexpr bool operator==(ReconnectKey lhs, ReconnectKey rhs)
{
    return lhs.generation == rhs.generation &&
        lhs.server_id == rhs.server_id &&
        lhs.endpoint_key == rhs.endpoint_key;
}

struct MachineState {
    State lifecycle{State::Idle};
    std::uint64_t generation{};
    ReconnectKey reconnect_key{};
    bool reconnect_queued{};
};

struct MachineTransitionResult {
    MachineState state{};
    EffectList effects{};
    DecisionReason reason{DecisionReason::InvalidTransition};
    DecisionStatus status{DecisionStatus::Rejected};

    constexpr bool accepted() const
    {
        return status == DecisionStatus::Accepted;
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
            return { true, { EventKind::Create, source, message_id, 0u, 0u, 0u } };
        case GBE_kDotaPracticeLobbyJoin:
            return { true, { EventKind::Join, source, message_id, 0u, 0u, 0u } };
        case GBE_kDotaPracticeLobbyLaunch:
            return { true, { EventKind::Setup, source, message_id, 0u, 0u, 0u } };
        case 8052u:
            return { true, { EventKind::Loading, source, message_id, 0u, 0u, 0u } };
        case 8053u:
            return { true, { EventKind::Loaded, source, message_id, 0u, 0u, 0u } };
        case 7070u:
            return { true, { EventKind::Run, source, message_id, 0u, 0u, 0u } };
        case GBE_kDotaGameMatchSignOut:
            return { true, { EventKind::PostGame, source, message_id, 0u, 0u, 0u } };
        case GBE_kDotaPracticeLobbyLeave:
            return { true, { EventKind::Leave, source, message_id, 0u, 0u, 0u } };
        case GBE_kDotaAbandonCurrentGame:
            return { true, { EventKind::Abandon, source, message_id, 0u, 0u, 0u } };
        default:
            return {};
    }
}

constexpr Event reset_event()
{
    return { EventKind::Reset, EventSource::Internal, 0u, 0u, 0u, 0u };
}

constexpr Event recover_event()
{
    return { EventKind::Recover, EventSource::Internal, 0u, 0u, 0u, 0u };
}

constexpr Event reconnect_event(
    std::uint64_t generation,
    std::uint64_t server_id,
    std::uint64_t endpoint_key)
{
    return { EventKind::Reconnect, EventSource::Internal, 0u, generation, server_id, endpoint_key };
}

constexpr Event runtime_poll_event(std::uint64_t generation, std::uint32_t message_id)
{
    return { EventKind::RuntimePoll, EventSource::Direct, message_id, generation, 0u, 0u };
}

constexpr TransitionResult accepted_transition(State from, State to)
{
    return {
        to,
        { { Effect{ EffectKind::StateChanged, from, to, 0u } }, 1u },
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
        case EventKind::Recover:
        case EventKind::Reconnect:
        case EventKind::RuntimePoll:
            return rejected_transition(state, DecisionReason::InvalidTransition);
        case EventKind::Count:
            return rejected_transition(state, DecisionReason::InvalidTransition);
    }

    if (valid)
        return accepted_transition(state, target);
    if (state == target)
        return rejected_transition(state, DecisionReason::AlreadyInState);
    return rejected_transition(state, DecisionReason::InvalidTransition);
}

constexpr TransitionDisposition transition_disposition(State state, EventKind kind)
{
    const TransitionResult result = transition(
        state,
        { kind, EventSource::Internal, 0u, 0u, 0u, 0u });
    if (result.accepted())
        return TransitionDisposition::Accepted;
    if (result.reason == DecisionReason::AlreadyInState)
        return TransitionDisposition::Ignored;
    return TransitionDisposition::Rejected;
}

constexpr bool transition_table_complete()
{
    if (all_states.size() != static_cast<std::size_t>(State::Count) ||
        all_event_kinds.size() != static_cast<std::size_t>(EventKind::Count))
        return false;

    for (const State state : all_states) {
        for (const EventKind kind : all_event_kinds) {
            const TransitionResult result = transition(
                state,
                { kind, EventSource::Internal, 0u, 0u, 0u, 0u });
            const TransitionDisposition disposition = transition_disposition(state, kind);
            if (disposition == TransitionDisposition::Accepted &&
                (!result.accepted() || result.reason != DecisionReason::TransitionApplied || result.effects.empty()))
                return false;
            if (disposition == TransitionDisposition::Ignored &&
                (result.accepted() || result.reason != DecisionReason::AlreadyInState || !result.effects.empty()))
                return false;
            if (disposition == TransitionDisposition::Rejected &&
                (result.accepted() || result.reason != DecisionReason::InvalidTransition || !result.effects.empty()))
                return false;
        }
    }
    return true;
}

static_assert(transition_table_complete(), "lifecycle transition table must classify every state/event pair");

constexpr bool advances_generation(EventKind kind)
{
    return kind == EventKind::Create ||
        kind == EventKind::Join ||
        kind == EventKind::Leave ||
        kind == EventKind::Reset ||
        kind == EventKind::Recover;
}

constexpr MachineTransitionResult rejected_machine_transition(
    MachineState state,
    DecisionReason reason)
{
    return { state, {}, reason, DecisionStatus::Rejected };
}

constexpr MachineTransitionResult transition(MachineState state, const Event &event)
{
    if (event.generation != 0u && event.generation != state.generation)
        return rejected_machine_transition(state, DecisionReason::StaleGeneration);

    if (event.kind == EventKind::Reconnect) {
        const ReconnectKey key{ state.generation, event.server_id, event.endpoint_key };
        if (state.reconnect_queued && state.reconnect_key == key)
            return rejected_machine_transition(state, DecisionReason::ReconnectAlreadyQueued);

        state.reconnect_key = key;
        state.reconnect_queued = true;
        return {
            state,
            { { Effect{ EffectKind::ReconnectQueued, state.lifecycle, state.lifecycle, state.generation } }, 1u },
            DecisionReason::ReconnectQueued,
            DecisionStatus::Accepted,
        };
    }

    if (advances_generation(event.kind) && state.generation == std::numeric_limits<std::uint64_t>::max())
        return rejected_machine_transition(state, DecisionReason::GenerationExhausted);

    if (event.kind == EventKind::Recover) {
        ++state.generation;
        state.reconnect_key = {};
        state.reconnect_queued = false;
        return {
            state,
            { { Effect{ EffectKind::GenerationAdvanced, state.lifecycle, state.lifecycle, state.generation } }, 1u },
            DecisionReason::TransitionApplied,
            DecisionStatus::Accepted,
        };
    }

    const TransitionResult lifecycle_result = transition(state.lifecycle, event);
    if (!lifecycle_result.accepted())
        return rejected_machine_transition(state, lifecycle_result.reason);

    const State previous_lifecycle = state.lifecycle;
    state.lifecycle = lifecycle_result.state;
    EffectList effects{};
    effects.values[effects.count++] = {
        EffectKind::StateChanged,
        previous_lifecycle,
        state.lifecycle,
        state.generation,
    };

    if (advances_generation(event.kind)) {
        ++state.generation;
        state.reconnect_key = {};
        state.reconnect_queued = false;
        effects.values[effects.count++] = {
            EffectKind::GenerationAdvanced,
            state.lifecycle,
            state.lifecycle,
            state.generation,
        };
    }

    return { state, effects, DecisionReason::TransitionApplied, DecisionStatus::Accepted };
}

constexpr MachineTransitionResult transition_runtime_poll(
    MachineState state,
    const Event &event,
    std::uint32_t legacy_lobby_state,
    std::uint32_t legacy_game_state)
{
    if (event.kind != EventKind::RuntimePoll)
        return rejected_machine_transition(state, DecisionReason::InvalidTransition);
    if (event.generation != 0u && event.generation != state.generation)
        return rejected_machine_transition(state, DecisionReason::StaleGeneration);
    if (legacy_lobby_state == 2u && legacy_game_state == 10u)
        return rejected_machine_transition(state, DecisionReason::TerminalState);

    return {
        state,
        { { Effect{
            EffectKind::PracticeLobbyDetailsRequested,
            state.lifecycle,
            state.lifecycle,
            state.generation } }, 1u },
        DecisionReason::TransitionApplied,
        DecisionStatus::Accepted,
    };
}

struct CustomGameRequestState {
    MachineState machine{};
    std::uint32_t lobby_state{};
    std::uint32_t game_state{};
    std::uint32_t launch_phase{};
    bool has_custom_game{};
    bool has_launch_server_setup{};
};

struct CustomGameRequest {
    Event event{};
    std::uint32_t ready_state{};
    bool load_failed{};
};

constexpr MachineTransitionResult accepted_custom_game_request(
    MachineState state,
    State lifecycle)
{
    const State previous_lifecycle = state.lifecycle;
    state.lifecycle = lifecycle;
    EffectList effects{};
    if (previous_lifecycle != lifecycle) {
        effects.values[effects.count++] = {
            EffectKind::StateChanged,
            previous_lifecycle,
            lifecycle,
            state.generation,
        };
    }
    effects.values[effects.count++] = {
        EffectKind::CustomGameLifecycleActionsRequested,
        lifecycle,
        lifecycle,
        state.generation,
    };
    return { state, effects, DecisionReason::TransitionApplied, DecisionStatus::Accepted };
}

constexpr MachineTransitionResult transition_custom_game_request(
    const CustomGameRequestState &state,
    const CustomGameRequest &request,
    std::uint32_t setup_synced_launch_phase,
    std::uint32_t run_queued_launch_phase)
{
    if (request.event.generation != 0u && request.event.generation != state.machine.generation)
        return rejected_machine_transition(state.machine, DecisionReason::StaleGeneration);
    if (!state.has_custom_game)
        return rejected_machine_transition(state.machine, DecisionReason::RequestIgnored);

    switch (request.event.kind) {
        case EventKind::Run:
            if (request.ready_state != 1u || state.lobby_state != 2u || state.game_state >= 1u ||
                state.launch_phase < run_queued_launch_phase)
                return rejected_machine_transition(state.machine, DecisionReason::RequestIgnored);
            return accepted_custom_game_request(state.machine, State::Running);
        case EventKind::Loading:
            if (state.has_launch_server_setup && state.launch_phase >= setup_synced_launch_phase)
                return accepted_custom_game_request(state.machine, State::Loading);
            return accepted_custom_game_request(state.machine, state.machine.lifecycle);
        case EventKind::Loaded:
            return accepted_custom_game_request(
                state.machine,
                request.load_failed ? state.machine.lifecycle : State::Loaded);
        default:
            return rejected_machine_transition(state.machine, DecisionReason::InvalidTransition);
    }
}

struct RuntimeMemberRequest {
    std::uint64_t generation{};
    std::uint64_t steam_id{};
    bool connected{};
    std::uint32_t hero_id{};
    bool has_hero_id{};
};

constexpr MachineTransitionResult transition_runtime_member(
    MachineState state,
    const RuntimeMemberRequest &request)
{
    if (request.generation != 0u && request.generation != state.generation)
        return rejected_machine_transition(state, DecisionReason::StaleGeneration);
    if (request.steam_id == 0u)
        return rejected_machine_transition(state, DecisionReason::RequestIgnored);
    return {
        state,
        { { Effect{
            EffectKind::RuntimeMemberUpdateRequested,
            state.lifecycle,
            state.lifecycle,
            state.generation } }, 1u },
        DecisionReason::TransitionApplied,
        DecisionStatus::Accepted,
    };
}

struct RuntimeGameStateRequest {
    std::uint64_t generation{};
    bool custom_game_launch{};
    bool has_game_state{};
    std::uint32_t requested_game_state{};
    std::uint32_t lobby_state{};
    std::uint32_t current_game_state{};
    std::uint32_t launch_phase{};
};

enum class TeardownStage : std::uint8_t {
    Initiate,
    Finalize,
};

struct TeardownRequest {
    Event event{};
    TeardownStage stage{TeardownStage::Initiate};
    bool active{};
    bool pending{};
};

constexpr EffectKind teardown_effect_for(EventKind kind, TeardownStage stage)
{
    switch (kind) {
    case EventKind::Leave:
        return stage == TeardownStage::Finalize
            ? EffectKind::TeardownLeaveFinalizeRequested
            : EffectKind::TeardownLeaveInitiateRequested;
    case EventKind::Abandon:
        return stage == TeardownStage::Finalize
            ? EffectKind::TeardownAbandonFinalizeRequested
            : EffectKind::TeardownAbandonInitiateRequested;
    case EventKind::PostGame:
        return stage == TeardownStage::Finalize
            ? EffectKind::TeardownPostGameFinalizeRequested
            : EffectKind::TeardownPostGameInitiateRequested;
    case EventKind::Reset:
        return EffectKind::TeardownResetRequested;
    default:
        return EffectKind::TeardownResetRequested;
    }
}

constexpr MachineTransitionResult transition_teardown(
    MachineState state,
    const TeardownRequest &request)
{
    if (request.event.generation != 0u && request.event.generation != state.generation)
        return rejected_machine_transition(state, DecisionReason::StaleGeneration);
    if (request.event.kind != EventKind::Leave &&
        request.event.kind != EventKind::Abandon &&
        request.event.kind != EventKind::PostGame &&
        request.event.kind != EventKind::Reset)
        return rejected_machine_transition(state, DecisionReason::InvalidTransition);
    if (!request.active)
        return rejected_machine_transition(state, DecisionReason::RequestIgnored);
    if ((request.stage == TeardownStage::Initiate && request.pending) ||
        (request.stage == TeardownStage::Finalize && !request.pending))
        return rejected_machine_transition(state, DecisionReason::AlreadyInState);
    if (state.generation == std::numeric_limits<std::uint64_t>::max())
        return rejected_machine_transition(state, DecisionReason::GenerationExhausted);

    return {
        state,
        { { Effect{
            teardown_effect_for(request.event.kind, request.stage),
            state.lifecycle,
            state.lifecycle,
            state.generation } }, 1u },
        DecisionReason::TransitionApplied,
        DecisionStatus::Accepted,
    };
}

constexpr MachineTransitionResult transition_runtime_game_state(
    MachineState state,
    const RuntimeGameStateRequest &request,
    std::uint32_t run_queued_launch_phase)
{
    if (request.generation != 0u && request.generation != state.generation)
        return rejected_machine_transition(state, DecisionReason::StaleGeneration);
    if (!request.custom_game_launch || request.lobby_state != 2u ||
        request.launch_phase < run_queued_launch_phase || !request.has_game_state ||
        request.requested_game_state <= request.current_game_state)
        return rejected_machine_transition(state, DecisionReason::RequestIgnored);
    return {
        state,
        { { Effect{
            EffectKind::RuntimeGameStateUpdateRequested,
            state.lifecycle,
            state.lifecycle,
            state.generation } }, 1u },
        DecisionReason::TransitionApplied,
        DecisionStatus::Accepted,
    };
}

} // namespace gbe::dota_lifecycle_state_machine

#endif // __INCLUDED_GBE_DOTA_LIFECYCLE_STATE_MACHINE_H__

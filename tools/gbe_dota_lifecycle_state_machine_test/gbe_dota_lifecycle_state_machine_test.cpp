#include "../../dll/gbe_dota_lifecycle_state_machine.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

namespace lifecycle = gbe::dota_lifecycle_state_machine;

struct ExpectedMapping {
    std::uint32_t message_id;
    lifecycle::EventKind kind;
};

constexpr lifecycle::Event event(lifecycle::EventKind kind)
{
    return { kind, lifecycle::EventSource::Internal, 0u, 0u, 0u, 0u };
}

void assert_accepted(
    lifecycle::State from,
    lifecycle::EventKind kind,
    lifecycle::State expected)
{
    const lifecycle::TransitionResult result = lifecycle::transition(from, event(kind));
    assert(result.accepted());
    assert(result.status == lifecycle::DecisionStatus::Accepted);
    assert(result.reason == lifecycle::DecisionReason::TransitionApplied);
    assert(result.state == expected);
    assert(result.effects.count == 1u);
    assert(result.effects.values[0].kind == lifecycle::EffectKind::StateChanged);
    assert(result.effects.values[0].from == from);
    assert(result.effects.values[0].to == expected);
}

void assert_rejected(
    lifecycle::State state,
    lifecycle::EventKind kind,
    lifecycle::DecisionReason expected_reason)
{
    const lifecycle::TransitionResult result = lifecycle::transition(state, event(kind));
    assert(!result.accepted());
    assert(result.status == lifecycle::DecisionStatus::Rejected);
    assert(result.reason == expected_reason);
    assert(result.state == state);
    assert(result.effects.empty());
}

int main()
{
    constexpr std::array<ExpectedMapping, 9> mappings{{
        { GBE_kDotaPracticeLobbyCreate, lifecycle::EventKind::Create },
        { GBE_kDotaPracticeLobbyJoin, lifecycle::EventKind::Join },
        { GBE_kDotaPracticeLobbyLaunch, lifecycle::EventKind::Setup },
        { 8052u, lifecycle::EventKind::Loading },
        { 8053u, lifecycle::EventKind::Loaded },
        { 7070u, lifecycle::EventKind::Run },
        { GBE_kDotaGameMatchSignOut, lifecycle::EventKind::PostGame },
        { GBE_kDotaPracticeLobbyLeave, lifecycle::EventKind::Leave },
        { GBE_kDotaAbandonCurrentGame, lifecycle::EventKind::Abandon },
    }};

    for (const ExpectedMapping &expected : mappings) {
        const lifecycle::EventMapping direct = lifecycle::event_from_message(expected.message_id, false);
        const lifecycle::EventMapping wrapped = lifecycle::event_from_message(expected.message_id, true);

        assert(direct.mapped);
        assert(wrapped.mapped);
        assert(direct.event.kind == expected.kind);
        assert(wrapped.event.kind == expected.kind);
        assert(direct.event.message_id == expected.message_id);
        assert(wrapped.event.message_id == expected.message_id);
        assert(direct.event.source == lifecycle::EventSource::Direct);
        assert(wrapped.event.source == lifecycle::EventSource::Wrapped);
    }

    const lifecycle::EventMapping unknown = lifecycle::event_from_message(0xffffffffu, false);
    assert(!unknown.mapped);

    const lifecycle::Event reset = lifecycle::reset_event();
    assert(reset.kind == lifecycle::EventKind::Reset);
    assert(reset.source == lifecycle::EventSource::Internal);
    assert(reset.message_id == 0u);

    static_assert(static_cast<std::uint8_t>(lifecycle::State::Idle) == 0u);
    static_assert(static_cast<std::uint8_t>(lifecycle::State::PostGame) == 7u);

    assert_accepted(lifecycle::State::Idle, lifecycle::EventKind::Create, lifecycle::State::Created);
    assert_accepted(lifecycle::State::Idle, lifecycle::EventKind::Join, lifecycle::State::Joined);
    assert_accepted(lifecycle::State::Created, lifecycle::EventKind::Setup, lifecycle::State::Setup);
    assert_accepted(lifecycle::State::Joined, lifecycle::EventKind::Setup, lifecycle::State::Setup);
    assert_accepted(lifecycle::State::Setup, lifecycle::EventKind::Loading, lifecycle::State::Loading);
    assert_accepted(lifecycle::State::Loading, lifecycle::EventKind::Loaded, lifecycle::State::Loaded);
    assert_accepted(lifecycle::State::Loaded, lifecycle::EventKind::Run, lifecycle::State::Running);
    assert_accepted(lifecycle::State::Running, lifecycle::EventKind::PostGame, lifecycle::State::PostGame);
    assert_accepted(lifecycle::State::PostGame, lifecycle::EventKind::Reset, lifecycle::State::Idle);

    assert_accepted(lifecycle::State::Created, lifecycle::EventKind::Leave, lifecycle::State::PostGame);
    assert_accepted(lifecycle::State::Loading, lifecycle::EventKind::Abandon, lifecycle::State::PostGame);

    assert_rejected(
        lifecycle::State::Created,
        lifecycle::EventKind::Create,
        lifecycle::DecisionReason::AlreadyInState);
    assert_rejected(
        lifecycle::State::Idle,
        lifecycle::EventKind::Loading,
        lifecycle::DecisionReason::InvalidTransition);
    assert_rejected(
        lifecycle::State::Running,
        lifecycle::EventKind::Loaded,
        lifecycle::DecisionReason::InvalidTransition);
    assert_rejected(
        lifecycle::State::Idle,
        lifecycle::EventKind::Reset,
        lifecycle::DecisionReason::AlreadyInState);

    constexpr lifecycle::TransitionResult compile_time_transition = lifecycle::transition(
        lifecycle::State::Setup,
        event(lifecycle::EventKind::Loading));
    static_assert(compile_time_transition.accepted());
    static_assert(compile_time_transition.state == lifecycle::State::Loading);
    static_assert(compile_time_transition.effects.count == 1u);

    lifecycle::MachineState machine{};
    machine.generation = 5u;
    const lifecycle::MachineTransitionResult created = lifecycle::transition(
        machine,
        event(lifecycle::EventKind::Create));
    assert(created.accepted());
    assert(created.state.lifecycle == lifecycle::State::Created);
    assert(created.state.generation == 6u);
    assert(created.effects.count == 2u);
    assert(created.effects.values[1].kind == lifecycle::EffectKind::GenerationAdvanced);

    const lifecycle::MachineTransitionResult setup = lifecycle::transition(
        created.state,
        event(lifecycle::EventKind::Setup));
    assert(setup.accepted());
    assert(setup.state.generation == created.state.generation);
    assert(setup.effects.count == 1u);

    const lifecycle::MachineTransitionResult reconnect = lifecycle::transition(
        setup.state,
        lifecycle::reconnect_event(setup.state.generation, 9001u, 27015u));
    assert(reconnect.accepted());
    assert(reconnect.reason == lifecycle::DecisionReason::ReconnectQueued);
    assert(reconnect.state.reconnect_queued);
    assert(reconnect.effects.count == 1u);
    assert(reconnect.effects.values[0].kind == lifecycle::EffectKind::ReconnectQueued);

    const lifecycle::MachineTransitionResult duplicate_reconnect = lifecycle::transition(
        reconnect.state,
        lifecycle::reconnect_event(reconnect.state.generation, 9001u, 27015u));
    assert(!duplicate_reconnect.accepted());
    assert(duplicate_reconnect.reason == lifecycle::DecisionReason::ReconnectAlreadyQueued);
    assert(duplicate_reconnect.effects.empty());

    const lifecycle::MachineTransitionResult changed_endpoint = lifecycle::transition(
        reconnect.state,
        lifecycle::reconnect_event(reconnect.state.generation, 9001u, 27016u));
    assert(changed_endpoint.accepted());
    assert(changed_endpoint.state.reconnect_key.endpoint_key == 27016u);

    lifecycle::Event stale_loading = event(lifecycle::EventKind::Loading);
    stale_loading.generation = setup.state.generation - 1u;
    const lifecycle::MachineTransitionResult stale = lifecycle::transition(setup.state, stale_loading);
    assert(!stale.accepted());
    assert(stale.reason == lifecycle::DecisionReason::StaleGeneration);
    assert(stale.state.lifecycle == setup.state.lifecycle);
    assert(stale.state.generation == setup.state.generation);
    assert(stale.effects.empty());

    const lifecycle::MachineTransitionResult recovered = lifecycle::transition(
        reconnect.state,
        lifecycle::recover_event());
    assert(recovered.accepted());
    assert(recovered.state.generation == reconnect.state.generation + 1u);
    assert(!recovered.state.reconnect_queued);
    assert(recovered.effects.count == 1u);
    assert(recovered.effects.values[0].kind == lifecycle::EffectKind::GenerationAdvanced);

    const lifecycle::MachineTransitionResult reconnect_after_recover = lifecycle::transition(
        recovered.state,
        lifecycle::reconnect_event(recovered.state.generation, 9001u, 27015u));
    assert(reconnect_after_recover.accepted());

    lifecycle::MachineState exhausted{};
    exhausted.lifecycle = lifecycle::State::PostGame;
    exhausted.generation = std::numeric_limits<std::uint64_t>::max();
    const lifecycle::MachineTransitionResult exhausted_reset = lifecycle::transition(
        exhausted,
        lifecycle::reset_event());
    assert(!exhausted_reset.accepted());
    assert(exhausted_reset.reason == lifecycle::DecisionReason::GenerationExhausted);
    assert(exhausted_reset.state.lifecycle == lifecycle::State::PostGame);

    std::cout << "gbe_dota_lifecycle_state_machine_test passed\n";
    return 0;
}

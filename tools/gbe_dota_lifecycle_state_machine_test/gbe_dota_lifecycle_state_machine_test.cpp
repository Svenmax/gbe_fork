#include "../../dll/gbe_dota_lifecycle_state_machine.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace lifecycle = gbe::dota_lifecycle_state_machine;

struct ExpectedMapping {
    std::uint32_t message_id;
    lifecycle::EventKind kind;
};

constexpr lifecycle::Event event(lifecycle::EventKind kind)
{
    return { kind, lifecycle::EventSource::Internal, 0u };
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

    std::cout << "gbe_dota_lifecycle_state_machine_test passed\n";
    return 0;
}

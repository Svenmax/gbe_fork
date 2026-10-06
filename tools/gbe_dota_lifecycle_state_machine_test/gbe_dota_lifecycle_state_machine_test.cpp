#include "../../dll/gbe_dota_lifecycle_state_machine.h"

#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>

namespace lifecycle = gbe::dota_lifecycle_state_machine;

namespace {

[[noreturn]] void fail_check(const char *expression, int line)
{
    std::cerr << "lifecycle state machine check failed at line " << line
              << ": " << expression << '\n';
    std::abort();
}

} // namespace

#define assert(expression) ((expression) ? static_cast<void>(0) : fail_check(#expression, __LINE__))

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

void run_normal_launch_example()
{
    lifecycle::MachineState state{};
    state.generation = 40u;
    constexpr std::array<lifecycle::EventKind, 6> events{{
        lifecycle::EventKind::Create,
        lifecycle::EventKind::Setup,
        lifecycle::EventKind::Loading,
        lifecycle::EventKind::Loaded,
        lifecycle::EventKind::Run,
        lifecycle::EventKind::PostGame,
    }};
    constexpr std::array<lifecycle::State, 6> expected_states{{
        lifecycle::State::Created,
        lifecycle::State::Setup,
        lifecycle::State::Loading,
        lifecycle::State::Loaded,
        lifecycle::State::Running,
        lifecycle::State::PostGame,
    }};

    for (std::size_t index = 0u; index < events.size(); ++index) {
        const auto result = lifecycle::transition(state, event(events[index]));
        assert(result.accepted());
        state = result.state;
        assert(state.lifecycle == expected_states[index]);
    }
    assert(state.generation == 41u);
}

void run_load_failure_example()
{
    lifecycle::CustomGameRequestState state{};
    state.machine.lifecycle = lifecycle::State::Loading;
    state.machine.generation = 51u;
    state.has_custom_game = true;
    lifecycle::CustomGameRequest request{};
    request.event = { lifecycle::EventKind::Loaded, lifecycle::EventSource::Wrapped, 8053u, 51u, 0u, 0u };
    request.load_failed = true;

    const auto result = lifecycle::transition_custom_game_request(state, request, 2u, 3u);
    assert(result.accepted());
    assert(result.state.lifecycle == lifecycle::State::Loading);
    assert(result.state.generation == state.machine.generation);
    assert(!result.effects.contains(lifecycle::EffectKind::StateChanged));
    assert(result.effects.contains(lifecycle::EffectKind::CustomGameLifecycleActionsRequested));
}

void run_duplicate_and_out_of_order_example()
{
    const auto duplicate_loading = lifecycle::transition(
        lifecycle::State::Loading,
        event(lifecycle::EventKind::Loading));
    assert(!duplicate_loading.accepted());
    assert(duplicate_loading.reason == lifecycle::DecisionReason::AlreadyInState);

    const auto duplicate_loaded = lifecycle::transition(
        lifecycle::State::Loaded,
        event(lifecycle::EventKind::Loaded));
    assert(!duplicate_loaded.accepted());
    assert(duplicate_loaded.reason == lifecycle::DecisionReason::AlreadyInState);

    const auto out_of_order_loaded = lifecycle::transition(
        lifecycle::State::Setup,
        event(lifecycle::EventKind::Loaded));
    assert(!out_of_order_loaded.accepted());
    assert(out_of_order_loaded.reason == lifecycle::DecisionReason::InvalidTransition);
}

void run_teardown_examples()
{
    lifecycle::MachineState running{};
    running.lifecycle = lifecycle::State::Running;
    running.generation = 61u;

    const auto postgame = lifecycle::transition(running, event(lifecycle::EventKind::PostGame));
    assert(postgame.accepted());
    assert(postgame.state.lifecycle == lifecycle::State::PostGame);
    assert(postgame.state.generation == running.generation);

    const auto leave = lifecycle::transition(running, event(lifecycle::EventKind::Leave));
    assert(leave.accepted());
    assert(leave.state.lifecycle == lifecycle::State::PostGame);
    assert(leave.state.generation == running.generation + 1u);
    assert(leave.effects.contains(lifecycle::EffectKind::GenerationAdvanced));

    const auto abandon = lifecycle::transition(running, event(lifecycle::EventKind::Abandon));
    assert(abandon.accepted());
    assert(abandon.state.lifecycle == lifecycle::State::PostGame);
    assert(abandon.state.generation == running.generation);
}

void run_generation_boundary_examples()
{
    lifecycle::MachineState existing_lobby{};
    existing_lobby.lifecycle = lifecycle::State::Running;
    existing_lobby.generation = 81u;
    existing_lobby.reconnect_queued = true;

    const auto create = lifecycle::transition_generation_boundary(
        existing_lobby,
        { lifecycle::EventKind::Create, lifecycle::EventSource::Direct, GBE_kDotaPracticeLobbyCreate, 81u, 0u, 0u });
    assert(create.accepted());
    assert(create.state.lifecycle == lifecycle::State::Running);
    assert(create.state.generation == 82u);
    assert(!create.state.reconnect_queued);
    assert(create.effects.contains(lifecycle::EffectKind::GenerationAdvanced));

    const auto join = lifecycle::transition_generation_boundary(
        existing_lobby,
        { lifecycle::EventKind::Join, lifecycle::EventSource::Wrapped, GBE_kDotaPracticeLobbyJoin, 0u, 0u, 0u });
    assert(join.accepted());
    assert(join.state.generation == 82u);

    const auto stale = lifecycle::transition_generation_boundary(
        existing_lobby,
        { lifecycle::EventKind::Create, lifecycle::EventSource::Direct, GBE_kDotaPracticeLobbyCreate, 80u, 0u, 0u });
    assert(!stale.accepted());
    assert(stale.reason == lifecycle::DecisionReason::StaleGeneration);

    lifecycle::MachineState exhausted{};
    exhausted.generation = std::numeric_limits<std::uint64_t>::max();
    const auto exhausted_create = lifecycle::transition_generation_boundary(
        exhausted,
        { lifecycle::EventKind::Create, lifecycle::EventSource::Direct, GBE_kDotaPracticeLobbyCreate, exhausted.generation, 0u, 0u });
    assert(!exhausted_create.accepted());
    assert(exhausted_create.reason == lifecycle::DecisionReason::GenerationExhausted);

    const auto invalid = lifecycle::transition_generation_boundary(
        existing_lobby,
        { lifecycle::EventKind::PostGame, lifecycle::EventSource::Direct, GBE_kDotaGameMatchSignOut, existing_lobby.generation, 0u, 0u });
    assert(!invalid.accepted());
    assert(invalid.reason == lifecycle::DecisionReason::InvalidTransition);
}

void run_runtime_clear_boundary_examples()
{
    lifecycle::MachineState active{};
    active.lifecycle = lifecycle::State::Running;
    active.generation = 91u;
    active.reconnect_queued = true;

    const auto leave = lifecycle::transition_runtime_clear_boundary(
        active,
        { lifecycle::EventKind::Leave, lifecycle::EventSource::Direct, GBE_kDotaDestroyLobbyRequest, 91u, 0u, 0u });
    assert(leave.accepted());
    assert(leave.state.lifecycle == lifecycle::State::Running);
    assert(leave.state.generation == 92u);
    assert(!leave.state.reconnect_queued);
    assert(leave.effects.contains(lifecycle::EffectKind::GenerationAdvanced));

    const auto stale = lifecycle::transition_runtime_clear_boundary(
        active,
        { lifecycle::EventKind::Leave, lifecycle::EventSource::Direct, GBE_kDotaDestroyLobbyRequest, 90u, 0u, 0u });
    assert(!stale.accepted());
    assert(stale.reason == lifecycle::DecisionReason::StaleGeneration);

    const auto invalid = lifecycle::transition_runtime_clear_boundary(
        active,
        { lifecycle::EventKind::PostGame, lifecycle::EventSource::Direct, GBE_kDotaGameMatchSignOut, 91u, 0u, 0u });
    assert(!invalid.accepted());
    assert(invalid.reason == lifecycle::DecisionReason::InvalidTransition);
}

void run_reconnect_example()
{
    lifecycle::MachineState running{};
    running.lifecycle = lifecycle::State::Running;
    running.generation = 71u;

    const auto queued = lifecycle::transition(
        running,
        lifecycle::reconnect_event(71u, 9001u, 27015u));
    assert(queued.accepted());
    assert(queued.reason == lifecycle::DecisionReason::ReconnectQueued);
    assert(queued.state.lifecycle == lifecycle::State::Running);
    assert(queued.state.generation == running.generation);
    assert(queued.effects.contains(lifecycle::EffectKind::ReconnectQueued));

    const auto duplicate = lifecycle::transition(
        queued.state,
        lifecycle::reconnect_event(71u, 9001u, 27015u));
    assert(!duplicate.accepted());
    assert(duplicate.reason == lifecycle::DecisionReason::ReconnectAlreadyQueued);
}

void verify_progression_property(
    lifecycle::MachineState state,
    std::size_t remaining_depth)
{
    if (remaining_depth == 0u)
        return;

    constexpr std::array<lifecycle::EventKind, 10> lifecycle_events{{
        lifecycle::EventKind::Create,
        lifecycle::EventKind::Join,
        lifecycle::EventKind::Setup,
        lifecycle::EventKind::Loading,
        lifecycle::EventKind::Loaded,
        lifecycle::EventKind::Run,
        lifecycle::EventKind::PostGame,
        lifecycle::EventKind::Leave,
        lifecycle::EventKind::Abandon,
        lifecycle::EventKind::Reset,
    }};

    for (const lifecycle::EventKind kind : lifecycle_events) {
        const lifecycle::State previous = state.lifecycle;
        const auto result = lifecycle::transition(state, event(kind));
        if (result.accepted()) {
            if (result.state.lifecycle == lifecycle::State::Loaded)
                assert(previous == lifecycle::State::Loading);
            if (result.state.lifecycle == lifecycle::State::Running)
                assert(previous == lifecycle::State::Loaded);
            verify_progression_property(result.state, remaining_depth - 1u);
        } else {
            verify_progression_property(state, remaining_depth - 1u);
        }
    }
}

void run_state_machine_properties()
{
    // P15-A: event sequences cannot bypass Loading or Loaded progression.
    lifecycle::MachineState initial{};
    initial.generation = 80u;
    verify_progression_property(initial, 5u);

    // P15-B: stale generation input preserves current state and emits no effects.
    for (const lifecycle::State state : lifecycle::all_states) {
        for (const lifecycle::EventKind kind : lifecycle::all_event_kinds) {
            lifecycle::MachineState current{};
            current.lifecycle = state;
            current.generation = 91u;
            current.reconnect_key = { 91u, 9001u, 27015u };
            current.reconnect_queued = true;
            lifecycle::Event stale = event(kind);
            stale.generation = 90u;
            stale.server_id = 9002u;
            stale.endpoint_key = 27016u;
            const auto result = lifecycle::transition(current, stale);
            assert(!result.accepted());
            assert(result.reason == lifecycle::DecisionReason::StaleGeneration);
            assert(result.state.lifecycle == current.lifecycle);
            assert(result.state.generation == current.generation);
            assert(result.state.reconnect_key == current.reconnect_key);
            assert(result.state.reconnect_queued == current.reconnect_queued);
            assert(result.effects.empty());
        }
    }

    // P15-C: repeated transport lifecycle events are idempotent or stably rejected.
    constexpr std::array<lifecycle::EventKind, 10> repeatable_events{{
        lifecycle::EventKind::Create,
        lifecycle::EventKind::Join,
        lifecycle::EventKind::Setup,
        lifecycle::EventKind::Loading,
        lifecycle::EventKind::Loaded,
        lifecycle::EventKind::Run,
        lifecycle::EventKind::PostGame,
        lifecycle::EventKind::Leave,
        lifecycle::EventKind::Abandon,
        lifecycle::EventKind::Reset,
    }};
    for (const lifecycle::State state : lifecycle::all_states) {
        for (const lifecycle::EventKind kind : repeatable_events) {
            lifecycle::MachineState current{};
            current.lifecycle = state;
            current.generation = 101u;
            const auto first = lifecycle::transition(current, event(kind));
            const lifecycle::MachineState repeated_input = first.accepted() ? first.state : current;
            const auto repeated = lifecycle::transition(repeated_input, event(kind));
            assert(!repeated.accepted());
            assert(repeated.reason == lifecycle::DecisionReason::AlreadyInState ||
                repeated.reason == lifecycle::DecisionReason::InvalidTransition);
            assert(repeated.state.lifecycle == repeated_input.lifecycle);
            assert(repeated.state.generation == repeated_input.generation);
            assert(repeated.effects.empty());
        }
    }
    lifecycle::MachineState reconnect_state{};
    reconnect_state.lifecycle = lifecycle::State::Running;
    reconnect_state.generation = 102u;
    const auto first_reconnect = lifecycle::transition(
        reconnect_state,
        lifecycle::reconnect_event(102u, 9001u, 27015u));
    const auto repeated_reconnect = lifecycle::transition(
        first_reconnect.state,
        lifecycle::reconnect_event(102u, 9001u, 27015u));
    assert(!repeated_reconnect.accepted());
    assert(repeated_reconnect.reason == lifecycle::DecisionReason::ReconnectAlreadyQueued);
    assert(repeated_reconnect.effects.empty());

    // P15-D: load failure never emits a loaded state change.
    for (const lifecycle::State state : lifecycle::all_states) {
        lifecycle::CustomGameRequestState request_state{};
        request_state.machine.lifecycle = state;
        request_state.machine.generation = 111u;
        request_state.has_custom_game = true;
        lifecycle::CustomGameRequest request{};
        request.event = { lifecycle::EventKind::Loaded, lifecycle::EventSource::Direct, 8053u, 111u, 0u, 0u };
        request.load_failed = true;
        const auto result = lifecycle::transition_custom_game_request(request_state, request, 2u, 3u);
        assert(result.accepted());
        assert(result.state.lifecycle == state);
        assert(!result.effects.contains(lifecycle::EffectKind::StateChanged));
    }

    // P15-E: every active state reaches a stable, repeatable cleanup state.
    for (const lifecycle::State state : lifecycle::all_states) {
        if (state == lifecycle::State::Idle || state == lifecycle::State::PostGame)
            continue;
        lifecycle::MachineState active{};
        active.lifecycle = state;
        active.generation = 121u;
        for (const lifecycle::EventKind teardown_kind : {
                 lifecycle::EventKind::Leave,
                 lifecycle::EventKind::Abandon }) {
            const auto teardown = lifecycle::transition(active, event(teardown_kind));
            assert(teardown.accepted());
            assert(teardown.state.lifecycle == lifecycle::State::PostGame);
            const auto reset = lifecycle::transition(teardown.state, lifecycle::reset_event());
            assert(reset.accepted());
            assert(reset.state.lifecycle == lifecycle::State::Idle);
            const auto repeated_reset = lifecycle::transition(reset.state, lifecycle::reset_event());
            assert(!repeated_reset.accepted());
            assert(repeated_reset.reason == lifecycle::DecisionReason::AlreadyInState);
            assert(repeated_reset.state.lifecycle == lifecycle::State::Idle);
            assert(repeated_reset.effects.empty());
        }
    }
}

struct ReferenceResult {
    lifecycle::MachineState state{};
    lifecycle::EffectList effects{};
    lifecycle::DecisionReason reason{lifecycle::DecisionReason::InvalidTransition};
    lifecycle::DecisionStatus status{lifecycle::DecisionStatus::Rejected};
};

ReferenceResult reference_transition(
    lifecycle::MachineState state,
    const lifecycle::Event &input)
{
    if (input.generation != 0u && input.generation != state.generation)
        return { state, {}, lifecycle::DecisionReason::StaleGeneration, lifecycle::DecisionStatus::Rejected };

    if (input.kind == lifecycle::EventKind::Reconnect) {
        const lifecycle::ReconnectKey key{ state.generation, input.server_id, input.endpoint_key };
        if (state.reconnect_queued && state.reconnect_key == key)
            return { state, {}, lifecycle::DecisionReason::ReconnectAlreadyQueued, lifecycle::DecisionStatus::Rejected };
        state.reconnect_key = key;
        state.reconnect_queued = true;
        lifecycle::EffectList effects{};
        effects.values[effects.count++] = {
            lifecycle::EffectKind::ReconnectQueued,
            state.lifecycle,
            state.lifecycle,
            state.generation,
        };
        return { state, effects, lifecycle::DecisionReason::ReconnectQueued, lifecycle::DecisionStatus::Accepted };
    }

    const bool generation_boundary =
        input.kind == lifecycle::EventKind::Create ||
        input.kind == lifecycle::EventKind::Join ||
        input.kind == lifecycle::EventKind::Leave ||
        input.kind == lifecycle::EventKind::Reset ||
        input.kind == lifecycle::EventKind::Recover;
    if (generation_boundary && state.generation == std::numeric_limits<std::uint64_t>::max())
        return { state, {}, lifecycle::DecisionReason::GenerationExhausted, lifecycle::DecisionStatus::Rejected };

    if (input.kind == lifecycle::EventKind::Recover) {
        ++state.generation;
        state.reconnect_key = {};
        state.reconnect_queued = false;
        lifecycle::EffectList effects{};
        effects.values[effects.count++] = {
            lifecycle::EffectKind::GenerationAdvanced,
            state.lifecycle,
            state.lifecycle,
            state.generation,
        };
        return { state, effects, lifecycle::DecisionReason::TransitionApplied, lifecycle::DecisionStatus::Accepted };
    }

    lifecycle::State target = state.lifecycle;
    bool valid = false;
    switch (input.kind) {
        case lifecycle::EventKind::Create:
            target = lifecycle::State::Created;
            valid = state.lifecycle == lifecycle::State::Idle;
            break;
        case lifecycle::EventKind::Join:
            target = lifecycle::State::Joined;
            valid = state.lifecycle == lifecycle::State::Idle;
            break;
        case lifecycle::EventKind::Setup:
            target = lifecycle::State::Setup;
            valid = state.lifecycle == lifecycle::State::Created || state.lifecycle == lifecycle::State::Joined;
            break;
        case lifecycle::EventKind::Loading:
            target = lifecycle::State::Loading;
            valid = state.lifecycle == lifecycle::State::Setup;
            break;
        case lifecycle::EventKind::Loaded:
            target = lifecycle::State::Loaded;
            valid = state.lifecycle == lifecycle::State::Loading;
            break;
        case lifecycle::EventKind::Run:
            target = lifecycle::State::Running;
            valid = state.lifecycle == lifecycle::State::Loaded;
            break;
        case lifecycle::EventKind::PostGame:
        case lifecycle::EventKind::Leave:
        case lifecycle::EventKind::Abandon:
            target = lifecycle::State::PostGame;
            valid = state.lifecycle != lifecycle::State::Idle && state.lifecycle != lifecycle::State::PostGame;
            break;
        case lifecycle::EventKind::Reset:
            target = lifecycle::State::Idle;
            valid = state.lifecycle == lifecycle::State::PostGame;
            break;
        case lifecycle::EventKind::RuntimePoll:
        case lifecycle::EventKind::Count:
            return { state, {}, lifecycle::DecisionReason::InvalidTransition, lifecycle::DecisionStatus::Rejected };
        case lifecycle::EventKind::Recover:
        case lifecycle::EventKind::Reconnect:
            std::abort();
    }

    if (!valid) {
        const lifecycle::DecisionReason reason = state.lifecycle == target
            ? lifecycle::DecisionReason::AlreadyInState
            : lifecycle::DecisionReason::InvalidTransition;
        return { state, {}, reason, lifecycle::DecisionStatus::Rejected };
    }

    const lifecycle::State previous = state.lifecycle;
    state.lifecycle = target;
    lifecycle::EffectList effects{};
    effects.values[effects.count++] = {
        lifecycle::EffectKind::StateChanged,
        previous,
        target,
        state.generation,
    };
    if (generation_boundary) {
        ++state.generation;
        state.reconnect_key = {};
        state.reconnect_queued = false;
        effects.values[effects.count++] = {
            lifecycle::EffectKind::GenerationAdvanced,
            state.lifecycle,
            state.lifecycle,
            state.generation,
        };
    }
    return { state, effects, lifecycle::DecisionReason::TransitionApplied, lifecycle::DecisionStatus::Accepted };
}

std::uint64_t next_random(std::uint64_t &state)
{
    state ^= state << 13u;
    state ^= state >> 7u;
    state ^= state << 17u;
    return state;
}

bool effects_equal(const lifecycle::EffectList &lhs, const lifecycle::EffectList &rhs)
{
    if (lhs.count != rhs.count)
        return false;
    for (std::size_t index = 0u; index < lhs.count; ++index) {
        if (lhs.values[index].kind != rhs.values[index].kind ||
            lhs.values[index].from != rhs.values[index].from ||
            lhs.values[index].to != rhs.values[index].to ||
            lhs.values[index].generation != rhs.values[index].generation)
            return false;
    }
    return true;
}

void run_model_based_differential_test()
{
    constexpr std::array<std::uint64_t, 8> seeds{{
        0x0000000000000001ull,
        0x9e3779b97f4a7c15ull,
        0xd1b54a32d192ed03ull,
        0x94d049bb133111ebull,
        0x2545f4914f6cdd1dull,
        0x123456789abcdef0ull,
        0xfedcba9876543210ull,
        0x7fffffffffffffffull,
    }};

    for (const std::uint64_t seed : seeds) {
        std::uint64_t random_state = seed;
        lifecycle::MachineState production{};
        production.generation = next_random(random_state) % 32u;
        lifecycle::MachineState reference = production;

        for (std::size_t step = 0u; step < 512u; ++step) {
            const std::uint64_t random_value = next_random(random_state);
            lifecycle::Event input = event(lifecycle::all_event_kinds[
                random_value % lifecycle::all_event_kinds.size()]);
            const std::uint64_t generation_mode = (random_value >> 8u) % 4u;
            if (generation_mode == 1u)
                input.generation = production.generation;
            else if (generation_mode == 2u)
                input.generation = production.generation == 0u ? 1u : production.generation - 1u;
            else if (generation_mode == 3u)
                input.generation = production.generation == std::numeric_limits<std::uint64_t>::max()
                    ? production.generation - 1u
                    : production.generation + 1u;
            input.server_id = (random_value >> 16u) % 4u;
            input.endpoint_key = (random_value >> 24u) % 4u;

            const auto actual = lifecycle::transition(production, input);
            const ReferenceResult expected = reference_transition(reference, input);
            const bool matches =
                actual.state.lifecycle == expected.state.lifecycle &&
                actual.state.generation == expected.state.generation &&
                actual.state.reconnect_key == expected.state.reconnect_key &&
                actual.state.reconnect_queued == expected.state.reconnect_queued &&
                actual.reason == expected.reason &&
                actual.status == expected.status &&
                effects_equal(actual.effects, expected.effects);
            if (!matches) {
                std::cerr << "differential mismatch seed=" << seed
                          << " step=" << step
                          << " event=" << static_cast<unsigned>(input.kind)
                          << " generation=" << input.generation << '\n';
                std::abort();
            }
            production = actual.state;
            reference = expected.state;
        }
    }
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
    static_assert(lifecycle::all_states.size() == 8u);
    static_assert(lifecycle::all_event_kinds.size() == 13u);
    static_assert(lifecycle::transition_table_complete());

    std::size_t accepted_pairs = 0u;
    std::size_t rejected_pairs = 0u;
    std::size_t ignored_pairs = 0u;
    for (const lifecycle::State state : lifecycle::all_states) {
        for (const lifecycle::EventKind kind : lifecycle::all_event_kinds) {
            switch (lifecycle::transition_disposition(state, kind)) {
                case lifecycle::TransitionDisposition::Accepted:
                    ++accepted_pairs;
                    break;
                case lifecycle::TransitionDisposition::Rejected:
                    ++rejected_pairs;
                    break;
                case lifecycle::TransitionDisposition::Ignored:
                    ++ignored_pairs;
                    break;
            }
        }
    }
    assert(accepted_pairs == 26u);
    assert(rejected_pairs == 68u);
    assert(ignored_pairs == 10u);
    assert(accepted_pairs + rejected_pairs + ignored_pairs == 104u);

    run_normal_launch_example();
    run_load_failure_example();
    run_duplicate_and_out_of_order_example();
    run_teardown_examples();
    run_generation_boundary_examples();
    run_runtime_clear_boundary_examples();
    run_reconnect_example();
    run_state_machine_properties();
    run_model_based_differential_test();

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

    lifecycle::MachineState poll_state{};
    poll_state.lifecycle = lifecycle::State::Running;
    poll_state.generation = 42u;
    const lifecycle::MachineTransitionResult poll = lifecycle::transition_runtime_poll(
        poll_state,
        lifecycle::runtime_poll_event(42u, 7034u),
        1u,
        0u);
    assert(poll.accepted());
    assert(poll.state.lifecycle == poll_state.lifecycle);
    assert(poll.state.generation == poll_state.generation);
    assert(poll.effects.count == 1u);
    assert(poll.effects.values[0].kind == lifecycle::EffectKind::PracticeLobbyDetailsRequested);

    lifecycle::MachineState initial_poll_state{};
    const lifecycle::MachineTransitionResult initial_poll = lifecycle::transition_runtime_poll(
        initial_poll_state,
        lifecycle::runtime_poll_event(0u, 7034u),
        1u,
        0u);
    assert(initial_poll.accepted());
    assert(initial_poll.effects.count == 1u);

    const lifecycle::MachineTransitionResult terminal_poll = lifecycle::transition_runtime_poll(
        poll_state,
        lifecycle::runtime_poll_event(42u, 7034u),
        2u,
        10u);
    assert(!terminal_poll.accepted());
    assert(terminal_poll.reason == lifecycle::DecisionReason::TerminalState);
    assert(terminal_poll.effects.empty());

    const lifecycle::MachineTransitionResult stale_poll = lifecycle::transition_runtime_poll(
        poll_state,
        lifecycle::runtime_poll_event(41u, 7034u),
        1u,
        0u);
    assert(!stale_poll.accepted());
    assert(stale_poll.reason == lifecycle::DecisionReason::StaleGeneration);
    assert(stale_poll.effects.empty());

    lifecycle::CustomGameRequestState custom_game{};
    custom_game.machine.generation = 12u;
    custom_game.lobby_state = 2u;
    custom_game.game_state = 0u;
    custom_game.launch_phase = 3u;
    custom_game.has_custom_game = true;
    lifecycle::CustomGameRequest ready_up{};
    ready_up.event = { lifecycle::EventKind::Run, lifecycle::EventSource::Direct, 7070u, 0u, 0u, 0u };
    ready_up.ready_state = 1u;
    const auto ready_up_result = lifecycle::transition_custom_game_request(custom_game, ready_up, 2u, 3u);
    assert(ready_up_result.accepted());
    assert(ready_up_result.state.lifecycle == lifecycle::State::Running);
    assert(ready_up_result.effects.count == 2u);
    assert(ready_up_result.effects.values[1].kind == lifecycle::EffectKind::CustomGameLifecycleActionsRequested);
    assert(ready_up_result.effects.contains(lifecycle::EffectKind::CustomGameLifecycleActionsRequested));
    assert(!ready_up_result.effects.contains(lifecycle::EffectKind::ReconnectQueued));

    custom_game.game_state = 1u;
    const auto duplicate_ready_up = lifecycle::transition_custom_game_request(custom_game, ready_up, 2u, 3u);
    assert(!duplicate_ready_up.accepted());
    assert(duplicate_ready_up.reason == lifecycle::DecisionReason::RequestIgnored);

    custom_game.game_state = 0u;
    custom_game.launch_phase = 2u;
    custom_game.has_launch_server_setup = true;
    lifecycle::CustomGameRequest loading{};
    loading.event = { lifecycle::EventKind::Loading, lifecycle::EventSource::Wrapped, 8052u, 0u, 0u, 0u };
    const auto loading_result = lifecycle::transition_custom_game_request(custom_game, loading, 2u, 3u);
    assert(loading_result.accepted());
    assert(loading_result.state.lifecycle == lifecycle::State::Loading);

    custom_game.has_launch_server_setup = false;
    custom_game.launch_phase = 3u;
    const auto loading_fallback = lifecycle::transition_custom_game_request(custom_game, loading, 2u, 3u);
    assert(loading_fallback.accepted());
    assert(loading_fallback.state.lifecycle == custom_game.machine.lifecycle);
    assert(loading_fallback.effects.count == 1u);

    lifecycle::CustomGameRequest loaded{};
    loaded.event = { lifecycle::EventKind::Loaded, lifecycle::EventSource::Direct, 8053u, 0u, 0u, 0u };
    const auto loaded_result = lifecycle::transition_custom_game_request(custom_game, loaded, 2u, 3u);
    assert(loaded_result.accepted());
    assert(loaded_result.state.lifecycle == lifecycle::State::Loaded);

    loaded.load_failed = true;
    const auto load_failed_result = lifecycle::transition_custom_game_request(custom_game, loaded, 2u, 3u);
    assert(load_failed_result.accepted());
    assert(load_failed_result.state.lifecycle == custom_game.machine.lifecycle);
    assert(load_failed_result.effects.count == 1u);

    loading.event.generation = 11u;
    const auto stale_custom_loading = lifecycle::transition_custom_game_request(custom_game, loading, 2u, 3u);
    assert(!stale_custom_loading.accepted());
    assert(stale_custom_loading.reason == lifecycle::DecisionReason::StaleGeneration);

    lifecycle::MachineState runtime_state{};
    runtime_state.lifecycle = lifecycle::State::Running;
    runtime_state.generation = 21u;
    const auto connected_member = lifecycle::transition_runtime_member(
        runtime_state,
        { 21u, 7001u, true, 86u, true });
    assert(connected_member.accepted());
    assert(connected_member.state.lifecycle == runtime_state.lifecycle);
    assert(connected_member.effects.contains(lifecycle::EffectKind::RuntimeMemberUpdateRequested));

    const auto invalid_member = lifecycle::transition_runtime_member(
        runtime_state,
        { 21u, 0u, true, 86u, true });
    assert(!invalid_member.accepted());
    assert(invalid_member.reason == lifecycle::DecisionReason::RequestIgnored);

    const auto stale_member = lifecycle::transition_runtime_member(
        runtime_state,
        { 20u, 7001u, false, 0u, false });
    assert(!stale_member.accepted());
    assert(stale_member.reason == lifecycle::DecisionReason::StaleGeneration);

    lifecycle::RuntimeGameStateRequest game_state_request{};
    game_state_request.generation = 21u;
    game_state_request.custom_game_launch = true;
    game_state_request.has_game_state = true;
    game_state_request.requested_game_state = 2u;
    game_state_request.lobby_state = 2u;
    game_state_request.current_game_state = 1u;
    game_state_request.launch_phase = 3u;
    const auto game_state_update = lifecycle::transition_runtime_game_state(
        runtime_state,
        game_state_request,
        3u);
    assert(game_state_update.accepted());
    assert(game_state_update.state.lifecycle == runtime_state.lifecycle);
    assert(game_state_update.effects.contains(lifecycle::EffectKind::RuntimeGameStateUpdateRequested));

    game_state_request.requested_game_state = 1u;
    const auto duplicate_game_state = lifecycle::transition_runtime_game_state(
        runtime_state,
        game_state_request,
        3u);
    assert(!duplicate_game_state.accepted());
    assert(duplicate_game_state.reason == lifecycle::DecisionReason::RequestIgnored);

    game_state_request.requested_game_state = 2u;
    game_state_request.generation = 20u;
    const auto stale_game_state = lifecycle::transition_runtime_game_state(
        runtime_state,
        game_state_request,
        3u);
    assert(!stale_game_state.accepted());
    assert(stale_game_state.reason == lifecycle::DecisionReason::StaleGeneration);

    lifecycle::MachineState teardown_state{};
    teardown_state.lifecycle = lifecycle::State::Running;
    teardown_state.generation = 31u;
    lifecycle::TeardownRequest leave_request{};
    leave_request.event = { lifecycle::EventKind::Leave, lifecycle::EventSource::Direct, 7040u, 31u, 0u, 0u };
    leave_request.active = true;
    const auto leave_teardown = lifecycle::transition_teardown(teardown_state, leave_request);
    assert(leave_teardown.accepted());
    assert(leave_teardown.state.lifecycle == teardown_state.lifecycle);
    assert(leave_teardown.state.generation == teardown_state.generation);
    assert(leave_teardown.effects.contains(lifecycle::EffectKind::TeardownLeaveInitiateRequested));
    assert(leave_teardown.effects.count == 1u);

    leave_request.pending = true;
    const auto duplicate_leave = lifecycle::transition_teardown(teardown_state, leave_request);
    assert(!duplicate_leave.accepted());
    assert(duplicate_leave.reason == lifecycle::DecisionReason::AlreadyInState);

    lifecycle::TeardownRequest abandon_finalize{};
    abandon_finalize.event = { lifecycle::EventKind::Abandon, lifecycle::EventSource::Internal, 0u, 31u, 0u, 0u };
    abandon_finalize.stage = lifecycle::TeardownStage::Finalize;
    abandon_finalize.active = true;
    abandon_finalize.pending = true;
    const auto finalized_abandon = lifecycle::transition_teardown(teardown_state, abandon_finalize);
    assert(finalized_abandon.accepted());
    assert(finalized_abandon.state.generation == teardown_state.generation);
    assert(finalized_abandon.effects.contains(lifecycle::EffectKind::TeardownAbandonFinalizeRequested));

    abandon_finalize.event.generation = 30u;
    const auto stale_finalize = lifecycle::transition_teardown(teardown_state, abandon_finalize);
    assert(!stale_finalize.accepted());
    assert(stale_finalize.reason == lifecycle::DecisionReason::StaleGeneration);

    abandon_finalize.event.generation = 31u;
    abandon_finalize.pending = false;
    const auto missing_finalize = lifecycle::transition_teardown(teardown_state, abandon_finalize);
    assert(!missing_finalize.accepted());
    assert(missing_finalize.reason == lifecycle::DecisionReason::AlreadyInState);

    teardown_state.generation = std::numeric_limits<std::uint64_t>::max();
    leave_request.event.generation = teardown_state.generation;
    leave_request.pending = false;
    const auto exhausted_teardown = lifecycle::transition_teardown(teardown_state, leave_request);
    assert(!exhausted_teardown.accepted());
    assert(exhausted_teardown.reason == lifecycle::DecisionReason::GenerationExhausted);

    std::cout << "gbe_dota_lifecycle_state_machine_test passed\n";
    return 0;
}

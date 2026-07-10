#include "dll/gbe_dota_reconnect_network.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

int assertions{};
int failures{};

void expect(bool condition, const char *label)
{
    ++assertions;
    if (condition)
        return;
    ++failures;
    std::cerr << "failed: " << label << std::endl;
}

GBE_DotaReconnectContext make_context(
    std::uint64_t lobby_id = 100,
    std::uint64_t server_id = 200,
    const char *endpoint = "10.20.30.40:27015",
    std::uint64_t generation = 1)
{
    GBE_DotaReconnectContext context{};
    context.generation = generation;
    context.lobby_id = lobby_id;
    context.server_id = server_id;
    context.lobby_state = 2;
    context.game_state = 2;
    context.custom_game_id = 300;
    context.owner_steam_id = 400;
    std::strncpy(context.connect, endpoint, sizeof(context.connect) - 1);
    return context;
}

struct FakeProvider final : GBE_DotaReconnectContextProvider {
    bool has_primary{true};
    bool has_recovery{};
    bool eligible{true};
    GBE_DotaReconnectContext primary{make_context()};
    GBE_DotaReconnectContext recovery{make_context()};
    std::vector<std::string> *events{};

    bool get_context(
        std::uint64_t,
        bool allow_generic_recovery,
        GBE_DotaReconnectContext &context) override
    {
        if (events)
            events->push_back(allow_generic_recovery ? "context:recovery" : "context:primary");
        if (allow_generic_recovery) {
            if (!has_recovery)
                return false;
            context = recovery;
            return true;
        }
        if (!has_primary)
            return false;
        context = primary;
        return true;
    }

    bool reconnect_eligible() const override
    {
        if (events)
            events->push_back("eligible");
        return eligible;
    }
};

struct FakeConnector final : GBE_DotaReconnectDirectConnector {
    std::uint32_t returned_connection{17};
    std::uint32_t calls{};
    SteamNetworkingIPAddr address{};
    std::vector<SteamNetworkingConfigValue_t> options;
    std::vector<std::string> *events{};

    std::uint32_t connect_by_ip_address(
        const SteamNetworkingIPAddr &candidate_address,
        int option_count,
        const SteamNetworkingConfigValue_t *candidate_options) override
    {
        ++calls;
        address = candidate_address;
        options.assign(candidate_options, candidate_options + option_count);
        if (events)
            events->push_back("connect");
        return returned_connection;
    }
};

struct FakeQueue final : GBE_DotaReconnectCallbackQueue {
    std::uint32_t calls{};
    GameServerChangeRequested_t callback{};
    double delay{-1.0};
    std::uint64_t generation{};
    std::vector<std::string> *events{};

    void queue_game_server_change(
        const GameServerChangeRequested_t &candidate_callback,
        double candidate_delay,
        std::uint64_t candidate_generation) override
    {
        ++calls;
        callback = candidate_callback;
        delay = candidate_delay;
        generation = candidate_generation;
        if (events)
            events->push_back("queue");
    }
};

GBE_DotaReconnectPostResult execute(
    FakeProvider &provider,
    FakeConnector &connector,
    FakeQueue &queue,
    GBE_DotaSerializedConnectionState &state,
    std::uint32_t payload_size = 64,
    std::uint64_t local_steam_id = 500)
{
    return GBE_ExecuteDotaReconnectPostConnectionState(
        local_steam_id,
        payload_size,
        provider,
        connector,
        queue,
        state);
}

void test_first_connection_contract()
{
    std::vector<std::string> events;
    FakeProvider provider;
    FakeConnector connector;
    FakeQueue queue;
    GBE_DotaSerializedConnectionState state;
    provider.events = &events;
    connector.events = &events;
    queue.events = &events;

    const auto result = execute(provider, connector, queue, state);
    expect(result.skip_reason == GBE_DotaReconnectPostSkipReason::None, "first connection accepted");
    expect(result.direct_connect_attempted && result.direct_connect_succeeded, "first direct connect succeeds");
    expect(result.callback_queued && !result.callback_already_queued, "first callback queued");
    expect(connector.calls == 1 && queue.calls == 1, "first path calls dependencies once");
    expect(connector.address.GetIPv4() == 0x0a141e28 && connector.address.m_port == 27015, "IPv4 endpoint parsed");
    expect(connector.options.size() == 3, "three connection options");
    expect(connector.options[0].m_eValue == k_ESteamNetworkingConfig_IP_AllowWithoutAuth && connector.options[0].m_val.m_int32 == 2, "allow without auth option");
    expect(connector.options[1].m_eValue == k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth && connector.options[1].m_val.m_int32 == 2, "localhost allow without auth option");
    expect(connector.options[2].m_eValue == k_ESteamNetworkingConfig_Unencrypted && connector.options[2].m_val.m_int32 == 2, "unencrypted option");
    expect(std::string(queue.callback.m_rgchServer) == "10.20.30.40:27015", "callback server body");
    expect(queue.callback.m_rgchPassword[0] == '\0' && queue.delay == 0.0, "callback password and delay");
    expect(queue.generation == provider.primary.generation, "callback receives reconnect generation");
    expect(events == std::vector<std::string>({"context:primary", "eligible", "connect", "queue"}), "production call order");
    expect(state.generation == provider.primary.generation && state.lobby_id == provider.primary.lobby_id, "serialized state receives context identities");
}

void test_prepare_reserves_state_before_unlocked_effects()
{
    std::vector<std::string> events;
    FakeProvider provider;
    FakeConnector connector;
    FakeQueue queue;
    GBE_DotaSerializedConnectionState state;
    provider.events = &events;
    connector.events = &events;
    queue.events = &events;

    auto plan = GBE_PrepareDotaReconnectPostConnectionState(500, 64, provider, state);
    expect(events == std::vector<std::string>({"context:primary", "eligible"}), "prepare phase excludes external effects");
    expect(connector.calls == 0 && queue.calls == 0, "prepare phase does not call connector or callback queue");
    expect(plan.connect_direct && plan.queue_callback, "prepare phase records both effects");
    expect(state.engine_callback_queued(provider.primary.server_id, provider.primary.connect), "prepare phase reserves callback dedup key");

    const auto result = GBE_ExecuteDotaReconnectPostEffects(std::move(plan), connector, queue);
    expect(result.direct_connect_succeeded && result.callback_queued, "effect phase executes reserved work");
    expect(events == std::vector<std::string>({"context:primary", "eligible", "connect", "queue"}), "effect phase preserves external call order");
}

void test_serialized_instance_synchronization_domains()
{
    GBE_DotaSerializedConnectionSynchronizer shared_domain;
    auto shared_lock = shared_domain.acquire();
    std::atomic<bool> shared_probe_acquired{};
    std::thread shared_probe([&] {
        shared_probe_acquired = shared_domain.try_acquire().owns_lock();
    });
    shared_probe.join();
    expect(!shared_probe_acquired.load(), "same serialized instance domain rejects overlapping operation");
    shared_lock.unlock();
    expect(shared_domain.try_acquire().owns_lock(), "same serialized instance domain accepts work after release");

    GBE_DotaSerializedConnectionSynchronizer first_domain;
    GBE_DotaSerializedConnectionSynchronizer second_domain;
    auto first_lock = first_domain.acquire();
    std::atomic<bool> second_probe_acquired{};
    std::thread second_probe([&] {
        second_probe_acquired = second_domain.try_acquire().owns_lock();
    });
    second_probe.join();
    expect(second_probe_acquired.load(), "separate serialized instance domains allow concurrent work");
}

void test_dedup_and_generation_changes()
{
    FakeProvider provider;
    FakeConnector connector;
    FakeQueue queue;
    GBE_DotaSerializedConnectionState state;

    execute(provider, connector, queue, state, 64);
    const auto duplicate = execute(provider, connector, queue, state, 80);
    expect(connector.calls == 1 && queue.calls == 1, "same generation endpoint deduplicated");
    expect(duplicate.callback_already_queued && duplicate.retry_count == 2 && duplicate.size_changed, "duplicate retry metadata");

    provider.primary = make_context(101, 200, "10.20.30.40:27015");
    const auto same_generation_new_lobby = execute(provider, connector, queue, state);
    expect(connector.calls == 1 && queue.calls == 1, "same generation reused endpoint remains deduplicated after lobby id sync");
    expect(same_generation_new_lobby.callback_already_queued && state.lobby_id == 101, "same generation synchronizes the new lobby id");

    provider.primary = make_context(101, 201, "10.20.30.40:27015");
    execute(provider, connector, queue, state);
    expect(connector.calls == 2 && queue.calls == 2, "server change reconnects");

    provider.primary = make_context(101, 201, "10.20.30.41:27016");
    execute(provider, connector, queue, state);
    expect(connector.calls == 3 && queue.calls == 3, "endpoint change reconnects");

    provider.primary = make_context(101, 201, "10.20.30.41:27016", 2);
    const auto same_lobby_new_generation = execute(provider, connector, queue, state);
    expect(state.generation == 2, "same lobby propagates a newer generation into serialized state");
    expect(connector.calls == 4 && queue.calls == 4, "same lobby new generation reconnects reused server endpoint");
    expect(same_lobby_new_generation.callback_queued && !same_lobby_new_generation.callback_already_queued, "same lobby new generation queues callback again");
    expect(same_lobby_new_generation.retry_count == 1 && same_lobby_new_generation.size_changed, "generation change clears retry and payload state");

    provider.primary.lobby_id = 102;
    const auto same_generation_new_lobby_id = execute(provider, connector, queue, state);
    expect(connector.calls == 4 && queue.calls == 4, "same generation server endpoint remains deduplicated across lobby id sync");
    expect(same_generation_new_lobby_id.callback_already_queued && state.lobby_id == 102, "lobby id synchronizes without resetting generation state");
}

void test_recovery_and_skip_paths()
{
    FakeConnector connector;
    FakeQueue queue;
    GBE_DotaSerializedConnectionState state;

    FakeProvider recovery_provider;
    recovery_provider.has_primary = false;
    recovery_provider.has_recovery = true;
    recovery_provider.recovery = make_context();
    const auto recovered = execute(recovery_provider, connector, queue, state);
    expect(recovered.callback_queued, "generic recovery context used");

    FakeProvider no_context;
    no_context.has_primary = false;
    no_context.has_recovery = false;
    GBE_DotaSerializedConnectionState no_context_state;
    expect(execute(no_context, connector, queue, no_context_state).skip_reason == GBE_DotaReconnectPostSkipReason::NoContext, "missing context skipped");

    FakeProvider ordinary;
    ordinary.primary.custom_game_id = 0;
    GBE_DotaSerializedConnectionState ordinary_state;
    expect(execute(ordinary, connector, queue, ordinary_state).skip_reason == GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby, "ordinary lobby skipped");

    FakeProvider ineligible;
    ineligible.eligible = false;
    GBE_DotaSerializedConnectionState ineligible_state;
    expect(execute(ineligible, connector, queue, ineligible_state).skip_reason == GBE_DotaReconnectPostSkipReason::ReconnectIneligible, "ineligible reconnect skipped");

    FakeProvider owner;
    owner.primary.owner_steam_id = 500;
    GBE_DotaSerializedConnectionState owner_state;
    expect(execute(owner, connector, queue, owner_state).skip_reason == GBE_DotaReconnectPostSkipReason::LocalOwner, "local owner skipped");
}

void test_parse_and_connect_failures()
{
    FakeProvider provider;
    provider.primary = make_context(100, 200, "invalid-endpoint");
    FakeConnector connector;
    FakeQueue queue;
    GBE_DotaSerializedConnectionState state;

    const auto first = execute(provider, connector, queue, state);
    const auto second = execute(provider, connector, queue, state);
    expect(first.direct_connect_parse_failed && second.direct_connect_parse_failed, "parse failure remains retryable");
    expect(connector.calls == 0 && queue.calls == 1, "parse failure queues engine fallback once");

    FakeProvider failed_provider;
    FakeConnector failed_connector;
    FakeQueue failed_queue;
    GBE_DotaSerializedConnectionState failed_state;
    failed_connector.returned_connection = 0;
    const auto failed = execute(failed_provider, failed_connector, failed_queue, failed_state);
    execute(failed_provider, failed_connector, failed_queue, failed_state);
    expect(failed.direct_connect_attempted && !failed.direct_connect_succeeded, "invalid connection result reported");
    expect(failed_connector.calls == 1 && failed_queue.calls == 1, "completed failed attempt follows dedup policy");
}

void test_diagnostic_reason_and_source_serialization()
{
    namespace diagnostic = gbe::dota_diagnostic;

    const std::pair<diagnostic::Reason, const char *> reasons[] = {
        {diagnostic::Reason::Unknown, "unknown"},
        {diagnostic::Reason::None, "none"},
        {diagnostic::Reason::Selected, "selected"},
        {diagnostic::Reason::InvalidSource, "invalid_source"},
        {diagnostic::Reason::Inactive, "inactive"},
        {diagnostic::Reason::GameNotStarted, "game_not_started"},
        {diagnostic::Reason::MissingServerId, "missing_server_id"},
        {diagnostic::Reason::MissingEndpoint, "missing_endpoint"},
        {diagnostic::Reason::NoEligibleSource, "no_eligible_source"},
        {diagnostic::Reason::NoContext, "no_context"},
        {diagnostic::Reason::OrdinaryPracticeLobby, "ordinary_practice_lobby"},
        {diagnostic::Reason::ReconnectIneligible, "reconnect_ineligible"},
        {diagnostic::Reason::StateNotReady, "state_not_ready"},
        {diagnostic::Reason::LocalOwner, "local_owner"},
        {diagnostic::Reason::DisconnectCurrentGameAfterCacheUnsubscribed, "7035_disconnect_current_game_after_25"},
        {diagnostic::Reason::CustomRuntimeMemberRefresh, "7034_custom_runtime_member_refresh"},
        {diagnostic::Reason::LeaveChat, "7272_leave_chat"},
        {diagnostic::Reason::FinishedLoading, "8053_finished_loading"},
        {diagnostic::Reason::LoadFailed, "8053_load_failed"},
        {diagnostic::Reason::LaunchPoll, "7034_launch_poll"},
        {diagnostic::Reason::ParseFailed, "parse_failed"},
        {diagnostic::Reason::AlreadyQueued, "already_queued"},
        {diagnostic::Reason::StaleGeneration, "stale_generation"},
        {diagnostic::Reason::PreviousActionFailed, "previous_action_failed"},
        {diagnostic::Reason::RuntimeUpdateQueued, "runtime_update_queued"},
        {diagnostic::Reason::ActionFailed, "action_failed"},
    };
    for (const auto &entry : reasons) {
        expect(diagnostic::describe_reason(entry.first) == entry.second, "diagnostic reason serialization is stable");
        if (entry.first != diagnostic::Reason::Unknown)
            expect(diagnostic::reason_from_string(entry.second) == entry.first, "diagnostic reason parsing round-trips");
    }
    expect(diagnostic::reason_from_string("unregistered_reason") == diagnostic::Reason::Unknown, "unknown diagnostic reason falls back");

    const std::pair<diagnostic::Source, const char *> sources[] = {
        {diagnostic::Source::Unknown, "unknown"},
        {diagnostic::Source::Shared, "shared"},
        {diagnostic::Source::Recent, "recent"},
        {diagnostic::Source::Local, "local"},
        {diagnostic::Source::GenericRecovery, "generic_recovery"},
        {diagnostic::Source::Direct, "direct"},
        {diagnostic::Source::Wrapped, "wrapped"},
        {diagnostic::Source::DelayedTask, "delayed_task"},
        {diagnostic::Source::SerializedState, "serialized_state"},
        {diagnostic::Source::CallbackQueue, "callback_queue"},
    };
    for (const auto &entry : sources) {
        expect(diagnostic::describe_source(entry.first) == entry.second, "diagnostic source serialization is stable");
        if (entry.first != diagnostic::Source::Unknown)
            expect(diagnostic::source_from_string(entry.second) == entry.first, "diagnostic source parsing round-trips");
    }
    expect(diagnostic::source_from_string("unregistered_source") == diagnostic::Source::Unknown, "unknown diagnostic source falls back");

    expect(std::string(GBE_DescribeDotaReconnectPostSkipReason(GBE_DotaReconnectPostSkipReason::NoContext)) == "no_context", "legacy skip reason describe stays stable");
    expect(std::string(GBE_DescribeDotaReconnectPostSkipReason(GBE_DotaReconnectPostSkipReason::MissingEndpoint)) == "missing_endpoint", "legacy missing endpoint describe stays stable");

    diagnostic::Event event{
        "reconnect.callback",
        diagnostic::Reason::AlreadyQueued,
        diagnostic::Source::CallbackQueue,
        42u,
        7u,
        99u,
        "10.20.30.40:27015",
        "deduplicated",
    };
    event = diagnostic::with_message_id(event, 0u);
    event = diagnostic::with_job_id(event, 0u);
    const std::string formatted = diagnostic::format_event(event);
    expect(
        formatted == "event=reconnect.callback reason=already_queued source=callback_queue lobby_id=42 generation=7 server_id=99 endpoint=10.20.30.40:27015 decision=deduplicated message_id=0 job_id=0",
        "diagnostic event format and field order are stable");
    expect(formatted.find("payload") == std::string::npos && formatted.find("password") == std::string::npos && formatted.find("endpoint_raw") == std::string::npos, "diagnostic event omits sensitive and raw payload fields");

    const std::string minimal = diagnostic::format_event({"reconnect.context_selection"});
    expect(
        minimal == "event=reconnect.context_selection reason=unknown source=unknown lobby_id=0 generation=0 server_id=0 endpoint=- decision=- message_id=- job_id=-",
        "diagnostic event absent fields use stable placeholders");

    diagnostic::Event transition{
        "lifecycle.transition_decision",
        diagnostic::Reason::FinishedLoading,
        diagnostic::Source::Wrapped,
        7001u,
        12u,
        9001u,
        {},
        "planned",
    };
    transition = diagnostic::with_message_id(transition, 8053u);
    transition = diagnostic::with_job_id(transition, 0x8053ABCDu);
    expect(
        diagnostic::format_event(transition) == "event=lifecycle.transition_decision reason=8053_finished_loading source=wrapped lobby_id=7001 generation=12 server_id=9001 endpoint=- decision=planned message_id=8053 job_id=2152967117",
        "lifecycle transition event includes stable identity and request fields");

    expect(
        diagnostic::format_event({
            "lifecycle.action_execution",
            diagnostic::Reason::None,
            diagnostic::Source::Direct,
            7001u,
            12u,
            9001u,
            {},
            "shared_lobby_publish",
        }) == "event=lifecycle.action_execution reason=none source=direct lobby_id=7001 generation=12 server_id=9001 endpoint=- decision=shared_lobby_publish message_id=- job_id=-",
        "lifecycle action execution event includes stable action decision");

    expect(
        diagnostic::format_event({
            "lifecycle.action_skip",
            diagnostic::Reason::PreviousActionFailed,
            diagnostic::Source::Direct,
            7001u,
            12u,
            9001u,
            {},
            "shared_lobby_publish",
        }) == "event=lifecycle.action_skip reason=previous_action_failed source=direct lobby_id=7001 generation=12 server_id=9001 endpoint=- decision=shared_lobby_publish message_id=- job_id=-",
        "lifecycle conditional skip reason remains stable");

    expect(
        diagnostic::format_event({
            "lifecycle.action_failure",
            diagnostic::Reason::ActionFailed,
            diagnostic::Source::Wrapped,
            7001u,
            12u,
            9001u,
            {},
            "practice_lobby_details_update",
        }) == "event=lifecycle.action_failure reason=action_failed source=wrapped lobby_id=7001 generation=12 server_id=9001 endpoint=- decision=practice_lobby_details_update message_id=- job_id=-",
        "lifecycle action failure reason and source remain stable");

    diagnostic::Event delayed{
        "lifecycle.delayed_task_queued",
        diagnostic::Reason::CustomRuntimeMemberRefresh,
        diagnostic::Source::DelayedTask,
        7001u,
        12u,
        9001u,
        {},
        "runtime_lobby_details_update",
    };
    delayed = diagnostic::with_message_id(delayed, 7034u);
    delayed = diagnostic::with_job_id(delayed, 0x7034u);
    const std::string delayed_formatted = diagnostic::format_event(delayed);
    expect(
        delayed_formatted == "event=lifecycle.delayed_task_queued reason=7034_custom_runtime_member_refresh source=delayed_task lobby_id=7001 generation=12 server_id=9001 endpoint=- decision=runtime_lobby_details_update message_id=7034 job_id=28724",
        "lifecycle delayed task event includes message and job identity");

    const char *forbidden_fields[] = {
        "payload=",
        "session=",
        "password=",
        "raw_state=",
        "endpoint_raw=",
        "reason_text=",
    };
    for (const char *field : forbidden_fields)
        expect(delayed_formatted.find(field) == std::string::npos, "structured diagnostic event excludes sensitive free-form fields");
}

void test_properties()
{
    for (std::uint64_t seed = 1; seed <= 64; ++seed) {
        FakeProvider provider;
        provider.primary = make_context(1000 + seed, 2000 + seed, "10.1.2.3:27015");
        FakeConnector connector;
        FakeQueue queue;
        GBE_DotaSerializedConnectionState state;
        for (int repeat = 0; repeat < 5; ++repeat)
            execute(provider, connector, queue, state, static_cast<std::uint32_t>(seed + repeat));
        expect(queue.calls == 1, "P8-A callback at most once per generation key");

        provider.primary.generation += 1;
        execute(provider, connector, queue, state);
        expect(connector.calls == 2 && queue.calls == 2, "P8-B generation change restores connection opportunity");
    }

    FakeProvider provider_a;
    FakeProvider provider_b;
    FakeConnector connector_a;
    FakeConnector connector_b;
    FakeQueue queue_a;
    FakeQueue queue_b;
    GBE_DotaSerializedConnectionState state_a;
    GBE_DotaSerializedConnectionState state_b;
    execute(provider_a, connector_a, queue_a, state_a);
    execute(provider_a, connector_a, queue_a, state_a);
    execute(provider_b, connector_b, queue_b, state_b);
    expect(connector_a.calls == 1 && queue_a.calls == 1, "P8-C first instance remains deduplicated");
    expect(connector_b.calls == 1 && queue_b.calls == 1, "P8-C second instance remains independent");
}

} // namespace

int main()
{
    test_first_connection_contract();
    test_prepare_reserves_state_before_unlocked_effects();
    test_serialized_instance_synchronization_domains();
    test_dedup_and_generation_changes();
    test_recovery_and_skip_paths();
    test_parse_and_connect_failures();
    test_diagnostic_reason_and_source_serialization();
    test_properties();
    std::cout << "reconnect network assertions: " << assertions - failures << "/" << assertions << std::endl;
    return failures == 0 ? 0 : 1;
}

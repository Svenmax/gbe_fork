#include "dll/gbe_dota_reconnect_network.h"

#include <cstring>
#include <iostream>
#include <string>
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
    const char *endpoint = "10.20.30.40:27015")
{
    GBE_DotaReconnectContext context{};
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
    std::vector<std::string> *events{};

    void queue_game_server_change(
        const GameServerChangeRequested_t &candidate_callback,
        double candidate_delay) override
    {
        ++calls;
        callback = candidate_callback;
        delay = candidate_delay;
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
    expect(events == std::vector<std::string>({"context:primary", "eligible", "connect", "queue"}), "production call order");
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
    execute(provider, connector, queue, state);
    expect(connector.calls == 2 && queue.calls == 2, "new lobby generation reconnects reused endpoint");

    provider.primary = make_context(101, 201, "10.20.30.40:27015");
    execute(provider, connector, queue, state);
    expect(connector.calls == 3 && queue.calls == 3, "server change reconnects");

    provider.primary = make_context(101, 201, "10.20.30.41:27016");
    execute(provider, connector, queue, state);
    expect(connector.calls == 4 && queue.calls == 4, "endpoint change reconnects");
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

        provider.primary.lobby_id += 1;
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
    test_dedup_and_generation_changes();
    test_recovery_and_skip_paths();
    test_parse_and_connect_failures();
    test_properties();
    std::cout << "reconnect network assertions: " << assertions - failures << "/" << assertions << std::endl;
    return failures == 0 ? 0 : 1;
}

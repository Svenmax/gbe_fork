#include "dll/gbe_dota_composition_root.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

struct FakeLifecycleExecutor final : gbe::dota::LifecycleExecutor {
};

struct FakeCallbackScheduler final : gbe::dota::CallbackScheduler {
};

struct FakeContextProvider final : GBE_DotaReconnectContextProvider {
    bool get_context(std::uint64_t, bool, GBE_DotaReconnectContext &context) override
    {
        ++get_context_calls;
        context = next_context;
        return has_context;
    }

    bool reconnect_eligible() const override
    {
        ++eligible_calls;
        return eligible;
    }

    bool has_context{true};
    bool eligible{true};
    GBE_DotaReconnectContext next_context{};
    int get_context_calls{};
    mutable int eligible_calls{};
};

struct FakeDirectConnector final : GBE_DotaReconnectDirectConnector {
    std::uint32_t connect_by_ip_address(
        const SteamNetworkingIPAddr &,
        int,
        const SteamNetworkingConfigValue_t *) override
    {
        ++calls;
        return connection;
    }

    int calls{};
    std::uint32_t connection{77u};
};

struct FakeCallbackQueue final : GBE_DotaReconnectCallbackQueue {
    void queue_game_server_change(
        const GameServerChangeRequested_t &,
        double,
        std::uint64_t generation) override
    {
        ++calls;
        last_generation = generation;
    }

    int calls{};
    std::uint64_t last_generation{};
};

struct Fixture {
    gbe::dota_handler_registry::Entry registry_entries[2]{};
    FakeLifecycleExecutor *lifecycle_executor{};
    FakeCallbackScheduler *client_scheduler{};
    FakeCallbackScheduler *server_scheduler{};
    FakeContextProvider *client_context_provider{};
    FakeDirectConnector *client_direct_connector{};
    FakeCallbackQueue *client_callback_queue{};
    FakeContextProvider *server_context_provider{};
    FakeDirectConnector *server_direct_connector{};
    FakeCallbackQueue *server_callback_queue{};
    std::unique_ptr<gbe::dota::CompositionRoot> root;

    Fixture()
    {
        auto lifecycle = std::make_unique<FakeLifecycleExecutor>();
        lifecycle_executor = lifecycle.get();
        auto client_scheduler_owner = std::make_unique<FakeCallbackScheduler>();
        client_scheduler = client_scheduler_owner.get();
        auto client_context_owner = std::make_unique<FakeContextProvider>();
        client_context_provider = client_context_owner.get();
        auto client_direct_owner = std::make_unique<FakeDirectConnector>();
        client_direct_connector = client_direct_owner.get();
        auto client_queue_owner = std::make_unique<FakeCallbackQueue>();
        client_callback_queue = client_queue_owner.get();
        auto server_scheduler_owner = std::make_unique<FakeCallbackScheduler>();
        server_scheduler = server_scheduler_owner.get();
        auto server_context_owner = std::make_unique<FakeContextProvider>();
        server_context_provider = server_context_owner.get();
        auto server_direct_owner = std::make_unique<FakeDirectConnector>();
        server_direct_connector = server_direct_owner.get();
        auto server_queue_owner = std::make_unique<FakeCallbackQueue>();
        server_callback_queue = server_queue_owner.get();

        root = std::make_unique<gbe::dota::CompositionRoot>(
            std::move(lifecycle),
            gbe::dota::HandlerRegistryView{registry_entries, 2u},
            gbe::dota::RoleDependencies{
                std::move(client_scheduler_owner),
                std::move(client_context_owner),
                std::move(client_direct_owner),
                std::move(client_queue_owner)},
            gbe::dota::RoleDependencies{
                std::move(server_scheduler_owner),
                std::move(server_context_owner),
                std::move(server_direct_owner),
                std::move(server_queue_owner)});
    }
};

void test_root_binds_all_application_dependencies()
{
    Fixture fixture;

    expect_true(&fixture.root->lifecycle_executor() == fixture.lifecycle_executor, "root owns lifecycle executor");
    expect_true(fixture.root->handler_registry().entries != fixture.registry_entries, "root owns a registry copy");
    expect_true(fixture.root->handler_registry().size == 2u, "root binds registry size");
    expect_true(&fixture.root->client().callback_scheduler() == fixture.client_scheduler, "client role owns callback scheduler");
    expect_true(&fixture.root->server().callback_scheduler() == fixture.server_scheduler, "server role owns callback scheduler");
    expect_true(&fixture.root->client().reconnect_service().context_provider() == fixture.client_context_provider, "client role owns reconnect context provider");
    expect_true(&fixture.root->server().reconnect_service().direct_connector() == fixture.server_direct_connector, "server role owns reconnect connector");
}

void test_construction_does_not_execute_services()
{
    Fixture fixture;

    expect_true(fixture.client_context_provider->get_context_calls == 0, "root construction does not query client reconnect context");
    expect_true(fixture.client_context_provider->eligible_calls == 0, "root construction does not query client reconnect eligibility");
    expect_true(fixture.client_direct_connector->calls == 0, "root construction does not invoke client network effects");
    expect_true(fixture.client_callback_queue->calls == 0, "root construction does not enqueue client callbacks");
    expect_true(fixture.server_context_provider->get_context_calls == 0, "root construction does not query server reconnect context");
    expect_true(fixture.server_context_provider->eligible_calls == 0, "root construction does not query server reconnect eligibility");
    expect_true(fixture.server_direct_connector->calls == 0, "root construction does not invoke server network effects");
    expect_true(fixture.server_callback_queue->calls == 0, "root construction does not enqueue server callbacks");
}

void test_root_owns_shared_lobby_state()
{
    Fixture fixture;
    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.generation = 9u;
    state.lobby_id = 101u;
    fixture.root->lobby_store().publish(state);

    const auto snapshot = fixture.root->lobby_store().snapshot();
    expect_true(snapshot.valid, "root store publishes valid state");
    expect_true(snapshot.generation == 9u, "root store preserves generation");
    expect_true(snapshot.lobby_id == 101u, "root store preserves lobby id");
    expect_true(&fixture.root->client().lobby_store() == &fixture.root->server().lobby_store(), "roles share one application lobby store");
}

void test_roots_isolate_owned_state()
{
    Fixture first;
    Fixture second;
    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.lobby_id = 202u;
    first.root->lobby_store().publish(state);

    expect_true(first.root->lobby_store().snapshot().valid, "first root stores its lobby state");
    expect_true(!second.root->lobby_store().snapshot().valid, "second root starts with isolated lobby state");
}

void test_reconnect_service_uses_bound_adapters()
{
    Fixture fixture;
    fixture.client_context_provider->next_context.generation = 12u;
    fixture.client_context_provider->next_context.server_id = 44u;
    fixture.client_context_provider->next_context.lobby_state = 2u;
    fixture.client_context_provider->next_context.game_state = 2u;
    fixture.client_context_provider->next_context.custom_game_id = 55u;
    fixture.client_context_provider->next_context.owner_steam_id = 76561198000000002ull;
    std::snprintf(
        fixture.client_context_provider->next_context.connect,
        sizeof(fixture.client_context_provider->next_context.connect),
        "%s",
        "127.0.0.1:27015");

    GBE_DotaSerializedConnectionState connection_state;
    const auto result = fixture.root->client().reconnect_service().execute_post_connection_state(
        76561198000000001ull,
        32u,
        connection_state);

    expect_true(result.has_context, "reconnect service obtains context from bound provider");
    expect_true(fixture.client_direct_connector->calls == 1, "reconnect service uses owned direct connector");
    expect_true(fixture.client_callback_queue->calls == 1, "reconnect service uses owned callback queue");
    expect_true(fixture.client_callback_queue->last_generation == 12u, "reconnect service forwards generation");
}

} // namespace

int main()
{
    test_root_binds_all_application_dependencies();
    test_construction_does_not_execute_services();
    test_root_owns_shared_lobby_state();
    test_roots_isolate_owned_state();
    test_reconnect_service_uses_bound_adapters();

    if (failures != 0) {
        std::fprintf(stderr, "gbe_dota_composition_root_test failed: %d\n", failures);
        return 1;
    }

    std::puts("gbe_dota_composition_root_test passed");
    return 0;
}

#include "dll/gbe_dota_composition_root.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

struct LifetimeState {
    int live_dependencies{};
    int destroyed_dependencies{};
    int delayed_effects{};
    int callbacks{};
};

struct FakeLifecycleExecutor final : gbe::dota::LifecycleExecutor {
    explicit FakeLifecycleExecutor(std::shared_ptr<LifetimeState> lifetime = {})
        : lifetime(std::move(lifetime))
    {
        if (this->lifetime)
            ++this->lifetime->live_dependencies;
    }

    ~FakeLifecycleExecutor() override
    {
        if (lifetime) {
            --lifetime->live_dependencies;
            ++lifetime->destroyed_dependencies;
        }
    }

    std::shared_ptr<LifetimeState> lifetime;
};

struct FakeCallbackScheduler final : gbe::dota::CallbackScheduler {
    explicit FakeCallbackScheduler(std::uint32_t identity = 0u, std::shared_ptr<LifetimeState> lifetime = {})
        : identity(identity), lifetime(std::move(lifetime))
    {
        if (this->lifetime)
            ++this->lifetime->live_dependencies;
    }

    ~FakeCallbackScheduler() override
    {
        if (lifetime) {
            --lifetime->live_dependencies;
            ++lifetime->destroyed_dependencies;
        }
    }

    std::uint32_t identity{};
    std::shared_ptr<LifetimeState> lifetime;
};

struct FakeContextProvider final : GBE_DotaReconnectContextProvider {
    explicit FakeContextProvider(std::shared_ptr<LifetimeState> lifetime = {})
        : lifetime(std::move(lifetime))
    {
        if (this->lifetime)
            ++this->lifetime->live_dependencies;
    }

    ~FakeContextProvider() override
    {
        if (lifetime) {
            --lifetime->live_dependencies;
            ++lifetime->destroyed_dependencies;
        }
    }

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
    std::shared_ptr<LifetimeState> lifetime;
};

struct FakeDirectConnector final : GBE_DotaReconnectDirectConnector {
    explicit FakeDirectConnector(std::shared_ptr<LifetimeState> lifetime = {})
        : lifetime(std::move(lifetime))
    {
        if (this->lifetime)
            ++this->lifetime->live_dependencies;
    }

    ~FakeDirectConnector() override
    {
        if (lifetime) {
            --lifetime->live_dependencies;
            ++lifetime->destroyed_dependencies;
        }
    }

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
    std::shared_ptr<LifetimeState> lifetime;
};

struct FakeCallbackQueue final : GBE_DotaReconnectCallbackQueue {
    explicit FakeCallbackQueue(std::shared_ptr<LifetimeState> lifetime = {})
        : lifetime(std::move(lifetime))
    {
        if (this->lifetime)
            ++this->lifetime->live_dependencies;
    }

    ~FakeCallbackQueue() override
    {
        if (lifetime) {
            --lifetime->live_dependencies;
            ++lifetime->destroyed_dependencies;
        }
    }

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
    std::shared_ptr<LifetimeState> lifetime;
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
    std::shared_ptr<LifetimeState> lifetime;

    explicit Fixture(std::uint32_t identity = 0u, std::shared_ptr<LifetimeState> lifetime = {})
        : lifetime(std::move(lifetime))
    {
        auto lifecycle = std::make_unique<FakeLifecycleExecutor>(this->lifetime);
        lifecycle_executor = lifecycle.get();
        auto client_scheduler_owner = std::make_unique<FakeCallbackScheduler>(identity + 1u, this->lifetime);
        client_scheduler = client_scheduler_owner.get();
        auto client_context_owner = std::make_unique<FakeContextProvider>(this->lifetime);
        client_context_provider = client_context_owner.get();
        auto client_direct_owner = std::make_unique<FakeDirectConnector>(this->lifetime);
        client_direct_connector = client_direct_owner.get();
        auto client_queue_owner = std::make_unique<FakeCallbackQueue>(this->lifetime);
        client_callback_queue = client_queue_owner.get();
        auto server_scheduler_owner = std::make_unique<FakeCallbackScheduler>(identity + 2u, this->lifetime);
        server_scheduler = server_scheduler_owner.get();
        auto server_context_owner = std::make_unique<FakeContextProvider>(this->lifetime);
        server_context_provider = server_context_owner.get();
        auto server_direct_owner = std::make_unique<FakeDirectConnector>(this->lifetime);
        server_direct_connector = server_direct_owner.get();
        auto server_queue_owner = std::make_unique<FakeCallbackQueue>(this->lifetime);
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

void configure_reconnect_context(FakeContextProvider &provider, std::uint64_t generation)
{
    provider.next_context.generation = generation;
    provider.next_context.server_id = 44u;
    provider.next_context.lobby_state = 2u;
    provider.next_context.game_state = 2u;
    provider.next_context.custom_game_id = 55u;
    provider.next_context.owner_steam_id = 76561198000000002ull;
    std::snprintf(
        provider.next_context.connect,
        sizeof(provider.next_context.connect),
        "%s",
        "127.0.0.1:27015");
}

GBE_DotaReconnectPostResult execute_reconnect(gbe::dota::RoleContext &role)
{
    GBE_DotaSerializedConnectionState connection_state;
    return role.reconnect_service().execute_post_connection_state(
        76561198000000001ull,
        32u,
        connection_state);
}

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
    Fixture first(100u);
    Fixture second(200u);
    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.lobby_id = 202u;
    first.root->lobby_store().publish(state);

    expect_true(first.root->lobby_store().snapshot().valid, "first root stores its lobby state");
    expect_true(!second.root->lobby_store().snapshot().valid, "second root starts with isolated lobby state");
    expect_true(first.client_scheduler != second.client_scheduler, "roots own distinct client callback schedulers");
    expect_true(first.server_scheduler != second.server_scheduler, "roots own distinct gameserver callback schedulers");
    expect_true(first.client_context_provider != second.client_context_provider, "roots own distinct reconnect context providers");
    expect_true(first.client_direct_connector != second.client_direct_connector, "roots own distinct reconnect connectors");
    expect_true(first.client_callback_queue != second.client_callback_queue, "roots own distinct reconnect callback queues");
}

void test_client_assembly_uses_client_dependencies_only()
{
    Fixture fixture;
    configure_reconnect_context(*fixture.client_context_provider, 12u);
    const auto result = execute_reconnect(fixture.root->client());

    expect_true(result.has_context, "reconnect service obtains context from bound provider");
    expect_true(fixture.client_direct_connector->calls == 1, "reconnect service uses owned direct connector");
    expect_true(fixture.client_callback_queue->calls == 1, "reconnect service uses owned callback queue");
    expect_true(fixture.client_callback_queue->last_generation == 12u, "reconnect service forwards generation");
    expect_true(fixture.server_context_provider->get_context_calls == 0, "client assembly does not query gameserver context");
    expect_true(fixture.server_direct_connector->calls == 0, "client assembly does not use gameserver connector");
    expect_true(fixture.server_callback_queue->calls == 0, "client assembly does not use gameserver callback queue");
}

void test_gameserver_assembly_uses_gameserver_dependencies_only()
{
    Fixture fixture;
    configure_reconnect_context(*fixture.server_context_provider, 23u);
    const auto result = execute_reconnect(fixture.root->server());

    expect_true(result.has_context, "gameserver assembly obtains its reconnect context");
    expect_true(fixture.server_direct_connector->calls == 1, "gameserver assembly uses owned direct connector");
    expect_true(fixture.server_callback_queue->calls == 1, "gameserver assembly uses owned callback queue");
    expect_true(fixture.server_callback_queue->last_generation == 23u, "gameserver assembly forwards generation");
    expect_true(fixture.client_context_provider->get_context_calls == 0, "gameserver assembly does not query client context");
    expect_true(fixture.client_direct_connector->calls == 0, "gameserver assembly does not use client connector");
    expect_true(fixture.client_callback_queue->calls == 0, "gameserver assembly does not use client callback queue");
}

void test_offline_fake_assemblies_are_isolated()
{
    Fixture first(300u);
    Fixture second(400u);
    configure_reconnect_context(*first.client_context_provider, 31u);
    configure_reconnect_context(*second.client_context_provider, 41u);

    execute_reconnect(first.root->client());

    expect_true(first.client_direct_connector->calls == 1, "first offline fake records its network effect");
    expect_true(first.client_callback_queue->last_generation == 31u, "first offline fake records its generation");
    expect_true(second.client_context_provider->get_context_calls == 0, "second offline fake context remains untouched");
    expect_true(second.client_direct_connector->calls == 0, "second offline fake connector remains untouched");
    expect_true(second.client_callback_queue->calls == 0, "second offline fake callback queue remains untouched");
    expect_true(first.client_scheduler->identity == 301u, "first offline fake owns its callback scheduler identity");
    expect_true(second.client_scheduler->identity == 401u, "second offline fake owns its callback scheduler identity");
}

void test_delayed_work_expires_with_root_dependencies()
{
    auto lifetime = std::make_shared<LifetimeState>();
    std::vector<std::function<void()>> delayed_work;
    std::vector<std::function<void()>> callbacks;
    {
        Fixture fixture(500u, lifetime);
        expect_true(lifetime->live_dependencies == 9, "root owns all lifecycle-tested dependencies");
        std::weak_ptr<LifetimeState> weak_lifetime = lifetime;
        delayed_work.emplace_back([weak_lifetime]() {
            if (auto state = weak_lifetime.lock(); state && state->live_dependencies > 0)
                ++state->delayed_effects;
        });
        callbacks.emplace_back([weak_lifetime]() {
            if (auto state = weak_lifetime.lock(); state && state->live_dependencies > 0)
                ++state->callbacks;
        });
    }

    expect_true(lifetime->live_dependencies == 0, "root destruction releases every owned dependency");
    expect_true(lifetime->destroyed_dependencies == 9, "every owned dependency is destroyed exactly once");
    delayed_work.front()();
    callbacks.front()();
    expect_true(lifetime->delayed_effects == 0, "delayed work becomes inert after dependency destruction");
    expect_true(lifetime->callbacks == 0, "callback becomes inert after dependency destruction");
}

void test_recreated_root_starts_without_previous_state()
{
    {
        Fixture first(600u);
        GBE_SharedDotaLobbyState state;
        state.valid = true;
        state.generation = 61u;
        state.lobby_id = 601u;
        first.root->lobby_store().publish(state);
        configure_reconnect_context(*first.client_context_provider, 62u);
        execute_reconnect(first.root->client());
        expect_true(first.client_callback_queue->calls == 1, "first root records callback activity before destruction");
    }

    Fixture second(700u);
    const auto snapshot = second.root->lobby_store().snapshot();
    expect_true(!snapshot.valid, "recreated root starts with empty lobby state");
    expect_true(second.client_context_provider->get_context_calls == 0, "recreated root starts with unused reconnect context");
    expect_true(second.client_direct_connector->calls == 0, "recreated root starts with unused direct connector");
    expect_true(second.client_callback_queue->calls == 0, "recreated root starts with an empty callback queue history");
    expect_true(second.client_callback_queue->last_generation == 0u, "recreated root does not inherit callback generation");
}

} // namespace

int main()
{
    test_root_binds_all_application_dependencies();
    test_construction_does_not_execute_services();
    test_root_owns_shared_lobby_state();
    test_roots_isolate_owned_state();
    test_client_assembly_uses_client_dependencies_only();
    test_gameserver_assembly_uses_gameserver_dependencies_only();
    test_offline_fake_assemblies_are_isolated();
    test_delayed_work_expires_with_root_dependencies();
    test_recreated_root_starts_without_previous_state();

    if (failures != 0) {
        std::fprintf(stderr, "gbe_dota_composition_root_test failed: %d\n", failures);
        return 1;
    }

    std::puts("gbe_dota_composition_root_test passed");
    return 0;
}

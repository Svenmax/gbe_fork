#ifndef GBE_DOTA_COMPOSITION_ROOT_H
#define GBE_DOTA_COMPOSITION_ROOT_H

#include "gbe_dota_handler_registry.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_reconnect_network.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace gbe::dota {

class LifecycleExecutor {
public:
    virtual ~LifecycleExecutor() = default;
};

class CallbackScheduler {
public:
    virtual ~CallbackScheduler() = default;
};

struct HandlerRegistryView {
    const dota_handler_registry::Entry *entries{};
    std::size_t size{};
};

class ReconnectService {
public:
    ReconnectService(
        GBE_DotaReconnectContextProvider &context_provider,
        GBE_DotaReconnectDirectConnector &direct_connector,
        GBE_DotaReconnectCallbackQueue &callback_queue);

    GBE_DotaReconnectPostPlan prepare_post_connection_state(
        std::uint64_t local_steam_id,
        std::uint32_t payload_size,
        GBE_DotaSerializedConnectionState &connection_state);
    GBE_DotaReconnectPostResult execute_post_effects(GBE_DotaReconnectPostPlan plan);
    GBE_DotaReconnectPostResult execute_post_connection_state(
        std::uint64_t local_steam_id,
        std::uint32_t payload_size,
        GBE_DotaSerializedConnectionState &connection_state);

    GBE_DotaReconnectContextProvider &context_provider() const;
    GBE_DotaReconnectDirectConnector &direct_connector() const;
    GBE_DotaReconnectCallbackQueue &callback_queue() const;

private:
    GBE_DotaReconnectContextProvider &context_provider_;
    GBE_DotaReconnectDirectConnector &direct_connector_;
    GBE_DotaReconnectCallbackQueue &callback_queue_;
};

struct RoleDependencies {
    std::unique_ptr<CallbackScheduler> callback_scheduler;
    std::unique_ptr<GBE_DotaReconnectContextProvider> context_provider;
    std::unique_ptr<GBE_DotaReconnectDirectConnector> direct_connector;
    std::unique_ptr<GBE_DotaReconnectCallbackQueue> callback_queue;
};

class RoleContext {
public:
    RoleContext(
        dota_lobby_state::Store &lobby_store,
        RoleDependencies dependencies);

    RoleContext(const RoleContext &) = delete;
    RoleContext &operator=(const RoleContext &) = delete;
    RoleContext(RoleContext &&) = delete;
    RoleContext &operator=(RoleContext &&) = delete;

    dota_lobby_state::Store &lobby_store();
    CallbackScheduler &callback_scheduler() const;
    ReconnectService &reconnect_service();

private:
    dota_lobby_state::Store &lobby_store_;
    std::unique_ptr<CallbackScheduler> callback_scheduler_;
    std::unique_ptr<GBE_DotaReconnectContextProvider> context_provider_;
    std::unique_ptr<GBE_DotaReconnectDirectConnector> direct_connector_;
    std::unique_ptr<GBE_DotaReconnectCallbackQueue> callback_queue_;
    ReconnectService reconnect_service_;
};

class CompositionRoot {
public:
    CompositionRoot(
        std::unique_ptr<LifecycleExecutor> lifecycle_executor,
        HandlerRegistryView handler_registry,
        RoleDependencies client_dependencies,
        RoleDependencies server_dependencies);

    CompositionRoot(const CompositionRoot &) = delete;
    CompositionRoot &operator=(const CompositionRoot &) = delete;
    CompositionRoot(CompositionRoot &&) = delete;
    CompositionRoot &operator=(CompositionRoot &&) = delete;

    dota_lobby_state::Store &lobby_store();
    LifecycleExecutor &lifecycle_executor() const;
    HandlerRegistryView handler_registry() const;
    RoleContext &client();
    RoleContext &server();

private:
    GBE_SharedDotaLobbyState lobby_state_;
    std::recursive_mutex lobby_mutex_;
    dota_lobby_state::Store lobby_store_;
    std::unique_ptr<LifecycleExecutor> lifecycle_executor_;
    std::vector<dota_handler_registry::Entry> handler_registry_entries_;
    RoleContext client_;
    RoleContext server_;
};

} // namespace gbe::dota

#endif

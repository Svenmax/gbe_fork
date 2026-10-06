#include "gbe_dota_composition_root.h"

#include <stdexcept>
#include <utility>

namespace gbe::dota {
namespace {

template <typename T>
T &require_dependency(const std::unique_ptr<T> &dependency, const char *name)
{
    if (!dependency)
        throw std::invalid_argument(name);
    return *dependency;
}

std::vector<dota_handler_registry::Entry> copy_registry(HandlerRegistryView registry)
{
    if (registry.size == 0u)
        return {};
    if (!registry.entries)
        throw std::invalid_argument("handler_registry");
    return {registry.entries, registry.entries + registry.size};
}

} // namespace

ReconnectService::ReconnectService(
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue)
    : context_provider_(context_provider),
      direct_connector_(direct_connector),
      callback_queue_(callback_queue)
{
}

GBE_DotaReconnectPostPlan ReconnectService::prepare_post_connection_state(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaSerializedConnectionState &connection_state)
{
    return GBE_PrepareDotaReconnectPostConnectionState(
        local_steam_id,
        payload_size,
        context_provider_,
        connection_state);
}

GBE_DotaReconnectPostResult ReconnectService::execute_post_effects(GBE_DotaReconnectPostPlan plan)
{
    return GBE_ExecuteDotaReconnectPostEffects(
        std::move(plan),
        direct_connector_,
        callback_queue_);
}

GBE_DotaReconnectPostResult ReconnectService::execute_post_connection_state(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaSerializedConnectionState &connection_state)
{
    return GBE_ExecuteDotaReconnectPostConnectionState(
        local_steam_id,
        payload_size,
        context_provider_,
        direct_connector_,
        callback_queue_,
        connection_state);
}

GBE_DotaReconnectContextProvider &ReconnectService::context_provider() const
{
    return context_provider_;
}

GBE_DotaReconnectDirectConnector &ReconnectService::direct_connector() const
{
    return direct_connector_;
}

GBE_DotaReconnectCallbackQueue &ReconnectService::callback_queue() const
{
    return callback_queue_;
}

RoleContext::RoleContext(
    dota_lobby_state::Store &lobby_store,
    RoleDependencies dependencies)
    : lobby_store_(lobby_store),
      callback_scheduler_(std::move(dependencies.callback_scheduler)),
      context_provider_(std::move(dependencies.context_provider)),
      direct_connector_(std::move(dependencies.direct_connector)),
      callback_queue_(std::move(dependencies.callback_queue)),
      reconnect_service_(
          require_dependency(context_provider_, "context_provider"),
          require_dependency(direct_connector_, "direct_connector"),
          require_dependency(callback_queue_, "callback_queue"))
{
    require_dependency(callback_scheduler_, "callback_scheduler");
}

dota_lobby_state::Store &RoleContext::lobby_store()
{
    return lobby_store_;
}

CallbackScheduler &RoleContext::callback_scheduler() const
{
    return *callback_scheduler_;
}

ReconnectService &RoleContext::reconnect_service()
{
    return reconnect_service_;
}

CompositionRoot::CompositionRoot(
    std::unique_ptr<LifecycleExecutor> lifecycle_executor,
    HandlerRegistryView handler_registry,
    RoleDependencies client_dependencies,
    RoleDependencies server_dependencies)
    : lobby_store_(lobby_state_, lobby_mutex_),
      lifecycle_executor_(std::move(lifecycle_executor)),
      handler_registry_entries_(copy_registry(handler_registry)),
      client_(lobby_store_, std::move(client_dependencies)),
      server_(lobby_store_, std::move(server_dependencies))
{
    require_dependency(lifecycle_executor_, "lifecycle_executor");
}

dota_lobby_state::Store &CompositionRoot::lobby_store()
{
    return lobby_store_;
}

LifecycleExecutor &CompositionRoot::lifecycle_executor() const
{
    return *lifecycle_executor_;
}

HandlerRegistryView CompositionRoot::handler_registry() const
{
    return HandlerRegistryView{
        handler_registry_entries_.data(),
        handler_registry_entries_.size()};
}

RoleContext &CompositionRoot::client()
{
    return client_;
}

RoleContext &CompositionRoot::server()
{
    return server_;
}

} // namespace gbe::dota

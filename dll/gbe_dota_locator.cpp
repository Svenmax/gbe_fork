#include "gbe_dota_locator.h"
#include "gbe_dota_runtime_state.h"

#include <stdexcept>

namespace {

gbe::dota_lobby_state::Store *shared_dota_lobby_store{};
gbe::dota::RuntimeState *dota_runtime_state{};

} // namespace

gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore()
{
    if (!shared_dota_lobby_store)
        throw std::logic_error("Shared Dota lobby Store is not bound");
    return *shared_dota_lobby_store;
}

void GBE_BindSharedDotaLobbyStateStore(gbe::dota_lobby_state::Store &store)
{
    if (shared_dota_lobby_store && shared_dota_lobby_store != &store)
        throw std::logic_error("Shared Dota lobby Store is already bound");
    shared_dota_lobby_store = &store;
}

void GBE_UnbindSharedDotaLobbyStateStore(gbe::dota_lobby_state::Store &store)
{
    if (shared_dota_lobby_store == &store)
        shared_dota_lobby_store = nullptr;
}

void GBE_BindDotaRuntimeState(gbe::dota::RuntimeState &state)
{
    if (dota_runtime_state && dota_runtime_state != &state)
        throw std::logic_error("Dota runtime state is already bound");
    dota_runtime_state = &state;
}

void GBE_UnbindDotaRuntimeState(gbe::dota::RuntimeState &state)
{
    if (dota_runtime_state == &state)
        dota_runtime_state = nullptr;
}

gbe::dota::RuntimeState &GBE_DotaRuntimeState()
{
    if (!dota_runtime_state)
        throw std::logic_error("Dota runtime state is not bound");
    return *dota_runtime_state;
}

namespace gbe::dota {

LocatorBindingGuard::LocatorBindingGuard(
    dota_lobby_state::Store &lobby_store,
    RuntimeState &runtime_state)
    : lobby_store_(&lobby_store)
{
    GBE_BindSharedDotaLobbyStateStore(lobby_store);
    try {
        GBE_BindDotaRuntimeState(runtime_state);
        runtime_state_ = &runtime_state;
    } catch (...) {
        GBE_UnbindSharedDotaLobbyStateStore(lobby_store);
        lobby_store_ = nullptr;
        throw;
    }
}

LocatorBindingGuard::~LocatorBindingGuard()
{
    if (runtime_state_)
        GBE_UnbindDotaRuntimeState(*runtime_state_);
    if (lobby_store_)
        GBE_UnbindSharedDotaLobbyStateStore(*lobby_store_);
}

} // namespace gbe::dota

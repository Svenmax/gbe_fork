#ifndef GBE_DOTA_LOCATOR_H
#define GBE_DOTA_LOCATOR_H

#include "gbe_dota_lobby_state_store.h"

namespace gbe::dota {
struct RuntimeState;
}

namespace gbe::dota {

class LocatorBindingGuard {
public:
    LocatorBindingGuard(
        dota_lobby_state::Store &lobby_store,
        RuntimeState &runtime_state);
    ~LocatorBindingGuard();

    LocatorBindingGuard(const LocatorBindingGuard &) = delete;
    LocatorBindingGuard &operator=(const LocatorBindingGuard &) = delete;
    LocatorBindingGuard(LocatorBindingGuard &&) = delete;
    LocatorBindingGuard &operator=(LocatorBindingGuard &&) = delete;

private:
    dota_lobby_state::Store *lobby_store_{};
    RuntimeState *runtime_state_{};
};

} // namespace gbe::dota

gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore();
void GBE_BindSharedDotaLobbyStateStore(gbe::dota_lobby_state::Store &store);
void GBE_UnbindSharedDotaLobbyStateStore(gbe::dota_lobby_state::Store &store);
void GBE_BindDotaRuntimeState(gbe::dota::RuntimeState &state);
void GBE_UnbindDotaRuntimeState(gbe::dota::RuntimeState &state);
gbe::dota::RuntimeState &GBE_DotaRuntimeState();

#endif

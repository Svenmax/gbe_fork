#include "dll/gbe_dota_handler_registry.h"

namespace registry = gbe::dota_handler_registry;
namespace router = gbe::dota_gc_router;

constexpr registry::Entry entry{
    7001u,
    registry::RequestMode::DirectAndWrapped,
    registry::SessionPolicy::ForwardWrappedSession,
    registry::LifecycleClass::LobbyLifecycle,
    nullptr,
    registry::HandlerId::PracticeLobbyCreate,
    "smoke:test_lobby_create_records_cache_subscription_before_pushes",
};

static_assert(registry::supports_mode(entry.modes, router::DotaGcRequestPath::Direct));
static_assert(registry::supports_mode(entry.modes, router::DotaGcRequestPath::Wrapped));
static_assert(!registry::supports_mode(entry.modes, router::DotaGcRequestPath::Unknown));
static_assert(registry::forwards_wrapped_session(entry.session_policy));
static_assert(entry.handler == registry::HandlerId::PracticeLobbyCreate);
static_assert(entry.fixture[0] == 's');

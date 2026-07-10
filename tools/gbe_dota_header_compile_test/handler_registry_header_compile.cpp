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

constexpr registry::Entry entries[] = {entry};

static_assert(registry::supports_mode(entry.modes, router::DotaGcRequestPath::Direct));
static_assert(registry::supports_mode(entry.modes, router::DotaGcRequestPath::Wrapped));
static_assert(!registry::supports_mode(entry.modes, router::DotaGcRequestPath::Unknown));
static_assert(registry::forwards_wrapped_session(entry.session_policy));
static_assert(entry.handler == registry::HandlerId::PracticeLobbyCreate);
static_assert(entry.fixture[0] == 's');
static_assert(registry::is_high_risk(entry.lifecycle));
static_assert(registry::has_test_fixture(entry));
static_assert(registry::find_entry(entries, 1u, 7001u, router::DotaGcRequestPath::Direct) == &entries[0]);
static_assert(registry::find_entry(entries, 1u, 7002u, router::DotaGcRequestPath::Direct) == nullptr);
static_assert(registry::has_unique_message_ids_per_mode(entries, 1u));
static_assert(registry::all_high_risk_entries_have_fixture(entries, 1u));

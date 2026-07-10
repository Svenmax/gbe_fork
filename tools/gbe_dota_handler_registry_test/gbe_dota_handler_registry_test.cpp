#include "dll/gbe_dota_handler_registry.h"

#include <cstdint>
#include <cstdio>

namespace registry = gbe::dota_handler_registry;
namespace router = gbe::dota_gc_router;

namespace {

int assertions{};
int failures{};

void expect(bool condition, const char *message)
{
    ++assertions;
    if (condition)
        return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
}

constexpr registry::Entry kEntries[] = {
    { 100u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, nullptr, registry::HandlerId::UploadRate, nullptr },
    { 100u, registry::RequestMode::Wrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, nullptr, registry::HandlerId::JoinChatChannel, "smoke:test_chat_join_channel" },
    { 200u, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, nullptr, registry::HandlerId::PracticeLobbyJoin, "replay:lobby_lifecycle:lobby_join_by_id" },
};

constexpr registry::Entry kDuplicateDirectEntries[] = {
    { 300u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, nullptr, registry::HandlerId::Rank, nullptr },
    { 300u, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyRead, nullptr, registry::HandlerId::LobbyList, nullptr },
};

void test_message_ids_are_unique_per_mode()
{
    expect(registry::has_unique_message_ids_per_mode(kEntries, 3u), "same message ID may be split across direct and wrapped modes");
    expect(!registry::has_unique_message_ids_per_mode(kDuplicateDirectEntries, 2u), "overlapping direct message IDs are rejected");
}

void test_mode_matching()
{
    expect(registry::supports_mode(registry::RequestMode::Direct, router::DotaGcRequestPath::Direct), "direct mode matches direct path");
    expect(!registry::supports_mode(registry::RequestMode::Direct, router::DotaGcRequestPath::Wrapped), "direct mode excludes wrapped path");
    expect(registry::supports_mode(registry::RequestMode::Wrapped, router::DotaGcRequestPath::Wrapped), "wrapped mode matches wrapped path");
    expect(registry::supports_mode(registry::RequestMode::DirectAndWrapped, router::DotaGcRequestPath::Direct), "dual mode matches direct path");
    expect(registry::supports_mode(registry::RequestMode::DirectAndWrapped, router::DotaGcRequestPath::Wrapped), "dual mode matches wrapped path");
    expect(!registry::supports_mode(registry::RequestMode::DirectAndWrapped, router::DotaGcRequestPath::Unknown), "known modes exclude unknown path");
}

void test_session_policy()
{
    expect(!registry::forwards_wrapped_session(registry::SessionPolicy::Ignore), "ignore policy drops wrapped session");
    expect(registry::forwards_wrapped_session(registry::SessionPolicy::ForwardWrappedSession), "forward policy preserves wrapped session");
}

void test_unknown_message_returns_null()
{
    expect(registry::find_entry(kEntries, 3u, 999u, router::DotaGcRequestPath::Direct) == nullptr, "unknown direct message has no entry");
    expect(registry::find_entry(kEntries, 3u, 999u, router::DotaGcRequestPath::Wrapped) == nullptr, "unknown wrapped message has no entry");
    expect(registry::find_entry(kEntries, 3u, 100u, router::DotaGcRequestPath::Unknown) == nullptr, "unknown request path has no entry");
}

void test_handler_selection()
{
    const registry::Entry *direct = registry::find_entry(kEntries, 3u, 100u, router::DotaGcRequestPath::Direct);
    const registry::Entry *wrapped = registry::find_entry(kEntries, 3u, 100u, router::DotaGcRequestPath::Wrapped);
    const registry::Entry *dual_direct = registry::find_entry(kEntries, 3u, 200u, router::DotaGcRequestPath::Direct);
    const registry::Entry *dual_wrapped = registry::find_entry(kEntries, 3u, 200u, router::DotaGcRequestPath::Wrapped);

    expect(direct && direct->handler == registry::HandlerId::UploadRate, "direct lookup selects direct handler");
    expect(wrapped && wrapped->handler == registry::HandlerId::JoinChatChannel, "wrapped lookup selects wrapped handler");
    expect(dual_direct && dual_direct->handler == registry::HandlerId::PracticeLobbyJoin, "dual entry serves direct lookup");
    expect(dual_wrapped == dual_direct, "dual entry serves both modes with one identity");
    expect(direct && direct->session_policy == registry::SessionPolicy::Ignore, "selected direct handler keeps ignore session policy");
    expect(wrapped && wrapped->session_policy == registry::SessionPolicy::ForwardWrappedSession, "selected wrapped handler keeps forward session policy");
}

} // namespace

int main()
{
    test_message_ids_are_unique_per_mode();
    test_mode_matching();
    test_session_policy();
    test_unknown_message_returns_null();
    test_handler_selection();

    std::printf("handler registry assertions: %d/%d\n", assertions - failures, assertions);
    return failures == 0 ? 0 : 1;
}

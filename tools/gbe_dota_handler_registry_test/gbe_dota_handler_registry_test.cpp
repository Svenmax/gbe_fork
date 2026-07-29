#include "dll/gbe_dota_handler_registry.h"

#include <array>
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

constexpr std::array<registry::HandlerId, 8u> kPropertyHandlers{
    registry::HandlerId::JoinChatChannel,
    registry::HandlerId::PracticeLobbyCreate,
    registry::HandlerId::LobbyList,
    registry::HandlerId::InviteToLobby,
    registry::HandlerId::PracticeLobbyJoin,
    registry::HandlerId::PracticeLobbyLaunch,
    registry::HandlerId::UploadRate,
    registry::HandlerId::Rank,
};

std::uint32_t next_random(std::uint32_t &state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

registry::RequestMode property_mode(std::uint32_t value)
{
    switch (value % 3u) {
    case 0u:
        return registry::RequestMode::Direct;
    case 1u:
        return registry::RequestMode::Wrapped;
    default:
        return registry::RequestMode::DirectAndWrapped;
    }
}

registry::LifecycleClass property_lifecycle(std::uint32_t value)
{
    switch (value % 4u) {
    case 0u:
        return registry::LifecycleClass::None;
    case 1u:
        return registry::LifecycleClass::LobbyRead;
    case 2u:
        return registry::LifecycleClass::LobbyMutation;
    default:
        return registry::LifecycleClass::LobbyLifecycle;
    }
}

bool same_selected_entry(const registry::Entry *left, const registry::Entry *right)
{
    if (!left || !right)
        return left == right;
    return left->message_id == right->message_id &&
        left->modes == right->modes &&
        left->session_policy == right->session_policy &&
        left->lifecycle == right->lifecycle &&
        left->handler == right->handler &&
        left->fixture == right->fixture;
}

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

void test_property_message_ids_are_unique_per_mode()
{
    for (std::uint32_t seed = 1u; seed <= 64u; ++seed) {
        std::uint32_t random = seed;
        std::array<registry::Entry, 16u> entries{};
        for (std::size_t index = 0u; index < entries.size(); ++index) {
            entries[index].message_id = seed * 100u + static_cast<std::uint32_t>(index);
            entries[index].modes = property_mode(next_random(random));
            entries[index].handler = kPropertyHandlers[index % kPropertyHandlers.size()];
        }
        expect(registry::has_unique_message_ids_per_mode(entries.data(), entries.size()), "P9-A generated unique table passes per-mode uniqueness");

        entries[15].message_id = entries[0].message_id;
        entries[15].modes = entries[0].modes;
        expect(!registry::has_unique_message_ids_per_mode(entries.data(), entries.size()), "P9-A generated overlapping duplicate fails per-mode uniqueness");
    }
}

void test_property_high_risk_entries_have_fixtures()
{
    for (std::uint32_t seed = 1u; seed <= 64u; ++seed) {
        std::uint32_t random = seed;
        std::array<registry::Entry, 16u> entries{};
        std::size_t first_high_risk = entries.size();
        for (std::size_t index = 0u; index < entries.size(); ++index) {
            entries[index].message_id = seed * 100u + static_cast<std::uint32_t>(index);
            entries[index].modes = property_mode(next_random(random));
            entries[index].lifecycle = property_lifecycle(next_random(random));
            entries[index].handler = kPropertyHandlers[index % kPropertyHandlers.size()];
            if (registry::is_high_risk(entries[index].lifecycle)) {
                entries[index].fixture = "smoke:test_property_fixture";
                if (first_high_risk == entries.size())
                    first_high_risk = index;
            }
        }
        if (first_high_risk == entries.size()) {
            first_high_risk = 0u;
            entries[0].lifecycle = registry::LifecycleClass::LobbyMutation;
            entries[0].fixture = "smoke:test_property_fixture";
        }

        expect(registry::all_high_risk_entries_have_fixture(entries.data(), entries.size()), "P9-B generated high-risk table accepts complete fixtures");
        entries[first_high_risk].fixture = nullptr;
        expect(!registry::all_high_risk_entries_have_fixture(entries.data(), entries.size()), "P9-B generated high-risk table rejects missing fixture");
    }
}

void test_property_registry_order_does_not_change_lookup()
{
    for (std::uint32_t seed = 1u; seed <= 64u; ++seed) {
        std::uint32_t random = seed;
        std::array<registry::Entry, 16u> canonical{};
        for (std::size_t index = 0u; index < canonical.size(); ++index) {
            canonical[index].message_id = seed * 100u + static_cast<std::uint32_t>(index);
            canonical[index].modes = property_mode(next_random(random));
            canonical[index].session_policy = registry::supports_mode(canonical[index].modes, router::DotaGcRequestPath::Wrapped)
                ? registry::SessionPolicy::ForwardWrappedSession
                : registry::SessionPolicy::Ignore;
            canonical[index].lifecycle = property_lifecycle(next_random(random));
            canonical[index].handler = kPropertyHandlers[index % kPropertyHandlers.size()];
            canonical[index].fixture = registry::is_high_risk(canonical[index].lifecycle)
                ? "smoke:test_property_fixture"
                : nullptr;
        }

        auto shuffled = canonical;
        for (std::size_t remaining = shuffled.size(); remaining > 1u; --remaining) {
            const std::size_t swap_index = next_random(random) % remaining;
            const registry::Entry temporary = shuffled[remaining - 1u];
            shuffled[remaining - 1u] = shuffled[swap_index];
            shuffled[swap_index] = temporary;
        }

        bool selections_match = true;
        for (const registry::Entry &expected_entry : canonical) {
            const auto message_id = expected_entry.message_id;
            selections_match = selections_match && same_selected_entry(
                registry::find_entry(canonical.data(), canonical.size(), message_id, router::DotaGcRequestPath::Direct),
                registry::find_entry(shuffled.data(), shuffled.size(), message_id, router::DotaGcRequestPath::Direct));
            selections_match = selections_match && same_selected_entry(
                registry::find_entry(canonical.data(), canonical.size(), message_id, router::DotaGcRequestPath::Wrapped),
                registry::find_entry(shuffled.data(), shuffled.size(), message_id, router::DotaGcRequestPath::Wrapped));
        }
        expect(selections_match, "P9-C generated registry permutation preserves every lookup result");
    }
}

} // namespace

int main()
{
    test_message_ids_are_unique_per_mode();
    test_mode_matching();
    test_session_policy();
    test_unknown_message_returns_null();
    test_handler_selection();
    test_property_message_ids_are_unique_per_mode();
    test_property_high_risk_entries_have_fixtures();
    test_property_registry_order_does_not_change_lookup();

    std::printf("handler registry assertions: %d/%d\n", assertions - failures, assertions);
    return failures == 0 ? 0 : 1;
}

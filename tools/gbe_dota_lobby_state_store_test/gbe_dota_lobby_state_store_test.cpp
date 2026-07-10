#include "dll/gbe_dota_lobby_state_store.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
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

void expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *message)
{
    if (actual == expected)
        return;

    std::fprintf(
        stderr,
        "FAIL: %s (actual=%llu expected=%llu)\n",
        message,
        static_cast<unsigned long long>(actual),
        static_cast<unsigned long long>(expected));
    ++failures;
}

void expect_eq_string(const std::string &actual, const std::string &expected, const char *message)
{
    if (actual == expected)
        return;

    std::fprintf(stderr, "FAIL: %s (actual=%s expected=%s)\n", message, actual.c_str(), expected.c_str());
    ++failures;
}

struct Fixture {
    GBE_SharedDotaLobbyState state;
    std::recursive_mutex mutex;
    gbe::dota_lobby_state::Store store{state, mutex};
};

GBE_SharedDotaLobbyState populated_state(std::uint64_t generation)
{
    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.active = true;
    state.generation = generation;
    state.lobby_id = 101u;
    state.generic_lobby_id = 202u;
    state.custom_game.game_id = 303u;
    state.server_id = 404u;
    state.connect = "127.0.0.1:27015";
    state.owner_name = "owner";
    state.members.push_back(GBE_DotaLobbyMemberState{});
    state.cache_service_list.push_back(42u);
    return state;
}

void test_default_snapshot_is_empty()
{
    Fixture fixture;
    const auto snapshot = fixture.store.snapshot();

    expect_true(!snapshot.valid, "default snapshot is invalid");
    expect_eq_u64(snapshot.generation, 0u, "default snapshot has no generation");
    expect_eq_u64(snapshot.lobby_id, 0u, "default snapshot has no lobby ID");
    expect_true(snapshot.connect.empty(), "default snapshot has no endpoint");
    expect_true(snapshot.members.empty(), "default snapshot has no members");
}

void test_publish_replaces_complete_snapshot()
{
    Fixture fixture;
    const auto expected = populated_state(7u);

    fixture.store.publish(expected);
    const auto snapshot = fixture.store.snapshot();

    expect_true(snapshot.valid, "publish stores validity");
    expect_eq_u64(snapshot.generation, 7u, "publish stores generation");
    expect_eq_u64(snapshot.lobby_id, 101u, "publish stores lobby ID");
    expect_eq_u64(snapshot.custom_game.game_id, 303u, "publish stores custom game");
    expect_eq_string(snapshot.connect, "127.0.0.1:27015", "publish stores endpoint");
    expect_eq_u64(snapshot.members.size(), 1u, "publish stores members");
    expect_eq_u64(snapshot.cache_service_list.size(), 1u, "publish stores cache services");
}

void test_snapshot_is_immutable_copy()
{
    Fixture fixture;
    auto input = populated_state(8u);
    fixture.store.publish(input);

    input.connect = "mutated-input";
    auto snapshot = fixture.store.snapshot();
    snapshot.connect = "mutated-snapshot";
    snapshot.members.clear();

    const auto current = fixture.store.snapshot();
    expect_eq_string(current.connect, "127.0.0.1:27015", "snapshot is detached from caller mutations");
    expect_eq_u64(current.members.size(), 1u, "snapshot vectors are detached from caller mutations");
}

void test_update_commits_complete_candidate()
{
    Fixture fixture;
    fixture.store.publish(populated_state(9u));

    const auto result = fixture.store.update([](auto &candidate) {
        candidate.server_id = 505u;
        candidate.connect = "10.0.0.5:27015";
        candidate.members.push_back(GBE_DotaLobbyMemberState{});
    });
    const auto snapshot = fixture.store.snapshot();

    expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::Applied, "update reports applied");
    expect_eq_u64(snapshot.server_id, 505u, "update commits scalar fields");
    expect_eq_string(snapshot.connect, "10.0.0.5:27015", "update commits string fields");
    expect_eq_u64(snapshot.members.size(), 2u, "update commits vector fields");
}

void test_compare_update_applies_matching_generation()
{
    Fixture fixture;
    fixture.store.publish(populated_state(10u));

    const auto result = fixture.store.compare_update(10u, [](auto &candidate) {
        candidate.server_id = 606u;
    });

    expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::Applied, "matching generation update reports applied");
    expect_eq_u64(fixture.store.snapshot().server_id, 606u, "matching generation update commits changes");
}

void test_compare_update_rejects_stale_generation_without_mutation()
{
    Fixture fixture;
    fixture.store.publish(populated_state(11u));
    bool mutator_called = false;

    const auto result = fixture.store.compare_update(10u, [&](auto &candidate) {
        mutator_called = true;
        candidate.server_id = 707u;
    });
    const auto snapshot = fixture.store.snapshot();

    expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration, "stale generation update reports stale");
    expect_true(!mutator_called, "stale generation update skips the mutator");
    expect_eq_u64(snapshot.server_id, 404u, "stale generation update preserves state");
    expect_eq_string(snapshot.connect, "127.0.0.1:27015", "stale generation update preserves complete snapshot");
}

void test_clear_resets_complete_state()
{
    Fixture fixture;
    fixture.store.publish(populated_state(12u));

    fixture.store.clear();
    const auto snapshot = fixture.store.snapshot();

    expect_true(!snapshot.valid, "clear resets validity");
    expect_eq_u64(snapshot.generation, 0u, "clear resets generation");
    expect_eq_u64(snapshot.lobby_id, 0u, "clear resets lobby ID");
    expect_eq_u64(snapshot.server_id, 0u, "clear resets server ID");
    expect_true(snapshot.connect.empty(), "clear resets endpoint");
    expect_true(snapshot.members.empty(), "clear resets members");
    expect_true(snapshot.cache_service_list.empty(), "clear resets cache services");
}

} // namespace

int main()
{
    test_default_snapshot_is_empty();
    test_publish_replaces_complete_snapshot();
    test_snapshot_is_immutable_copy();
    test_update_commits_complete_candidate();
    test_compare_update_applies_matching_generation();
    test_compare_update_rejects_stale_generation_without_mutation();
    test_clear_resets_complete_state();

    if (failures != 0) {
        std::fprintf(stderr, "gbe_dota_lobby_state_store_test failed: %d assertion(s)\n", failures);
        return 1;
    }

    std::printf("gbe_dota_lobby_state_store_test passed\n");
    return 0;
}

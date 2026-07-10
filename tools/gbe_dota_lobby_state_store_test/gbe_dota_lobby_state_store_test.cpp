#include "dll/gbe_dota_lobby_state_store.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
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

void test_monotonic_publish_accepts_current_and_newer_generations()
{
    Fixture fixture;
    fixture.store.publish(populated_state(8u));

    auto current = populated_state(8u);
    current.server_id = 505u;
    const auto current_result = fixture.store.publish_if_generation_current_or_newer(current);

    auto newer = populated_state(9u);
    newer.server_id = 606u;
    const auto newer_result = fixture.store.publish_if_generation_current_or_newer(newer);
    const auto snapshot = fixture.store.snapshot();

    expect_true(current_result == gbe::dota_lobby_state::StoreUpdateResult::Applied, "monotonic publish accepts current generation");
    expect_true(newer_result == gbe::dota_lobby_state::StoreUpdateResult::Applied, "monotonic publish accepts newer generation");
    expect_eq_u64(snapshot.generation, 9u, "monotonic publish advances generation");
    expect_eq_u64(snapshot.server_id, 606u, "monotonic publish commits newer state");
}

void test_monotonic_publish_rejects_older_generation()
{
    Fixture fixture;
    fixture.store.publish(populated_state(10u));

    auto stale = populated_state(9u);
    stale.lobby_id = 808u;
    stale.server_id = 707u;
    stale.connect = "stale-publish:27015";
    const auto result = fixture.store.publish_if_generation_current_or_newer(stale);
    const auto snapshot = fixture.store.snapshot();

    expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration, "monotonic publish reports stale generation");
    expect_eq_u64(snapshot.generation, 10u, "monotonic publish preserves current generation");
    expect_eq_u64(snapshot.lobby_id, 101u, "monotonic publish preserves current lobby ID");
    expect_eq_u64(snapshot.server_id, 404u, "monotonic publish preserves current server ID");
    expect_eq_string(snapshot.connect, "127.0.0.1:27015", "monotonic publish preserves current endpoint");
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

void test_delayed_writer_cannot_overwrite_new_generation()
{
    Fixture fixture;
    const auto delayed_generation = 12u;
    fixture.store.publish(populated_state(delayed_generation));

    auto replacement = populated_state(13u);
    replacement.lobby_id = 808u;
    replacement.server_id = 909u;
    replacement.connect = "10.0.0.9:27015";
    fixture.store.publish(replacement);

    const auto result = fixture.store.compare_update(delayed_generation, [](auto &candidate) {
        candidate.server_id = 707u;
        candidate.connect = "stale-writer:27015";
    });
    const auto snapshot = fixture.store.snapshot();

    expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration, "delayed writer reports stale generation");
    expect_eq_u64(snapshot.generation, 13u, "delayed writer preserves replacement generation");
    expect_eq_u64(snapshot.lobby_id, 808u, "delayed writer preserves replacement lobby ID");
    expect_eq_u64(snapshot.server_id, 909u, "delayed writer preserves replacement server ID");
    expect_eq_string(snapshot.connect, "10.0.0.9:27015", "delayed writer preserves replacement endpoint");
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

void test_concurrent_readers_observe_complete_versions()
{
    Fixture fixture;
    constexpr std::uint64_t iterations = 2000u;
    constexpr int reader_count = 4;
    std::atomic<bool> start{false};
    std::atomic<bool> writer_done{false};
    std::atomic<int> inconsistent_snapshots{0};

    auto make_version = [](std::uint64_t version) {
        GBE_SharedDotaLobbyState state;
        state.valid = true;
        state.generation = version;
        state.lobby_id = version;
        state.server_id = version;
        state.connect = std::to_string(version);
        state.cache_service_list.push_back(static_cast<std::uint32_t>(version));
        return state;
    };
    fixture.store.publish(make_version(0u));

    std::vector<std::thread> readers;
    readers.reserve(reader_count);
    for (int reader = 0; reader < reader_count; ++reader) {
        readers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            do {
                const auto snapshot = fixture.store.snapshot();
                const auto version = snapshot.generation;
                const bool complete =
                    snapshot.valid &&
                    snapshot.lobby_id == version &&
                    snapshot.server_id == version &&
                    snapshot.connect == std::to_string(version) &&
                    snapshot.cache_service_list.size() == 1u &&
                    snapshot.cache_service_list.front() == static_cast<std::uint32_t>(version);
                if (!complete)
                    inconsistent_snapshots.fetch_add(1, std::memory_order_relaxed);
            } while (!writer_done.load(std::memory_order_acquire));
        });
    }

    std::thread writer([&] {
        start.store(true, std::memory_order_release);
        for (std::uint64_t version = 1u; version <= iterations; ++version)
            fixture.store.publish(make_version(version));
        writer_done.store(true, std::memory_order_release);
    });

    writer.join();
    for (auto &reader : readers)
        reader.join();

    expect_eq_u64(
        static_cast<std::uint64_t>(inconsistent_snapshots.load()),
        0u,
        "concurrent readers observe one complete state version");
    expect_eq_u64(fixture.store.snapshot().generation, iterations, "concurrent writer commits the final version");
}

void test_concurrent_compare_updates_commit_matching_generation_only()
{
    Fixture fixture;
    constexpr std::uint64_t generation = 200u;
    constexpr int writer_count = 4;
    constexpr int updates_per_writer = 500;
    fixture.store.publish(populated_state(generation));

    std::atomic<bool> start{false};
    std::atomic<int> applied_updates{0};
    std::atomic<int> stale_updates{0};
    std::vector<std::thread> writers;
    writers.reserve(writer_count + 1);

    for (int writer = 0; writer < writer_count; ++writer) {
        writers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (int update = 0; update < updates_per_writer; ++update) {
                const auto result = fixture.store.compare_update(generation, [](auto &candidate) {
                    ++candidate.server_id;
                    candidate.members.push_back(GBE_DotaLobbyMemberState{});
                });
                if (result == gbe::dota_lobby_state::StoreUpdateResult::Applied)
                    applied_updates.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    writers.emplace_back([&] {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (int update = 0; update < updates_per_writer; ++update) {
            const auto result = fixture.store.compare_update(generation - 1u, [](auto &candidate) {
                candidate.server_id = 0u;
                candidate.connect = "stale-writer:27015";
                candidate.members.clear();
            });
            if (result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration)
                stale_updates.fetch_add(1, std::memory_order_relaxed);
        }
    });

    start.store(true, std::memory_order_release);
    for (auto &writer : writers)
        writer.join();

    const auto snapshot = fixture.store.snapshot();
    const auto expected_applied = static_cast<std::uint64_t>(writer_count * updates_per_writer);
    expect_eq_u64(static_cast<std::uint64_t>(applied_updates.load()), expected_applied, "all matching concurrent updates apply");
    expect_eq_u64(static_cast<std::uint64_t>(stale_updates.load()), updates_per_writer, "all stale concurrent updates are rejected");
    expect_eq_u64(snapshot.generation, generation, "concurrent compare updates preserve generation");
    expect_eq_u64(snapshot.server_id, 404u + expected_applied, "concurrent compare updates commit every scalar increment");
    expect_eq_u64(snapshot.members.size(), 1u + expected_applied, "concurrent compare updates commit every vector append");
    expect_eq_string(snapshot.connect, "127.0.0.1:27015", "stale concurrent updates preserve endpoint");
}

void test_concurrent_clear_and_publish_expose_complete_states()
{
    Fixture fixture;
    constexpr std::uint64_t iterations = 1000u;
    constexpr int reader_count = 4;
    std::atomic<bool> start{false};
    std::atomic<bool> writer_done{false};
    std::atomic<int> inconsistent_snapshots{0};

    auto make_version = [](std::uint64_t version) {
        GBE_SharedDotaLobbyState state;
        state.valid = true;
        state.active = true;
        state.generation = version;
        state.lobby_id = version;
        state.server_id = version;
        state.connect = std::to_string(version);
        state.members.resize(1u);
        state.cache_service_list.push_back(static_cast<std::uint32_t>(version));
        return state;
    };

    std::vector<std::thread> readers;
    readers.reserve(reader_count);
    for (int reader = 0; reader < reader_count; ++reader) {
        readers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            do {
                const auto snapshot = fixture.store.snapshot();
                const bool complete_empty =
                    !snapshot.valid &&
                    !snapshot.active &&
                    snapshot.generation == 0u &&
                    snapshot.lobby_id == 0u &&
                    snapshot.server_id == 0u &&
                    snapshot.connect.empty() &&
                    snapshot.members.empty() &&
                    snapshot.cache_service_list.empty();
                const auto version = snapshot.generation;
                const bool complete_published =
                    snapshot.valid &&
                    snapshot.active &&
                    version != 0u &&
                    snapshot.lobby_id == version &&
                    snapshot.server_id == version &&
                    snapshot.connect == std::to_string(version) &&
                    snapshot.members.size() == 1u &&
                    snapshot.cache_service_list.size() == 1u &&
                    snapshot.cache_service_list.front() == static_cast<std::uint32_t>(version);
                if (!complete_empty && !complete_published)
                    inconsistent_snapshots.fetch_add(1, std::memory_order_relaxed);
            } while (!writer_done.load(std::memory_order_acquire));
        });
    }

    std::thread writer([&] {
        start.store(true, std::memory_order_release);
        for (std::uint64_t version = 1u; version <= iterations; ++version) {
            fixture.store.publish(make_version(version));
            fixture.store.clear();
        }
        fixture.store.publish(make_version(iterations + 1u));
        writer_done.store(true, std::memory_order_release);
    });

    writer.join();
    for (auto &reader : readers)
        reader.join();

    const auto final_snapshot = fixture.store.snapshot();
    expect_eq_u64(static_cast<std::uint64_t>(inconsistent_snapshots.load()), 0u, "concurrent clear and publish expose complete states");
    expect_eq_u64(final_snapshot.generation, iterations + 1u, "concurrent clear and publish commit final version");
    expect_eq_u64(final_snapshot.lobby_id, iterations + 1u, "final published state remains complete after clear interleaving");
}

void test_property_snapshots_always_represent_complete_versions()
{
    Fixture fixture;

    for (std::uint64_t seed = 1u; seed <= 64u; ++seed) {
        for (std::uint64_t step = 1u; step <= 32u; ++step) {
            const auto version = seed * 1000u + step;
            GBE_SharedDotaLobbyState state;
            state.valid = true;
            state.active = (version % 2u) != 0u;
            state.generation = version;
            state.lobby_id = version + 1u;
            state.generic_lobby_id = version + 2u;
            state.server_id = version + 3u;
            state.connect = std::to_string(version);
            state.owner_name = "owner-" + std::to_string(version);
            state.members.resize(static_cast<std::size_t>(version % 4u + 1u));
            state.cache_service_list.push_back(static_cast<std::uint32_t>(version));
            fixture.store.publish(state);

            const auto snapshot = fixture.store.snapshot();
            const bool complete =
                snapshot.valid == state.valid &&
                snapshot.active == state.active &&
                snapshot.generation == state.generation &&
                snapshot.lobby_id == state.lobby_id &&
                snapshot.generic_lobby_id == state.generic_lobby_id &&
                snapshot.server_id == state.server_id &&
                snapshot.connect == state.connect &&
                snapshot.owner_name == state.owner_name &&
                snapshot.members.size() == state.members.size() &&
                snapshot.cache_service_list == state.cache_service_list;
            expect_true(complete, "P7-A snapshot represents one complete published version");
        }
    }
}

void test_property_stale_updates_never_change_store()
{
    for (std::uint64_t seed = 1u; seed <= 64u; ++seed) {
        Fixture fixture;
        auto current = populated_state(seed + 100u);
        current.lobby_id = seed * 10u + 1u;
        current.generic_lobby_id = seed * 10u + 2u;
        current.server_id = seed * 10u + 3u;
        current.connect = "current-" + std::to_string(seed);
        current.owner_name = "owner-" + std::to_string(seed);
        fixture.store.publish(current);
        const auto before = fixture.store.snapshot();
        bool mutator_called = false;

        const auto result = fixture.store.compare_update(current.generation - 1u, [&](auto &candidate) {
            mutator_called = true;
            candidate = GBE_SharedDotaLobbyState{};
        });
        const auto after = fixture.store.snapshot();
        const bool unchanged =
            after.valid == before.valid &&
            after.active == before.active &&
            after.generation == before.generation &&
            after.lobby_id == before.lobby_id &&
            after.generic_lobby_id == before.generic_lobby_id &&
            after.server_id == before.server_id &&
            after.connect == before.connect &&
            after.owner_name == before.owner_name &&
            after.members.size() == before.members.size() &&
            after.cache_service_list == before.cache_service_list;

        expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration, "P7-B stale update reports stale");
        expect_true(!mutator_called, "P7-B stale update skips mutator");
        expect_true(unchanged, "P7-B stale update preserves the complete store state");
    }
}

} // namespace

int main()
{
    test_default_snapshot_is_empty();
    test_publish_replaces_complete_snapshot();
    test_snapshot_is_immutable_copy();
    test_monotonic_publish_accepts_current_and_newer_generations();
    test_monotonic_publish_rejects_older_generation();
    test_update_commits_complete_candidate();
    test_compare_update_applies_matching_generation();
    test_compare_update_rejects_stale_generation_without_mutation();
    test_delayed_writer_cannot_overwrite_new_generation();
    test_clear_resets_complete_state();
    test_concurrent_readers_observe_complete_versions();
    test_concurrent_compare_updates_commit_matching_generation_only();
    test_concurrent_clear_and_publish_expose_complete_states();
    test_property_snapshots_always_represent_complete_versions();
    test_property_stale_updates_never_change_store();

    if (failures != 0) {
        std::fprintf(stderr, "gbe_dota_lobby_state_store_test failed: %d assertion(s)\n", failures);
        return 1;
    }

    std::printf("gbe_dota_lobby_state_store_test passed\n");
    return 0;
}

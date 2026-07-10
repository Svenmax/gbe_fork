#include "dll/gbe_dota_lobby_state_store.h"
#include "dll/gbe_dota_reconnect_context.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures{};

void expect(bool condition, const char *label)
{
    if (condition)
        return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label);
}

GBE_SharedDotaLobbyState make_version(std::uint64_t version)
{
    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.active = true;
    state.generation = version;
    state.lobby_id = version * 10u + 1u;
    state.generic_lobby_id = version * 10u + 2u;
    state.custom_game.game_id = version * 10u + 3u;
    state.state = 2u;
    state.game_state = 2u;
    state.server_id = version * 10u + 4u;
    state.owner_steam_id = version * 10u + 5u;
    state.owner_connected = true;
    state.connect = "127.0.0.1:" + std::to_string(20000u + version);
    state.owner_name = "owner-" + std::to_string(version);
    state.members.resize(static_cast<std::size_t>(version % 3u + 1u));
    state.cache_service_list.push_back(static_cast<std::uint32_t>(version));
    return state;
}

bool is_empty(const GBE_SharedDotaLobbyState &snapshot)
{
    return !snapshot.valid &&
        !snapshot.active &&
        snapshot.generation == 0u &&
        snapshot.lobby_id == 0u &&
        snapshot.server_id == 0u &&
        snapshot.connect.empty() &&
        snapshot.members.empty() &&
        snapshot.cache_service_list.empty();
}

bool is_complete_version(const GBE_SharedDotaLobbyState &snapshot)
{
    const std::uint64_t version = snapshot.generation;
    return snapshot.valid &&
        snapshot.active &&
        version != 0u &&
        snapshot.lobby_id == version * 10u + 1u &&
        snapshot.generic_lobby_id == version * 10u + 2u &&
        snapshot.custom_game.game_id == version * 10u + 3u &&
        snapshot.state == 2u &&
        snapshot.game_state == 2u &&
        snapshot.server_id == version * 10u + 4u &&
        snapshot.owner_steam_id == version * 10u + 5u &&
        snapshot.owner_connected &&
        snapshot.connect == "127.0.0.1:" + std::to_string(20000u + version) &&
        (snapshot.owner_name == "owner-" + std::to_string(version) ||
            snapshot.owner_name == "updated-" + std::to_string(version)) &&
        snapshot.members.size() == static_cast<std::size_t>(version % 3u + 1u) &&
        snapshot.cache_service_list.size() == 1u &&
        snapshot.cache_service_list.front() == static_cast<std::uint32_t>(version);
}

bool builds_matching_context(const GBE_SharedDotaLobbyState &snapshot)
{
    GBE_DotaReconnectSharedStateSnapshot reconnect_snapshot{};
    reconnect_snapshot.valid = snapshot.valid;
    reconnect_snapshot.active = snapshot.active;
    reconnect_snapshot.generation = snapshot.generation;
    reconnect_snapshot.lobby_id = snapshot.lobby_id;
    reconnect_snapshot.lobby_state = snapshot.state;
    reconnect_snapshot.game_state = snapshot.game_state;
    reconnect_snapshot.server_id = snapshot.server_id;
    reconnect_snapshot.has_connect = !snapshot.connect.empty();
    reconnect_snapshot.custom_game_id = snapshot.custom_game.game_id;
    reconnect_snapshot.owner_connected = snapshot.owner_connected;
    reconnect_snapshot.launch_phase = snapshot.launch_phase;
    reconnect_snapshot.owner_steam_id = snapshot.owner_steam_id;
    std::strncpy(reconnect_snapshot.connect, snapshot.connect.c_str(), sizeof(reconnect_snapshot.connect) - 1u);

    const auto source = gbe::dota_reconnect::source_from_shared_snapshot(reconnect_snapshot);
    GBE_DotaReconnectContext context{};
    return gbe::dota_reconnect::build_context(source, context) == gbe::dota_reconnect::RejectReason::None &&
        context.generation == snapshot.generation &&
        context.lobby_id == snapshot.lobby_id &&
        context.server_id == snapshot.server_id &&
        context.custom_game_id == snapshot.custom_game.game_id &&
        context.owner_steam_id == snapshot.owner_steam_id &&
        snapshot.connect == context.connect;
}

void test_bounded_store_and_context_stress()
{
    constexpr std::uint64_t iterations = 1000u;
    constexpr int reader_count = 4;
    GBE_SharedDotaLobbyState state;
    std::recursive_mutex mutex;
    gbe::dota_lobby_state::Store store(state, mutex);
    store.publish(make_version(1u));

    std::atomic<bool> start{};
    std::atomic<bool> writer_done{};
    std::atomic<std::uint64_t> reads{};
    std::atomic<std::uint64_t> torn_snapshots{};
    std::atomic<std::uint64_t> invalid_contexts{};
    std::atomic<std::uint64_t> stale_rejections{};
    std::atomic<std::uint64_t> stale_mutator_calls{};

    std::vector<std::thread> readers;
    readers.reserve(reader_count);
    for (int reader = 0; reader < reader_count; ++reader) {
        readers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            do {
                const auto snapshot = store.snapshot();
                reads.fetch_add(1u, std::memory_order_relaxed);
                if (is_empty(snapshot))
                    continue;
                if (!is_complete_version(snapshot)) {
                    torn_snapshots.fetch_add(1u, std::memory_order_relaxed);
                    continue;
                }
                if (!builds_matching_context(snapshot))
                    invalid_contexts.fetch_add(1u, std::memory_order_relaxed);
            } while (!writer_done.load(std::memory_order_acquire));
        });
    }

    std::thread writer([&] {
        start.store(true, std::memory_order_release);
        for (std::uint64_t version = 2u; version <= iterations; ++version) {
            store.publish_if_generation_current_or_newer(make_version(version));
            store.compare_update(version, [version](auto &candidate) {
                candidate.owner_name = "updated-" + std::to_string(version);
            });
            const auto stale_result = store.compare_update(version - 1u, [&](auto &candidate) {
                stale_mutator_calls.fetch_add(1u, std::memory_order_relaxed);
                candidate = GBE_SharedDotaLobbyState{};
            });
            if (stale_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration)
                stale_rejections.fetch_add(1u, std::memory_order_relaxed);
            if (version % 5u == 0u)
                store.clear();
        }
        store.publish(make_version(iterations + 1u));
        writer_done.store(true, std::memory_order_release);
    });

    writer.join();
    for (auto &reader : readers)
        reader.join();

    const auto final_snapshot = store.snapshot();
    expect(reads.load() >= static_cast<std::uint64_t>(reader_count), "bounded stress executes every reader");
    expect(torn_snapshots.load() == 0u, "bounded stress exposes no torn snapshots");
    expect(invalid_contexts.load() == 0u, "bounded stress builds context from one snapshot version");
    expect(stale_rejections.load() == iterations - 1u, "bounded stress rejects every stale write");
    expect(stale_mutator_calls.load() == 0u, "bounded stress never executes stale mutator");
    expect(is_complete_version(final_snapshot), "bounded stress preserves complete final state");
    expect(final_snapshot.generation == iterations + 1u, "bounded stress commits final generation");
}

} // namespace

int main()
{
    test_bounded_store_and_context_stress();
    if (failures != 0)
        return 1;
    std::printf("gbe_dota_concurrency_stress_test passed\n");
    return 0;
}

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
    const auto source = gbe::dota_reconnect::source_from_shared_lobby_snapshot(snapshot);
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

// D.11.2: queue + deferred Slot + generation interleaving (production Slot model).
// Mirrors GBE_ConsumeDotaDeferredTask: Current iff lobby_id + generation match.
struct DeferredSlot {
    std::uint64_t lobby_id{};
    std::uint64_t generation{};
    bool pending{};
};

enum class DeferredStatus : std::uint8_t { Current, Stale, Empty };

struct QueuedItem {
    std::uint64_t sequence{};
    std::uint64_t generation{};
    std::uint64_t lobby_id{};
};

DeferredStatus consume_deferred(DeferredSlot &slot, std::uint64_t current_lobby, std::uint64_t current_generation)
{
    if (!slot.pending)
        return DeferredStatus::Empty;
    const auto status = (slot.lobby_id == current_lobby && slot.generation == current_generation)
        ? DeferredStatus::Current
        : DeferredStatus::Stale;
    slot = {};
    return status;
}

void test_queue_deferred_generation_interleave()
{
    constexpr std::uint64_t iterations = 600u;
    constexpr int consumer_count = 3;

    GBE_SharedDotaLobbyState state;
    std::recursive_mutex mutex;
    gbe::dota_lobby_state::Store store(state, mutex);
    store.publish(make_version(1u));

    std::mutex queue_mutex;
    std::vector<QueuedItem> queue;
    DeferredSlot deferred{};
    std::uint64_t current_generation = 1u;
    std::uint64_t current_lobby = make_version(1u).lobby_id;
    std::uint64_t next_sequence = 1u;

    std::atomic<bool> start{};
    std::atomic<bool> producer_done{};
    std::atomic<std::uint64_t> drained{};
    std::atomic<std::uint64_t> deferred_current{};
    std::atomic<std::uint64_t> deferred_stale{};
    std::atomic<std::uint64_t> deferred_empty_consumes{};
    std::atomic<std::uint64_t> current_queue_items{};
    std::atomic<std::uint64_t> stale_queue_items{};
    std::atomic<std::uint64_t> torn_snapshots{};

    auto consume_one = [&] {
        QueuedItem item{};
        bool have_item = false;
        DeferredStatus deferred_status = DeferredStatus::Empty;
        std::uint64_t observed_generation = 0u;
        std::uint64_t observed_lobby = 0u;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            observed_generation = current_generation;
            observed_lobby = current_lobby;
            if (!queue.empty()) {
                item = queue.front();
                queue.erase(queue.begin());
                have_item = true;
            }
            deferred_status = consume_deferred(deferred, observed_lobby, observed_generation);
        }

        if (deferred_status == DeferredStatus::Current)
            deferred_current.fetch_add(1u, std::memory_order_relaxed);
        else if (deferred_status == DeferredStatus::Stale)
            deferred_stale.fetch_add(1u, std::memory_order_relaxed);
        else
            deferred_empty_consumes.fetch_add(1u, std::memory_order_relaxed);

        if (have_item) {
            drained.fetch_add(1u, std::memory_order_relaxed);
            if (item.generation == observed_generation && item.lobby_id == observed_lobby)
                current_queue_items.fetch_add(1u, std::memory_order_relaxed);
            else
                stale_queue_items.fetch_add(1u, std::memory_order_relaxed);
        }

        const auto snapshot = store.snapshot();
        if (!is_empty(snapshot) && !is_complete_version(snapshot))
            torn_snapshots.fetch_add(1u, std::memory_order_relaxed);

        return have_item || deferred_status != DeferredStatus::Empty;
    };

    std::vector<std::thread> consumers;
    consumers.reserve(consumer_count);
    for (int i = 0; i < consumer_count; ++i) {
        consumers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
            }
            while (!producer_done.load(std::memory_order_acquire))
                consume_one();
            while (consume_one()) {
            }
        });
    }

    std::thread producer([&] {
        start.store(true, std::memory_order_release);
        for (std::uint64_t step = 1u; step <= iterations; ++step) {
            const bool advance = (step % 7u == 0u);
            std::uint64_t publish_version = 0u;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (advance) {
                    publish_version = current_generation + 1u;
                    current_generation = publish_version;
                    current_lobby = make_version(publish_version).lobby_id;
                } else {
                    publish_version = current_generation;
                }
            }
            if (advance) {
                store.publish_if_generation_current_or_newer(make_version(publish_version));
            } else {
                store.compare_update(publish_version, [publish_version](auto &candidate) {
                    candidate.owner_name = "updated-" + std::to_string(publish_version);
                });
            }

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                queue.push_back(QueuedItem{next_sequence++, current_generation, current_lobby});
                if (step % 3u == 0u)
                    deferred = DeferredSlot{current_lobby, current_generation, true};
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    producer.join();
    for (auto &t : consumers)
        t.join();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        expect(queue.empty(), "interleave stress drains queue");
        expect(!deferred.pending, "interleave stress clears deferred slot");
    }
    expect(drained.load() == iterations, "interleave stress drains every queued item");
    expect(torn_snapshots.load() == 0u, "interleave stress exposes no torn snapshots");
    expect(deferred_current.load() + deferred_stale.load() > 0u, "interleave stress exercises deferred consume");
    expect(
        current_queue_items.load() + stale_queue_items.load() == drained.load(),
        "interleave stress classifies every queue item");
    expect(deferred_stale.load() > 0u || deferred_current.load() > 0u, "interleave stress records deferred outcomes");
    const auto final_snapshot = store.snapshot();
    expect(final_snapshot.generation >= 1u, "interleave stress keeps store generation");
}

} // namespace

int main()
{
    test_bounded_store_and_context_stress();
    test_queue_deferred_generation_interleave();
    if (failures != 0)
        return 1;
    std::printf("gbe_dota_concurrency_stress_test passed\n");
    return 0;
}

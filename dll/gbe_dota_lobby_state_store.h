#ifndef GBE_DOTA_LOBBY_STATE_STORE_H
#define GBE_DOTA_LOBBY_STATE_STORE_H

#include "gbe_dota_lobby_state.h"

#include <cstdint>
#include <functional>
#include <mutex>

namespace gbe::dota_lobby_state {

enum class StoreUpdateResult {
    Applied,
    StaleGeneration,
};

class Store {
public:
    using Snapshot = GBE_SharedDotaLobbyState;
    using Mutator = std::function<void(Snapshot &)>;

    Store(Snapshot &state, std::recursive_mutex &mutex);

    Snapshot snapshot() const;

    // Generation-gated production write path. Cross-role publishes must enter here.
    StoreUpdateResult publish_if_generation_current_or_newer(Snapshot state);
    StoreUpdateResult update_if_generation_current_or_newer(
        std::uint64_t generation,
        const Mutator &mutator);
    StoreUpdateResult compare_clear(std::uint64_t expected_generation);
    StoreUpdateResult compare_update(std::uint64_t expected_generation, const Mutator &mutator);

    // Ungated writes: test/fixture bootstrap only. Production dll code must not call these
    // (enforced by tools/_audit_gc_refactor.py audit_store_write_discipline).
    void publish(Snapshot state);
    void clear();
    StoreUpdateResult update(const Mutator &mutator);

private:
    // Process-shared state. Every access is serialized by mutex_; mutators
    // must remain local data transformations and must not call external APIs.
    Snapshot &state_;
    std::recursive_mutex &mutex_;
};

} // namespace gbe::dota_lobby_state

#endif

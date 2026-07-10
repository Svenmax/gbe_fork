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
    void publish(Snapshot state);
    void clear();
    StoreUpdateResult update(const Mutator &mutator);
    StoreUpdateResult compare_update(std::uint64_t expected_generation, const Mutator &mutator);

private:
    Snapshot &state_;
    std::recursive_mutex &mutex_;
};

} // namespace gbe::dota_lobby_state

#endif

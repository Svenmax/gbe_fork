#include "gbe_dota_lobby_state_store.h"

#include <utility>

namespace gbe::dota_lobby_state {

Store::Store(Snapshot &state, std::recursive_mutex &mutex)
    : state_(state), mutex_(mutex)
{
}

Store::Snapshot Store::snapshot() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return state_;
}

void Store::publish(Snapshot state)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    state_ = std::move(state);
}

StoreUpdateResult Store::publish_if_generation_current_or_newer(Snapshot state)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state.generation < state_.generation)
        return StoreUpdateResult::StaleGeneration;
    if (state.generation == state_.generation && !state_.valid && state.valid)
        return StoreUpdateResult::StaleGeneration;

    state_ = std::move(state);
    return StoreUpdateResult::Applied;
}

void Store::clear()
{
    publish(Snapshot{});
}

StoreUpdateResult Store::compare_clear(std::uint64_t expected_generation)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_.generation != expected_generation)
        return StoreUpdateResult::StaleGeneration;

    Snapshot tombstone{};
    tombstone.generation = expected_generation;
    state_ = std::move(tombstone);
    return StoreUpdateResult::Applied;
}

StoreUpdateResult Store::update(const Mutator &mutator)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Snapshot next = state_;
    mutator(next);
    state_ = std::move(next);
    return StoreUpdateResult::Applied;
}

StoreUpdateResult Store::compare_update(std::uint64_t expected_generation, const Mutator &mutator)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_.generation != expected_generation)
        return StoreUpdateResult::StaleGeneration;

    Snapshot next = state_;
    mutator(next);
    state_ = std::move(next);
    return StoreUpdateResult::Applied;
}

} // namespace gbe::dota_lobby_state

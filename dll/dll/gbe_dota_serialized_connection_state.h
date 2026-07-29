#ifndef __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__
#define __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__

#include "../gbe_dota_connection_dedup_key.h"

#include <cstdint>
#include <mutex>
#include <string>

class GBE_DotaSerializedConnectionSynchronizer {
    std::mutex mutex;

public:
    std::unique_lock<std::mutex> acquire()
    {
        return std::unique_lock<std::mutex>(mutex);
    }

    std::unique_lock<std::mutex> try_acquire()
    {
        return std::unique_lock<std::mutex>(mutex, std::try_to_lock);
    }
};

struct GBE_DotaSerializedConnectionState {
    // Owned by one Steam_Networking_Sockets_Serialized instance. Access is
    // serialized by that instance's GBE_DotaSerializedConnectionSynchronizer.
    std::uint64_t generation{};
    std::uint64_t last_post_server_id{};
    std::uint32_t retry_count{};
    std::uint32_t last_post_size{};
    gbe::dota_connection::DedupKey callback_key;
    gbe::dota_connection::DedupKey direct_connect_key;

    void begin_generation(std::uint64_t current_generation);
    void begin_server(std::uint64_t server_id);
    bool should_connect_direct(std::uint64_t server_id, const std::string &endpoint) const;
    void record_direct_connect(std::uint64_t server_id, const std::string &endpoint);
    bool engine_callback_queued(std::uint64_t server_id, const std::string &endpoint) const;
    void record_engine_callback(std::uint64_t server_id, const std::string &endpoint);
};

#endif // __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__

#ifndef __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__
#define __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__

#include <cstdint>
#include <string>

struct GBE_DotaSerializedConnectionState {
    std::uint64_t lobby_id{};
    std::uint64_t last_post_server_id{};
    std::uint32_t retry_count{};
    std::uint32_t last_post_size{};
    std::uint64_t callback_server_id{};
    std::string callback_endpoint;
    std::uint64_t direct_connect_server_id{};
    std::string direct_connect_endpoint;

    void begin_lobby(std::uint64_t current_lobby_id);
    void begin_server(std::uint64_t server_id);
    bool should_connect_direct(std::uint64_t server_id, const std::string &endpoint) const;
    void record_direct_connect(std::uint64_t server_id, const std::string &endpoint);
    bool engine_callback_queued(std::uint64_t server_id, const std::string &endpoint) const;
    void record_engine_callback(std::uint64_t server_id, const std::string &endpoint);
};

#endif // __INCLUDED_GBE_DOTA_SERIALIZED_CONNECTION_STATE_H__

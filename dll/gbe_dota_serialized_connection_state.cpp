#include "dll/gbe_dota_serialized_connection_state.h"

void GBE_DotaSerializedConnectionState::begin_lobby(
    std::uint64_t current_lobby_id,
    std::uint64_t current_generation)
{
    if (lobby_id == current_lobby_id) {
        generation = current_generation;
        return;
    }
    *this = {};
    lobby_id = current_lobby_id;
    generation = current_generation;
}

void GBE_DotaSerializedConnectionState::begin_server(std::uint64_t server_id)
{
    if (last_post_server_id == server_id)
        return;
    last_post_server_id = server_id;
    retry_count = 0;
    last_post_size = 0;
    callback_server_id = 0;
    callback_endpoint.clear();
}

bool GBE_DotaSerializedConnectionState::should_connect_direct(
    std::uint64_t server_id,
    const std::string &endpoint) const
{
    return direct_connect_server_id != server_id || direct_connect_endpoint != endpoint;
}

void GBE_DotaSerializedConnectionState::record_direct_connect(
    std::uint64_t server_id,
    const std::string &endpoint)
{
    direct_connect_server_id = server_id;
    direct_connect_endpoint = endpoint;
}

bool GBE_DotaSerializedConnectionState::engine_callback_queued(
    std::uint64_t server_id,
    const std::string &endpoint) const
{
    return callback_server_id == server_id && callback_endpoint == endpoint;
}

void GBE_DotaSerializedConnectionState::record_engine_callback(
    std::uint64_t server_id,
    const std::string &endpoint)
{
    callback_server_id = server_id;
    callback_endpoint = endpoint;
}

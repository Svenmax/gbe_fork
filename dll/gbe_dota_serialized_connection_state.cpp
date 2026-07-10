#include "dll/gbe_dota_serialized_connection_state.h"

void GBE_DotaSerializedConnectionState::begin_lobby(
    std::uint64_t current_lobby_id,
    std::uint64_t current_generation)
{
    lobby_id = current_lobby_id;
    if (generation == current_generation)
        return;

    generation = current_generation;
    last_post_server_id = 0;
    retry_count = 0;
    last_post_size = 0;
    callback_key = {};
    direct_connect_key = {};
}

void GBE_DotaSerializedConnectionState::begin_server(std::uint64_t server_id)
{
    if (last_post_server_id == server_id)
        return;
    last_post_server_id = server_id;
    retry_count = 0;
    last_post_size = 0;
    callback_key = {};
}

bool GBE_DotaSerializedConnectionState::should_connect_direct(
    std::uint64_t server_id,
    const std::string &endpoint) const
{
    return direct_connect_key != gbe::dota_connection::DedupKey{
        gbe::dota_lobby_generation::Generation{generation},
        server_id,
        endpoint};
}

void GBE_DotaSerializedConnectionState::record_direct_connect(
    std::uint64_t server_id,
    const std::string &endpoint)
{
    direct_connect_key = {
        gbe::dota_lobby_generation::Generation{generation},
        server_id,
        endpoint};
}

bool GBE_DotaSerializedConnectionState::engine_callback_queued(
    std::uint64_t server_id,
    const std::string &endpoint) const
{
    return callback_key == gbe::dota_connection::DedupKey{
        gbe::dota_lobby_generation::Generation{generation},
        server_id,
        endpoint};
}

void GBE_DotaSerializedConnectionState::record_engine_callback(
    std::uint64_t server_id,
    const std::string &endpoint)
{
    callback_key = {
        gbe::dota_lobby_generation::Generation{generation},
        server_id,
        endpoint};
}

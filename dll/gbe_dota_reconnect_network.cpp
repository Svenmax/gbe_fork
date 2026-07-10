#include "gbe_dota_reconnect_network.h"

#include "gbe_proto_wire.h"

#include <cstdlib>
#include <cstring>

namespace {

bool parse_ipv4_endpoint(const std::string &endpoint, SteamNetworkingIPAddr &address)
{
    const std::size_t port_pos = endpoint.find(':');
    if (port_pos == std::string::npos)
        return false;

    unsigned long octets[4] = {};
    std::size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const std::size_t end = endpoint.find(i == 3 ? ':' : '.', start);
        if (end == std::string::npos || end <= start)
            return false;
        const std::string segment = endpoint.substr(start, end - start);
        char *parse_end = nullptr;
        octets[i] = std::strtoul(segment.c_str(), &parse_end, 10);
        if (!parse_end || *parse_end != '\0' || octets[i] > 255)
            return false;
        start = end + 1;
    }
    if (start != port_pos + 1)
        return false;

    char *port_end = nullptr;
    const unsigned long port = std::strtoul(endpoint.c_str() + port_pos + 1, &port_end, 10);
    if (!port_end || *port_end != '\0' || port == 0 || port > 65535)
        return false;

    const std::uint32_t ip = static_cast<std::uint32_t>(
        (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
    address.SetIPv4(ip, static_cast<std::uint16_t>(port));
    return true;
}

} // namespace

GBE_DotaReconnectPostResult GBE_ExecuteDotaReconnectPostConnectionState(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue,
    GBE_DotaSerializedConnectionState &connection_state)
{
    GBE_DotaReconnectPostResult result{};
    result.has_context = context_provider.get_context(local_steam_id, false, result.context);
    if (result.has_context && result.context.custom_game_id == 0ull) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby;
        return result;
    }

    const bool eligible = context_provider.reconnect_eligible();
    if (!eligible) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::ReconnectIneligible;
        return result;
    }

    if (!result.has_context) {
        result.has_context = context_provider.get_context(local_steam_id, true, result.context);
        if (!result.has_context) {
            result.skip_reason = GBE_DotaReconnectPostSkipReason::NoContext;
            return result;
        }
    }
    if (result.context.custom_game_id == 0ull) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby;
        return result;
    }

    result.endpoint = gbe::proto_wire::select_dota_arcade_connect_endpoint_for_local_player(
        result.context.connect,
        local_steam_id,
        result.context.owner_steam_id);
    if (!GBE_DotaReconnectContextIsStarted(result.context)) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::StateNotReady;
        return result;
    }
    if (result.context.connect[0] == '\0' || result.endpoint.empty()) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::MissingEndpoint;
        return result;
    }
    if (local_steam_id != 0ull && local_steam_id == result.context.owner_steam_id) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::LocalOwner;
        return result;
    }

    connection_state.begin_lobby(result.context.lobby_id);
    connection_state.begin_server(result.context.server_id);

    if (connection_state.should_connect_direct(result.context.server_id, result.endpoint)) {
        result.direct_connect_attempted = true;
        SteamNetworkingIPAddr address{};
        result.direct_connect_parse_failed = !parse_ipv4_endpoint(result.endpoint, address);
        if (!result.direct_connect_parse_failed) {
            SteamNetworkingConfigValue_t options[3] = {};
            options[0].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);
            options[1].SetInt32(k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth, 2);
            options[2].SetInt32(k_ESteamNetworkingConfig_Unencrypted, 2);
            result.connection = direct_connector.connect_by_ip_address(address, 3, options);
            result.direct_connect_succeeded = result.connection != 0u;
        }
        if (!result.direct_connect_parse_failed)
            connection_state.record_direct_connect(result.context.server_id, result.endpoint);
    }

    ++connection_state.retry_count;
    result.retry_count = connection_state.retry_count;
    result.size_changed = payload_size != connection_state.last_post_size;
    connection_state.last_post_size = payload_size;
    result.callback_already_queued = connection_state.engine_callback_queued(
        result.context.server_id,
        result.endpoint);
    if (result.callback_already_queued)
        return result;

    GameServerChangeRequested_t server_change{};
    std::strncpy(server_change.m_rgchServer, result.endpoint.c_str(), sizeof(server_change.m_rgchServer) - 1);
    server_change.m_rgchServer[sizeof(server_change.m_rgchServer) - 1] = '\0';
    callback_queue.queue_game_server_change(server_change, 0.0);
    connection_state.record_engine_callback(result.context.server_id, result.endpoint);
    result.callback_queued = true;
    return result;
}

const char *GBE_DescribeDotaReconnectPostSkipReason(GBE_DotaReconnectPostSkipReason reason)
{
    switch (reason) {
        case GBE_DotaReconnectPostSkipReason::None: return "none";
        case GBE_DotaReconnectPostSkipReason::NoContext: return "no_context";
        case GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby: return "ordinary_practice_lobby";
        case GBE_DotaReconnectPostSkipReason::ReconnectIneligible: return "reconnect_ineligible";
        case GBE_DotaReconnectPostSkipReason::StateNotReady: return "state_not_ready";
        case GBE_DotaReconnectPostSkipReason::MissingEndpoint: return "missing_endpoint";
        case GBE_DotaReconnectPostSkipReason::LocalOwner: return "local_owner";
    }
    return "unknown";
}

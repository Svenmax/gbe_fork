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

GBE_DotaReconnectPostPlan GBE_PrepareDotaReconnectPostConnectionState(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaSerializedConnectionState &connection_state)
{
    GBE_DotaReconnectPostPlan plan{};
    GBE_DotaReconnectPostResult &result = plan.result;
    result.has_context = context_provider.get_context(local_steam_id, false, result.context);
    if (result.has_context && result.context.custom_game_id == 0ull) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby;
        return plan;
    }

    const bool eligible = context_provider.reconnect_eligible();
    if (!eligible) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::ReconnectIneligible;
        return plan;
    }

    if (!result.has_context) {
        result.has_context = context_provider.get_context(local_steam_id, true, result.context);
        if (!result.has_context) {
            result.skip_reason = GBE_DotaReconnectPostSkipReason::NoContext;
            return plan;
        }
    }
    if (result.context.custom_game_id == 0ull) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::OrdinaryPracticeLobby;
        return plan;
    }

    result.endpoint = gbe::proto_wire::select_dota_arcade_connect_endpoint_for_local_player(
        result.context.connect,
        local_steam_id,
        result.context.owner_steam_id);
    if (!GBE_DotaReconnectContextIsStarted(result.context)) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::StateNotReady;
        return plan;
    }
    if (result.context.connect[0] == '\0' || result.endpoint.empty()) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::MissingEndpoint;
        return plan;
    }
    if (local_steam_id != 0ull && local_steam_id == result.context.owner_steam_id) {
        result.skip_reason = GBE_DotaReconnectPostSkipReason::LocalOwner;
        return plan;
    }

    connection_state.begin_generation(result.context.generation);
    connection_state.begin_server(result.context.server_id);

    if (connection_state.should_connect_direct(result.context.server_id, result.endpoint)) {
        result.direct_connect_attempted = true;
        result.direct_connect_parse_failed = !parse_ipv4_endpoint(result.endpoint, plan.direct_address);
        if (!result.direct_connect_parse_failed) {
            plan.direct_options[0].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);
            plan.direct_options[1].SetInt32(k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth, 2);
            plan.direct_options[2].SetInt32(k_ESteamNetworkingConfig_Unencrypted, 2);
            plan.connect_direct = true;
            connection_state.record_direct_connect(result.context.server_id, result.endpoint);
        }
    }

    ++connection_state.retry_count;
    result.retry_count = connection_state.retry_count;
    result.size_changed = payload_size != connection_state.last_post_size;
    connection_state.last_post_size = payload_size;
    result.callback_already_queued = connection_state.engine_callback_queued(
        result.context.server_id,
        result.endpoint);
    if (result.callback_already_queued)
        return plan;

    std::strncpy(plan.server_change.m_rgchServer, result.endpoint.c_str(), sizeof(plan.server_change.m_rgchServer) - 1);
    plan.server_change.m_rgchServer[sizeof(plan.server_change.m_rgchServer) - 1] = '\0';
    plan.queue_callback = true;
    connection_state.record_engine_callback(result.context.server_id, result.endpoint);
    return plan;
}

GBE_DotaReconnectPostResult GBE_ExecuteDotaReconnectPostEffects(
    GBE_DotaReconnectPostPlan plan,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue)
{
    GBE_DotaReconnectPostResult &result = plan.result;
    if (plan.connect_direct) {
        result.connection = direct_connector.connect_by_ip_address(
            plan.direct_address,
            static_cast<int>(plan.direct_options.size()),
            plan.direct_options.data());
        result.direct_connect_succeeded = result.connection != 0u;
    }
    if (plan.queue_callback) {
        callback_queue.queue_game_server_change(plan.server_change, 0.0, result.context.generation);
        result.callback_queued = true;
    }
    return result;
}

GBE_DotaReconnectPostResult GBE_ExecuteDotaReconnectPostConnectionState(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue,
    GBE_DotaSerializedConnectionState &connection_state)
{
    return GBE_ExecuteDotaReconnectPostEffects(
        GBE_PrepareDotaReconnectPostConnectionState(
            local_steam_id,
            payload_size,
            context_provider,
            connection_state),
        direct_connector,
        callback_queue);
}

const char *GBE_DescribeDotaReconnectPostSkipReason(GBE_DotaReconnectPostSkipReason reason)
{
    return gbe::dota_diagnostic::describe_reason(reason).data();
}

#include "dll/gbe_dota_reconnect_network_adapter.h"

#include "dll/callsystem.h"
#include "dll/steam_networking_sockets.h"

GBE_DotaReconnectNetworkAdapter::GBE_DotaReconnectNetworkAdapter(
    SteamCallBacks *callbacks,
    Steam_Networking_Sockets *direct_sockets)
    : callbacks(callbacks), direct_sockets(direct_sockets)
{
}

bool GBE_DotaReconnectNetworkAdapter::get_context(
    std::uint64_t local_steam_id,
    bool allow_generic_recovery,
    GBE_DotaReconnectContext &context)
{
    const bool has_context = GBE_GetDotaReconnectContext(&context);
    if (has_context || !allow_generic_recovery)
        return has_context;

    const std::time_t now = std::time(nullptr);
    if (last_recover_probe_local_id != local_steam_id || last_recover_probe_time != now) {
        last_recover_probe_local_id = local_steam_id;
        last_recover_probe_time = now;
        last_recover_probe_context = GBE_DotaReconnectContext{};
        last_recover_probe_has_context = GBE_TryRecoverDotaReconnectContextFromGenericLobbies(
            local_steam_id,
            &last_recover_probe_context);
    }
    if (!last_recover_probe_has_context)
        return false;
    context = last_recover_probe_context;
    return true;
}

bool GBE_DotaReconnectNetworkAdapter::reconnect_eligible() const
{
    return GBE_IsDotaReconnectEligible();
}

std::uint32_t GBE_DotaReconnectNetworkAdapter::connect_by_ip_address(
    const SteamNetworkingIPAddr &address,
    int option_count,
    const SteamNetworkingConfigValue_t *options)
{
    return direct_sockets
        ? direct_sockets->ConnectByIPAddress(address, option_count, options)
        : 0u;
}

void GBE_DotaReconnectNetworkAdapter::queue_game_server_change(
    const GameServerChangeRequested_t &server_change,
    double delay_seconds)
{
    if (!callbacks)
        return;
    callbacks->addCBResult(
        server_change.k_iCallback,
        const_cast<GameServerChangeRequested_t *>(&server_change),
        sizeof(server_change),
        delay_seconds);
}

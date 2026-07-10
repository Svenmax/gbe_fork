#include "dll/gbe_dota_reconnect_network_adapter.h"

#include "dll/callsystem.h"
#include "dll/steam_networking_sockets.h"

#include <cstring>

namespace {

bool is_current_dota_reconnect_generation(const void *guard_context, unsigned int guard_context_size)
{
    if (!guard_context || guard_context_size != sizeof(std::uint64_t))
        return false;

    std::uint64_t expected_generation{};
    std::memcpy(&expected_generation, guard_context, sizeof(expected_generation));
    GBE_DotaReconnectContext current_context{};
    const bool has_context = GBE_GetDotaReconnectContext(&current_context);
    const bool current = has_context && current_context.generation == expected_generation;
    if (!current) {
        GBE_ReconnectLogEvent({
            "reconnect.callback_generation",
            gbe::dota_diagnostic::Reason::StaleGeneration,
            gbe::dota_diagnostic::Source::DelayedTask,
            has_context ? current_context.lobby_id : 0u,
            expected_generation,
            has_context ? current_context.server_id : 0u,
            has_context ? current_context.connect : "",
            "rejected",
        });
    }
    return current;
}

} // namespace

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
    double delay_seconds,
    std::uint64_t generation)
{
    if (!callbacks)
        return;
    callbacks->addCBResult(
        server_change.k_iCallback,
        const_cast<GameServerChangeRequested_t *>(&server_change),
        sizeof(server_change),
        delay_seconds,
        false,
        SteamCallExecutionGuard(
            is_current_dota_reconnect_generation,
            &generation,
            sizeof(generation)));
}

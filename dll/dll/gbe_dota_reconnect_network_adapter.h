#ifndef __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_ADAPTER_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_ADAPTER_H__

#include "gbe_dota_reconnect_network.h"

#include <ctime>

class SteamCallBacks;
class Steam_Networking_Sockets;

class GBE_DotaReconnectNetworkAdapter final :
    public GBE_DotaReconnectContextProvider,
    public GBE_DotaReconnectDirectConnector,
    public GBE_DotaReconnectCallbackQueue
{
    SteamCallBacks *callbacks{};
    Steam_Networking_Sockets *direct_sockets{};
    std::time_t last_recover_probe_time{};
    std::uint64_t last_recover_probe_local_id{};
    bool last_recover_probe_has_context{};
    GBE_DotaReconnectContext last_recover_probe_context{};

public:
    GBE_DotaReconnectNetworkAdapter(
        SteamCallBacks *callbacks,
        Steam_Networking_Sockets *direct_sockets);

    bool get_context(
        std::uint64_t local_steam_id,
        bool allow_generic_recovery,
        GBE_DotaReconnectContext &context) override;
    bool reconnect_eligible() const override;
    std::uint32_t connect_by_ip_address(
        const SteamNetworkingIPAddr &address,
        int option_count,
        const SteamNetworkingConfigValue_t *options) override;
    void queue_game_server_change(
        const GameServerChangeRequested_t &server_change,
        double delay_seconds) override;
};

#endif // __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_ADAPTER_H__

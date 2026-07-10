#ifndef __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_H__

#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_serialized_connection_state.h"
#include "gbe_dota_diagnostic_event.h"
#include "steam/isteamfriends.h"
#include "steam/steamnetworkingtypes.h"

#include <array>
#include <cstdint>
#include <string>

struct GBE_DotaReconnectContextProvider {
    virtual ~GBE_DotaReconnectContextProvider() = default;
    virtual bool get_context(
        std::uint64_t local_steam_id,
        bool allow_generic_recovery,
        GBE_DotaReconnectContext &context) = 0;
    virtual bool reconnect_eligible() const = 0;
};

struct GBE_DotaReconnectDirectConnector {
    virtual ~GBE_DotaReconnectDirectConnector() = default;
    virtual std::uint32_t connect_by_ip_address(
        const SteamNetworkingIPAddr &address,
        int option_count,
        const SteamNetworkingConfigValue_t *options) = 0;
};

struct GBE_DotaReconnectCallbackQueue {
    virtual ~GBE_DotaReconnectCallbackQueue() = default;
    virtual void queue_game_server_change(
        const GameServerChangeRequested_t &server_change,
        double delay_seconds,
        std::uint64_t generation) = 0;
};

using GBE_DotaReconnectPostSkipReason = gbe::dota_diagnostic::Reason;

struct GBE_DotaReconnectPostResult {
    bool has_context{};
    GBE_DotaReconnectContext context{};
    GBE_DotaReconnectPostSkipReason skip_reason{GBE_DotaReconnectPostSkipReason::None};
    std::string endpoint;
    bool direct_connect_attempted{};
    bool direct_connect_succeeded{};
    bool direct_connect_parse_failed{};
    std::uint32_t connection{};
    std::uint32_t retry_count{};
    bool size_changed{};
    bool callback_already_queued{};
    bool callback_queued{};
};

struct GBE_DotaReconnectPostPlan {
    GBE_DotaReconnectPostResult result;
    bool connect_direct{};
    SteamNetworkingIPAddr direct_address{};
    std::array<SteamNetworkingConfigValue_t, 3> direct_options{};
    bool queue_callback{};
    GameServerChangeRequested_t server_change{};
};

GBE_DotaReconnectPostPlan GBE_PrepareDotaReconnectPostConnectionState(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaSerializedConnectionState &connection_state);

GBE_DotaReconnectPostResult GBE_ExecuteDotaReconnectPostEffects(
    GBE_DotaReconnectPostPlan plan,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue);

GBE_DotaReconnectPostResult GBE_ExecuteDotaReconnectPostConnectionState(
    std::uint64_t local_steam_id,
    std::uint32_t payload_size,
    GBE_DotaReconnectContextProvider &context_provider,
    GBE_DotaReconnectDirectConnector &direct_connector,
    GBE_DotaReconnectCallbackQueue &callback_queue,
    GBE_DotaSerializedConnectionState &connection_state);

const char *GBE_DescribeDotaReconnectPostSkipReason(GBE_DotaReconnectPostSkipReason reason);

#endif // __INCLUDED_GBE_DOTA_RECONNECT_NETWORK_H__

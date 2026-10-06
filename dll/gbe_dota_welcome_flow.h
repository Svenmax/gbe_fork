#ifndef GBE_DOTA_WELCOME_FLOW_H
#define GBE_DOTA_WELCOME_FLOW_H

#include "gbe_dota_protocol_constants.h"

#include <cstdint>

// Pure ClientHello side-effect plan (Phase E1 / GP-09 L1.5).
// Production: parse/build/adapt first, then plan, then execute in order.
// Offline tests assert disposition and ordered flags without coordinator IO.
namespace gbe::dota_welcome_flow {

enum class ClientHelloDisposition : std::uint8_t {
    RejectParse = 0,
    AcceptWithoutWelcome = 1,
    AcceptWithWelcome = 2,
};

struct ClientHelloPlanInput {
    bool parse_ok{};
    bool welcome_build_ok{};
    bool direct_message{};
    bool top_custom_games_available{};
};

struct ClientHelloPlan {
    ClientHelloDisposition disposition{ClientHelloDisposition::RejectParse};
    bool push_welcome{};
    std::uint32_t welcome_emsg_unmasked{};
    bool push_top_custom_games{};
    bool push_login_sync{};
};

inline ClientHelloPlan plan_client_hello(const ClientHelloPlanInput &input)
{
    ClientHelloPlan plan{};
    if (!input.parse_ok) {
        plan.disposition = ClientHelloDisposition::RejectParse;
        return plan;
    }
    if (!input.welcome_build_ok) {
        plan.disposition = ClientHelloDisposition::AcceptWithoutWelcome;
        return plan;
    }

    plan.disposition = ClientHelloDisposition::AcceptWithWelcome;
    plan.push_welcome = true;
    plan.welcome_emsg_unmasked =
        input.direct_message ? GBE_kEMsgGCClientWelcome : GBE_kEMsgClientFromGC;
    plan.push_top_custom_games = input.top_custom_games_available;
    plan.push_login_sync = input.direct_message;
    return plan;
}

// Pure ServerHello side-effect plan (Phase E2 / GP-09 L1.5).
// Production: parse + set context, gather flags, plan, then execute pushes in order.
// Offline tests assert skip/push dispositions without coordinator IO.
enum class ServerHelloDisposition : std::uint8_t {
    RejectParse = 0,
    AcceptSkipAlreadyConnected = 1,
    AcceptSkipWelcomeQueued = 2,
    AcceptWithoutWelcome = 3,
    AcceptWithWelcome = 4,
};

struct ServerHelloPlanInput {
    bool parse_ok{};
    bool is_server{};
    bool welcome_received{};
    bool server_welcome_already_queued{};
    bool welcome_build_ok{};
    bool active_lobby_with_id{};
    bool cache_build_ok{};
};

struct ServerHelloPlan {
    ServerHelloDisposition disposition{ServerHelloDisposition::RejectParse};
    bool push_server_welcome{};
    std::uint32_t welcome_emsg_unmasked{GBE_kEMsgGCServerWelcome};
    bool push_cache_subscribed{};
};

inline ServerHelloPlan plan_server_hello(const ServerHelloPlanInput &input)
{
    ServerHelloPlan plan{};
    if (!input.parse_ok) {
        plan.disposition = ServerHelloDisposition::RejectParse;
        return plan;
    }
    if (input.is_server && input.welcome_received) {
        plan.disposition = ServerHelloDisposition::AcceptSkipAlreadyConnected;
        return plan;
    }
    if (input.is_server && input.server_welcome_already_queued) {
        plan.disposition = ServerHelloDisposition::AcceptSkipWelcomeQueued;
        return plan;
    }
    if (!input.welcome_build_ok) {
        plan.disposition = ServerHelloDisposition::AcceptWithoutWelcome;
        return plan;
    }

    plan.disposition = ServerHelloDisposition::AcceptWithWelcome;
    plan.push_server_welcome = true;
    plan.welcome_emsg_unmasked = GBE_kEMsgGCServerWelcome;
    plan.push_cache_subscribed =
        input.is_server && input.active_lobby_with_id && input.cache_build_ok;
    return plan;
}

} // namespace gbe::dota_welcome_flow

#endif // GBE_DOTA_WELCOME_FLOW_H

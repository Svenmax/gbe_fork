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

} // namespace gbe::dota_welcome_flow

#endif // GBE_DOTA_WELCOME_FLOW_H

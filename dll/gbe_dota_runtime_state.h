#ifndef GBE_DOTA_RUNTIME_STATE_H
#define GBE_DOTA_RUNTIME_STATE_H

#include "dll/gbe_dota_reconnect_shared.h"
#include "gbe_dota_payload_wire_helpers.h"

#include <atomic>

namespace gbe::dota {

struct RuntimeState {
    bool recent_reconnect_context_valid{};
    GBE_DotaReconnectContext recent_reconnect_context{};
    std::atomic<bool> reconnect_eligible{true};
    GBE_DotaServerHelloContext last_server_hello_context{};
};

} // namespace gbe::dota

#endif

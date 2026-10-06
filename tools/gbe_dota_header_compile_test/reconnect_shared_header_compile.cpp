#include "dll/dll/gbe_dota_reconnect_shared.h"

GBE_DotaReconnectContext make_reconnect_context()
{
    GBE_DotaReconnectContext context{};
    context.generation = 1;
    return context;
}

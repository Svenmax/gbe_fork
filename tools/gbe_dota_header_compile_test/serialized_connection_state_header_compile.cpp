#include "dll/dll/gbe_dota_serialized_connection_state.h"

GBE_DotaSerializedConnectionState make_serialized_connection_state()
{
    GBE_DotaSerializedConnectionState state{};
    state.generation = 1;
    return state;
}

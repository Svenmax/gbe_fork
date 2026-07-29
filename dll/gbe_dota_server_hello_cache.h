#ifndef __INCLUDED_GBE_DOTA_SERVER_HELLO_CACHE_H__
#define __INCLUDED_GBE_DOTA_SERVER_HELLO_CACHE_H__

#include "gbe_dota_payload_wire_helpers.h"

bool GBE_HasLastDotaServerHelloContext();
const GBE_DotaServerHelloContext &GBE_GetLastDotaServerHelloContext();
void GBE_SetLastDotaServerHelloContext(const GBE_DotaServerHelloContext &context);
void GBE_ClearLastDotaServerHelloContext();

#endif // __INCLUDED_GBE_DOTA_SERVER_HELLO_CACHE_H__

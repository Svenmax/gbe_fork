#ifndef __INCLUDED_GBE_DOTA_GC_DIAGNOSTICS_H__
#define __INCLUDED_GBE_DOTA_GC_DIAGNOSTICS_H__

#include <string>

#include <steam/steamtypes.h>

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);
const char *GBE_DescribeDotaLaunchPhase(uint32 phase);
void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message);
void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size);
void GBE_LogDotaResponsePacket(
    const char *reason,
    uint32 inner_emsg,
    bool wrapped,
    const std::string &inner_payload,
    const std::string &outbound_payload,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state);

#endif // __INCLUDED_GBE_DOTA_GC_DIAGNOSTICS_H__

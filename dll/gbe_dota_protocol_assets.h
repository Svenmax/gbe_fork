#ifndef __INCLUDED_GBE_DOTA_PROTOCOL_ASSETS_H__
#define __INCLUDED_GBE_DOTA_PROTOCOL_ASSETS_H__

#include <array>

#include <steam/steamtypes.h>

extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint;
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64;
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64;
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32;
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64;
extern const char * const GBE_kOldDotaPracticeLobbyLobbyIdText;
extern const char * const GBE_kOldDotaPracticeLobbyLobbyIdTextAlt;
extern const char * const GBE_kDotaOfficial032PracticeLobby26Hex;
extern const char * const GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex;
extern const uint8 GBE_kDotaPracticeLobbyCacheSubscribedTemplate[312];
extern const uint32 GBE_kSteamTicketAuthComplete;
extern const char * const GBE_kDotaAbandonPersonaStateInitHex;

#endif // __INCLUDED_GBE_DOTA_PROTOCOL_ASSETS_H__

#ifndef GBE_DOTA_LOBBY_HANDLER_HELPERS_H
#define GBE_DOTA_LOBBY_HANDLER_HELPERS_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <steam/steamtypes.h>

struct GBE_DotaCustomGameDetails;
class Settings;
namespace gbe::proto_wire {
struct DotaPracticeLobbyDetailsRequest;
}

// Domain helpers shared by split lobby handler TUs (stage D.10.1).
void GBE_ApplyDotaCustomGameDetailsRequest(
    const gbe::proto_wire::DotaPracticeLobbyDetailsRequest &request,
    GBE_DotaCustomGameDetails &custom_game);
void GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(
    Settings *settings,
    GBE_DotaCustomGameDetails &custom_game);
uint64 GBE_GenerateDotaLobbyId();
uint64 GBE_GenerateDotaMatchId();
bool GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
    uint64 lobby_id,
    uint64 inviter_steam_id,
    uint64 invitee_steam_id,
    const std::string &inviter_name,
    const std::vector<std::pair<uint64, std::string>> &members,
    std::string &message);
bool GBE_IsDotaLobbyInviteCacheSubscribedPayload(const std::string &message);

#endif

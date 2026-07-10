#ifndef __INCLUDED_GBE_DOTA_RECONNECT_CONTEXT_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_CONTEXT_H__

#include "dll/gbe_dota_reconnect_shared.h"

#include <cstdint>
#include <string>
#include <vector>

struct GBE_LocalLobby;

enum class GBE_DotaReconnectSourceKind : std::uint8_t {
    Shared,
    Recent,
    Local,
    GenericRecovery,
};

struct GBE_DotaReconnectSource {
    GBE_DotaReconnectSourceKind kind{GBE_DotaReconnectSourceKind::Shared};
    bool valid{};
    bool active{};
    std::uint64_t lobby_id{};
    std::uint32_t lobby_state{};
    std::uint32_t game_state{};
    std::uint64_t server_id{};
    std::uint64_t custom_game_id{};
    bool owner_connected{};
    std::uint32_t launch_phase{};
    std::uint64_t owner_steam_id{};
    std::string connect;
};

namespace gbe::dota_reconnect {

using SourceKind = GBE_DotaReconnectSourceKind;

enum class RejectReason : std::uint8_t {
    None,
    InvalidSource,
    Inactive,
    GameNotStarted,
    MissingServerId,
    MissingEndpoint,
    NoEligibleSource,
};

using Source = GBE_DotaReconnectSource;

struct Selection {
    bool selected{};
    SourceKind source_kind{SourceKind::Shared};
    RejectReason reject_reason{RejectReason::NoEligibleSource};
    GBE_DotaReconnectContext context{};
};

Source source_from_shared_snapshot(const GBE_DotaReconnectSharedStateSnapshot &snapshot);
Source source_from_context(const GBE_DotaReconnectContext &context, bool valid, SourceKind kind);
Source source_from_local_lobby(const GBE_LocalLobby &lobby, SourceKind kind = SourceKind::Local);
Source source_from_generic_lobby(const GBE_LocalLobby &lobby, std::uint64_t local_steam_id);
RejectReason build_context(const Source &source, GBE_DotaReconnectContext &context);
Selection select_context(const std::vector<Source> &sources);
const char *describe_source_kind(SourceKind kind);
const char *describe_reject_reason(RejectReason reason);

} // namespace gbe::dota_reconnect

#endif // __INCLUDED_GBE_DOTA_RECONNECT_CONTEXT_H__

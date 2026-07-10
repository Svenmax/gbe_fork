#include "gbe_dota_reconnect_context.h"

#include "gbe_dota_lobby_state.h"
#include "gbe_proto_wire.h"

#include <cstring>
#include <limits>

namespace gbe::dota_reconnect {
namespace {

int source_priority(SourceKind kind)
{
    switch (kind) {
        case SourceKind::Shared: return 0;
        case SourceKind::Recent: return 1;
        case SourceKind::Local: return 2;
        case SourceKind::GenericRecovery: return 3;
    }
    return 4;
}

} // namespace

Source source_from_shared_lobby_snapshot(const GBE_SharedDotaLobbyState &snapshot)
{
    Source source{};
    source.kind = SourceKind::Shared;
    source.valid = snapshot.valid;
    source.active = snapshot.active;
    source.generation = snapshot.generation;
    source.lobby_id = snapshot.lobby_id;
    source.lobby_state = snapshot.state;
    source.game_state = snapshot.game_state;
    source.server_id = snapshot.server_id;
    source.custom_game_id = snapshot.custom_game.game_id;
    source.owner_connected = snapshot.owner_connected;
    source.launch_phase = snapshot.launch_phase;
    source.owner_steam_id = snapshot.owner_steam_id;
    source.connect = snapshot.connect;
    return source;
}

Source source_from_context(const GBE_DotaReconnectContext &context, bool valid, SourceKind kind)
{
    Source source{};
    source.kind = kind;
    source.valid = valid;
    source.active = true;
    source.generation = context.generation;
    source.lobby_id = context.lobby_id;
    source.lobby_state = context.lobby_state;
    source.game_state = context.game_state;
    source.server_id = context.server_id;
    source.custom_game_id = context.custom_game_id;
    source.owner_steam_id = context.owner_steam_id;
    source.connect = context.connect;
    return source;
}

Source source_from_local_lobby(const GBE_LocalLobby &lobby, SourceKind kind)
{
    Source source{};
    source.kind = kind;
    source.valid = true;
    source.active = lobby.active;
    source.generation = lobby.generation;
    source.lobby_id = lobby.lobby_id;
    source.lobby_state = lobby.state;
    source.game_state = lobby.game_state;
    source.server_id = lobby.server_id;
    source.custom_game_id = lobby.custom_game.game_id;
    source.owner_connected = lobby.owner_connected;
    source.launch_phase = lobby.launch_phase;
    source.owner_steam_id = lobby.owner_steam_id;
    source.connect = lobby.connect;
    return source;
}

Source source_from_generic_lobby(const GBE_LocalLobby &lobby, std::uint64_t local_steam_id)
{
    Source source = source_from_local_lobby(lobby, SourceKind::GenericRecovery);
    bool local_in_lobby = local_steam_id == 0ull;
    for (const GBE_DotaLobbyMemberState &member : lobby.members) {
        if (member.steam_id == local_steam_id) {
            local_in_lobby = true;
            break;
        }
    }
    source.valid = source.custom_game_id != 0ull &&
        (local_steam_id == 0ull || source.owner_steam_id != local_steam_id) &&
        local_in_lobby;
    return source;
}

RejectReason build_context(const Source &source, GBE_DotaReconnectContext &context)
{
    context = GBE_DotaReconnectContext{};
    const auto eligibility = gbe::dota_lobby_state::compute_reconnect_eligibility_decision(
        source.valid,
        source.active,
        source.lobby_state,
        source.game_state,
        source.server_id,
        !source.connect.empty(),
        source.custom_game_id,
        source.owner_connected,
        source.launch_phase);
    if (!eligibility.source_valid)
        return RejectReason::InvalidSource;
    if (!eligibility.active)
        return RejectReason::Inactive;
    if (!eligibility.started)
        return RejectReason::GameNotStarted;
    if (!eligibility.has_server_id)
        return RejectReason::MissingServerId;
    if (!eligibility.has_connect)
        return RejectReason::MissingEndpoint;

    const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(source.connect);
    if (endpoint.empty())
        return RejectReason::MissingEndpoint;

    context.generation = source.generation;
    context.lobby_id = source.lobby_id;
    context.server_id = source.server_id;
    context.lobby_state = source.lobby_state;
    context.game_state = source.game_state;
    context.custom_game_id = source.custom_game_id;
    std::strncpy(context.connect, endpoint.c_str(), sizeof(context.connect) - 1);
    context.connect[sizeof(context.connect) - 1] = '\0';
    context.owner_steam_id = source.owner_steam_id;
    return RejectReason::None;
}

Selection select_context(const std::vector<Source> &sources)
{
    Selection selection{};
    int first_rejected_priority = std::numeric_limits<int>::max();
    for (int priority = 0; priority <= source_priority(SourceKind::GenericRecovery); ++priority) {
        for (const Source &source : sources) {
            const SourceKind kind = source.kind;
            if (source_priority(kind) != priority)
                continue;
            GBE_DotaReconnectContext context{};
            const RejectReason reject_reason = build_context(source, context);
            if (reject_reason != RejectReason::None) {
                if (priority < first_rejected_priority) {
                    first_rejected_priority = priority;
                    selection.source_kind = kind;
                    selection.reject_reason = reject_reason;
                }
                continue;
            }
            selection.selected = true;
            selection.source_kind = kind;
            selection.reject_reason = RejectReason::None;
            selection.context = context;
            return selection;
        }
    }
    return selection;
}

const char *describe_source_kind(SourceKind kind)
{
    return gbe::dota_diagnostic::describe_source(kind).data();
}

const char *describe_reject_reason(RejectReason reason)
{
    return gbe::dota_diagnostic::describe_reason(reason).data();
}

} // namespace gbe::dota_reconnect

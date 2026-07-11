#include "dll/steam_game_coordinator.h"

#include "gbe_dota_gc_internal.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_proto_wire.h"

namespace registry = gbe::dota_handler_registry;

registry::View Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry()
{
    auto adapt_join_chat_channel = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaJoinChatChannelRequest(c.body, c.wrapped, sess); };
    auto adapt_practice_lobby_create = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyCreateRequest(c.body, c.request_job_id, c.has_request_job, c.wrapped, sess); };
    auto adapt_lobby_list = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaLobbyListRequest(c.has_request_job, c.request_job_id, c.wrapped, sess); };
    auto adapt_custom_lobby_list = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaCustomLobbyListRequest(c.body, c.wrapped, sess); };
    auto adapt_friend_practice_lobby_list = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaFriendPracticeLobbyListRequest(c.wrapped, sess); };
    auto adapt_invite_to_lobby = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaInviteToLobbyRequest(c.body, c.wrapped, sess); };
    auto adapt_lobby_invite_response = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaLobbyInviteResponseRequest(c.body, c.wrapped, sess); };
    auto adapt_practice_lobby_join = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyJoinRequest(c.body, c.request_job_id, c.has_request_job, c.wrapped, sess); };
    auto adapt_practice_lobby_leave = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyLeaveRequest(c.wrapped, sess); };
    auto adapt_practice_lobby_launch = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyLaunchRequest(c.body, c.wrapped, sess, c.has_request_job, c.request_job_id); };
    auto adapt_practice_lobby_set_details = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbySetDetailsRequest(c.body, c.wrapped, sess); };
    auto adapt_practice_lobby_set_team_slot = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbySetTeamSlotRequest(c.body, c.request_job_id, c.has_request_job, c.wrapped, sess); };
    auto adapt_practice_lobby_kick = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyKickRequest(c.body, c.wrapped, sess); };
    auto adapt_practice_lobby_join_broadcast = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(c.body, c.request_job_id, c.has_request_job, c.wrapped, sess); };
    auto adapt_lobby_update_broadcast_info = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(c.body, c.wrapped, sess); };
    auto adapt_practice_lobby_close_broadcast = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *sess) -> bool { return self->GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(c.body, c.wrapped, sess); };
    auto adapt_custom_game_lifecycle = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return self->GBE_HandleDotaCustomGameLifecycleRequest(c); };
    auto adapt_direct_7427_notifications = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDota7427NotificationsRequest(c.has_request_job, c.request_job_id); };
    auto adapt_direct_upload_rate = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaUploadRateRequest(c.has_request_job, c.request_job_id); };
    auto adapt_direct_rank = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaRankRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };
    auto adapt_direct_profile_card = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaProfileCardRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };
    auto adapt_direct_lookup_account_name = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaLookupAccountNameRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };
    auto adapt_direct_emoticon_data = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaEmoticonDataRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };
    auto adapt_direct_conduct_scorecard = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaConductScorecardRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };
    auto adapt_direct_coaching_summary = +[](Steam_Game_Coordinator *self, const gbe::dota_gc_router::DotaGcRequestContext &c, const std::string *) -> bool { return c.path == gbe::dota_gc_router::DotaGcRequestPath::Direct && self->GBE_HandleDotaCoachingSummaryRequest(reinterpret_cast<const uint8 *>(c.body.data()), c.body.size(), c.has_request_job, c.request_job_id); };

    static const registry::Entry kTable[] = {
        { GBE_kDotaJoinChatChannel, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_join_chat_channel, registry::HandlerId::JoinChatChannel, "smoke:test_chat_join_channel" },
        { GBE_kDotaPracticeLobbyCreate, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_practice_lobby_create, registry::HandlerId::PracticeLobbyCreate, "replay:lobby_lifecycle:lobby_create_with_passkey" },
        { GBE_kDotaLobbyList, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyRead, adapt_lobby_list, registry::HandlerId::LobbyList, nullptr },
        { GBE_kDotaCustomLobbyListRequest, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyRead, adapt_custom_lobby_list, registry::HandlerId::CustomLobbyList, nullptr },
        { GBE_kDotaFriendPracticeLobbyListRequest, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyRead, adapt_friend_practice_lobby_list, registry::HandlerId::FriendPracticeLobbyList, nullptr },
        { GBE_kGCInviteToLobby, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_invite_to_lobby, registry::HandlerId::InviteToLobby, "smoke:test_lobby_invite_to_lobby_preserves_wrapped_response" },
        { GBE_kGCLobbyInviteResponse, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_lobby_invite_response, registry::HandlerId::LobbyInviteResponse, "smoke:test_lobby_invite_response_decline_pushes_remove_then_unsubscribe" },
        { GBE_kDotaPracticeLobbyJoin, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_practice_lobby_join, registry::HandlerId::PracticeLobbyJoin, "replay:lobby_lifecycle:lobby_join_by_id" },
        { GBE_kDotaPracticeLobbyLeave, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_practice_lobby_leave, registry::HandlerId::PracticeLobbyLeave, "replay:game_flow:lobby_leave_teardown" },
        { GBE_kDotaPracticeLobbyLaunch, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_practice_lobby_launch, registry::HandlerId::PracticeLobbyLaunch, "replay:game_flow:lobby_launch_allpick" },
        { GBE_kDotaPracticeLobbySetDetails, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_practice_lobby_set_details, registry::HandlerId::PracticeLobbySetDetails, "smoke:test_lobby_set_details_mutates_before_publish_and_details_update" },
        { GBE_kDotaPracticeLobbySetTeamSlot, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_practice_lobby_set_team_slot, registry::HandlerId::PracticeLobbySetTeamSlot, "smoke:test_lobby_set_team_slot_publishes_before_details_and_ack" },
        { GBE_kDotaPracticeLobbyKick, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_practice_lobby_kick, registry::HandlerId::PracticeLobbyKick, "replay:lobby_lifecycle:lobby_kick_player" },
        { GBE_kDotaPracticeLobbyJoinBroadcastChannel, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_practice_lobby_join_broadcast, registry::HandlerId::PracticeLobbyJoinBroadcastChannel, "smoke:test_lobby_join_broadcast_publishes_before_details_and_ack" },
        { GBE_kDotaLobbyUpdateBroadcastChannelInfo, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_lobby_update_broadcast_info, registry::HandlerId::LobbyUpdateBroadcastChannelInfo, "smoke:test_lobby_update_broadcast_publishes_before_details" },
        { GBE_kDotaPracticeLobbyCloseBroadcastChannel, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyMutation, adapt_practice_lobby_close_broadcast, registry::HandlerId::PracticeLobbyCloseBroadcastChannel, "smoke:test_lobby_close_broadcast_publishes_before_details" },
        { 7070u, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_custom_game_lifecycle, registry::HandlerId::CustomGameReadyUp, "smoke:test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence" },
        { 8052u, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_custom_game_lifecycle, registry::HandlerId::CustomGameStartedLoading, "smoke:test_custom_game_lifecycle_8052_direct_wrapped_action_sequence_equivalence" },
        { 8053u, registry::RequestMode::DirectAndWrapped, registry::SessionPolicy::ForwardWrappedSession, registry::LifecycleClass::LobbyLifecycle, adapt_custom_game_lifecycle, registry::HandlerId::CustomGameFinishedLoading, "smoke:test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence" },
        { 7427u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_7427_notifications, registry::HandlerId::Notifications7427, nullptr },
        { 4523u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_upload_rate, registry::HandlerId::UploadRate, nullptr },
        { 8879u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_rank, registry::HandlerId::Rank, nullptr },
        { 7534u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_profile_card, registry::HandlerId::ProfileCard, nullptr },
        { 2581u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_lookup_account_name, registry::HandlerId::LookupAccountName, nullptr },
        { 7503u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_emoticon_data, registry::HandlerId::EmoticonData, nullptr },
        { 8095u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_conduct_scorecard, registry::HandlerId::ConductScorecard, nullptr },
        { 8800u, registry::RequestMode::Direct, registry::SessionPolicy::Ignore, registry::LifecycleClass::None, adapt_direct_coaching_summary, registry::HandlerId::CoachingSummary, nullptr },
    };
    return {kTable, sizeof(kTable) / sizeof(kTable[0])};
}

bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const gbe::dota_gc_router::DotaGcRequestContext &context)
{
    if (!context.valid)
        return false;
    const registry::Entry *entry = registry::find_entry(handler_registry.entries, handler_registry.size, context.inner_emsg, context.path);
    if (!entry)
        return false;
    const std::string *session = registry::forwards_wrapped_session(entry->session_policy)
        ? gbe::dota_gc_router::outer_session_field_or_null(context) : nullptr;
    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Received %s %u has_job=%u request_job=%llu session_raw_size=%zu body_size=%zu body_prefix=%s",
        context.wrapped ? "wrapped" : "direct", context.inner_emsg, context.has_request_job ? 1u : 0u,
        static_cast<unsigned long long>(context.request_job_id), context.outer_session_field_raw.size(), context.body.size(),
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.body.data()), context.body.size(), 48).c_str());
    return entry->adapter(this, context, session);
}

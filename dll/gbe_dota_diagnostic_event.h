#ifndef GBE_DOTA_DIAGNOSTIC_EVENT_H
#define GBE_DOTA_DIAGNOSTIC_EVENT_H

#include <cstdint>
#include <string>
#include <string_view>

namespace gbe::dota_diagnostic {

enum class Reason : std::uint8_t {
    Unknown,
    None,
    Selected,
    InvalidSource,
    Inactive,
    GameNotStarted,
    MissingServerId,
    MissingEndpoint,
    NoEligibleSource,
    NoContext,
    OrdinaryPracticeLobby,
    ReconnectIneligible,
    StateNotReady,
    LocalOwner,
    DisconnectCurrentGameAfterCacheUnsubscribed,
    CustomRuntimeMemberRefresh,
    LeaveChat,
    FinishedLoading,
    LoadFailed,
    LaunchPoll,
    ParseFailed,
    AlreadyQueued,
    StaleGeneration,
    PreviousActionFailed,
    RuntimeUpdateQueued,
    ActionFailed,
};

enum class Source : std::uint8_t {
    Unknown,
    Shared,
    Recent,
    Local,
    GenericRecovery,
    Direct,
    Wrapped,
    DelayedTask,
    SerializedState,
    CallbackQueue,
};

struct Event {
    std::string_view event;
    Reason reason{Reason::Unknown};
    Source source{Source::Unknown};
    std::uint64_t lobby_id{};
    std::uint64_t generation{};
    std::uint64_t server_id{};
    std::string_view endpoint;
    std::string_view decision;
    std::uint32_t message_id{};
    bool has_message_id{};
    std::uint64_t job_id{};
    bool has_job_id{};
};

constexpr std::string_view describe_reason(Reason reason)
{
    switch (reason) {
    case Reason::None: return "none";
    case Reason::Selected: return "selected";
    case Reason::InvalidSource: return "invalid_source";
    case Reason::Inactive: return "inactive";
    case Reason::GameNotStarted: return "game_not_started";
    case Reason::MissingServerId: return "missing_server_id";
    case Reason::MissingEndpoint: return "missing_endpoint";
    case Reason::NoEligibleSource: return "no_eligible_source";
    case Reason::NoContext: return "no_context";
    case Reason::OrdinaryPracticeLobby: return "ordinary_practice_lobby";
    case Reason::ReconnectIneligible: return "reconnect_ineligible";
    case Reason::StateNotReady: return "state_not_ready";
    case Reason::LocalOwner: return "local_owner";
    case Reason::DisconnectCurrentGameAfterCacheUnsubscribed: return "7035_disconnect_current_game_after_25";
    case Reason::CustomRuntimeMemberRefresh: return "7034_custom_runtime_member_refresh";
    case Reason::LeaveChat: return "7272_leave_chat";
    case Reason::FinishedLoading: return "8053_finished_loading";
    case Reason::LoadFailed: return "8053_load_failed";
    case Reason::LaunchPoll: return "7034_launch_poll";
    case Reason::ParseFailed: return "parse_failed";
    case Reason::AlreadyQueued: return "already_queued";
    case Reason::StaleGeneration: return "stale_generation";
    case Reason::PreviousActionFailed: return "previous_action_failed";
    case Reason::RuntimeUpdateQueued: return "runtime_update_queued";
    case Reason::ActionFailed: return "action_failed";
    case Reason::Unknown: return "unknown";
    }
    return "unknown";
}

constexpr Reason reason_from_string(std::string_view value)
{
    for (std::uint8_t raw = static_cast<std::uint8_t>(Reason::None);
         raw <= static_cast<std::uint8_t>(Reason::ActionFailed);
         ++raw) {
        const Reason reason = static_cast<Reason>(raw);
        if (describe_reason(reason) == value)
            return reason;
    }
    return Reason::Unknown;
}

constexpr std::string_view describe_source(Source source)
{
    switch (source) {
    case Source::Shared: return "shared";
    case Source::Recent: return "recent";
    case Source::Local: return "local";
    case Source::GenericRecovery: return "generic_recovery";
    case Source::Direct: return "direct";
    case Source::Wrapped: return "wrapped";
    case Source::DelayedTask: return "delayed_task";
    case Source::SerializedState: return "serialized_state";
    case Source::CallbackQueue: return "callback_queue";
    case Source::Unknown: return "unknown";
    }
    return "unknown";
}

constexpr Source source_from_string(std::string_view value)
{
    for (std::uint8_t raw = static_cast<std::uint8_t>(Source::Shared);
         raw <= static_cast<std::uint8_t>(Source::CallbackQueue);
         ++raw) {
        const Source source = static_cast<Source>(raw);
        if (describe_source(source) == value)
            return source;
    }
    return Source::Unknown;
}

inline std::string format_event(const Event &event)
{
    std::string output = "event=";
    output.append(event.event);
    output += " reason=";
    output.append(describe_reason(event.reason));
    output += " source=";
    output.append(describe_source(event.source));
    output += " lobby_id=" + std::to_string(event.lobby_id);
    output += " generation=" + std::to_string(event.generation);
    output += " server_id=" + std::to_string(event.server_id);
    output += " endpoint=";
    output.append(event.endpoint.empty() ? std::string_view{"-"} : event.endpoint);
    output += " decision=";
    output.append(event.decision.empty() ? std::string_view{"-"} : event.decision);
    output += " message_id=";
    output += event.has_message_id ? std::to_string(event.message_id) : "-";
    output += " job_id=";
    output += event.has_job_id ? std::to_string(event.job_id) : "-";
    return output;
}

constexpr Event with_message_id(Event event, std::uint32_t message_id)
{
    event.message_id = message_id;
    event.has_message_id = true;
    return event;
}

constexpr Event with_job_id(Event event, std::uint64_t job_id)
{
    event.job_id = job_id;
    event.has_job_id = true;
    return event;
}

} // namespace gbe::dota_diagnostic

#endif

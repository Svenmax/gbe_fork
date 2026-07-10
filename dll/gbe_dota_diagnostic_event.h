#ifndef GBE_DOTA_DIAGNOSTIC_EVENT_H
#define GBE_DOTA_DIAGNOSTIC_EVENT_H

#include <cstdint>
#include <string_view>

namespace gbe::dota_diagnostic {

struct Event {
    std::string_view event;
    std::string_view reason;
    std::string_view source;
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

#include "dll/gbe_dota_diagnostic_event.h"

#include <type_traits>

namespace diagnostic = gbe::dota_diagnostic;

constexpr diagnostic::Event base_event{
    "reconnect.context_selected",
    "selected",
    "shared",
    42u,
    7u,
    99u,
    "127.0.0.1:27015",
    "connect",
};

constexpr diagnostic::Event message_event = diagnostic::with_message_id(base_event, 7034u);
constexpr diagnostic::Event job_event = diagnostic::with_job_id(message_event, 0u);

static_assert(std::is_aggregate<diagnostic::Event>::value);
static_assert(base_event.event == "reconnect.context_selected");
static_assert(base_event.reason == "selected");
static_assert(base_event.source == "shared");
static_assert(base_event.lobby_id == 42u);
static_assert(base_event.generation == 7u);
static_assert(base_event.server_id == 99u);
static_assert(base_event.endpoint == "127.0.0.1:27015");
static_assert(base_event.decision == "connect");
static_assert(!base_event.has_message_id);
static_assert(!base_event.has_job_id);
static_assert(message_event.has_message_id && message_event.message_id == 7034u);
static_assert(job_event.has_job_id && job_event.job_id == 0u);

#include "../../dll/gbe_dota_lifecycle_state_machine.h"

static_assert(
    gbe::dota_lifecycle_state_machine::reset_event().kind ==
    gbe::dota_lifecycle_state_machine::EventKind::Reset);

#include "../../dll/gbe_dota_lifecycle_state_machine.h"

static_assert(
    gbe::dota_lifecycle_state_machine::reset_event().kind ==
    gbe::dota_lifecycle_state_machine::EventKind::Reset);

static_assert(
    gbe::dota_lifecycle_state_machine::transition(
        gbe::dota_lifecycle_state_machine::State::PostGame,
        gbe::dota_lifecycle_state_machine::reset_event()).state ==
    gbe::dota_lifecycle_state_machine::State::Idle);

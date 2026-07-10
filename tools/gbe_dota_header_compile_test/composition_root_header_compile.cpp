#include "dll/gbe_dota_composition_root.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<gbe::dota::CompositionRoot>);
static_assert(!std::is_move_constructible_v<gbe::dota::CompositionRoot>);

#include "../../dll/gbe_dota_lifecycle_state_machine.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace lifecycle = gbe::dota_lifecycle_state_machine;

struct ExpectedMapping {
    std::uint32_t message_id;
    lifecycle::EventKind kind;
};

int main()
{
    constexpr std::array<ExpectedMapping, 9> mappings{{
        { GBE_kDotaPracticeLobbyCreate, lifecycle::EventKind::Create },
        { GBE_kDotaPracticeLobbyJoin, lifecycle::EventKind::Join },
        { GBE_kDotaPracticeLobbyLaunch, lifecycle::EventKind::Setup },
        { 8052u, lifecycle::EventKind::Loading },
        { 8053u, lifecycle::EventKind::Loaded },
        { 7070u, lifecycle::EventKind::Run },
        { GBE_kDotaGameMatchSignOut, lifecycle::EventKind::PostGame },
        { GBE_kDotaPracticeLobbyLeave, lifecycle::EventKind::Leave },
        { GBE_kDotaAbandonCurrentGame, lifecycle::EventKind::Abandon },
    }};

    for (const ExpectedMapping &expected : mappings) {
        const lifecycle::EventMapping direct = lifecycle::event_from_message(expected.message_id, false);
        const lifecycle::EventMapping wrapped = lifecycle::event_from_message(expected.message_id, true);

        assert(direct.mapped);
        assert(wrapped.mapped);
        assert(direct.event.kind == expected.kind);
        assert(wrapped.event.kind == expected.kind);
        assert(direct.event.message_id == expected.message_id);
        assert(wrapped.event.message_id == expected.message_id);
        assert(direct.event.source == lifecycle::EventSource::Direct);
        assert(wrapped.event.source == lifecycle::EventSource::Wrapped);
    }

    const lifecycle::EventMapping unknown = lifecycle::event_from_message(0xffffffffu, false);
    assert(!unknown.mapped);

    const lifecycle::Event reset = lifecycle::reset_event();
    assert(reset.kind == lifecycle::EventKind::Reset);
    assert(reset.source == lifecycle::EventSource::Internal);
    assert(reset.message_id == 0u);

    static_assert(static_cast<std::uint8_t>(lifecycle::State::Idle) == 0u);
    static_assert(static_cast<std::uint8_t>(lifecycle::State::PostGame) == 7u);

    std::cout << "gbe_dota_lifecycle_state_machine_test passed\n";
    return 0;
}

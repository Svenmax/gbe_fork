#include "test_fixture.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

struct LobbySnapshot
{
    bool active{};
    uint64 lobby_id{};
    uint64 generation{};
    uint32 state{};
    uint32 game_state{};
};

LobbySnapshot snapshot(const Steam_Game_Coordinator &gc)
{
    return {
        gc.GBE_local_lobby.active,
        gc.GBE_local_lobby.lobby_id,
        gc.GBE_local_lobby.generation,
        gc.GBE_local_lobby.state,
        gc.GBE_local_lobby.game_state,
    };
}

std::string normalized_lobby_id(uint64 lobby_id, uint64 replay_lobby_id)
{
    if (lobby_id == 0)
        return "none";
    if (lobby_id == replay_lobby_id)
        return "lobby-1";
    return "other";
}

void append_snapshot(std::ostringstream &trace, const char *label, const LobbySnapshot &value, uint64 replay_lobby_id)
{
    trace << label
          << " active=" << (value.active ? 1 : 0)
          << " lobby=" << normalized_lobby_id(value.lobby_id, replay_lobby_id)
          << " generation=" << value.generation
          << " state=" << value.state
          << " game_state=" << value.game_state
          << '\n';
}

void append_effects(std::ostringstream &trace, const ActionRecorder &recorder, size_t begin)
{
    trace << "effects";
    for (size_t i = begin; i < recorder.actions.size(); ++i) {
        const RecordedAction &action = recorder.actions[i];
        trace << (i == begin ? " " : " -> ") << action.type_name();
        if (action.type == GBE_DotaActionType::PushIncomingNow ||
            action.type == GBE_DotaActionType::PushIncoming ||
            action.type == GBE_DotaActionType::ServerGcForward) {
            trace << "(emsg=" << (action.msg_type & ~Steam_Game_Coordinator::protobuf_mask) << ")";
        }
        if (!action.reason.empty())
            trace << "[reason=" << action.reason << "]";
    }
    trace << '\n';
}

gbe::dota_gc_router::DotaGcRequestContext request(uint32 emsg, JobID_t job_id = 0)
{
    gbe::dota_gc_router::DotaGcRequestContext context{};
    context.valid = true;
    context.inner_emsg = emsg;
    context.request_job_id = job_id;
    context.has_request_job = job_id != 0;
    context.path = gbe::dota_gc_router::DotaGcRequestPath::Direct;
    return context;
}

bool run_step(
    TestFixture &fixture,
    std::ostringstream &trace,
    const char *handler,
    const gbe::dota_gc_router::DotaGcRequestContext &context,
    uint64 replay_lobby_id)
{
    const LobbySnapshot before = snapshot(fixture.gc);
    const size_t effects_begin = fixture.recorder.actions.size();
    const bool handled = fixture.gc.GBE_DispatchDotaPostLoginRequest(context);
    const LobbySnapshot after = snapshot(fixture.gc);

    trace << "handler=" << handler << " handled=" << (handled ? 1 : 0) << '\n';
    append_snapshot(trace, "before", before, replay_lobby_id);
    append_snapshot(trace, "after", after, replay_lobby_id == 0 ? after.lobby_id : replay_lobby_id);
    append_effects(trace, fixture.recorder, effects_begin);
    return handled;
}

std::string read_file(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: gbe_dota_behavior_replay <golden-trace>\n";
        return 2;
    }

    TestFixture fixture;
    fixture.reset();
    fixture.gc.handler_registry = Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry();

    std::ostringstream trace;
    if (!run_step(fixture, trace, "PracticeLobbyCreate", request(7038u, 1001u), 0))
        return 1;

    const uint64 replay_lobby_id = fixture.gc.GBE_local_lobby.lobby_id;
    if (!run_step(fixture, trace, "PracticeLobbyLaunch", request(7041u, 1002u), replay_lobby_id))
        return 1;
    if (!run_step(fixture, trace, "PracticeLobbyLeave", request(7040u, 1003u), replay_lobby_id))
        return 1;

    const std::string actual = trace.str();
    const std::string expected = read_file(argv[1]);
    if (actual != expected) {
        std::cerr << "behavior replay mismatch\n--- expected ---\n"
                  << expected << "--- actual ---\n" << actual;
        return 1;
    }

    std::cout << actual;
    return 0;
}

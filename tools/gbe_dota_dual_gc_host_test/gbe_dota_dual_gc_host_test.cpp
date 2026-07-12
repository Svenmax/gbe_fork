// Dual-role host path gate tests (Phase A).
//
// Aligns with production assembly semantics in dll/steam_client.cpp:
//   - one shared gbe::dota_lobby_state::Store
//   - client local lobby + server local lobby (two role-local caches)
//
// Does NOT link full Steam_Game_Coordinator (offline gate). Host rules under
// test are pure transitions and one-shot keys mirrored from production:
//   - publish_local_lobby_to_shared / adopt_shared_lobby_to_local
//   - peer restore conditions (match_handlers 7034 owner hero)
//   - showcase / wearable one-shot keys (steam_game_coordinator.cpp)
//
// HOST_AUTHORITY_TABLE: see
// .monkeycode/specs/2026-07-12-gc-behavior-gate-stabilization/host_authority.md
//
// H1-H5 may fail if production rules drift; Phase A allows red baseline for
// full-GC paths. Pure-state cases below must stay green.

#include "dll/gbe_dota_lobby_generation.h"
#include "dll/gbe_dota_lobby_state.h"
#include "dll/gbe_dota_lobby_state_store.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace {

int g_failures = 0;

void expect_true(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
}

void expect_eq_u32(std::uint32_t actual, std::uint32_t expected, const char *message)
{
    if (actual == expected)
        return;
    std::fprintf(stderr, "FAIL: %s actual=%u expected=%u\n", message, actual, expected);
    ++g_failures;
}

void expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *message)
{
    if (actual == expected)
        return;
    std::fprintf(
        stderr,
        "FAIL: %s actual=%llu expected=%llu\n",
        message,
        static_cast<unsigned long long>(actual),
        static_cast<unsigned long long>(expected));
    ++g_failures;
}

// Mirrors Steam_Game_Coordinator showcase / wearable one-shot keys.
struct HostShowcaseKey {
    std::uint64_t generation{};
    std::uint64_t lobby_id{};
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_hero_id{};

    bool has_pushed(
        std::uint64_t current_generation,
        std::uint64_t lobby_id,
        std::uint64_t owner_steam_id,
        std::uint32_t owner_hero_id) const
    {
        return generation == current_generation &&
            this->lobby_id == lobby_id &&
            this->owner_steam_id == owner_steam_id &&
            this->owner_hero_id == owner_hero_id &&
            owner_hero_id != 0u;
    }

    void mark(
        std::uint64_t current_generation,
        std::uint64_t lobby_id,
        std::uint64_t owner_steam_id,
        std::uint32_t owner_hero_id)
    {
        generation = current_generation;
        this->lobby_id = lobby_id;
        this->owner_steam_id = owner_steam_id;
        this->owner_hero_id = owner_hero_id;
    }

    void clear()
    {
        *this = {};
    }
};

struct HostWearableKey {
    std::uint64_t generation{};
    std::uint64_t steam_id{};
    std::uint32_t hero_id{};

    bool has_refreshed(
        std::uint64_t current_generation,
        std::uint64_t steam_id,
        std::uint32_t hero_id) const
    {
        return generation == current_generation &&
            this->steam_id == steam_id &&
            this->hero_id == hero_id;
    }

    void mark(std::uint64_t current_generation, std::uint64_t steam_id, std::uint32_t hero_id)
    {
        generation = current_generation;
        this->steam_id = steam_id;
        this->hero_id = hero_id;
    }

    void clear()
    {
        *this = {};
    }
};

// Dual-role fixture: shared Store + two local lobbies + host one-shot keys on server role.
struct DualGcFixture {
    GBE_SharedDotaLobbyState shared_backing{};
    std::recursive_mutex shared_mutex{};
    gbe::dota_lobby_state::Store store{shared_backing, shared_mutex};

    GBE_LocalLobby client_local{};
    GBE_LocalLobby server_local{};
    gbe::dota_lobby_generation::Counter client_generation{};
    gbe::dota_lobby_generation::Counter server_generation{};

    HostShowcaseKey server_showcase{};
    HostWearableKey server_wearable{};

    std::uint64_t lobby_id{0x1001ull};
    std::uint64_t owner_steam_id{0x110000100000001ull};
    std::uint32_t owner_hero_id{42u};

    void seed_active_pair(std::uint32_t client_hero, std::uint32_t server_hero)
    {
        const auto gen = client_generation.advance(gbe::dota_lobby_generation::Boundary::Create);
        expect_true(gen.advanced, "seed generation advance");
        server_generation = gbe::dota_lobby_generation::Counter(gen.current);

        auto fill = [&](GBE_LocalLobby &lobby, std::uint32_t hero) {
            lobby = {};
            lobby.active = true;
            lobby.generation = gen.current.value;
            lobby.lobby_id = lobby_id;
            lobby.owner_steam_id = owner_steam_id;
            lobby.owner_hero_id = hero;
            lobby.state = 2u;
            lobby.game_state = 2u;
            lobby.match_id = 9001ull;
        };
        fill(client_local, client_hero);
        fill(server_local, server_hero);
    }

    void publish_from(bool from_server)
    {
        GBE_SharedDotaLobbyState snap = store.snapshot();
        const GBE_LocalLobby &local = from_server ? server_local : client_local;
        gbe::dota_lobby_state::publish_local_lobby_to_shared(local, from_server, snap);
        const auto result = store.publish_if_generation_current_or_newer(snap);
        expect_true(result == gbe::dota_lobby_state::StoreUpdateResult::Applied, "publish applied");
    }

    void restore_into(bool into_server)
    {
        const auto snap = store.snapshot();
        GBE_LocalLobby &local = into_server ? server_local : client_local;
        gbe::dota_lobby_state::adopt_shared_lobby_to_local(snap, false, false, local);
    }

    // Production peer restore gate + apply_owner_hero_id (match_handlers 7034).
    bool try_peer_restore_owner_hero_to_server()
    {
        if (!gbe::dota_lobby_state::should_peer_restore_owner_hero_from_client(
                true, server_local, client_local))
            return false;
        return gbe::dota_lobby_state::apply_owner_hero_id(server_local, client_local.owner_hero_id);
    }

    std::uint64_t server_gen() const
    {
        return server_generation.current().value;
    }

    void advance_server_generation()
    {
        const auto r = server_generation.advance(gbe::dota_lobby_generation::Boundary::Reset);
        expect_true(r.advanced, "server generation advance");
        server_local.generation = r.current.value;
    }
};

void test_harness_smoke()
{
    DualGcFixture fx;
    fx.seed_active_pair(0u, 0u);
    fx.publish_from(true);
    const auto snap = fx.store.snapshot();
    expect_true(snap.valid, "shared valid after publish");
    expect_eq_u64(snap.lobby_id, fx.lobby_id, "shared lobby_id");
    expect_true(fx.client_local.active && fx.server_local.active, "both locals active");
}

// H1: server hero 0, client known hero → peer restore fills server.
void test_h1_peer_restore_owner_hero()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, 0u);
    expect_eq_u32(fx.server_local.owner_hero_id, 0u, "H1 pre: server hero 0");
    expect_eq_u32(fx.client_local.owner_hero_id, fx.owner_hero_id, "H1 pre: client hero known");

    const bool restored = fx.try_peer_restore_owner_hero_to_server();
    expect_true(restored, "H1: peer restore applied");
    expect_eq_u32(fx.server_local.owner_hero_id, fx.owner_hero_id, "H1: server hero restored");

    fx.publish_from(true);
    expect_eq_u32(fx.store.snapshot().owner_hero_id, fx.owner_hero_id, "H1: shared hero after server publish");
}

// H5: restore must not wipe known local hero when shared hero is 0.
void test_h5_adopt_preserves_known_local_hero()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, 0u);

    // Server publishes with hero 0 → shared hero 0.
    fx.publish_from(true);
    expect_eq_u32(fx.store.snapshot().owner_hero_id, 0u, "H5 pre: shared hero 0");

    // Client still knows hero; adopt must preserve (preserve_known_owner_hero).
    expect_eq_u32(fx.client_local.owner_hero_id, fx.owner_hero_id, "H5 pre: client known hero");
    fx.restore_into(false);
    expect_eq_u32(fx.client_local.owner_hero_id, fx.owner_hero_id, "H5: adopt preserves client hero");
}

// H5b: publish with local hero 0 must not wipe shared known hero.
void test_h5_publish_preserves_known_shared_hero()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, fx.owner_hero_id);
    fx.publish_from(false);
    expect_eq_u32(fx.store.snapshot().owner_hero_id, fx.owner_hero_id, "H5b pre: shared has hero");

    fx.server_local.owner_hero_id = 0u;
    const auto shared_pre = fx.store.snapshot();
    expect_true(
        gbe::dota_lobby_state::should_preserve_known_owner_hero_on_publish(fx.server_local, shared_pre),
        "H5b: preserve rule true for local hero0 + known shared");
    fx.publish_from(true);
    expect_eq_u32(
        fx.store.snapshot().owner_hero_id,
        fx.owner_hero_id,
        "H5b: publish local hero0 preserves shared hero");
}

// H5c: apply_owner_hero_id is single write path (non-zero only).
void test_h5c_apply_owner_hero_api()
{
    DualGcFixture fx;
    fx.seed_active_pair(0u, 0u);
    expect_true(!gbe::dota_lobby_state::apply_owner_hero_id(fx.server_local, 0u), "H5c: zero rejected");
    expect_true(gbe::dota_lobby_state::apply_owner_hero_id(fx.server_local, 7u), "H5c: first apply");
    expect_eq_u32(fx.server_local.owner_hero_id, 7u, "H5c: hero set");
    expect_true(!gbe::dota_lobby_state::apply_owner_hero_id(fx.server_local, 7u), "H5c: same hero no-op");
    expect_true(gbe::dota_lobby_state::apply_owner_hero_id(fx.server_local, 9u), "H5c: change hero");
    expect_eq_u32(fx.server_local.owner_hero_id, 9u, "H5c: hero updated");
}

// H2: showcase one-shot same generation only once.
void test_h2_showcase_once_per_generation()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, fx.owner_hero_id);
    const auto gen = fx.server_gen();

    expect_true(
        !fx.server_showcase.has_pushed(gen, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id),
        "H2 pre: not pushed");
    fx.server_showcase.mark(gen, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id);
    expect_true(
        fx.server_showcase.has_pushed(gen, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id),
        "H2: first mark active");
    // Second push suppressed.
    const bool would_push_again =
        !fx.server_showcase.has_pushed(gen, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id);
    expect_true(!would_push_again, "H2: second push suppressed same generation");
}

// H3: wearable refresh same steam+hero once per generation.
void test_h3_wearable_once_per_generation()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, fx.owner_hero_id);
    const auto gen = fx.server_gen();

    expect_true(
        !fx.server_wearable.has_refreshed(gen, fx.owner_steam_id, fx.owner_hero_id),
        "H3 pre: not refreshed");
    fx.server_wearable.mark(gen, fx.owner_steam_id, fx.owner_hero_id);
    expect_true(
        fx.server_wearable.has_refreshed(gen, fx.owner_steam_id, fx.owner_hero_id),
        "H3: first mark active");
    const bool would_refresh_again =
        !fx.server_wearable.has_refreshed(gen, fx.owner_steam_id, fx.owner_hero_id);
    expect_true(!would_refresh_again, "H3: second refresh suppressed");
}

// H4: generation advance invalidates one-shot keys.
void test_h4_generation_invalidates_oneshot()
{
    DualGcFixture fx;
    fx.seed_active_pair(fx.owner_hero_id, fx.owner_hero_id);
    const auto gen0 = fx.server_gen();
    fx.server_showcase.mark(gen0, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id);
    fx.server_wearable.mark(gen0, fx.owner_steam_id, fx.owner_hero_id);

    fx.advance_server_generation();
    const auto gen1 = fx.server_gen();
    expect_true(gen1 != gen0, "H4: generation advanced");

    expect_true(
        !fx.server_showcase.has_pushed(gen1, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id),
        "H4: showcase stale after generation advance");
    expect_true(
        !fx.server_wearable.has_refreshed(gen1, fx.owner_steam_id, fx.owner_hero_id),
        "H4: wearable stale after generation advance");

    fx.server_showcase.mark(gen1, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id);
    expect_true(
        fx.server_showcase.has_pushed(gen1, fx.lobby_id, fx.owner_steam_id, fx.owner_hero_id),
        "H4: showcase can mark again on new generation");
}

// Deferred dual-track model (documents current production split; pure Slot truth for B).
struct DeferredSlot {
    std::uint64_t lobby_id{};
    std::uint64_t generation{};
    bool pending{};
};

enum class DeferredStatus : std::uint8_t { Current, Stale, Empty };

DeferredStatus consume_slot(DeferredSlot &slot, std::uint64_t current_generation)
{
    if (!slot.pending)
        return DeferredStatus::Empty;
    const auto status =
        (slot.generation == current_generation) ? DeferredStatus::Current : DeferredStatus::Stale;
    slot = {};
    return status;
}

void test_deferred_slot_model()
{
    DeferredSlot slot{};
    expect_true(consume_slot(slot, 1) == DeferredStatus::Empty, "deferred empty");

    slot = {100ull, 1ull, true};
    expect_true(consume_slot(slot, 1) == DeferredStatus::Current, "deferred current");
    expect_true(!slot.pending, "deferred cleared after consume");

    slot = {100ull, 1ull, true};
    expect_true(consume_slot(slot, 2) == DeferredStatus::Stale, "deferred stale after gen advance");
}

} // namespace

int main()
{
    test_harness_smoke();
    test_h1_peer_restore_owner_hero();
    test_h5_adopt_preserves_known_local_hero();
    test_h5_publish_preserves_known_shared_hero();
    test_h5c_apply_owner_hero_api();
    test_h2_showcase_once_per_generation();
    test_h3_wearable_once_per_generation();
    test_h4_generation_invalidates_oneshot();
    test_deferred_slot_model();

    if (g_failures != 0) {
        std::fprintf(stderr, "gbe_dota_dual_gc_host_test: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("gbe_dota_dual_gc_host_test: all passed\n");
    return 0;
}

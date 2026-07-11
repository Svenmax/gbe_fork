#include "dll/gbe_dota_locator.h"
#include "dll/gbe_dota_runtime_state.h"

#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>

std::string get_full_program_path()
{
    return {};
}

namespace {

int failures{};

void expect_true(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

template <typename Function>
bool throws_logic_error(Function function)
{
    try {
        function();
    } catch (const std::logic_error &) {
        return true;
    }
    return false;
}

struct Fixture
{
    GBE_SharedDotaLobbyState state;
    std::recursive_mutex mutex;
    gbe::dota_lobby_state::Store store{state, mutex};
    gbe::dota::RuntimeState runtime;
};

void test_guard_binds_and_unbinds_both_locators()
{
    Fixture fixture;
    {
        gbe::dota::LocatorBindingGuard guard(fixture.store, fixture.runtime);
        expect_true(&GBE_GetSharedDotaLobbyStateStore() == &fixture.store, "guard binds lobby Store");
        expect_true(&GBE_DotaRuntimeState() == &fixture.runtime, "guard binds runtime state");
    }

    expect_true(throws_logic_error([] { (void)GBE_GetSharedDotaLobbyStateStore(); }), "guard destruction unbinds lobby Store");
    expect_true(throws_logic_error([] { (void)GBE_DotaRuntimeState(); }), "guard destruction unbinds runtime state");
}

void test_partial_binding_failure_rolls_back_store()
{
    Fixture occupied;
    Fixture candidate;
    GBE_BindDotaRuntimeState(occupied.runtime);

    expect_true(
        throws_logic_error([&] { gbe::dota::LocatorBindingGuard guard(candidate.store, candidate.runtime); }),
        "runtime binding conflict fails guard construction");
    expect_true(throws_logic_error([] { (void)GBE_GetSharedDotaLobbyStateStore(); }), "failed guard construction rolls back lobby Store");
    expect_true(&GBE_DotaRuntimeState() == &occupied.runtime, "failed guard construction preserves existing runtime binding");

    GBE_UnbindDotaRuntimeState(occupied.runtime);
}

void test_owner_construction_failure_unbinds_guard()
{
    Fixture fixture;
    struct FailingOwner
    {
        gbe::dota::LocatorBindingGuard guard;

        FailingOwner(gbe::dota_lobby_state::Store &store, gbe::dota::RuntimeState &runtime)
            : guard(store, runtime)
        {
            throw std::runtime_error("construction failed");
        }
    };

    try {
        FailingOwner owner(fixture.store, fixture.runtime);
        (void)owner;
    } catch (const std::runtime_error &) {
    }

    expect_true(throws_logic_error([] { (void)GBE_GetSharedDotaLobbyStateStore(); }), "owner construction failure unbinds lobby Store");
    expect_true(throws_logic_error([] { (void)GBE_DotaRuntimeState(); }), "owner construction failure unbinds runtime state");
}

} // namespace

int main()
{
    test_guard_binds_and_unbinds_both_locators();
    test_partial_binding_failure_rolls_back_store();
    test_owner_construction_failure_unbinds_guard();

    if (failures != 0) {
        std::fprintf(stderr, "gbe_dota_locator_test failed: %d assertion(s)\n", failures);
        return 1;
    }

    std::printf("gbe_dota_locator_test passed\n");
    return 0;
}

#include "dll/callsystem.h"

#include <cstring>
#include <iostream>

std::recursive_mutex global_mutex;

SteamAPICall_t generate_steam_api_call_id()
{
    static SteamAPICall_t next_id = 1;
    return next_id++;
}

bool check_timedout(
    std::chrono::high_resolution_clock::time_point old,
    double timeout,
    std::chrono::high_resolution_clock::time_point now)
{
    return std::chrono::duration<double>(now - old).count() >= timeout;
}

namespace {

constexpr int kTestCallback = 77001;
int assertions{};
int failures{};

void expect(bool condition, const char *label)
{
    ++assertions;
    if (condition)
        return;
    ++failures;
    std::cerr << "failed: " << label << std::endl;
}

std::uint64_t current_generation{};

bool guard_allows(const void *context, unsigned int context_size)
{
    if (!context || context_size != sizeof(std::uint64_t))
        return false;
    std::uint64_t expected_generation{};
    std::memcpy(&expected_generation, context, sizeof(expected_generation));
    return expected_generation == current_generation;
}

class TestCallback final : public CCallbackBase {
public:
    int calls{};
    int value{};

    void Run(void *parameter) override
    {
        ++calls;
        std::memcpy(&value, parameter, sizeof(value));
    }

    void Run(void *parameter, bool, SteamAPICall_t) override
    {
        Run(parameter);
    }

    int GetCallbackSizeBytes() override
    {
        return sizeof(value);
    }
};

void run_results(SteamCallResults &results)
{
    global_mutex.lock();
    results.runCallResults();
    global_mutex.unlock();
}

void test_registered_callback_guard()
{
    SteamCallResults results;
    SteamCallBacks callbacks(&results);
    TestCallback callback;
    callbacks.addCallBack(kTestCallback, &callback);

    current_generation = 2;
    std::uint64_t generation = 1;
    int stale_value = 11;
    callbacks.addCBResult(
        kTestCallback,
        &stale_value,
        sizeof(stale_value),
        0.0,
        false,
        SteamCallExecutionGuard(guard_allows, &generation, sizeof(generation)));
    run_results(results);
    expect(callback.calls == 0, "registered callback rejects stale execution");

    generation = current_generation;
    int current_value = 12;
    callbacks.addCBResult(
        kTestCallback,
        &current_value,
        sizeof(current_value),
        0.0,
        false,
        SteamCallExecutionGuard(guard_allows, &generation, sizeof(generation)));
    run_results(results);
    expect(callback.calls == 1 && callback.value == current_value, "registered callback dispatches current execution");
}

void test_late_registration_replay_guard()
{
    SteamCallResults results;
    SteamCallBacks callbacks(&results);
    current_generation = 4;
    std::uint64_t generation = 3;
    int value = 21;
    callbacks.addCBResult(
        kTestCallback,
        &value,
        sizeof(value),
        0.0,
        false,
        SteamCallExecutionGuard(guard_allows, &generation, sizeof(generation)));

    TestCallback callback;
    callbacks.addCallBack(kTestCallback, &callback);
    run_results(results);
    expect(callback.calls == 0, "late-registration replay rejects stale execution");

    callbacks.runCallBacks();
    generation = current_generation;
    int current_value = 22;
    callbacks.addCBResult(
        kTestCallback,
        &current_value,
        sizeof(current_value),
        0.0,
        false,
        SteamCallExecutionGuard(guard_allows, &generation, sizeof(generation)));
    callbacks.rmCallBack(kTestCallback, &callback);

    TestCallback replay_callback;
    callbacks.addCallBack(kTestCallback, &replay_callback);
    run_results(results);
    expect(
        replay_callback.calls == 1 && replay_callback.value == current_value,
        "late-registration replay dispatches current execution");
}

} // namespace

int main()
{
    test_registered_callback_guard();
    test_late_registration_replay_guard();
    std::cout << "callsystem guard assertions: " << assertions - failures << "/" << assertions << std::endl;
    return failures == 0 ? 0 : 1;
}

#include "dll/callsystem.h"

#include <cstring>
#include <iostream>
#include <thread>

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
int cb_all_calls{};
int cb_all_value{};
bool cb_all_process_lock_available{};

void capture_all(std::vector<char> result, int)
{
    ++cb_all_calls;
    if (result.size() == sizeof(cb_all_value))
        std::memcpy(&cb_all_value, result.data(), sizeof(cb_all_value));
    std::thread lock_probe([] {
        cb_all_process_lock_available = global_mutex.try_lock();
        if (cb_all_process_lock_available)
            global_mutex.unlock();
    });
    lock_probe.join();
}

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
    bool process_lock_available{};

    void Run(void *parameter) override
    {
        ++calls;
        std::memcpy(&value, parameter, sizeof(value));
        std::thread lock_probe([this] {
            process_lock_available = global_mutex.try_lock();
            if (process_lock_available)
                global_mutex.unlock();
        });
        lock_probe.join();
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
    std::unique_lock<std::recursive_mutex> lock(global_mutex);
    results.runCallResults(lock);
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
    expect(callback.process_lock_available, "registered callback executes outside process lock");
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
    expect(replay_callback.process_lock_available, "replayed callback executes outside process lock");
}

void test_callback_all_executes_outside_process_lock()
{
    SteamCallResults results;
    SteamCallBacks callbacks(&results);
    TestCallback callback;
    callbacks.addCallBack(kTestCallback, &callback);
    results.setCbAll(capture_all);
    cb_all_calls = 0;
    cb_all_value = 0;
    cb_all_process_lock_available = false;

    int value = 31;
    callbacks.addCBResult(kTestCallback, &value, sizeof(value), 0.0);
    run_results(results);

    expect(cb_all_calls == 1 && cb_all_value == value, "callback-all receives queued result");
    expect(cb_all_process_lock_available, "callback-all executes outside process lock");
}

} // namespace

int main()
{
    test_registered_callback_guard();
    test_late_registration_replay_guard();
    test_callback_all_executes_outside_process_lock();
    std::cout << "callsystem guard assertions: " << assertions - failures << "/" << assertions << std::endl;
    return failures == 0 ? 0 : 1;
}

/* Smoke tests for the handler-level test harness.
 *
 * This file drives representative handlers from each extracted domain
 * and asserts on the recorded action sequence. The harness proves that:
 * 1. Real handler code can be compiled and executed in an offline test
 *    environment without the full Steam SDK.
 * 2. Side effects are captured in order by the ActionRecorder.
 * 3. The action sequence is deterministic and matches the documented
 *    side-effect order for each handler.
 *
 * Phase 3.1.7-3.1.10 logic refactors will extend these tests with
 * finer-grained assertions on parsing, mutation, and message construction
 * once the pure helpers are extracted.
 */

// Prevent the real Steam SDK headers from being included by pre-defining guards
#define __INCLUDED_STEAM_GAME_COORDINATOR_H__
#define __INCLUDED_DLL_H__
#define BASE_INCLUDE_H
#define __INCLUDED_CALLSYSTEM_H__
#define __INCLUDED_ECON_ITEM_H__
#define __INCLUDED_COMMON_INCLUDES__

// Prevent protobuf headers from being included (we provide stubs)
#define STEAMMESSAGES_PB_H
#define BASE_GCMESSAGES_PB_H
#define ECON_GCMESSAGES_PB_H
#define GCSDK_GCMESSAGES_PB_H
#define GCSYSTEMMSGS_PB_H
#define TF_GCMESSAGES_PB_H

// Provide stub types
#include "stubs.h"

// Include GBE headers for function declarations and constants
#include "dll/gbe_dota_protocol_constants.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// =====================================================================
// Test infrastructure
// =====================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg); \
            ++g_tests_failed; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            std::fprintf(stderr, "FAIL: %s:%d: %s (got %lld, expected %lld)\n", \
                         __FILE__, __LINE__, msg, \
                         static_cast<long long>(actual), \
                         static_cast<long long>(expected)); \
            ++g_tests_failed; \
            return; \
        } \
    } while (0)

#define RUN_TEST(test_func) \
    do { \
        ++g_tests_run; \
        test_func(); \
        if (g_tests_failed > 0) break; \
    } while (0)

// =====================================================================
// Protobuf varint encoding helpers
// =====================================================================

static void encode_varint(std::string &out, uint64_t value)
{
    while (value > 0x7F) {
        out.push_back(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<char>(value));
}

static void encode_varint_field(std::string &out, uint32_t field_number, uint64_t value)
{
    uint32_t tag = (field_number << 3) | 0; // wire type 0 = varint
    encode_varint(out, tag);
    encode_varint(out, value);
}

// =====================================================================
// Test fixture: create a coordinator with test items
// =====================================================================

struct TestFixture
{
    Steam_Game_Coordinator gc;
    Settings settings;
    Networking network;
    ActionRecorder recorder;

    TestFixture()
    {
        gc.settings = &settings;
        gc.network = &network;
        gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
        gc.is_server = false;
        g_action_recorder = &recorder;
    }

    ~TestFixture()
    {
        g_action_recorder = nullptr;
    }

    void reset()
    {
        recorder.clear();
        gc.items.clear();
        gc.GBE_local_lobby = GBE_LocalLobby{};
        gc.GBE_dota_private_lobby_snapshot_replayed = false;
        gc.test_set_active_server_lobby(false);
    }

    Econ_Item &add_item(uint64_t id, uint32_t def_index = 100)
    {
        Econ_Item item;
        item.id = id;
        item.def = def_index;
        item.level = 1;
        item.quantity = 1;
        item.style = 0;
        gc.items.push_back(std::move(item));
        return gc.items.back();
    }
};

// =====================================================================
// Inventory domain smoke tests
// =====================================================================

// Test: UnlockItemStyle with a valid item and a consumable.
// Expected action sequence:
//   1. PushIncomingNow(emsg=22)  — SO Update for the unlocked item
//   2. PushIncomingNow(emsg=24)  — SO Destroy for the consumed consumable
//   3. PushIncomingNow(emsg=2572) — Response
static void test_inventory_unlock_style_with_consumable()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.add_item(0xBBB2, 300); // consumable

    // Build request body: field 1 = unlock_item_id, field 2 = style_index, field 3 = consumable_item_id
    std::string body;
    encode_varint_field(body, 1, 0xAAA1);     // unlock_item_id
    encode_varint_field(body, 2, 1);           // unlock_style_index
    encode_varint_field(body, 3, 0xBBB2);     // consumable_item_id

    bool result = tf.gc.GBE_HandleDotaUnlockItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 100);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "should record 3 actions");

    // Verify action sequence
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, RecordedAction::PushIncomingNow, "first action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 22u, "first push should be SO Update (emsg=22)");

    TEST_ASSERT_EQ(tf.recorder.actions[1].type, RecordedAction::PushIncomingNow, "second action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 24u, "second push should be SO Destroy (emsg=24)");

    TEST_ASSERT_EQ(tf.recorder.actions[2].type, RecordedAction::PushIncomingNow, "third action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[2].msg_type & ~0x80000000u), 2572u, "third push should be response (emsg=2572)");

    // Verify the consumable was deleted from items
    bool consumable_found = false;
    for (const auto &item : tf.gc.items) {
        if (item.id == 0xBBB2) consumable_found = true;
    }
    TEST_ASSERT(!consumable_found, "consumable item should be deleted");

    ++g_tests_passed;
}

// Test: UnlockItemStyle with a valid item but no consumable.
// Expected action sequence:
//   1. PushIncomingNow(emsg=22)  — SO Update
//   2. PushIncomingNow(emsg=2572) — Response
static void test_inventory_unlock_style_no_consumable()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_varint_field(body, 1, 0xAAA1);     // unlock_item_id
    encode_varint_field(body, 2, 2);           // unlock_style_index
    // No field 3 (consumable_item_id = 0)

    bool result = tf.gc.GBE_HandleDotaUnlockItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "should record 2 actions");

    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 22u, "first push should be SO Update (emsg=22)");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 2572u, "second push should be response (emsg=2572)");

    ++g_tests_passed;
}

// Test: UnlockItemStyle with item not found.
// Expected action sequence:
//   1. PushIncomingNow(emsg=2572) — Response (no SO Update since item not found)
static void test_inventory_unlock_style_item_not_found()
{
    TestFixture tf;
    tf.reset();
    // No items in inventory

    std::string body;
    encode_varint_field(body, 1, 0x9999);     // unlock_item_id (not in inventory)
    encode_varint_field(body, 2, 1);           // unlock_style_index

    bool result = tf.gc.GBE_HandleDotaUnlockItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "should record 1 action (response only)");
    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 2572u, "should push response (emsg=2572)");

    ++g_tests_passed;
}

// Test: UnlockItemStyle with invalid style index (>= 32).
// Expected action sequence: empty (handler returns true early after GBE_ApplyDotaUnlockStyleBitmask fails)
static void test_inventory_unlock_style_invalid_index()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_varint_field(body, 1, 0xAAA1);     // unlock_item_id
    encode_varint_field(body, 2, 99);          // unlock_style_index (invalid, >= 32)

    bool result = tf.gc.GBE_HandleDotaUnlockItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 0u, "should record 0 actions (rejected invalid style)");

    ++g_tests_passed;
}

// Test: SetItemStyle with a valid item, is_server=true (no server GC forwarding).
// Expected action sequence:
//   1. CallbackItemUpdated
//   2. SaveItemsToFile
//   3. PushIncomingNow(emsg=2578) — Response
static void test_inventory_set_style_success()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xCCC3, 400);
    tf.gc.is_server = true; // Skip server GC forwarding path

    std::string body;
    encode_varint_field(body, 1, 0xCCC3);     // style_item_id
    encode_varint_field(body, 2, 1);           // style_index

    bool result = tf.gc.GBE_HandleDotaSetItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");

    // Verify the item style was updated
    bool style_updated = false;
    for (const auto &item : tf.gc.items) {
        if (item.id == 0xCCC3 && item.style == 1) style_updated = true;
    }
    TEST_ASSERT(style_updated, "item style should be updated to 1");

    // Verify action sequence: CallbackItemUpdated -> SaveItemsToFile -> PushIncomingNow(2578)
    bool found_callback = false;
    bool found_save = false;
    bool found_response = false;
    int callback_idx = -1, save_idx = -1, response_idx = -1;

    for (size_t i = 0; i < tf.recorder.actions.size(); ++i) {
        const auto &a = tf.recorder.actions[i];
        if (a.type == RecordedAction::CallbackItemUpdated) {
            found_callback = true;
            callback_idx = static_cast<int>(i);
        }
        if (a.type == RecordedAction::SaveItemsToFile) {
            found_save = true;
            save_idx = static_cast<int>(i);
        }
        if (a.type == RecordedAction::PushIncomingNow && (a.msg_type & ~0x80000000u) == 2578u) {
            found_response = true;
            response_idx = static_cast<int>(i);
        }
    }

    TEST_ASSERT(found_callback, "should record CallbackItemUpdated");
    TEST_ASSERT(found_save, "should record SaveItemsToFile");
    TEST_ASSERT(found_response, "should record PushIncomingNow(emsg=2578)");

    // Verify order: callback before save before response
    if (callback_idx >= 0 && save_idx >= 0 && response_idx >= 0) {
        TEST_ASSERT(callback_idx < save_idx, "CallbackItemUpdated should come before SaveItemsToFile");
        TEST_ASSERT(save_idx < response_idx, "SaveItemsToFile should come before response");
    }

    ++g_tests_passed;
}

// Test: SetItemStyle with item not found.
// Expected: no CallbackItemUpdated, no SaveItemsToFile, but response is still sent.
static void test_inventory_set_style_item_not_found()
{
    TestFixture tf;
    tf.reset();
    tf.gc.is_server = true;

    std::string body;
    encode_varint_field(body, 1, 0x9999);     // style_item_id (not in inventory)
    encode_varint_field(body, 2, 1);           // style_index

    bool result = tf.gc.GBE_HandleDotaSetItemStyleRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT_EQ(tf.recorder.count_type(RecordedAction::CallbackItemUpdated), 0u,
                   "should NOT record CallbackItemUpdated (item not found)");
    TEST_ASSERT_EQ(tf.recorder.count_type(RecordedAction::SaveItemsToFile), 0u,
                   "should NOT record SaveItemsToFile (item not found)");
    TEST_ASSERT_EQ(tf.recorder.count_type(RecordedAction::PushIncomingNow), 1u,
                   "should still push response");

    ++g_tests_passed;
}

// =====================================================================
// Chat domain smoke test
// =====================================================================
// NOTE: The chat handler smoke test requires compiling gbe_dota_chat_handlers.cpp
// against extended stubs. This will be added during Phase 3.1.8 (chat logic
// refactor) when the chat pure helpers are extracted and the stub surface
// is finalized. For now, the harness infrastructure (ActionRecorder, fixture
// loader, recording coordinator) is proven by the inventory smoke tests above.

// =====================================================================
// Lobby domain smoke test
// =====================================================================
// NOTE: The lobby handler smoke test requires compiling gbe_dota_lobby_handlers.cpp
// against extended stubs (17 lobby handlers + 6 statics, deeper lobby-state
// coupling). This will be added during Phase 3.1.9 (lobby logic refactor).

// =====================================================================
// Match domain smoke test
// =====================================================================
// NOTE: The match handler smoke test requires compiling gbe_dota_match_handlers.cpp
// against extended stubs (7034 launch flow, custom-game loading). This will
// be added during Phase 3.1.10 (match/misc logic refactor).

// =====================================================================
// Main
// =====================================================================

int main()
{
    std::printf("=== gbe_dota_handler_test ===\n");

    std::printf("[run] test_inventory_unlock_style_with_consumable\n");
    RUN_TEST(test_inventory_unlock_style_with_consumable);

    std::printf("[run] test_inventory_unlock_style_no_consumable\n");
    RUN_TEST(test_inventory_unlock_style_no_consumable);

    std::printf("[run] test_inventory_unlock_style_item_not_found\n");
    RUN_TEST(test_inventory_unlock_style_item_not_found);

    std::printf("[run] test_inventory_unlock_style_invalid_index\n");
    RUN_TEST(test_inventory_unlock_style_invalid_index);

    std::printf("[run] test_inventory_set_style_success\n");
    RUN_TEST(test_inventory_set_style_success);

    std::printf("[run] test_inventory_set_style_item_not_found\n");
    RUN_TEST(test_inventory_set_style_item_not_found);

    std::printf("\n=== Results: %d passed, %d failed, %d total ===\n",
                g_tests_passed, g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}

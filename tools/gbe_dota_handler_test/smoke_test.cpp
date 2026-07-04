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
#include <cstdint>
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

// Encode a length-delimited sub-message (wire type 2) for a given field number.
// Used to build ClientToGCEquipItemsRequest bodies (repeated field 1 sub-messages).
static void encode_length_delimited(std::string &out, uint32_t field_number, const std::string &sub_message)
{
    uint32_t tag = (field_number << 3) | 2; // wire type 2 = length-delimited
    encode_varint(out, tag);
    encode_varint(out, sub_message.size());
    out += sub_message;
}

// Encode a single equip op into a ClientToGCEquipItemsRequest body.
// Sub-message layout: field 1 = item_id, field 2 = new_class,
// field 3 = new_slot, field 4 = style_index (optional, omitted when 255).
static void encode_equip_op(std::string &out, uint64_t item_id, uint32_t new_class, uint32_t new_slot, uint32_t style_index = 255u)
{
    std::string sub;
    encode_varint_field(sub, 1, item_id);
    encode_varint_field(sub, 2, new_class);
    encode_varint_field(sub, 3, new_slot);
    if (style_index != 255u)
        encode_varint_field(sub, 4, style_index);
    encode_length_delimited(out, 1, sub);
}

static std::string make_leave_chat_body(uint64_t channel_id)
{
    std::string body;
    encode_varint_field(body, 1u, channel_id);
    return body;
}

static bool has_single_push(const ActionRecorder &recorder, uint32 expected_emsg)
{
    return recorder.actions.size() == 1u &&
        recorder.actions[0].type == GBE_DotaActionType::PushIncomingNow &&
        (recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask) == expected_emsg;
}

static bool read_header_jobs(const std::string &message, uint32_t &msg_type, JobID_t &target_job, JobID_t &source_job)
{
    if (message.size() < sizeof(uint32_t) + sizeof(JobID_t) + sizeof(JobID_t))
        return false;
    std::memcpy(&msg_type, message.data(), sizeof(msg_type));
    std::memcpy(&target_job, message.data() + sizeof(msg_type), sizeof(target_job));
    std::memcpy(&source_job, message.data() + sizeof(msg_type) + sizeof(target_job), sizeof(source_job));
    return true;
}

// =====================================================================
// Test fixture: create a coordinator with test items
// =====================================================================

struct TestFixture
{
    Steam_Game_Coordinator gc;
    Settings settings;
    Networking network;
    SteamCallBacks callbacks;
    ActionRecorder recorder;

    TestFixture()
    {
        gc.settings = &settings;
        gc.network = &network;
        gc.callbacks = &callbacks;
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
        // Clear the global server-GC hook so each test starts from a clean slate.
        g_test_steam_client.steam_game_coordinator = nullptr;
        g_test_steam_client.steam_gameserver_game_coordinator = nullptr;
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

static void test_csteamid_stub_behavior()
{
    CSteamID lobby_id(42u, k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat);
    CSteamID ordinary_chat_id(43u, 1u, k_EUniversePublic, k_EAccountTypeChat);
    CSteamID individual_id(44u, k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeIndividual);
    CSteamID console_user_id(45u, k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeConsoleUser);
    CSteamID game_server_id(46u, 1u, k_EUniversePublic, k_EAccountTypeGameServer);

    TEST_ASSERT(lobby_id.IsLobby(), "chat ID with lobby flag should be a lobby");
    TEST_ASSERT(!ordinary_chat_id.IsLobby(), "chat ID without lobby flag should not be a lobby");
    TEST_ASSERT(individual_id.BIndividualAccount(), "individual account should be individual");
    TEST_ASSERT(console_user_id.BIndividualAccount(), "console user account should be individual-compatible");
    TEST_ASSERT(!game_server_id.BIndividualAccount(), "game server account should not be individual-compatible");
    TEST_ASSERT_EQ(lobby_id.GetAccountID(), 42u, "account id should round-trip");

    ++g_tests_passed;
}

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
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "first action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 22u, "first push should be SO Update (emsg=22)");

    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::PushIncomingNow, "second action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 24u, "second push should be SO Destroy (emsg=24)");

    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::PushIncomingNow, "third action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[2].msg_type & ~0x80000000u), 2572u, "third push should be response (emsg=2572)");
    TEST_ASSERT_EQ(tf.recorder.actions[2].source_job, 0u, "response should not be synthetic header action");

    uint32_t header_msg_type = 0;
    JobID_t target_job = 0;
    JobID_t source_job = 0;
    TEST_ASSERT(read_header_jobs(tf.recorder.actions[0].msg_body, header_msg_type, target_job, source_job), "SO update should carry test header");
    TEST_ASSERT_EQ(header_msg_type & ~Steam_Game_Coordinator::protobuf_mask, 22u, "SO update header emsg should be 22");
    TEST_ASSERT_EQ(target_job, k_GIDNil, "SO update currently uses nil target job");
    TEST_ASSERT_EQ(source_job, k_GIDNil, "SO update should use nil source job");

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
        if (a.type == GBE_DotaActionType::CallbackItemUpdated) {
            found_callback = true;
            callback_idx = static_cast<int>(i);
        }
        if (a.type == GBE_DotaActionType::SaveItemsToFile) {
            found_save = true;
            save_idx = static_cast<int>(i);
        }
        if (a.type == GBE_DotaActionType::PushIncomingNow && (a.msg_type & ~0x80000000u) == 2578u) {
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
    TEST_ASSERT_EQ(tf.recorder.count_type(GBE_DotaActionType::CallbackItemUpdated), 0u,
                   "should NOT record CallbackItemUpdated (item not found)");
    TEST_ASSERT_EQ(tf.recorder.count_type(GBE_DotaActionType::SaveItemsToFile), 0u,
                   "should NOT record SaveItemsToFile (item not found)");
    TEST_ASSERT_EQ(tf.recorder.count_type(GBE_DotaActionType::PushIncomingNow), 1u,
                   "should still push response");

    ++g_tests_passed;
}

// Test: EquipItems with one valid op, is_server=true (skips server-GC
// forward, network broadcast, and lobby snapshot refresh paths).
// Expected action sequence:
//   1. PushIncomingNow(emsg=26)   — SO UpdateMultiple for modified item
//   2. PushIncomingNow(emsg=2570) — Response with cache version
//   3. SaveItemsToFile
static void test_inventory_equip_basic()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.gc.is_server = true; // Skip server-GC-forward + network + snapshot paths

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);

    bool result = tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");

    // Verify the item's equip_states was updated
    bool equip_updated = false;
    for (const auto &item : tf.gc.items) {
        if (item.id == 0xAAA1) {
            auto it = item.equip_states.find(2u);
            if (it != item.equip_states.end() && it->second == 3u)
                equip_updated = true;
        }
    }
    TEST_ASSERT(equip_updated, "item equip_states should have (class=2, slot=3)");

    // Verify action sequence: PushIncomingNow(26) -> PushIncomingNow(2570) -> SaveItemsToFile
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "should record 3 actions");

    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "first action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 26u, "first push should be SO UpdateMultiple (emsg=26)");

    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::PushIncomingNow, "second action should be PushIncomingNow");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 2570u, "second push should be response (emsg=2570)");
    TEST_ASSERT(tf.recorder.actions[1].reason == "", "direct equip response should be sent without GBE_PushDotaResponse reason metadata");
    TEST_ASSERT(!tf.recorder.actions[1].wrapped, "equip response should be unwrapped in direct handler test");

    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::SaveItemsToFile, "third action should be SaveItemsToFile");

    ++g_tests_passed;
}

// Test: EquipItems with empty body (parse fails, handler returns true early).
// Expected: no side effects recorded.
static void test_inventory_equip_empty()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.gc.is_server = true;

    bool result = tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(""), 0, false, 0);

    TEST_ASSERT(result, "handler should return true even on parse failure");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 0u, "should record 0 actions (parse failure, early return)");

    ++g_tests_passed;
}

// Test: EquipItems with full server-GC-forward + network-broadcast +
// lobby-snapshot-refresh path. Verifies the documented ordering invariant:
// local response(2570) precedes server-GC forward, network broadcast, and
// lobby snapshot refresh. Full item cache (CacheSubscribed) precedes emsg
// 21/26 when forwarding to server GC.
//
// Expected action sequence:
//   1. PushIncomingNow(emsg=26)     — local SO UpdateMultiple
//   2. PushIncomingNow(emsg=2570)   — local response
//   3. SaveItemsToFile
//   4. ServerGcForward(emsg=0)      — full cache push (CacheSubscribed)
//   5. ServerGcForward(emsg=21)     — SO Create for modified item
//   6. ServerGcForward(emsg=26)     — SO UpdateMultiple forward
//   7. NetworkBroadcast             — broadcast equipped items to gameservers
//   8. LobbySnapshotRefresh         — refresh stale lobby snapshot
static void test_inventory_equip_full_forward()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.gc.is_server = false;
    tf.gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;

    // Wire a server GC that owns an active lobby
    Steam_Game_Coordinator server_gc;
    server_gc.test_set_active_server_lobby(true);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    // Configure local lobby so the lobby-snapshot-refresh path fires
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 1;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 2u;
    tf.gc.GBE_dota_private_lobby_snapshot_replayed = true;

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);

    bool result = tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");

    // Verify the full documented action sequence
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 8u, "should record 8 actions for full forward path");

    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "1: PushIncomingNow (SO UpdateMultiple)");
    TEST_ASSERT_EQ((tf.recorder.actions[0].msg_type & ~0x80000000u), 26u, "1: emsg=26");

    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::PushIncomingNow, "2: PushIncomingNow (response)");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 2570u, "2: emsg=2570");

    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::SaveItemsToFile, "3: SaveItemsToFile");

    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::ServerGcForward, "4: ServerGcForward (cache push)");
    TEST_ASSERT_EQ((tf.recorder.actions[3].msg_type & ~0x80000000u), 0u, "4: emsg=0 (CacheSubscribed)");
    TEST_ASSERT_EQ(tf.recorder.actions[3].item_id, 1u, "4: cache push should target a server GC");
    TEST_ASSERT_EQ(tf.recorder.actions[3].server_gc_source_item_count, 1u, "4: cache push should include source items");
    TEST_ASSERT(tf.recorder.actions[3].server_gc_unsubscribe_first, "4: cache push should unsubscribe before subscribe");
    TEST_ASSERT(tf.recorder.actions[3].reason == "equip_forward_host_resubscribe_server", "4: cache push reason should identify equip forward");

    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::ServerGcForward, "5: ServerGcForward (SO Create)");
    TEST_ASSERT_EQ((tf.recorder.actions[4].msg_type & ~0x80000000u), 21u, "5: emsg=21");

    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::ServerGcForward, "6: ServerGcForward (SO UpdateMultiple forward)");
    TEST_ASSERT_EQ((tf.recorder.actions[5].msg_type & ~0x80000000u), 26u, "6: emsg=26");

    TEST_ASSERT_EQ(tf.recorder.actions[6].type, GBE_DotaActionType::NetworkBroadcast, "7: NetworkBroadcast");

    TEST_ASSERT_EQ(tf.recorder.actions[7].type, GBE_DotaActionType::LobbySnapshotRefresh, "8: LobbySnapshotRefresh");

    ++g_tests_passed;
}

// =====================================================================
// Chat domain smoke test
// =====================================================================

static void test_chat_join_channel()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0xCAFEu;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";

    std::string body;
    encode_length_delimited(body, 2u, "dota_lobby_chat");
    encode_varint_field(body, 4u, 3u);

    const std::string session_raw = "outer-session-token";
    bool result = tf.gc.GBE_HandleDotaJoinChatChannelRequest(body, true, &session_raw);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.has_chat_channel, "join chat should mark chat channel active");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.chat_channel_type, 3u, "join chat should keep channel type");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "join chat should publish and push response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "publish should happen before response");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::PushIncomingNow, "response should be pushed");
    TEST_ASSERT_EQ(tf.recorder.actions[1].msg_type & ~Steam_Game_Coordinator::protobuf_mask, GBE_kDotaJoinChatChannelResponse, "response should be 7010");
    TEST_ASSERT(tf.recorder.actions[1].wrapped, "7010 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[1].session_raw == session_raw, "7010 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7009_7010", "7010 response should record reason");
    TEST_ASSERT(!tf.recorder.actions[1].msg_body.empty(), "7010 response should carry payload");

    ++g_tests_passed;
}

static void test_chat_leave_postgame_channel_order()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0xCAFEu;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0xBEEFu;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.has_chat_channel = true;
    tf.gc.GBE_local_lobby.chat_channel_id = 0x7014u;
    tf.gc.GBE_local_lobby.chat_channel_name = "postgame";
    tf.gc.GBE_local_lobby.chat_channel_type = 18u;
    tf.gc.GBE_local_lobby.abandon_postgame_active = true;
    GBE_shared_dota_lobby_state.valid = true;

    const std::string session_raw = "leave-session-token";
    const std::string body = make_leave_chat_body(tf.gc.GBE_local_lobby.chat_channel_id);
    bool result = tf.gc.GBE_HandleDotaLeaveChatChannelRequest(body, true, &session_raw);

    TEST_ASSERT(result, "leave chat handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "postgame leave should push 7014 then publish state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "first action should be 7014 response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask, GBE_kDotaOtherLeftChannel, "first response should be 7014");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "7014 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "7014 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7272_7014", "7014 response should record reason");
    TEST_ASSERT(!tf.recorder.actions[0].msg_body.empty(), "7014 response should carry payload");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "second action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7272_leave_chat", "publish reason should identify leave chat");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.has_chat_channel, "postgame leave should clear local chat channel after 7014");
    GBE_shared_dota_lobby_state.valid = false;

    ++g_tests_passed;
}

static void test_lobby_abandon_current_game_disconnect_queues_25()
{
    TestFixture tf;
    tf.reset();
    tf.gc.is_server = true;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7035u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 1u;
    tf.gc.GBE_local_lobby.server_id = 0x55u;
    tf.gc.GBE_local_lobby.owner_connected = true;
    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;

    bool result = tf.gc.GBE_HandleDotaAbandonCurrentGameRequest(false, nullptr);

    TEST_ASSERT(result, "abandon handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "current-game disconnect should queue only 25");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "queued action should be push");
    TEST_ASSERT_EQ(tf.recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask, GBE_kDotaCacheUnsubscribed, "queued response should be 25");
    TEST_ASSERT(!tf.recorder.actions[0].msg_body.empty(), "25 response should carry payload");
    TEST_ASSERT(GBE_pending_reset_after_cache_unsubscribed, "25 should mark reset pending");
    TEST_ASSERT_EQ(GBE_pending_reset_after_cache_unsubscribed_lobby_id, 0x7035u, "pending reset should record lobby id");

    ++g_tests_passed;
}

static void test_lobby_abandon_ready_teardown_queues_postgame_response()
{
    TestFixture tf;
    tf.reset();
    tf.gc.is_server = false;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7036u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 1u;
    tf.gc.GBE_local_lobby.chat_channel_id = 0x7014u;

    const std::string session_raw = "abandon-session";
    bool result = tf.gc.GBE_HandleDotaAbandonCurrentGameRequest(true, &session_raw);

    TEST_ASSERT(result, "ready abandon handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "ready teardown should queue postgame response via stub");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "teardown action should be push");
    TEST_ASSERT_EQ(tf.recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask, GBE_kDotaOtherLeftChannel, "teardown response should be 7014 in stub");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "teardown response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "teardown response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "postgame_teardown_7014", "teardown response should record reason");

    ++g_tests_passed;
}

// =====================================================================
// Misc domain smoke tests
// =====================================================================

static void test_misc_minimal_varint_success()
{
    TestFixture tf;
    tf.reset();

    bool result = tf.gc.GBE_HandleDotaMinimalVarintSuccessRequest(1234u, 5678u, "test", "test", true, 9001u);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(has_single_push(tf.recorder, 5678u), "minimal varint success should push one response");
    TEST_ASSERT(tf.recorder.actions[0].reason == "test", "minimal response should record push note");
    TEST_ASSERT(!tf.recorder.actions[0].wrapped, "minimal response should be unwrapped");
    TEST_ASSERT(!tf.recorder.actions[0].msg_body.empty(), "minimal response should include payload");

    ++g_tests_passed;
}

static void test_misc_7427_notifications()
{
    TestFixture tf;
    tf.reset();

    bool result = tf.gc.GBE_HandleDota7427NotificationsRequest(true, 9002u);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(has_single_push(tf.recorder, 7428u), "7427 notification request should push 7428 response");

    ++g_tests_passed;
}

static void test_misc_upload_rate()
{
    TestFixture tf;
    tf.reset();

    bool result = tf.gc.GBE_HandleDotaUploadRateRequest(true, 9003u);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(has_single_push(tf.recorder, 4524u), "upload-rate request should push 4524 response");

    ++g_tests_passed;
}

static void test_misc_rank()
{
    TestFixture tf;
    tf.reset();

    std::string body;
    encode_varint_field(body, 1, 1u);
    encode_varint_field(body, 2, 5u);

    bool result = tf.gc.GBE_HandleDotaRankRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 9004u);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(has_single_push(tf.recorder, 8880u), "rank request should push 8880 response");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8879_8880", "rank response should record reason");
    TEST_ASSERT(!tf.recorder.actions[0].msg_body.empty(), "rank response should include payload");

    ++g_tests_passed;
}

// =====================================================================
// Match domain smoke tests
// =====================================================================

static void setup_custom_game_lobby(TestFixture &tf)
{
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x805300u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 0u;
    tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
    tf.gc.GBE_local_lobby.custom_game.game_id = 0xBEEF00u;
}

static void test_match_ready_up_queues_7170_then_runtime_update()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    std::string body;
    encode_varint_field(body, 1u, 1u);
    bool result = tf.gc.GBE_HandleDotaCustomGameReadyUpRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 0x7070u);

    TEST_ASSERT(result, "ready-up handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "ready-up should queue response then publish runtime state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "first action should be 7170 response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask, 7170u, "first response should be 7170");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "second action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7070_custom_game_ready_up_run_ack", "publish reason should identify ready-up ack");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 1u, "ready-up should advance to wait-for-players");

    ++g_tests_passed;
}

static void test_match_started_loading_updates_custom_game_before_publish()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    std::string body;
    encode_varint_field(body, 1u, tf.gc.GBE_local_lobby.lobby_id);
    encode_varint_field(body, 2u, 0x8052u);
    encode_varint_field(body, 4u, 12345u);
    bool result = tf.gc.GBE_HandleDotaCustomGameStartedLoadingRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 0x8052u);

    TEST_ASSERT(result, "started-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "8052 should publish one lobby state refresh when run advance stub declines");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "8052 action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8052_started_loading", "8052 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.custom_game.game_id, 0x8052u, "8052 should update custom game id before publish");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_start_time, 12345u, "8052 should update start time before publish");

    ++g_tests_passed;
}

static void test_match_finished_loading_marks_loaded_before_publish()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    std::string body;
    encode_varint_field(body, 1u, tf.gc.GBE_local_lobby.lobby_id);
    encode_varint_field(body, 2u, 33u);
    encode_varint_field(body, 3u, 0u);
    encode_varint_field(body, 4u, 4u);
    bool result = tf.gc.GBE_HandleDotaCustomGameFinishedLoadingRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 0x8053u);

    TEST_ASSERT(result, "finished-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "8053 success should publish one lobby state refresh");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "8053 action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_finished_loading", "8053 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseLoaded, "8053 should mark launch loaded before publish");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 1u, "8053 should ensure at least wait-for-players state");

    ++g_tests_passed;
}

// =====================================================================
// Main
// =====================================================================

int main()
{
    std::printf("=== gbe_dota_handler_test ===\n");

    std::printf("[run] test_csteamid_stub_behavior\n");
    RUN_TEST(test_csteamid_stub_behavior);

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

    std::printf("[run] test_inventory_equip_basic\n");
    RUN_TEST(test_inventory_equip_basic);

    std::printf("[run] test_inventory_equip_empty\n");
    RUN_TEST(test_inventory_equip_empty);

    std::printf("[run] test_inventory_equip_full_forward\n");
    RUN_TEST(test_inventory_equip_full_forward);

    std::printf("[run] test_chat_join_channel\n");
    RUN_TEST(test_chat_join_channel);

    std::printf("[run] test_chat_leave_postgame_channel_order\n");
    RUN_TEST(test_chat_leave_postgame_channel_order);

    std::printf("[run] test_lobby_abandon_current_game_disconnect_queues_25\n");
    RUN_TEST(test_lobby_abandon_current_game_disconnect_queues_25);

    std::printf("[run] test_lobby_abandon_ready_teardown_queues_postgame_response\n");
    RUN_TEST(test_lobby_abandon_ready_teardown_queues_postgame_response);

    std::printf("[run] test_misc_minimal_varint_success\n");
    RUN_TEST(test_misc_minimal_varint_success);

    std::printf("[run] test_misc_7427_notifications\n");
    RUN_TEST(test_misc_7427_notifications);

    std::printf("[run] test_misc_upload_rate\n");
    RUN_TEST(test_misc_upload_rate);

    std::printf("[run] test_misc_rank\n");
    RUN_TEST(test_misc_rank);

    std::printf("[run] test_match_ready_up_queues_7170_then_runtime_update\n");
    RUN_TEST(test_match_ready_up_queues_7170_then_runtime_update);

    std::printf("[run] test_match_started_loading_updates_custom_game_before_publish\n");
    RUN_TEST(test_match_started_loading_updates_custom_game_before_publish);

    std::printf("[run] test_match_finished_loading_marks_loaded_before_publish\n");
    RUN_TEST(test_match_finished_loading_marks_loaded_before_publish);

    std::printf("\n=== Results: %d passed, %d failed, %d total ===\n",
                g_tests_passed, g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}

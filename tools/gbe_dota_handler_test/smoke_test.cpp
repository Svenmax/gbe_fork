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

struct TestEquipPlannerSummary {
    bool parse_failed{};
    size_t equip_op_count{};
    size_t modified_item_count{};
    size_t action_count{};
    bool has_server_forward{};
    bool broadcast_equipped_items{};
    const char *snapshot_refresh_reason{};
    uint16 first_item_slot{};
    uint8 first_item_style{};
    GBE_DotaActionType actions[8]{};
    uint32 action_emsgs[8]{};
};

TestEquipPlannerSummary test_plan_equip_items_request(
    const uint8 *body,
    size_t body_size,
    const std::vector<Econ_Item> &items,
    uint64_t cache_version,
    bool is_dota_client,
    bool server_gc_has_active_lobby,
    bool lobby_snapshot_refresh_available);

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

class WireBodyBuilder
{
public:
    WireBodyBuilder &varint(uint32_t field_number, uint64_t value)
    {
        encode_varint_field(m_body, field_number, value);
        return *this;
    }

    WireBodyBuilder &bytes(uint32_t field_number, const std::string &value)
    {
        encode_length_delimited(m_body, field_number, value);
        return *this;
    }

    WireBodyBuilder &message(uint32_t field_number, const WireBodyBuilder &nested)
    {
        encode_length_delimited(m_body, field_number, nested.str());
        return *this;
    }

    const std::string &str() const { return m_body; }
    std::string take() const { return m_body; }

private:
    std::string m_body;
};

// Encode a single equip op into a ClientToGCEquipItemsRequest body.
// Sub-message layout: field 1 = item_id, field 2 = new_class,
// field 3 = new_slot, field 4 = style_index (optional, omitted when 255).
static void encode_equip_op(std::string &out, uint64_t item_id, uint32_t new_class, uint32_t new_slot, uint32_t style_index = 255u)
{
    WireBodyBuilder sub;
    sub.varint(1u, item_id)
        .varint(2u, new_class)
        .varint(3u, new_slot);
    if (style_index != 255u)
        sub.varint(4u, style_index);
    encode_length_delimited(out, 1u, sub.str());
}

static std::string make_dota7034_connected_player_body(uint64_t steam_id, uint32_t hero_id)
{
    WireBodyBuilder player;
    player.varint(1u, steam_id)
        .varint(2u, hero_id);

    WireBodyBuilder body;
    body.message(1u, player);
    return body.take();
}

static std::string make_dota7034_disconnected_player_body(uint64_t steam_id, uint32_t lobby_state, uint32_t game_state)
{
    WireBodyBuilder leaver_state;
    leaver_state.varint(1u, lobby_state)
        .varint(2u, game_state);

    WireBodyBuilder player;
    player.varint(1u, steam_id)
        .message(3u, leaver_state);

    WireBodyBuilder body;
    body.message(7u, player);
    return body.take();
}

static std::string make_dota7034_game_state_body(uint32_t game_state, uint32_t send_reason)
{
    WireBodyBuilder body;
    body.varint(2u, game_state)
        .varint(8u, send_reason);
    return body.take();
}

static std::string make_lobby_set_details_body(uint64_t lobby_id, const std::string &room_name, uint32_t server_region, uint32_t game_mode)
{
    WireBodyBuilder body;
    body.varint(1u, lobby_id)
        .bytes(2u, room_name)
        .varint(4u, server_region)
        .varint(5u, game_mode);
    return body.take();
}

static std::string make_leave_chat_body(uint64_t channel_id)
{
    WireBodyBuilder body;
    body.varint(1u, channel_id);
    return body.take();
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

static uint32_t action_emsg(const RecordedAction &action)
{
    return action.msg_type & ~Steam_Game_Coordinator::protobuf_mask;
}

static void expect_push_action(const RecordedAction &action, uint32_t expected_emsg, const char *context)
{
    TEST_ASSERT_EQ(action.type, GBE_DotaActionType::PushIncomingNow, context);
    TEST_ASSERT_EQ(action_emsg(action), expected_emsg, context);
}

static void expect_push_payload(const RecordedAction &action, uint32_t expected_emsg, const char *context)
{
    expect_push_action(action, expected_emsg, context);
    TEST_ASSERT(!action.msg_body.empty(), context);
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
        gc.GBE_ClearDotaLoginSyncSent();
        gc.GBE_ClearDotaPrivateLobbySnapshotReplayed();
        gc.GBE_ClearDotaHostShowcaseEquipPushed();
        gc.GBE_ClearLastDotaLaunchStatePushedGameState();
        gc.GBE_ClearLastDotaLaunchPersonaSignature();
        gc.GBE_ClearLastDotaDirectConnectCallbackSignature();
        gc.test_set_active_server_lobby(false);
        gc.test_clear_next_lobby_capture();
        // Clear the global server-GC hook so each test starts from a clean slate.
        g_test_steam_client.steam_matchmaking = nullptr;
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
    expect_push_action(tf.recorder.actions[0], 22u, "first push should be SO Update (emsg=22)");
    expect_push_action(tf.recorder.actions[1], 24u, "second push should be SO Destroy (emsg=24)");
    expect_push_action(tf.recorder.actions[2], 2572u, "third push should be response (emsg=2572)");
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

    expect_push_action(tf.recorder.actions[0], 22u, "first push should be SO Update (emsg=22)");
    expect_push_action(tf.recorder.actions[1], 2572u, "second push should be response (emsg=2572)");

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
    expect_push_action(tf.recorder.actions[0], 2572u, "should push response (emsg=2572)");

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
        if (a.type == GBE_DotaActionType::PushIncomingNow && action_emsg(a) == 2578u) {
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

    expect_push_action(tf.recorder.actions[0], 26u, "first push should be SO UpdateMultiple (emsg=26)");
    expect_push_action(tf.recorder.actions[1], 2570u, "second push should be response (emsg=2570)");
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
    tf.gc.GBE_MarkDotaPrivateLobbySnapshotReplayed();

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
    TEST_ASSERT_EQ(tf.recorder.actions[3].steam_id, 12345u, "4: cache push should use local steam id");
    TEST_ASSERT_EQ(tf.recorder.actions[3].server_gc_source_item_count, 1u, "4: cache push should include source items");
    TEST_ASSERT(tf.recorder.actions[3].server_gc_unsubscribe_first, "4: cache push should unsubscribe before subscribe");
    TEST_ASSERT(tf.recorder.actions[3].reason == "equip_forward_host_resubscribe_server", "4: cache push reason should identify equip forward");

    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::ServerGcForward, "5: ServerGcForward (SO Create)");
    TEST_ASSERT_EQ((tf.recorder.actions[4].msg_type & ~0x80000000u), 21u, "5: emsg=21");

    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::ServerGcForward, "6: ServerGcForward (SO UpdateMultiple forward)");
    TEST_ASSERT_EQ((tf.recorder.actions[5].msg_type & ~0x80000000u), 26u, "6: emsg=26");

    TEST_ASSERT_EQ(tf.recorder.actions[6].type, GBE_DotaActionType::NetworkBroadcast, "7: NetworkBroadcast");
    TEST_ASSERT_EQ(tf.recorder.actions[6].source_id, 12345u, "7: network broadcast should use local steam id as source");

    TEST_ASSERT_EQ(tf.recorder.actions[7].type, GBE_DotaActionType::LobbySnapshotRefresh, "8: LobbySnapshotRefresh");
    TEST_ASSERT(tf.recorder.actions[7].reason == "equip_items_refresh", "8: snapshot refresh reason should identify equip replay");

    ++g_tests_passed;
}

static void test_inventory_equip_planner_empty_body()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(""), 0, tf.gc.items, 100u, true, true, true);

    TEST_ASSERT(summary.parse_failed, "planner should report parse failure for empty body");
    TEST_ASSERT_EQ(summary.action_count, 0u, "parse failure should produce no actions");
    TEST_ASSERT_EQ(summary.modified_item_count, 0u, "parse failure should not modify items");

    ++g_tests_passed;
}

static void test_inventory_equip_planner_single_item_order()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), tf.gc.items, 100u, false, false, false);

    TEST_ASSERT(!summary.parse_failed, "planner should parse one equip op");
    TEST_ASSERT_EQ(summary.equip_op_count, 1u, "planner should keep one op");
    TEST_ASSERT_EQ(summary.modified_item_count, 1u, "planner should mark one item modified");
    TEST_ASSERT_EQ(summary.first_item_slot, 3u, "planner should apply equip slot");
    TEST_ASSERT_EQ(summary.action_count, 3u, "planner should produce local update, response, save");
    TEST_ASSERT_EQ(summary.actions[0], GBE_DotaActionType::PushIncomingNow, "first action should push local SO update");
    TEST_ASSERT_EQ((summary.action_emsgs[0] & ~Steam_Game_Coordinator::protobuf_mask), 26u, "first emsg should be 26");
    TEST_ASSERT_EQ((summary.action_emsgs[1] & ~Steam_Game_Coordinator::protobuf_mask), 2570u, "second emsg should be 2570");
    TEST_ASSERT_EQ(summary.actions[2], GBE_DotaActionType::SaveItemsToFile, "third action should save items");

    ++g_tests_passed;
}

static void test_inventory_equip_planner_multi_item_order()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.add_item(0xAAA2, 201);

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);
    encode_equip_op(body, 0xAAA2, 4u, 5u);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), tf.gc.items, 100u, true, true, true);

    TEST_ASSERT_EQ(summary.equip_op_count, 2u, "planner should keep two ops");
    TEST_ASSERT_EQ(summary.modified_item_count, 2u, "planner should mark two items modified");
    TEST_ASSERT(summary.has_server_forward, "planner should request server forward");
    TEST_ASSERT(summary.broadcast_equipped_items, "planner should request network broadcast");
    TEST_ASSERT(summary.snapshot_refresh_reason != nullptr, "planner should request snapshot refresh");
    TEST_ASSERT_EQ(summary.actions[0], GBE_DotaActionType::PushIncomingNow, "first action should be local update");
    TEST_ASSERT_EQ(summary.actions[1], GBE_DotaActionType::PushIncomingNow, "second action should be local response");
    TEST_ASSERT_EQ(summary.actions[2], GBE_DotaActionType::SaveItemsToFile, "third action should save before external effects");
    TEST_ASSERT_EQ(summary.actions[3], GBE_DotaActionType::ServerGcForward, "server full cache should follow local save");
    TEST_ASSERT_EQ((summary.action_emsgs[3] & ~Steam_Game_Coordinator::protobuf_mask), 0u, "server full cache action should precede 21/26");

    ++g_tests_passed;
}

static void test_inventory_equip_planner_missing_item()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_equip_op(body, 0xDEAD, 2u, 3u);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), tf.gc.items, 100u, false, false, false);

    TEST_ASSERT(!summary.parse_failed, "missing item request should still parse");
    TEST_ASSERT_EQ(summary.modified_item_count, 0u, "missing item should not modify items without a matching equipped slot");
    TEST_ASSERT_EQ(summary.action_count, 2u, "missing item should still response and save");
    TEST_ASSERT_EQ((summary.action_emsgs[0] & ~Steam_Game_Coordinator::protobuf_mask), 2570u, "first action should be response when no SO update exists");
    TEST_ASSERT_EQ(summary.actions[1], GBE_DotaActionType::SaveItemsToFile, "second action should save");

    ++g_tests_passed;
}

static void test_inventory_equip_planner_style_bitmask_input()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u, 7u);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), tf.gc.items, 100u, false, false, false);

    TEST_ASSERT_EQ(summary.modified_item_count, 1u, "style equip op should modify one item");
    TEST_ASSERT_EQ(summary.first_item_slot, 3u, "style equip op should still apply slot");
    TEST_ASSERT_EQ(summary.first_item_style, 7u, "style equip op should apply style index");
    TEST_ASSERT_EQ((summary.action_emsgs[0] & ~Steam_Game_Coordinator::protobuf_mask), 26u, "style equip op should produce local SO update before response");

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

    const std::string body = WireBodyBuilder()
        .bytes(2u, "dota_lobby_chat")
        .varint(4u, 3u)
        .take();

    const std::string session_raw = "outer-session-token";
    bool result = tf.gc.GBE_HandleDotaJoinChatChannelRequest(body, true, &session_raw);

    TEST_ASSERT(result, "handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.has_chat_channel, "join chat should mark chat channel active");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.chat_channel_type, 3u, "join chat should keep channel type");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "join chat should publish and push response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "publish should happen before response");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaJoinChatChannelResponse, "7010 response should be pushed with payload");
    TEST_ASSERT(tf.recorder.actions[1].wrapped, "7010 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[1].session_raw == session_raw, "7010 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7009_7010", "7010 response should record reason");

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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "postgame leave should push 7014, update rich presence, then publish state");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaOtherLeftChannel, "first action should be 7014 response with payload");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "7014 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "7014 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7272_7014", "7014 response should record reason");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::RichPresenceUpdate, "second action should update rich presence");
    TEST_ASSERT(tf.recorder.actions[1].status == "#DOTA_RP_INIT", "postgame leave rich presence status should reset to init");
    TEST_ASSERT(tf.recorder.actions[1].lobby_state == "SERVERSETUP", "postgame leave rich presence lobby state should reset to serversetup");
    TEST_ASSERT(!tf.recorder.actions[1].include_party, "postgame leave rich presence should clear party state");
    TEST_ASSERT(!tf.recorder.actions[1].include_lobby, "postgame leave rich presence should clear lobby state");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbySnapshotRefresh, "third action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7272_leave_chat", "publish reason should identify leave chat");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.has_chat_channel, "postgame leave should clear local chat channel after 7014");
    GBE_shared_dota_lobby_state.valid = false;

    ++g_tests_passed;
}

static void test_chat_leave_postgame_skips_stale_republish_after_shared_clear()
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
    GBE_shared_dota_lobby_state.valid = false;

    const std::string session_raw = "stale-leave-session-token";
    const std::string body = make_leave_chat_body(tf.gc.GBE_local_lobby.chat_channel_id);
    bool result = tf.gc.GBE_HandleDotaLeaveChatChannelRequest(body, true, &session_raw);

    TEST_ASSERT(result, "stale postgame leave handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "stale postgame leave should push 7014 and update rich presence before cleanup without publishing state");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaOtherLeftChannel, "stale postgame leave should still push 7014");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "stale 7014 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "stale 7014 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7272_7014", "stale 7014 response should keep normal leave response reason");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::RichPresenceUpdate, "stale postgame leave should update rich presence after 7014");
    TEST_ASSERT(tf.recorder.actions[1].status == "#DOTA_RP_INIT", "stale leave rich presence status should reset to init");
    TEST_ASSERT(tf.recorder.actions[1].lobby_state == "SERVERSETUP", "stale leave rich presence lobby state should reset to serversetup");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::GenericLobbyLeave, "stale postgame cleanup should happen after rich presence update");
    TEST_ASSERT_EQ(tf.recorder.actions[2].item_id, 0xCAFEu, "stale postgame cleanup should record the stale lobby id");
    TEST_ASSERT(!GBE_HasSharedDotaLobbyState(), "stale postgame leave should not republish cleared shared state");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "stale postgame leave should clear local lobby after skipping publish");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0ull, "stale postgame leave should clear local lobby id");

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
    tf.gc.GBE_ClearPendingResetAfterCacheUnsubscribed();

    bool result = tf.gc.GBE_HandleDotaAbandonCurrentGameRequest(false, nullptr);

    TEST_ASSERT(result, "abandon handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "current-game disconnect should queue only 25");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaCacheUnsubscribed, "25 response should be queued with payload");
    TEST_ASSERT(tf.gc.GBE_HasPendingResetAfterCacheUnsubscribed(), "25 should mark reset pending");
    TEST_ASSERT_EQ(GBE_pending_reset_after_cache_unsubscribed_lobby_id, 0x7035u, "pending reset should record lobby id");

    ++g_tests_passed;
}

static void test_lobby_leave_queues_25_then_clears_local_lobby()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7042u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x704200u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.state = 1u;
    tf.gc.GBE_local_lobby.game_state = 0u;
    tf.gc.GBE_local_lobby.has_chat_channel = true;
    tf.gc.GBE_local_lobby.chat_channel_id = 0x704201u;

    const std::string session_raw = "leave-lobby-session-token";
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyLeaveRequest(true, &session_raw);

    TEST_ASSERT(result, "leave lobby handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "leave lobby should push one cache-unsubscribed response");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaCacheUnsubscribed, "leave lobby response should be emsg 25 with payload");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "leave lobby response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "leave lobby response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7040_leave_25", "leave lobby response should record reason");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "leave lobby should clear active flag after 25 response");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0u, "leave lobby should clear lobby id after 25 response");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generic_lobby_id, 0u, "leave lobby should clear generic lobby id after 25 response");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.has_chat_channel, "leave lobby should clear chat channel state");

    ++g_tests_passed;
}

static void test_lobby_destroy_queues_25_then_8247_and_clears_lobby()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x8246u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x824600u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.state = 1u;
    tf.gc.GBE_local_lobby.game_state = 0u;
    tf.gc.GBE_local_lobby.has_chat_channel = true;
    tf.gc.GBE_local_lobby.chat_channel_id = 0x824601u;

    const JobID_t request_job = 0x8247ABCDu;
    const std::string session_raw = "destroy-lobby-session-token";
    bool result = tf.gc.GBE_HandleDotaDestroyLobbyRequest(request_job, true, true, &session_raw);

    TEST_ASSERT(result, "destroy lobby handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "destroy lobby should push cache-unsubscribe envelope then destroy response envelope when request job exists");
    expect_push_payload(tf.recorder.actions[0], GBE_kEMsgClientFromGC, "first response should use ClientFromGC envelope with payload");
    expect_push_payload(tf.recorder.actions[1], GBE_kEMsgClientFromGC, "second response should use ClientFromGC envelope with payload");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "destroy lobby should clear active flag before returning");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0u, "destroy lobby should clear lobby id before returning");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generic_lobby_id, 0u, "destroy lobby should clear generic lobby id before returning");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.has_chat_channel, "destroy lobby should clear chat channel state before returning");

    ++g_tests_passed;
}

static void test_lobby_kick_removes_member_then_publishes_details()
{
    TestFixture tf;
    tf.reset();
    Steam_Matchmaking matchmaking;
    g_test_steam_client.steam_matchmaking = &matchmaking;

    const uint64_t owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    const uint32_t target_account_id = 0x334455u;
    const uint64_t target_steam_id = 0x110000100334455u;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7081u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x708100u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.owner_account_id = tf.settings.get_local_steam_id().GetAccountID();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.members.push_back(GBE_DotaLobbyMemberState{owner_steam_id, tf.gc.GBE_local_lobby.owner_account_id, 0u, 0u, 0u, true, 0u});
    tf.gc.GBE_local_lobby.members.push_back(GBE_DotaLobbyMemberState{target_steam_id, target_account_id, 1u, 2u, 0u, true, 0u});

    const std::string body = WireBodyBuilder()
        .varint(3u, target_account_id)
        .take();
    const std::string session_raw = "kick-lobby-session-token";
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyKickRequest(body, true, &session_raw);

    TEST_ASSERT(result, "kick handler should return true");
    TEST_ASSERT_EQ(tf.recorder.lobby_kicks.size(), 1u, "kick should call matchmaking once");
    TEST_ASSERT_EQ(tf.recorder.lobby_kicks[0].lobby_id, tf.gc.GBE_local_lobby.generic_lobby_id, "kick should target generic lobby id");
    TEST_ASSERT_EQ(tf.recorder.lobby_kicks[0].member_id, target_steam_id, "kick should target member steam id from account id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members.size(), 2u, "kick should preserve member slot count");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[0].steam_id, owner_steam_id, "kick should preserve owner member");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[1].steam_id, 0u, "kick should clear target member slot");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[1].account_id, 0u, "kick should clear target account id");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "kick should publish shared state once through recorder actions");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "kick should publish lobby state before details update");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7081_kick_member", "kick publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "kick should send one details update");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "kick details update should happen after shared state publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "kick details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "kick details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7081", "kick details update reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_set_details_mutates_before_publish_and_details_update()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7046u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x704600u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.room_name = "before";
    tf.gc.GBE_local_lobby.server_region = 1u;
    tf.gc.GBE_local_lobby.game_mode = 2u;

    const std::string session_raw = "set-details-session-token";
    const std::string body = make_lobby_set_details_body(tf.gc.GBE_local_lobby.lobby_id, "after-room", 12u, 7u);
    bool result = tf.gc.GBE_HandleDotaPracticeLobbySetDetailsRequest(body, true, &session_raw);

    TEST_ASSERT(result, "set details handler should return true");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.room_name.size(), 10u, "set details should update room name length");
    TEST_ASSERT(tf.gc.GBE_local_lobby.room_name == "after-room", "set details should update room name before returning");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.server_region, 12u, "set details should update server region before returning");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_mode, 7u, "set details should update game mode before returning");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "set details should publish local member data, shared state, and metadata");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "set details should publish local member data before shared state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7046_set_details", "set details local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "set details should publish shared lobby state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7046_set_details", "set details publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbyMetadataPublish, "set details should publish metadata after shared state");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7046_set_details", "set details metadata reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "set details should send one details update");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 3u, "set details update should happen after metadata publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "set details details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "set details details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7046", "set details details update reason should be preserved");

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

static void test_lobby_normal_signout_pending_clear_resets_state()
{
    TestFixture tf;
    tf.reset();

    tf.settings.set_lobby(CSteamID(0x2500u));
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x2500u;
    tf.gc.GBE_SetLastDotaLaunchStatePushedGameState(5u);
    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = true;
    GBE_shared_dota_lobby_state.lobby_id = 0x2500u;

    tf.gc.GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(0x2500u);
    TEST_ASSERT(tf.gc.GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(), "normal signout pending flag should be set");

    const uint64_t consumed_lobby_id = tf.gc.GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();
    TEST_ASSERT_EQ(consumed_lobby_id, 0x2500u, "normal signout consume should return pending lobby id");
    TEST_ASSERT(!tf.gc.GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(), "normal signout consume should clear pending flag");
    TEST_ASSERT_EQ(tf.gc.GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(), 0u, "normal signout pending lobby id should be cleared");

    tf.gc.push_incoming_now(GBE_kDotaCacheUnsubscribed | Steam_Game_Coordinator::protobuf_mask, std::to_string(consumed_lobby_id));
    tf.gc.GBE_ClearDotaLobbyRuntimeState();
    tf.gc.GBE_ClearSettingsLobbyForDotaSignout();

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "normal signout finalize should clear local lobby state after cache unsubscribe");
    TEST_ASSERT(!GBE_HasSharedDotaLobbyState(), "normal signout finalize should clear shared lobby state after cache unsubscribe");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 0u, "normal signout finalize should clear launch-state dedupe after cache unsubscribe");
    TEST_ASSERT_EQ(tf.settings.get_lobby().ConvertToUint64(), 0u, "normal signout finalize should clear settings lobby");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "normal signout finalize should record cache unsubscribe before settings clear");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaCacheUnsubscribed, "normal signout finalize should push cache unsubscribe first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::SettingsLobbyClear, "normal signout settings clear should happen after cache unsubscribe");
    TEST_ASSERT_EQ(tf.recorder.actions[1].item_id, consumed_lobby_id, "normal signout settings clear should preserve consumed lobby id");

    tf.gc.GBE_ClearSettingsLobbyForDotaSignout();
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "settings lobby clear should be a no-op when settings lobby is already empty");

    ++g_tests_passed;
}

static void test_lobby_runtime_reset_clears_local_shared_and_last_launch_state()
{
    TestFixture tf;
    tf.reset();

    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x5100u;
    tf.gc.GBE_SetLastDotaLaunchStatePushedGameState(7u);
    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = true;
    GBE_shared_dota_lobby_state.lobby_id = 0x5100u;
    GBE_shared_dota_lobby_state.state = 4u;
    GBE_shared_dota_lobby_state.game_state = 7u;

    tf.gc.GBE_ClearDotaLobbyRuntimeState();

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "runtime reset should clear local lobby active flag");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0u, "runtime reset should clear local lobby id");
    TEST_ASSERT(!GBE_HasSharedDotaLobbyState(), "runtime reset should clear shared lobby validity");
    TEST_ASSERT_EQ(GBE_GetSharedDotaLobbyIdOrZero(), 0u, "runtime reset should clear shared lobby id");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 0u, "runtime reset should clear last pushed launch game state");

    ++g_tests_passed;
}

static void test_lobby_launch_updates_rich_presence_after_initial_details()
{
    TestFixture tf;
    tf.reset();

    tf.settings.m_local_steam_id = CSteamID(0x110000100704100u);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x704100u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x704101u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.m_local_steam_id.ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "Launch Owner";
    tf.gc.GBE_local_lobby.state = 1u;
    tf.gc.GBE_local_lobby.game_state = 0u;

    const std::string body;
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyLaunchRequest(body, false, nullptr, false, 0u);

    TEST_ASSERT(result, "7041 launch handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 4u, "7041 standard launch should publish, push initial details, update rich presence, then build persona state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "7041 should publish shared lobby state first");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7041_launch_init", "7041 publish reason should be preserved");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaPracticeLobbyDetailsUpdate, "7041 should push initial details before rich presence");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7041_initial_26", "7041 initial details reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::RichPresenceUpdate, "7041 should update rich presence after initial details");
    TEST_ASSERT(tf.recorder.actions[2].status == "#DOTA_RP_INIT", "7041 rich presence status should be preserved");
    TEST_ASSERT(tf.recorder.actions[2].lobby_state == "SERVERSETUP", "7041 rich presence lobby state should be preserved");
    TEST_ASSERT(!tf.recorder.actions[2].include_party, "7041 rich presence should clear party state");
    TEST_ASSERT(tf.recorder.actions[2].include_lobby, "7041 rich presence should include lobby");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::LaunchPersonaState, "7041 should build persona state after rich presence");
    TEST_ASSERT(tf.recorder.actions[3].status == "#DOTA_RP_INIT", "7041 persona status should be preserved");
    TEST_ASSERT(tf.recorder.actions[3].lobby_state == "SERVERSETUP", "7041 persona lobby state should be preserved");
    TEST_ASSERT(!tf.recorder.actions[3].include_party, "7041 persona state should clear party state");
    TEST_ASSERT(tf.recorder.actions[3].include_lobby, "7041 persona state should include lobby");
    TEST_ASSERT(tf.recorder.actions[3].reason == "7041_launch_init", "7041 persona reason should be preserved");

    ++g_tests_passed;
}

static void setup_postgame_observation_lobby(
    TestFixture &tf,
    uint64_t lobby_id,
    uint64_t owner_steam_id,
    uint32_t owner_account_id,
    const std::string &owner_name)
{
    tf.gc.is_server = false;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = lobby_id;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0xBEEFu;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 2u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.owner_account_id = owner_account_id;
    tf.gc.GBE_local_lobby.owner_name = owner_name;

    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = true;
    GBE_shared_dota_lobby_state.lobby_id = lobby_id;
    GBE_shared_dota_lobby_state.state = 2u;
    GBE_shared_dota_lobby_state.game_state = 2u;
}

static void setup_local_owner_postgame_observation_lobby(TestFixture &tf, uint64_t lobby_id)
{
    setup_postgame_observation_lobby(
        tf,
        lobby_id,
        tf.settings.get_local_steam_id().ConvertToUint64(),
        tf.settings.get_local_steam_id().GetAccountID(),
        tf.settings.get_local_name());
}

static void setup_remote_owner_postgame_observation_lobby(TestFixture &tf, uint64_t lobby_id)
{
    setup_postgame_observation_lobby(tf, lobby_id, 0x200000u, 0x200000u, "Remote Host");
}

static void setup_arcade_active_lobby(TestFixture &tf, uint64_t match_id, uint64_t custom_game_id)
{
    tf.gc.GBE_local_lobby.match_id = match_id;
    tf.gc.GBE_local_lobby.launch_phase = 3u;
    tf.gc.GBE_local_lobby.custom_game.game_id = custom_game_id;
}

static void queue_postgame_observation_capture(TestFixture &tf)
{
    GBE_LocalLobby postgame_capture = tf.gc.GBE_local_lobby;
    postgame_capture.state = 3u;
    postgame_capture.game_state = 4u;
    tf.gc.test_set_next_lobby_capture(postgame_capture);
}

static void test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state()
{
    TestFixture tf;
    tf.reset();

    Steam_Game_Coordinator server_gc;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.is_server = true;
    server_gc.test_set_active_server_lobby(true);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    setup_local_owner_postgame_observation_lobby(tf, 0x5101u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("host_client_postgame_observation_test");

    TEST_ASSERT(result, "host client postgame observation should report handled runtime change");
    TEST_ASSERT(GBE_HasSharedDotaLobbyState(), "host client postgame observation should preserve server-owned shared state");
    TEST_ASSERT_EQ(GBE_GetSharedDotaLobbyIdOrZero(), 0x5101u, "shared lobby id should remain server-owned lobby id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 3u, "local lobby should still observe postgame state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "host client skip should only send refreshed details update");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "host client skip should push details update");

    ++g_tests_passed;
}

static void test_lobby_player_postgame_observation_clears_shared_state_after_details_update()
{
    TestFixture tf;
    tf.reset();

    setup_remote_owner_postgame_observation_lobby(tf, 0x5102u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("player_postgame_observation_test");

    TEST_ASSERT(result, "player postgame observation should run cleanup");
    TEST_ASSERT(!GBE_HasSharedDotaLobbyState(), "player postgame cleanup should clear shared state");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "player postgame cleanup should clear local lobby");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "player postgame cleanup should push details update then cache unsubscribe");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "player cleanup should push postgame details first");
    expect_push_action(tf.recorder.actions[1], GBE_kDotaCacheUnsubscribed, "player cleanup should push cache unsubscribe after cleanup");

    ++g_tests_passed;
}

static void test_lobby_arcade_active_postgame_observation_preserves_shared_state()
{
    TestFixture tf;
    tf.reset();

    setup_remote_owner_postgame_observation_lobby(tf, 0x5103u);
    setup_arcade_active_lobby(tf, 0x9000u, 0x7777u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("arcade_active_postgame_observation_test");

    TEST_ASSERT(result, "arcade active postgame observation should report handled runtime change");
    TEST_ASSERT(GBE_HasSharedDotaLobbyState(), "arcade active postgame observation should preserve shared state");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "arcade active postgame observation should preserve local lobby");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 3u, "arcade active local lobby should still observe postgame state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 0u, "arcade active skip should not push cleanup or details update");

    ++g_tests_passed;
}

static void test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip()
{
    TestFixture tf;
    tf.reset();

    Steam_Game_Coordinator server_gc;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.is_server = true;
    server_gc.test_set_active_server_lobby(true);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    setup_local_owner_postgame_observation_lobby(tf, 0x5104u);
    setup_arcade_active_lobby(tf, 0x9001u, 0x7778u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("host_client_arcade_postgame_observation_test");

    TEST_ASSERT(result, "host client arcade postgame observation should report handled runtime change");
    TEST_ASSERT(GBE_HasSharedDotaLobbyState(), "host client arcade postgame observation should preserve server-owned shared state");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "host client arcade postgame observation should preserve local lobby");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 3u, "host client arcade local lobby should observe postgame state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 0u, "host-client arcade skip should preserve state without cleanup or details update");

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

    const std::string body = WireBodyBuilder()
        .varint(1u, 1u)
        .varint(2u, 5u)
        .take();

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

    const std::string body = WireBodyBuilder()
        .varint(1u, 1u)
        .take();
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

    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(2u, 0x8052u)
        .varint(4u, 12345u)
        .take();
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

    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(2u, 33u)
        .varint(3u, 0u)
        .varint(4u, 4u)
        .take();
    bool result = tf.gc.GBE_HandleDotaCustomGameFinishedLoadingRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 0x8053u);

    TEST_ASSERT(result, "finished-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "8053 success should publish local member data before shared state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "8053 should publish local member data first");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_finished_loading", "8053 local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "8053 action should publish lobby state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "8053_finished_loading", "8053 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseLoaded, "8053 should mark launch loaded before publish");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 1u, "8053 should ensure at least wait-for-players state");

    ++g_tests_passed;
}

static void test_match_finished_loading_failure_preserves_reason()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(2u, 44u)
        .varint(3u, 2u)
        .bytes(4u, "#GameUI_Disconnect_Test")
        .take();
    bool result = tf.gc.GBE_HandleDotaCustomGameFinishedLoadingRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, 0x8053u);

    TEST_ASSERT(result, "failed finished-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "8053 load failure should publish one lobby state refresh");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "8053 load failure action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_load_failed", "8053 failure publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "8053 load failure should publish one details update");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "8053_load_failed", "8053 failure details update reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "8053 failure should leave launch phase queued");

    ++g_tests_passed;
}

static void test_match_7034_connected_player_updates_runtime_before_response()
{
    TestFixture tf;
    tf.reset();

    const uint64_t owner_steam_id = 0x110000100ABCDEFu;
    const uint32_t hero_id = 86u;
    const JobID_t source_job = 0x7034ABCDu;
    tf.settings.m_local_steam_id = CSteamID(owner_steam_id);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703400u;
    tf.gc.GBE_local_lobby.match_id = 0x703401u;
    tf.gc.GBE_local_lobby.server_id = 0x703402u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.owner_hero_id = 0u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 1u;

    const std::string body = make_dota7034_connected_player_body(owner_steam_id, hero_id);
    bool result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(result, "7034 connected player handler should return true");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 1u, "7034 should record one runtime state mutation");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].steam_id, owner_steam_id, "runtime state should target request steam id");
    TEST_ASSERT(tf.recorder.runtime_states[0].connected, "runtime state should mark player connected");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].action_sequence_index, 0u, "runtime state should mutate before any response action is recorded");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].hero_id, hero_id, "runtime state should preserve request hero id");
    TEST_ASSERT(tf.recorder.runtime_states[0].has_hero_id, "runtime state should preserve has_hero_id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.owner_hero_id, hero_id, "owner hero should update before response path completes");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "7034 should publish runtime state, queue runtime update, then respond");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "first action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7034_connected_player", "publish reason should identify connected player");
    expect_push_action(tf.recorder.actions[1], 26u, "second action should queue runtime update");
    expect_push_payload(tf.recorder.actions[2], 7034u, "third action should push 7034 response with payload");

    ++g_tests_passed;
}

static void test_match_7034_disconnected_player_updates_runtime_before_response()
{
    TestFixture tf;
    tf.reset();

    const uint64_t owner_steam_id = 0x110000100111111u;
    const uint64_t disconnected_steam_id = 0x110000100222222u;
    const JobID_t source_job = 0x7034DCu;
    tf.settings.m_local_steam_id = CSteamID(owner_steam_id);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7034D0u;
    tf.gc.GBE_local_lobby.match_id = 0x7034D1u;
    tf.gc.GBE_local_lobby.server_id = 0x7034D2u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 10u;

    const std::string body = make_dota7034_disconnected_player_body(disconnected_steam_id, 3u, 10u);
    bool result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(result, "7034 disconnected player handler should return true");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 1u, "7034 should record one disconnected runtime mutation");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].steam_id, disconnected_steam_id, "runtime state should target disconnected steam id");
    TEST_ASSERT(!tf.recorder.runtime_states[0].connected, "runtime state should mark player disconnected");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].action_sequence_index, 0u, "disconnected runtime state should mutate before any response action is recorded");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].hero_id, 0u, "disconnected runtime state should clear hero id");
    TEST_ASSERT(!tf.recorder.runtime_states[0].has_hero_id, "disconnected runtime state should not carry hero id");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "7034 disconnected should publish runtime state then respond");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "first action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7034_disconnected_player", "publish reason should identify disconnected player");
    expect_push_payload(tf.recorder.actions[1], 7034u, "second action should push 7034 response with payload");

    ++g_tests_passed;
}

static void test_match_7034_game_state_runtime_update_before_response()
{
    TestFixture tf;
    tf.reset();

    const uint64_t owner_steam_id = 0x110000100333333u;
    const JobID_t source_job = 0x703402u;
    tf.settings.m_local_steam_id = CSteamID(owner_steam_id);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703420u;
    tf.gc.GBE_local_lobby.match_id = 0x703421u;
    tf.gc.GBE_local_lobby.server_id = 0x703422u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 2u;
    tf.gc.GBE_local_lobby.game_mode = 1u;

    const std::string body = make_dota7034_game_state_body(2u, 2u);
    bool result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(result, "7034 game_state handler should return true");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 0u, "game_state-only request should not mutate member runtime state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "7034 game_state should queue runtime update then respond");
    expect_push_payload(tf.recorder.actions[0], 26u, "first action should push runtime update with payload");
    TEST_ASSERT(tf.recorder.actions[0].msg_body.find("runtime AP hero_selection fallback strategy_time") != std::string::npos, "runtime update payload should preserve strategy-time reason");
    expect_push_payload(tf.recorder.actions[1], 7034u, "second action should push 7034 response with payload");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 2u, "runtime update should keep lobby state at in-game");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 3u, "runtime update should advance game_state to strategy time fallback target");

    ++g_tests_passed;
}

static void test_match_7034_host_showcase_repush_guard_marks_once()
{
    TestFixture tf;
    tf.reset();

    const uint64_t owner_steam_id = 0x110000100555555u;
    const JobID_t source_job = 0x703455u;
    tf.settings.m_local_steam_id = CSteamID(owner_steam_id);
    tf.gc.is_server = true;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703450u;
    tf.gc.GBE_local_lobby.match_id = 0x703451u;
    tf.gc.GBE_local_lobby.server_id = 0x703452u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 3u;

    Steam_Game_Coordinator client_gc;
    client_gc.items.push_back(Econ_Item{});
    g_test_steam_client.steam_game_coordinator = &client_gc;

    const std::string body = make_dota7034_game_state_body(4u, 2u);
    bool first_result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(first_result, "7034 showcase handler should return true on first request");
    TEST_ASSERT(tf.gc.GBE_HasPushedDotaHostShowcaseEquip(), "first showcase request should mark host equip repushed");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "first showcase request should repush cache then respond");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::ServerGcForward, "first showcase action should repush host equipped items");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7034_showcase_host_equip_repush", "showcase repush reason should be preserved");
    expect_push_payload(tf.recorder.actions[1], 7034u, "second action should push 7034 response with payload");

    tf.recorder.clear();
    bool second_result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(second_result, "7034 showcase handler should return true on second request");
    TEST_ASSERT(tf.gc.GBE_HasPushedDotaHostShowcaseEquip(), "second showcase request should keep host equip repush marked");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "second showcase request should skip cache repush and still respond");
    expect_push_payload(tf.recorder.actions[0], 7034u, "second request should still push 7034 response with payload");

    ++g_tests_passed;
}

static void test_match_7034_launch_poll_records_details_update_before_response()
{
    TestFixture tf;
    tf.reset();

    const uint64_t owner_steam_id = 0x110000100444444u;
    const JobID_t source_job = 0x7034FEu;
    tf.settings.m_local_steam_id = CSteamID(owner_steam_id);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7034F0u;
    tf.gc.GBE_local_lobby.match_id = 0x7034F1u;
    tf.gc.GBE_local_lobby.server_id = 0x7034F2u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.state = 1u;
    tf.gc.GBE_local_lobby.game_state = 0u;
    tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;

    std::string body;
    bool result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), true, source_job);

    TEST_ASSERT(result, "7034 launch poll handler should return true");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 0u, "launch poll should not mutate member runtime state");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "launch poll should request one details update fallback");
    TEST_ASSERT(!tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "launch poll should not preserve server id in fallback details update");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override.empty(), "launch poll should not pass a message override");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7034_launch_poll", "launch poll details update reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "launch poll should still emit connected players response through recorder actions");
    expect_push_payload(tf.recorder.actions[0], 7034u, "recorded action should push 7034 response with payload");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 1u, "launch poll should keep lobby state unchanged");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 0u, "launch poll should keep game_state unchanged");

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

    std::printf("[run] test_inventory_equip_planner_empty_body\n");
    RUN_TEST(test_inventory_equip_planner_empty_body);

    std::printf("[run] test_inventory_equip_planner_single_item_order\n");
    RUN_TEST(test_inventory_equip_planner_single_item_order);

    std::printf("[run] test_inventory_equip_planner_multi_item_order\n");
    RUN_TEST(test_inventory_equip_planner_multi_item_order);

    std::printf("[run] test_inventory_equip_planner_missing_item\n");
    RUN_TEST(test_inventory_equip_planner_missing_item);

    std::printf("[run] test_inventory_equip_planner_style_bitmask_input\n");
    RUN_TEST(test_inventory_equip_planner_style_bitmask_input);

    std::printf("[run] test_chat_join_channel\n");
    RUN_TEST(test_chat_join_channel);

    std::printf("[run] test_chat_leave_postgame_channel_order\n");
    RUN_TEST(test_chat_leave_postgame_channel_order);

    std::printf("[run] test_chat_leave_postgame_skips_stale_republish_after_shared_clear\n");
    RUN_TEST(test_chat_leave_postgame_skips_stale_republish_after_shared_clear);

    std::printf("[run] test_lobby_abandon_current_game_disconnect_queues_25\n");
    RUN_TEST(test_lobby_abandon_current_game_disconnect_queues_25);

    std::printf("[run] test_lobby_leave_queues_25_then_clears_local_lobby\n");
    RUN_TEST(test_lobby_leave_queues_25_then_clears_local_lobby);

    std::printf("[run] test_lobby_destroy_queues_25_then_8247_and_clears_lobby\n");
    RUN_TEST(test_lobby_destroy_queues_25_then_8247_and_clears_lobby);

    std::printf("[run] test_lobby_kick_removes_member_then_publishes_details\n");
    RUN_TEST(test_lobby_kick_removes_member_then_publishes_details);

    std::printf("[run] test_lobby_set_details_mutates_before_publish_and_details_update\n");
    RUN_TEST(test_lobby_set_details_mutates_before_publish_and_details_update);

    std::printf("[run] test_lobby_abandon_ready_teardown_queues_postgame_response\n");
    RUN_TEST(test_lobby_abandon_ready_teardown_queues_postgame_response);

    std::printf("[run] test_lobby_normal_signout_pending_clear_resets_state\n");
    RUN_TEST(test_lobby_normal_signout_pending_clear_resets_state);

    std::printf("[run] test_lobby_runtime_reset_clears_local_shared_and_last_launch_state\n");
    RUN_TEST(test_lobby_runtime_reset_clears_local_shared_and_last_launch_state);

    std::printf("[run] test_lobby_launch_updates_rich_presence_after_initial_details\n");
    RUN_TEST(test_lobby_launch_updates_rich_presence_after_initial_details);

    std::printf("[run] test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state\n");
    RUN_TEST(test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state);

    std::printf("[run] test_lobby_player_postgame_observation_clears_shared_state_after_details_update\n");
    RUN_TEST(test_lobby_player_postgame_observation_clears_shared_state_after_details_update);

    std::printf("[run] test_lobby_arcade_active_postgame_observation_preserves_shared_state\n");
    RUN_TEST(test_lobby_arcade_active_postgame_observation_preserves_shared_state);

    std::printf("[run] test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip\n");
    RUN_TEST(test_lobby_host_client_postgame_observation_takes_precedence_over_arcade_skip);

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

    std::printf("[run] test_match_finished_loading_failure_preserves_reason\n");
    RUN_TEST(test_match_finished_loading_failure_preserves_reason);

    std::printf("[run] test_match_7034_connected_player_updates_runtime_before_response\n");
    RUN_TEST(test_match_7034_connected_player_updates_runtime_before_response);

    std::printf("[run] test_match_7034_disconnected_player_updates_runtime_before_response\n");
    RUN_TEST(test_match_7034_disconnected_player_updates_runtime_before_response);

    std::printf("[run] test_match_7034_game_state_runtime_update_before_response\n");
    RUN_TEST(test_match_7034_game_state_runtime_update_before_response);

    std::printf("[run] test_match_7034_host_showcase_repush_guard_marks_once\n");
    RUN_TEST(test_match_7034_host_showcase_repush_guard_marks_once);

    std::printf("[run] test_match_7034_launch_poll_records_details_update_before_response\n");
    RUN_TEST(test_match_7034_launch_poll_records_details_update_before_response);

    std::printf("\n=== Results: %d passed, %d failed, %d total ===\n",
                g_tests_passed, g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}

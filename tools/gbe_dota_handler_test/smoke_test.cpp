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
#include "test_fixture.h"

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

bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

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

static gbe::dota_gc_router::DotaGcRequestContext make_dispatch_context(
    uint32 inner_emsg,
    gbe::dota_gc_router::DotaGcRequestPath path,
    std::string body = {},
    JobID_t request_job_id = 0,
    std::string outer_session_field_raw = {})
{
    gbe::dota_gc_router::DotaGcRequestContext context{};
    context.valid = true;
    context.inner_emsg = inner_emsg;
    context.body = std::move(body);
    context.request_job_id = request_job_id;
    context.has_request_job = request_job_id != 0;
    context.path = path;
    context.wrapped = path == gbe::dota_gc_router::DotaGcRequestPath::Wrapped;
    context.outer_session_field_raw = std::move(outer_session_field_raw);
    return context;
}

static void install_production_dispatcher(TestFixture &tf)
{
    tf.gc.handler_registry = Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry();
}

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
    server_gc.is_server = true;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.test_set_active_server_lobby(true);
    server_gc.GBE_local_lobby.active = true;
    server_gc.GBE_local_lobby.lobby_id = 1u;
    server_gc.GBE_local_lobby.owner_steam_id = 12345u;
    g_test_steam_client.steam_game_coordinator = &tf.gc;
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    // Configure local lobby so the lobby-snapshot-refresh path fires
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 1;
    tf.gc.GBE_local_lobby.owner_steam_id = 12345u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 2u;
    tf.gc.GBE_MarkDotaPrivateLobbySnapshotReplayed();

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);

    bool result = tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0);

    TEST_ASSERT(result, "handler should return true");

    // Verify the full documented action sequence
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 1u, "2569 should update owner runtime state once");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].action_sequence_index, 0u, "owner hero should become known before equip responses and cache propagation");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].hero_id, 2u, "runtime state should carry inferred hero id");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 13u, "should publish inferred hero before forward actions and early replay");

    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "1: publish inferred owner hero");
    TEST_ASSERT(tf.recorder.actions[0].reason == "2569_owner_hero_inferred", "1: publish reason should identify inferred hero");

    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::PushIncomingNow, "2: PushIncomingNow (SO UpdateMultiple)");
    TEST_ASSERT_EQ((tf.recorder.actions[1].msg_type & ~0x80000000u), 26u, "2: emsg=26");

    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::PushIncomingNow, "3: PushIncomingNow (response)");
    TEST_ASSERT_EQ((tf.recorder.actions[2].msg_type & ~0x80000000u), 2570u, "3: emsg=2570");

    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::SaveItemsToFile, "4: SaveItemsToFile");

    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::ServerGcForward, "5: ServerGcForward (cache push)");
    TEST_ASSERT_EQ((tf.recorder.actions[4].msg_type & ~0x80000000u), 0u, "5: emsg=0 (CacheSubscribed)");
    TEST_ASSERT_EQ(tf.recorder.actions[4].item_id, 1u, "5: cache push should target a server GC");
    TEST_ASSERT_EQ(tf.recorder.actions[4].steam_id, 12345u, "5: cache push should use local steam id");
    TEST_ASSERT_EQ(tf.recorder.actions[4].server_gc_source_item_count, 1u, "5: cache push should include source items");
    TEST_ASSERT(tf.recorder.actions[4].server_gc_unsubscribe_first, "5: cache push should unsubscribe before subscribe");
    TEST_ASSERT(tf.recorder.actions[4].reason == "equip_forward_host_resubscribe_server", "5: cache push reason should identify equip forward");
    TEST_ASSERT_EQ(server_gc.all_user_items[CSteamID(12345u)].size(), 1u, "server GC should mirror the equipped host item");

    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::ServerGcForward, "6: ServerGcForward (SO Create)");
    TEST_ASSERT_EQ((tf.recorder.actions[5].msg_type & ~0x80000000u), 21u, "6: emsg=21");

    TEST_ASSERT_EQ(tf.recorder.actions[6].type, GBE_DotaActionType::ServerGcForward, "7: ServerGcForward (SO UpdateMultiple forward)");
    TEST_ASSERT_EQ((tf.recorder.actions[6].msg_type & ~0x80000000u), 26u, "7: emsg=26");

    TEST_ASSERT_EQ(tf.recorder.actions[7].type, GBE_DotaActionType::NetworkBroadcast, "8: NetworkBroadcast");
    TEST_ASSERT_EQ(tf.recorder.actions[7].source_id, 12345u, "8: network broadcast should use local steam id as source");

    TEST_ASSERT_EQ(tf.recorder.actions[8].type, GBE_DotaActionType::LobbySnapshotRefresh, "9: LobbySnapshotRefresh");
    TEST_ASSERT(tf.recorder.actions[8].reason == "equip_items_refresh", "9: snapshot refresh reason should identify equip replay");
    TEST_ASSERT_EQ(server_gc.GBE_local_lobby.owner_hero_id, 2u, "2569 should establish the host hero before 7034");
    TEST_ASSERT_EQ(tf.recorder.actions[9].type, GBE_DotaActionType::ServerGcForward, "10: early owner hero cache replay");
    TEST_ASSERT(tf.recorder.actions[9].reason == "7034_owner_hero_known_server", "early replay should reuse the owner hero server seam");
    TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[10]), 26u, "11: early owner hero client update");
    TEST_ASSERT(tf.recorder.actions[10].reason == "7034_owner_hero_known_client", "early replay should reuse the owner hero client seam");
    TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[11]), 1029u, "12: first wearable refresh");
    TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[12]), 1029u, "13: delayed wearable refresh");

    ++g_tests_passed;
}

static void test_inventory_equip_inferred_hero_requires_valid_single_hero_items()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.add_item(0xAAA2, 201);
    tf.gc.is_server = false;
    tf.gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 1u;

    Steam_Game_Coordinator server_gc;
    server_gc.is_server = true;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.test_set_active_server_lobby_id(1u);
    server_gc.GBE_local_lobby.active = true;
    server_gc.GBE_local_lobby.lobby_id = 1u;
    server_gc.GBE_local_lobby.owner_steam_id = 12345u;
    g_test_steam_client.steam_game_coordinator = &tf.gc;
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    std::string missing_item_body;
    encode_equip_op(missing_item_body, 0xFFFFu, 76u, 1u);
    TEST_ASSERT(tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(missing_item_body.data()), missing_item_body.size(), false, 0),
        "missing item request should be consumed");
    TEST_ASSERT_EQ(server_gc.GBE_local_lobby.owner_hero_id, 0u, "missing item should not infer owner hero");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 0u, "missing item should not update runtime state");

    tf.recorder.clear();
    std::string mixed_hero_body;
    encode_equip_op(mixed_hero_body, 0xAAA1, 76u, 1u);
    encode_equip_op(mixed_hero_body, 0xAAA2, 108u, 2u);
    TEST_ASSERT(tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(mixed_hero_body.data()), mixed_hero_body.size(), false, 0),
        "mixed hero request should be consumed");
    TEST_ASSERT_EQ(server_gc.GBE_local_lobby.owner_hero_id, 0u, "mixed hero request should preserve unknown owner hero");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 0u, "mixed hero request should keep 7034 as authority");

    ++g_tests_passed;
}

static void test_inventory_equip_same_hero_does_not_repeat_known_hero_replay()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);
    tf.gc.is_server = false;
    tf.gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 1u;

    Steam_Game_Coordinator server_gc;
    server_gc.is_server = true;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.test_set_active_server_lobby_id(1u);
    server_gc.GBE_local_lobby.active = true;
    server_gc.GBE_local_lobby.lobby_id = 1u;
    server_gc.GBE_local_lobby.owner_steam_id = 12345u;
    server_gc.GBE_local_lobby.owner_hero_id = 76u;
    g_test_steam_client.steam_game_coordinator = &tf.gc;
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    std::string body;
    encode_equip_op(body, 0xAAA1, 76u, 1u);
    TEST_ASSERT(tf.gc.GBE_HandleDotaEquipItemsRequest(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0),
        "same hero request should be consumed");
    TEST_ASSERT_EQ(server_gc.GBE_local_lobby.owner_hero_id, 76u, "same hero request should preserve owner hero");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 1u, "same hero request should still synchronize member runtime state");
    for (const RecordedAction &action : tf.recorder.actions) {
        TEST_ASSERT(action.reason != "7034_owner_hero_known_server", "same hero request should not repeat known-hero cache replay");
        TEST_ASSERT(action.reason != "7034_owner_hero_known_client", "same hero request should not repeat local wearable replay");
    }

    ++g_tests_passed;
}

static void test_inventory_remote_cache_forward_preserves_aliased_items()
{
    TestFixture tf;
    tf.reset();

    const CSteamID remote_steam_id(0x110000100222222u);
    Steam_Game_Coordinator server_gc;
    server_gc.is_server = true;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;

    Econ_Item equipped_item{};
    equipped_item.id = 0xBEEFu;
    equipped_item.def = 123u;
    equipped_item.equip_states[86u] = 2u;
    server_gc.all_user_items[remote_steam_id].push_back(equipped_item);

    const std::vector<Econ_Item> &remote_items = server_gc.all_user_items.at(remote_steam_id);
    TEST_ASSERT(GBE_PushDotaPlayerEquippedItemsCacheToGC(
        &server_gc,
        remote_steam_id,
        remote_items,
        false,
        "test_remote_alias"), "remote cache forward should succeed");
    TEST_ASSERT_EQ(server_gc.all_user_items.at(remote_steam_id).size(), 1u, "remote mirrored inventory should survive aliased source input");
    TEST_ASSERT_EQ(server_gc.all_user_items.at(remote_steam_id)[0].id, equipped_item.id, "remote mirrored inventory should preserve item identity");

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

static void test_inventory_equip_planner_remote_client_broadcasts_without_server_gc()
{
    TestFixture tf;
    tf.reset();
    tf.add_item(0xAAA1, 200);

    std::string body;
    encode_equip_op(body, 0xAAA1, 2u, 3u);

    TestEquipPlannerSummary summary = test_plan_equip_items_request(
        reinterpret_cast<const uint8 *>(body.data()), body.size(), tf.gc.items, 100u, true, false, false);

    TEST_ASSERT(!summary.has_server_forward, "remote client should skip local server GC forwarding");
    TEST_ASSERT(summary.broadcast_equipped_items, "remote client should broadcast equipped items to the host gameserver");

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

static void test_production_dispatcher_registry_contract()
{
    const auto view = Steam_Game_Coordinator::GBE_ProductionDotaHandlerRegistry();
    TEST_ASSERT(view.entries != nullptr, "production registry should expose entries");
    TEST_ASSERT_EQ(view.size, 27u, "production registry should retain all canonical entries");
    TEST_ASSERT(gbe::dota_handler_registry::has_unique_message_ids_per_mode(view.entries, view.size), "production registry modes should be unique");
    TEST_ASSERT(gbe::dota_handler_registry::all_high_risk_entries_have_fixture(view.entries, view.size), "high-risk production entries should retain fixtures");
    for (std::size_t index = 0; index < view.size; ++index) {
        TEST_ASSERT(view.entries[index].adapter != nullptr, "production registry adapter should be present");
        TEST_ASSERT(view.entries[index].handler != gbe::dota_handler_registry::HandlerId::Unknown, "production registry handler identity should be present");
    }
    ++g_tests_passed;
}

static void test_production_dispatcher_join_chat_modes_and_session()
{
    const std::string body = WireBodyBuilder().bytes(2u, "dota_lobby_chat").varint(4u, 3u).take();
    const std::string session_raw = "production-session-token";

    for (const auto path : {gbe::dota_gc_router::DotaGcRequestPath::Direct, gbe::dota_gc_router::DotaGcRequestPath::Wrapped}) {
        TestFixture tf;
        tf.reset();
        install_production_dispatcher(tf);
        tf.gc.GBE_local_lobby.active = true;
        tf.gc.GBE_local_lobby.lobby_id = 0xCAFEu;
        tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
        tf.gc.GBE_local_lobby.owner_name = "tester";

        const auto context = make_dispatch_context(GBE_kDotaJoinChatChannel, path, body, 0u, session_raw);
        const auto *entry = gbe::dota_handler_registry::find_entry(
            tf.gc.handler_registry.entries, tf.gc.handler_registry.size, context.inner_emsg, context.path);
        TEST_ASSERT(entry != nullptr, "production registry should select join-chat entry");
        TEST_ASSERT_EQ(entry->handler, gbe::dota_handler_registry::HandlerId::JoinChatChannel, "join-chat identity should match production mapping");
        TEST_ASSERT(tf.gc.GBE_DispatchDotaPostLoginRequest(context), "production dispatcher should execute join-chat handler");
        TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "join-chat dispatch should publish and respond");
        TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[1]), GBE_kDotaJoinChatChannelResponse, "join-chat dispatch should push 7010");
        TEST_ASSERT(tf.recorder.actions[1].wrapped == context.wrapped, "join-chat response should preserve request path");
        TEST_ASSERT(tf.recorder.actions[1].session_raw == (context.wrapped ? session_raw : std::string{}), "only wrapped join-chat should forward session");
    }
    ++g_tests_passed;
}

static void test_production_dispatcher_direct_only_and_fallbacks()
{
    TestFixture tf;
    tf.reset();
    install_production_dispatcher(tf);

    auto direct = make_dispatch_context(4523u, gbe::dota_gc_router::DotaGcRequestPath::Direct, {}, 9003u);
    const auto *entry = gbe::dota_handler_registry::find_entry(
        tf.gc.handler_registry.entries, tf.gc.handler_registry.size, direct.inner_emsg, direct.path);
    TEST_ASSERT(entry != nullptr, "production registry should select upload-rate entry");
    TEST_ASSERT_EQ(entry->handler, gbe::dota_handler_registry::HandlerId::UploadRate, "upload-rate identity should match production mapping");
    TEST_ASSERT(tf.gc.GBE_DispatchDotaPostLoginRequest(direct), "direct upload-rate request should dispatch");
    TEST_ASSERT(has_single_push(tf.recorder, 4524u), "direct upload-rate dispatch should push 4524");

    tf.recorder.clear();
    auto wrapped = make_dispatch_context(4523u, gbe::dota_gc_router::DotaGcRequestPath::Wrapped, {}, 9003u, "ignored-session");
    TEST_ASSERT(!tf.gc.GBE_DispatchDotaPostLoginRequest(wrapped), "wrapped upload-rate request should be rejected");
    TEST_ASSERT(tf.recorder.actions.empty(), "rejected wrapped request should have no effects");

    auto unknown = make_dispatch_context(0x00FFFFFFu, gbe::dota_gc_router::DotaGcRequestPath::Direct);
    TEST_ASSERT(!tf.gc.GBE_DispatchDotaPostLoginRequest(unknown), "unknown request should be rejected");
    unknown.valid = false;
    unknown.inner_emsg = GBE_kDotaJoinChatChannel;
    TEST_ASSERT(!tf.gc.GBE_DispatchDotaPostLoginRequest(unknown), "invalid context should be rejected");
    TEST_ASSERT(tf.recorder.actions.empty(), "fallback paths should have no effects");
    ++g_tests_passed;
}

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
    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    GBE_GetSharedDotaLobbyStateStore().publish(shared);

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
    GBE_GetSharedDotaLobbyStateStore().clear();

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
    GBE_GetSharedDotaLobbyStateStore().clear();

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
    TEST_ASSERT(!GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "stale postgame leave should not republish cleared shared state");
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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "current-game disconnect should suppress lobby before queueing 25");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::AbandonedLobbySuppressed, "current-game disconnect should mark abandoned lobby before 25");
    TEST_ASSERT_EQ(tf.recorder.actions[0].item_id, 0x7035u, "current-game disconnect should suppress the current lobby id");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7035_current_game_disconnect", "current-game disconnect suppress reason should be preserved");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaCacheUnsubscribed, "25 response should be queued after suppression with payload");
    TEST_ASSERT(tf.gc.GBE_HasPendingResetAfterCacheUnsubscribed(), "25 should mark reset pending");
    TEST_ASSERT_EQ(GBE_pending_reset_after_cache_unsubscribed_lobby_id, 0x7035u, "pending reset should record lobby id");

    ++g_tests_passed;
}

static void test_lobby_abandon_arcade_launch_failure_discards_before_25()
{
    TestFixture tf;
    tf.reset();
    tf.gc.is_server = false;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703500u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 2u;
    tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
    tf.gc.GBE_local_lobby.custom_game.game_id = 0x7035BEEFu;
    tf.gc.GBE_local_lobby.owner_connected = false;
    tf.gc.GBE_ClearPendingResetAfterCacheUnsubscribed();

    bool result = tf.gc.GBE_HandleDotaAbandonCurrentGameRequest(false, nullptr);

    TEST_ASSERT(result, "arcade launch failure abandon should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "arcade launch failure should discard, suppress, then queue 25");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LaunchMessagesDiscardedForAbandon, "arcade launch failure should discard queued launch messages before suppression");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7035_arcade_launch_failed_before_connect", "arcade launch failure discard reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::AbandonedLobbySuppressed, "arcade launch failure should suppress abandoned lobby before 25");
    TEST_ASSERT_EQ(tf.recorder.actions[1].item_id, 0x703500u, "arcade launch failure suppression should target current lobby id");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7035_arcade_launch_failed_before_connect", "arcade launch failure suppression reason should be preserved");
    expect_push_payload(tf.recorder.actions[2], GBE_kDotaCacheUnsubscribed, "arcade launch failure should queue 25 after suppression");
    TEST_ASSERT(tf.gc.GBE_HasPendingResetAfterCacheUnsubscribed(), "arcade launch failure should mark reset pending");
    TEST_ASSERT_EQ(GBE_pending_reset_after_cache_unsubscribed_lobby_id, 0x703500u, "arcade launch failure pending reset should record lobby id");

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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "leave lobby should suppress abandoned lobby before 25 response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::AbandonedLobbySuppressed, "leave lobby should mark abandoned lobby before 25");
    TEST_ASSERT_EQ(tf.recorder.actions[0].item_id, 0x7042u, "leave lobby suppression should target current lobby id");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7040_leave", "leave lobby suppression reason should be preserved");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaCacheUnsubscribed, "leave lobby response should be emsg 25 with payload");
    TEST_ASSERT(tf.recorder.actions[1].wrapped, "leave lobby response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[1].session_raw == session_raw, "leave lobby response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7040_leave_25", "leave lobby response should record reason");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "leave lobby should clear active flag after 25 response");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0u, "leave lobby should clear lobby id after 25 response");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generic_lobby_id, 0u, "leave lobby should clear generic lobby id after 25 response");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 1ull, "leave should retain the newly allocated generation after clearing lobby state");
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

static void test_lobby_set_team_slot_publishes_before_details_and_ack()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7047u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x704700u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_account_id = tf.settings.get_local_steam_id().GetAccountID();
    tf.gc.GBE_local_lobby.owner_name = "tester";
    tf.gc.GBE_local_lobby.members.push_back(GBE_DotaLobbyMemberState{
        tf.gc.GBE_local_lobby.owner_steam_id,
        tf.gc.GBE_local_lobby.owner_account_id,
        0u,
        0u,
        0u,
        true,
        0u
    });

    const std::string session_raw = "set-team-slot-session-token";
    const JobID_t request_job = 0x7047ABCDu;
    const std::string body = WireBodyBuilder()
        .varint(1u, GBE_kDotaTeamGoodGuys)
        .varint(2u, 3u)
        .varint(3u, 2u)
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbySetTeamSlotRequest(body, request_job, true, true, &session_raw);

    TEST_ASSERT(result, "set team slot handler should return true");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.owner_team, GBE_kDotaTeamGoodGuys, "set team slot should update owner team before publishing");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.owner_slot, 3u, "set team slot should update owner slot before publishing");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.bot_difficulty_radiant, 2u, "set team slot should update radiant bot difficulty before publishing");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "set team slot should publish local/shared state then ack");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "set team slot should publish local member data before shared state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7047_set_team_slot", "set team slot local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "set team slot should publish shared state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7047_set_team_slot", "set team slot shared publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "set team slot should send one details update");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 2u, "set team slot details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "set team slot details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "set team slot details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7047", "set team slot details update reason should be preserved");
    expect_push_payload(tf.recorder.actions[2], GBE_kDotaPracticeLobbyResponse, "set team slot should push 7055 ack after details update");
    TEST_ASSERT(tf.recorder.actions[2].wrapped, "set team slot 7055 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[2].session_raw == session_raw, "set team slot 7055 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7047_7055", "set team slot 7055 reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_join_broadcast_publishes_before_details_and_ack()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7149u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x714900u;

    const std::string session_raw = "join-broadcast-session-token";
    const JobID_t request_job = 0x7149ABCDu;
    const std::string body = WireBodyBuilder()
        .varint(1u, 77u)
        .bytes(2u, "caster room")
        .bytes(3u, "US")
        .bytes(4u, "en")
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(body, request_job, true, true, &session_raw);

    TEST_ASSERT(result, "join broadcast handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.has_broadcast_channel, "join broadcast should mark channel active before publishing");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.broadcast_channel_id, 77u, "join broadcast should preserve channel id");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_description == "caster room", "join broadcast should preserve description");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_country_code == "US", "join broadcast should preserve country code");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_language_code == "en", "join broadcast should preserve language code");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "join broadcast should publish shared state then ack");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "join broadcast should publish shared state before ack");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7149_join_broadcast", "join broadcast publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "join broadcast should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "join broadcast details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "join broadcast details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "join broadcast details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7149", "join broadcast details update reason should be preserved");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaPracticeLobbyResponse, "join broadcast should push 7055 ack after details update");
    TEST_ASSERT(tf.recorder.actions[1].wrapped, "join broadcast 7055 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[1].session_raw == session_raw, "join broadcast 7055 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7149_7055", "join broadcast 7055 reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_update_broadcast_publishes_before_details()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7367u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x736700u;
    tf.gc.GBE_local_lobby.has_broadcast_channel = true;
    tf.gc.GBE_local_lobby.broadcast_channel_id = 10u;
    tf.gc.GBE_local_lobby.broadcast_description = "old room";
    tf.gc.GBE_local_lobby.broadcast_country_code = "CA";
    tf.gc.GBE_local_lobby.broadcast_language_code = "fr";

    const std::string session_raw = "update-broadcast-session-token";
    const std::string body = WireBodyBuilder()
        .varint(1u, 88u)
        .bytes(2u, "GB")
        .bytes(3u, "updated room")
        .bytes(4u, "en")
        .take();
    bool result = tf.gc.GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(body, true, &session_raw);

    TEST_ASSERT(result, "update broadcast handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.has_broadcast_channel, "update broadcast should keep channel active before publishing");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.broadcast_channel_id, 88u, "update broadcast should preserve channel id");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_description == "updated room", "update broadcast should preserve description");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_country_code == "GB", "update broadcast should preserve country code");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_language_code == "en", "update broadcast should preserve language code");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "update broadcast should only publish shared state directly");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "update broadcast should publish shared state before details update");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7367_update_broadcast", "update broadcast publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "update broadcast should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "update broadcast details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "update broadcast details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "update broadcast details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7367", "update broadcast details update reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_close_broadcast_publishes_before_details()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x8054u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x805400u;
    tf.gc.GBE_local_lobby.has_broadcast_channel = true;
    tf.gc.GBE_local_lobby.broadcast_channel_id = 99u;
    tf.gc.GBE_local_lobby.broadcast_description = "closing room";
    tf.gc.GBE_local_lobby.broadcast_country_code = "JP";
    tf.gc.GBE_local_lobby.broadcast_language_code = "ja";

    const std::string session_raw = "close-broadcast-session-token";
    const std::string body = WireBodyBuilder()
        .varint(1u, 99u)
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(body, true, &session_raw);

    TEST_ASSERT(result, "close broadcast handler should return true");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.has_broadcast_channel, "close broadcast should mark channel inactive before publishing");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.broadcast_channel_id, 99u, "close broadcast should preserve channel id");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_description.empty(), "close broadcast should clear description");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_country_code.empty(), "close broadcast should clear country code");
    TEST_ASSERT(tf.gc.GBE_local_lobby.broadcast_language_code.empty(), "close broadcast should clear language code");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "close broadcast should only publish shared state directly");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "close broadcast should publish shared state before details update");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8054_close_broadcast", "close broadcast publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "close broadcast should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "close broadcast details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "close broadcast details update should preserve wrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == session_raw, "close broadcast details update should preserve session raw in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "8054", "close broadcast details update reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_invite_to_lobby_preserves_wrapped_response()
{
    TestFixture tf;
    tf.reset();
    Steam_Matchmaking matchmaking;
    g_test_steam_client.steam_matchmaking = &matchmaking;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x451200u;
    tf.gc.GBE_local_lobby.generic_lobby_id = CSteamID(0x4512u, k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat).ConvertToUint64();

    const std::string body = WireBodyBuilder()
        .varint(1u, 0x0102u)
        .varint(2u, 77u)
        .take();
    const std::string session_raw = "invite-session-token";
    const bool result = tf.gc.GBE_HandleDotaInviteToLobbyRequest(body, true, &session_raw);

    TEST_ASSERT(result, "invite handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "invite should push one invitation-created response");
    expect_push_payload(tf.recorder.actions[0], GBE_kGCInvitationCreated, "invite should push 4502");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "invite response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "invite response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "4512_invitation_created", "invite response should preserve reason");

    ++g_tests_passed;
}

static void test_lobby_invite_response_decline_pushes_remove_then_unsubscribe()
{
    TestFixture tf;
    tf.reset();

    const std::string body = WireBodyBuilder()
        .varint(1u, 0x451300u)
        .varint(2u, 0u)
        .varint(3u, 78u)
        .take();
    const std::string session_raw = "invite-response-session-token";
    const bool result = tf.gc.GBE_HandleDotaLobbyInviteResponseRequest(body, true, &session_raw);

    TEST_ASSERT(result, "declined invite response handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "declined invite should push remove then cache unsubscribe");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "declined invite should push remove 2011 first");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "remove response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session_raw, "remove response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[0].reason == "4513_decline_remove_2011", "remove response should preserve reason");
    expect_push_payload(tf.recorder.actions[1], GBE_kDotaCacheUnsubscribed, "declined invite should push cache unsubscribe second");
    TEST_ASSERT(tf.recorder.actions[1].wrapped, "cache unsubscribe should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[1].session_raw == session_raw, "cache unsubscribe should preserve session field");
    TEST_ASSERT(tf.recorder.actions[1].reason == "4513_decline_25", "cache unsubscribe should preserve reason");

    ++g_tests_passed;
}

static void test_lobby_create_records_cache_subscription_before_pushes()
{
    TestFixture tf;
    tf.reset();
    Steam_Matchmaking matchmaking;
    g_test_steam_client.steam_matchmaking = &matchmaking;

    const std::string session_raw = "create-lobby-session-token";
    const JobID_t request_job = 0x7038ABCDu;
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyCreateRequest(std::string(), request_job, true, true, &session_raw);

    TEST_ASSERT(result, "create lobby handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "create lobby should activate local lobby");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 7u, "create lobby should publish setup, record cache subscription, then push 24 and 7055");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "create should publish local member data first");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7038_create", "create local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::SettingsLobbySync, "create should sync settings lobby after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7038_create", "create settings sync reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbySnapshotRefresh, "create should publish shared lobby state after settings sync");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7038_create", "create shared publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::LobbyMetadataPublish, "create should publish metadata before cache subscription record");
    TEST_ASSERT(tf.recorder.actions[3].reason == "7038_create", "create metadata reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "create should record cache subscription before pushing 24");
    TEST_ASSERT(tf.recorder.actions[4].reason == "7038_create_wrapped", "create cache subscription record reason should preserve wrapped path");
    TEST_ASSERT(tf.recorder.actions[4].msg_body == "cache_subscribed", "create cache subscription record should preserve built cache body");
    expect_push_payload(tf.recorder.actions[5], GBE_kDotaCacheSubscribed, "create should push cache subscribed after recording it");
    TEST_ASSERT(tf.recorder.actions[5].wrapped, "create 24 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[5].session_raw == session_raw, "create 24 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[5].reason == "7038_24", "create 24 response reason should be preserved");
    expect_push_payload(tf.recorder.actions[6], GBE_kDotaPracticeLobbyResponse, "create should push 7055 after cache subscribed");
    TEST_ASSERT(tf.recorder.actions[6].wrapped, "create 7055 response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[6].session_raw == session_raw, "create 7055 response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[6].reason == "7038_7055", "create 7055 response reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_create_arcade_unsubscribes_previous_before_new_lobby()
{
    TestFixture tf;
    tf.reset();
    Steam_Matchmaking matchmaking;
    g_test_steam_client.steam_matchmaking = &matchmaking;

    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703800u;
    tf.gc.GBE_local_lobby.match_id = 0x7038AAu;
    tf.gc.GBE_local_lobby.custom_game.game_id = 0ull;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 1u;
    tf.gc.GBE_local_lobby.owner_team = 0u;
    tf.gc.GBE_local_lobby.owner_slot = 1u;

    Mod_entry arcade_mod{};
    arcade_mod.id = 0xC0FFEEu;
    arcade_mod.title = "Arcade Title";
    arcade_mod.metadata = "{\"addon_name\":\"arcade_addon\",\"map_name\":\"arcade_map\"}";
    tf.settings.m_mod_entries[arcade_mod.id] = arcade_mod;

    const std::string details = WireBodyBuilder()
        .bytes(26u, "123")
        .bytes(27u, "dota")
        .varint(29u, 0xC0FFEEu)
        .varint(30u, 1u)
        .varint(31u, 10u)
        .take();
    const std::string body = WireBodyBuilder()
        .bytes(7u, details)
        .take();

    const JobID_t request_job = 0x7038C0FFEEu;
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyCreateRequest(body, request_job, true, false, nullptr);

    TEST_ASSERT(result, "arcade create handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "arcade create should activate replacement lobby");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 1ull, "create should allocate exactly one generation");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.custom_game.game_id, 0xC0FFEEu, "arcade create should preserve requested custom game id");
    TEST_ASSERT(tf.gc.GBE_local_lobby.custom_game.mode == "arcade_addon", "arcade create should normalize custom mode from installed mod metadata");
    TEST_ASSERT(tf.gc.GBE_local_lobby.custom_game.map_name == "arcade_map", "arcade create should normalize custom map from installed mod metadata");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.owner_team, GBE_kDotaTeamGoodGuys, "arcade create should normalize owner team");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.owner_slot, 1u, "arcade create should normalize owner slot");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members.size(), 1u, "arcade create should keep one local owner member");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[0].team, GBE_kDotaTeamGoodGuys, "arcade create should normalize owner member team");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[0].slot, 1u, "arcade create should normalize owner member slot");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 8u, "arcade create should push previous 25 then setup, 24, and 7055");
    expect_push_payload(tf.recorder.actions[0], GBE_kDotaCacheUnsubscribed, "arcade create should unsubscribe previous lobby first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbyLocalMemberData, "arcade create should publish local member data after 25");
    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "arcade create should record new cache subscription before 24");
    expect_push_payload(tf.recorder.actions[6], GBE_kDotaCacheSubscribed, "arcade create should push new 24 after previous 25");
    expect_push_payload(tf.recorder.actions[7], GBE_kDotaPracticeLobbyResponse, "arcade create should ack 7055 after new 24");

    ++g_tests_passed;
}

static void test_lobby_join_records_cache_subscription_before_pushes()
{
    TestFixture tf;
    tf.reset();

    const std::string session_raw = "join-lobby-session-token";
    const JobID_t request_job = 0x7044ABCDu;
    const std::string body = WireBodyBuilder()
        .varint(1u, 0x704400u)
        .bytes(3u, "join-pass")
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyJoinRequest(body, request_job, true, false, &session_raw, true);

    TEST_ASSERT(result, "join lobby handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "join lobby should activate local lobby");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 1ull, "join should allocate exactly one generation");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0x704400ull, "join lobby should preserve requested lobby id");
    TEST_ASSERT(tf.gc.GBE_local_lobby.pass_key == "join-pass", "join lobby should preserve field 3 pass key");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 5u, "join lobby should publish local/shared state, record cache subscription, then push 24 and 7113");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "join should publish local member data first");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7044_join", "join local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "join should publish shared lobby state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7044_join", "join shared publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "join should record cache subscription before pushing 24");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7044_join_direct", "join cache subscription record reason should preserve direct path");
    TEST_ASSERT(tf.recorder.actions[2].msg_body == "cache_subscribed", "join cache subscription record should preserve built cache body");
    expect_push_payload(tf.recorder.actions[3], GBE_kDotaCacheSubscribed, "join should push cache subscribed after recording it");
    expect_push_payload(tf.recorder.actions[4], GBE_kDotaPracticeLobbyJoinResponse, "join should push 7113 after cache subscribed");

    ++g_tests_passed;
}

static void test_lobby_fast_leave_rejoin_same_id_rejects_old_generation_work()
{
    TestFixture tf;
    tf.reset();

    const uint64 lobby_id = 0x7044F00Dull;
    const std::string join_body = WireBodyBuilder()
        .varint(1u, lobby_id)
        .take();
    TEST_ASSERT(
        tf.gc.GBE_HandleDotaPracticeLobbyJoinRequest(join_body, 0x7044F001u, true, false, nullptr, true),
        "initial join should succeed");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, lobby_id, "initial join should use requested lobby id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 1ull, "initial join should allocate generation one");

    tf.gc.push_incoming(
        GBE_kDotaPracticeLobbyDetailsUpdate | Steam_Game_Coordinator::protobuf_mask,
        "old_generation_runtime",
        0.1,
        true,
        2u,
        4u);

    TEST_ASSERT(tf.gc.GBE_HandleDotaPracticeLobbyLeaveRequest(false, nullptr), "fast leave should succeed");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "fast leave should clear active lobby state");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 2ull, "fast leave should allocate the next generation");

    TEST_ASSERT(
        tf.gc.GBE_HandleDotaPracticeLobbyJoinRequest(join_body, 0x7044F002u, true, false, nullptr, true),
        "same-id rejoin should succeed");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "same-id rejoin should reactivate lobby state");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, lobby_id, "same-id rejoin should preserve protocol lobby identity");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 3ull, "same-id rejoin should allocate a distinct generation");

    const uint32 rejoined_state = tf.gc.GBE_local_lobby.state;
    const uint32 rejoined_game_state = tf.gc.GBE_local_lobby.game_state;
    const auto status = tf.gc.test_deliver_next_pending_message();
    TEST_ASSERT(
        status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Stale,
        "delayed work from the first join should be stale after same-id rejoin");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, rejoined_state, "stale delayed work should preserve rejoined lobby state");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, rejoined_game_state, "stale delayed work should preserve rejoined game state");
    TEST_ASSERT(tf.gc.incoming_messages.empty(), "stale delayed work should not enter the incoming queue");

    ++g_tests_passed;
}

static void test_lobby_join_empty_local_lobby_generates_lobby_id_with_pass_key_only()
{
    TestFixture tf;
    tf.reset();

    const std::string body = WireBodyBuilder()
        .bytes(3u, "pass-only")
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyJoinRequest(body, 0x7044DDu, true, false, nullptr, false);

    TEST_ASSERT(result, "pass-only join handler should return true");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "pass-only join should activate local lobby");
    TEST_ASSERT(tf.gc.GBE_local_lobby.lobby_id != 0ull, "pass-only join should generate a lobby id for empty local lobby");
    TEST_ASSERT(tf.gc.GBE_local_lobby.pass_key == "pass-only", "pass-only join should preserve pass key");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 4u, "pass-only join without ack should publish, record, then push only 24");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "pass-only join should publish local member data first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "pass-only join should publish shared state second");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "pass-only join should record cache subscription before 24");
    expect_push_payload(tf.recorder.actions[3], GBE_kDotaCacheSubscribed, "pass-only join should only push 24 when join response is disabled");

    ++g_tests_passed;
}

static void test_lobby_join_matched_generic_syncs_settings_before_publish()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x704401u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x70440100u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.get_local_steam_id().ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "tester";

    const JobID_t request_job = 0x7044BEEFu;
    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .take();
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyJoinRequest(body, request_job, true, false, nullptr, true);

    TEST_ASSERT(result, "matched generic join handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 6u, "matched generic join should sync settings before publishing and responses");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::SettingsLobbySync, "matched generic join should sync settings before local member data publish");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7044_join_generic", "matched generic settings sync reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbyLocalMemberData, "matched generic join should publish local member data after settings sync");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7044_join", "matched generic local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LobbySnapshotRefresh, "matched generic join should publish shared state after local member data");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7044_join", "matched generic shared publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "matched generic join should record cache subscription before pushing 24");
    TEST_ASSERT(tf.recorder.actions[3].reason == "7044_join_direct", "matched generic cache subscription record reason should preserve direct path");
    expect_push_payload(tf.recorder.actions[4], GBE_kDotaCacheSubscribed, "matched generic join should push 24 after cache record");
    expect_push_payload(tf.recorder.actions[5], GBE_kDotaPracticeLobbyJoinResponse, "matched generic join should push 7113 after 24");

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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 3u, "ready teardown should discard queued launch messages, suppress lobby, then postgame response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LaunchMessagesDiscardedForAbandon, "ready teardown should discard queued launch messages before suppression");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7035_ready_for_abandon_teardown", "ready teardown discard reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::AbandonedLobbySuppressed, "ready teardown should mark abandoned lobby before response");
    TEST_ASSERT_EQ(tf.recorder.actions[1].item_id, 0x7036u, "ready teardown suppression should target current lobby id");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7035_ready_for_abandon_teardown", "ready teardown suppression reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::PushIncomingNow, "teardown action should be push");
    TEST_ASSERT_EQ(tf.recorder.actions[2].msg_type & ~Steam_Game_Coordinator::protobuf_mask, GBE_kDotaOtherLeftChannel, "teardown response should be 7014 in stub");
    TEST_ASSERT(tf.recorder.actions[2].wrapped, "teardown response should preserve wrapped flag");
    TEST_ASSERT(tf.recorder.actions[2].session_raw == session_raw, "teardown response should preserve session field");
    TEST_ASSERT(tf.recorder.actions[2].reason == "postgame_teardown_7014", "teardown response should record reason");

    ++g_tests_passed;
}

static void test_lobby_abandon_finalize_after_other_left_resets_state()
{
    TestFixture tf;
    tf.reset();
    tf.gc.is_server = false;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7014u;
    tf.gc.GBE_local_lobby.abandon_postgame_active = true;

    tf.gc.GBE_SetPendingDotaAbandonFinalizeAfterOtherLeftChannel(0x7014u);
    TEST_ASSERT(tf.gc.GBE_HasPendingDotaAbandonFinalizeAfterOtherLeftChannel(), "abandon finalize pending flag should be set");

    const auto consumed = tf.gc.GBE_ConsumePendingDotaAbandonFinalizeAfterOtherLeftChannel();
    TEST_ASSERT(consumed.status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Current, "abandon finalize consume should report current");
    TEST_ASSERT_EQ(consumed.lobby_id, 0x7014u, "abandon finalize consume should return pending lobby id");
    TEST_ASSERT(!tf.gc.GBE_HasPendingDotaAbandonFinalizeAfterOtherLeftChannel(), "abandon finalize consume should clear pending flag");

    tf.gc.GBE_FinalizeDotaAbandonAfterOtherLeftChannel(consumed.lobby_id, "abandon_finalize_after_7014_test");

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "abandon finalize should clear local lobby state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "abandon finalize should execute one reset action");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::GcMemoryReset, "abandon finalize should reset GC memory");
    TEST_ASSERT(tf.recorder.actions[0].reason == "abandon_finalize_after_7014_test", "abandon finalize reset reason should be preserved");
    TEST_ASSERT(tf.recorder.actions[0].include_lobby, "abandon finalize reset should leave generic lobby");
    TEST_ASSERT(!tf.recorder.actions[0].include_party, "abandon finalize reset should preserve queued messages");

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
    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    shared.active = true;
    shared.lobby_id = 0x2500u;
    GBE_GetSharedDotaLobbyStateStore().publish(shared);

    tf.gc.GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(0x2500u);
    TEST_ASSERT(tf.gc.GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(), "normal signout pending flag should be set");

    const auto consumed = tf.gc.GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();
    TEST_ASSERT(consumed.status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Current, "normal signout consume should report current");
    TEST_ASSERT_EQ(consumed.lobby_id, 0x2500u, "normal signout consume should return pending lobby id");
    TEST_ASSERT(!tf.gc.GBE_HasPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(), "normal signout consume should clear pending flag");
    TEST_ASSERT(tf.gc.GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed().status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Empty, "normal signout pending slot should be empty after consume");

    tf.gc.push_incoming_now(GBE_kDotaCacheUnsubscribed | Steam_Game_Coordinator::protobuf_mask, std::to_string(consumed.lobby_id));
    tf.gc.GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(consumed.lobby_id, "normal_signout_pending_clear_test");

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "normal signout finalize should clear local lobby state after cache unsubscribe");
    TEST_ASSERT(!GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "normal signout finalize should clear shared lobby state after cache unsubscribe");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 0u, "normal signout finalize should clear launch-state dedupe after cache unsubscribe");
    TEST_ASSERT_EQ(tf.settings.get_lobby().ConvertToUint64(), 0u, "normal signout finalize should clear settings lobby");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 5u, "normal signout finalize should record cache unsubscribe before local cleanup actions");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaCacheUnsubscribed, "normal signout finalize should push cache unsubscribe first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::SettingsLobbyClear, "normal signout settings clear should happen after cache unsubscribe");
    TEST_ASSERT_EQ(tf.recorder.actions[1].item_id, consumed.lobby_id, "normal signout settings clear should preserve consumed lobby id");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LaunchPeripheralReset, "normal signout should reset launch peripheral after settings clear");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::DotaLobbyRuntimeClear, "normal signout should clear shared/local runtime after launch reset");
    TEST_ASSERT(tf.recorder.actions[3].reason == "normal_signout_pending_clear_test", "normal signout runtime clear reason should be recorded");
    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::RichPresenceClear, "normal signout should clear rich presence after runtime clear");

    tf.gc.GBE_ClearSettingsLobbyForDotaSignout();
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 5u, "settings lobby clear should be a no-op when settings lobby is already empty");

    ++g_tests_passed;
}

static void test_lobby_stale_postgame_task_is_rejected()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x2600u;
    tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Create, "test_create");
    tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
    tf.gc.GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(0x2600u);

    tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Reset, "test_reset");
    tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
    const auto consumed = tf.gc.GBE_ConsumePendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();

    TEST_ASSERT(consumed.status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Stale, "old postgame task should report stale");
    TEST_ASSERT(tf.gc.GBE_local_lobby.active, "stale postgame task should preserve current lobby state");
    TEST_ASSERT(tf.recorder.actions.empty(), "stale postgame task should not finalize or reset");

    ++g_tests_passed;
}

static void test_lobby_stale_delayed_runtime_task_is_rejected()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x2650u;
    tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Create, "test_create");
    tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
    tf.gc.push_incoming(GBE_kDotaPracticeLobbyDetailsUpdate | Steam_Game_Coordinator::protobuf_mask, "runtime", 0.1, true, 2u, 4u);

    tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Reset, "test_reset");
    tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
    const auto status = tf.gc.test_deliver_next_pending_message();

    TEST_ASSERT(status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Stale, "old delayed runtime task should report stale");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 0u, "stale delayed runtime task should not apply lobby state");
    TEST_ASSERT(tf.gc.incoming_messages.empty(), "stale delayed runtime task should not enter incoming queue");

    ++g_tests_passed;
}

static void test_lobby_stale_generation_action_property()
{
    const gbe::dota_lobby_generation::Boundary boundaries[] = {
        gbe::dota_lobby_generation::Boundary::Create,
        gbe::dota_lobby_generation::Boundary::Join,
        gbe::dota_lobby_generation::Boundary::Leave,
        gbe::dota_lobby_generation::Boundary::Reset,
        gbe::dota_lobby_generation::Boundary::Recover,
    };
    TestFixture tf;
    for (uint64 seed = 1u; seed <= 64u; ++seed) {
        tf.reset();
        tf.gc.GBE_local_lobby.active = true;
        tf.gc.GBE_local_lobby.lobby_id = 0x6000u + seed;
        tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Create, "P6_B_capture");
        tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
        tf.gc.push_incoming(
            GBE_kDotaPracticeLobbyDetailsUpdate | Steam_Game_Coordinator::protobuf_mask,
            "P6_B_stale_runtime",
            0.1,
            true,
            static_cast<uint32>(seed % 5u + 1u),
            static_cast<uint32>(seed % 9u + 1u));

        const auto boundary = boundaries[seed % 5u];
        tf.gc.GBE_AdvanceDotaLobbyGeneration(boundary, "P6_B_current");
        tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();
        tf.gc.GBE_local_lobby.state = static_cast<uint32>(100u + seed);
        tf.gc.GBE_local_lobby.game_state = static_cast<uint32>(200u + seed);
        const uint32 current_state = tf.gc.GBE_local_lobby.state;
        const uint32 current_game_state = tf.gc.GBE_local_lobby.game_state;

        const auto status = tf.gc.test_deliver_next_pending_message();
        TEST_ASSERT(
            status == Steam_Game_Coordinator::GBE_DotaDeferredTaskStatus::Stale,
            "P6-B old generation action should always report stale");
        TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, current_state, "P6-B stale action should never modify current lobby state");
        TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, current_game_state, "P6-B stale action should never modify current game state");
        TEST_ASSERT(tf.gc.incoming_messages.empty(), "P6-B stale action should never enter the incoming queue");
    }

    ++g_tests_passed;
}

static void test_lobby_reset_retains_new_generation()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x2700u;
    tf.gc.GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Create, "test_create");
    tf.gc.GBE_local_lobby.generation = tf.gc.GBE_CurrentDotaLobbyGeneration();

    tf.gc.ResetGCMemory("7035_disconnect_current_game_after_25", true, true);

    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.generation, 2ull, "reset should retain the newly allocated generation");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "reset should clear active lobby state");

    ++g_tests_passed;
}

static void test_lobby_runtime_reset_clears_local_shared_and_last_launch_state()
{
    TestFixture tf;
    tf.reset();

    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x5100u;
    tf.gc.GBE_SetLastDotaLaunchStatePushedGameState(7u);
    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    shared.active = true;
    shared.lobby_id = 0x5100u;
    shared.state = 4u;
    shared.game_state = 7u;
    GBE_GetSharedDotaLobbyStateStore().publish(shared);

    tf.gc.GBE_ClearDotaLobbyRuntimeState();

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "runtime reset should clear local lobby active flag");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, 0u, "runtime reset should clear local lobby id");
    const auto shared_after_reset = GBE_GetSharedDotaLobbyStateStore().snapshot();
    TEST_ASSERT(!shared_after_reset.valid, "runtime reset should clear shared lobby validity");
    TEST_ASSERT_EQ(shared_after_reset.lobby_id, 0u, "runtime reset should clear shared lobby id");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 0u, "runtime reset should clear last pushed launch game state");

    ++g_tests_passed;
}

static void test_lobby_runtime_reset_preserves_newer_shared_generation()
{
    TestFixture tf;
    tf.reset();

    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x5200u;
    tf.gc.GBE_local_lobby.generation = 20u;

    GBE_SharedDotaLobbyState replacement;
    replacement.valid = true;
    replacement.active = true;
    replacement.generation = 21u;
    replacement.lobby_id = 0x5201u;
    replacement.owner_name = "replacement";
    GBE_GetSharedDotaLobbyStateStore().publish(replacement);

    tf.gc.GBE_ClearDotaLobbyRuntimeState();

    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "runtime reset should clear stale local lobby state");
    const auto shared_after_reset = GBE_GetSharedDotaLobbyStateStore().snapshot();
    TEST_ASSERT(shared_after_reset.valid, "stale runtime clear should preserve newer shared state");
    TEST_ASSERT_EQ(shared_after_reset.generation, 21u, "stale runtime clear should preserve newer generation");
    TEST_ASSERT_EQ(shared_after_reset.lobby_id, 0x5201u, "stale runtime clear should preserve replacement lobby");

    ++g_tests_passed;
}

static gbe::dota_lobby_flow::LaunchStatePushPlanInput valid_launch_state_smoke_input()
{
    gbe::dota_lobby_flow::LaunchStatePushPlanInput input{};
    input.target_available = true;
    input.target_is_dota_profile = true;
    input.captured_lobby_active = true;
    input.lobby_state = 2u;
    input.lobby_game_state = 3u;
    input.lobby_server_id = 0x5100u;
    input.lobby_match_id = 0x5200u;
    input.lobby_connect_available = true;
    return input;
}

static void test_lobby_launch_state_push_smoke_action_sequence()
{
    TestFixture tf;
    tf.reset();

    GBE_LocalLobby lobby{};
    lobby.active = true;
    lobby.lobby_id = 0x5200u;
    lobby.state = 2u;
    lobby.game_state = 3u;
    lobby.server_id = 0x5100u;
    lobby.match_id = 0x5300u;
    lobby.connect = "127.0.0.1:27015";

    const auto plan = gbe::dota_lobby_flow::plan_launch_state_push(valid_launch_state_smoke_input());
    const bool executed = tf.gc.GBE_TestExecuteDotaLaunchStatePush(plan, lobby, "cache_payload", "details_payload", "launch_push_smoke");

    TEST_ASSERT(executed, "launch push smoke should execute valid plan");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 5u, "launch push smoke should record five actions");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyCacheSubscriptionRecord, "launch push should record cache subscription first");
    TEST_ASSERT(tf.recorder.actions[0].msg_body == "cache_payload", "launch push cache subscription should use 24 payload");
    TEST_ASSERT(tf.recorder.actions[0].reason == "launch_push_smoke", "launch push cache subscription should preserve reason");
    expect_push_action(tf.recorder.actions[1], GBE_kDotaCacheSubscribed, "launch push should push 24 second");
    TEST_ASSERT(tf.recorder.actions[1].msg_body == "cache_payload", "launch push 24 should use cache payload");
    expect_push_action(tf.recorder.actions[2], GBE_kDotaPracticeLobbyDetailsUpdate, "launch push should push 26 third");
    TEST_ASSERT(tf.recorder.actions[2].msg_body == "details_payload", "launch push 26 should use details payload");
    TEST_ASSERT(tf.recorder.actions[2].apply_lobby_state, "launch push 26 should apply lobby state");
    TEST_ASSERT_EQ(tf.recorder.actions[2].applied_lobby_state, 2u, "launch push 26 should apply captured state");
    TEST_ASSERT_EQ(tf.recorder.actions[2].applied_lobby_game_state, 3u, "launch push 26 should apply captured game state");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::RichPresenceUpdate, "launch push should reapply rich presence fourth");
    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::LaunchStateGameStateRecord, "launch push should record game state last");
    TEST_ASSERT_EQ(tf.recorder.actions[4].item_id, 3u, "launch push should record pushed game state value");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 3u, "launch push should update dedupe game state");

    ++g_tests_passed;
}

static void test_lobby_launch_state_push_smoke_duplicate_skip()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_SetLastDotaLaunchStatePushedGameState(3u);

    auto input = valid_launch_state_smoke_input();
    input.last_pushed_game_state = tf.gc.GBE_GetLastDotaLaunchStatePushedGameState();
    const auto plan = gbe::dota_lobby_flow::plan_launch_state_push(input);
    const bool executed = tf.gc.GBE_TestExecuteDotaLaunchStatePush(plan, GBE_LocalLobby{}, "cache_payload", "details_payload", "launch_push_duplicate");

    TEST_ASSERT(!executed, "duplicate launch push smoke should skip execution");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 0u, "duplicate launch push should not record side effects");
    TEST_ASSERT_EQ(tf.gc.GBE_GetLastDotaLaunchStatePushedGameState(), 3u, "duplicate launch push should preserve dedupe game state");

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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 5u, "7041 standard launch should reset peripherals, publish, push initial details, update rich presence, then build persona state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LaunchPeripheralReset, "7041 should reset launch peripheral state first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "7041 should publish shared lobby state after peripheral reset");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7041_launch_init", "7041 publish reason should be preserved");
    expect_push_payload(tf.recorder.actions[2], GBE_kDotaPracticeLobbyDetailsUpdate, "7041 should push initial details before rich presence");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7041_initial_26", "7041 initial details reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::RichPresenceUpdate, "7041 should update rich presence after initial details");
    TEST_ASSERT(tf.recorder.actions[3].status == "#DOTA_RP_INIT", "7041 rich presence status should be preserved");
    TEST_ASSERT(tf.recorder.actions[3].lobby_state == "SERVERSETUP", "7041 rich presence lobby state should be preserved");
    TEST_ASSERT(!tf.recorder.actions[3].include_party, "7041 rich presence should clear party state");
    TEST_ASSERT(tf.recorder.actions[3].include_lobby, "7041 rich presence should include lobby");
    TEST_ASSERT_EQ(tf.recorder.actions[4].type, GBE_DotaActionType::LaunchPersonaState, "7041 should build persona state after rich presence");
    TEST_ASSERT(tf.recorder.actions[4].status == "#DOTA_RP_INIT", "7041 persona status should be preserved");
    TEST_ASSERT(tf.recorder.actions[4].lobby_state == "SERVERSETUP", "7041 persona lobby state should be preserved");
    TEST_ASSERT(!tf.recorder.actions[4].include_party, "7041 persona state should clear party state");
    TEST_ASSERT(tf.recorder.actions[4].include_lobby, "7041 persona state should include lobby");
    TEST_ASSERT(tf.recorder.actions[4].reason == "7041_launch_init", "7041 persona reason should be preserved");

    ++g_tests_passed;
}

static void test_lobby_custom_launch_updates_rich_presence_before_setup_flow()
{
    TestFixture tf;
    tf.reset();

    tf.settings.m_local_steam_id = CSteamID(0x110000100704101u);
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x704101u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x704102u;
    tf.gc.GBE_local_lobby.owner_steam_id = tf.settings.m_local_steam_id.ConvertToUint64();
    tf.gc.GBE_local_lobby.owner_name = "Custom Launch Owner";
    tf.gc.GBE_local_lobby.state = 1u;
    tf.gc.GBE_local_lobby.game_state = 0u;
    tf.gc.GBE_local_lobby.custom_game.game_id = 0xCAFE7041u;
    tf.gc.GBE_local_lobby.custom_game.map_name = "custom_launch_map";
    tf.gc.test_set_custom_game_launch_setup_flow_result(true);

    const std::string body;
    bool result = tf.gc.GBE_HandleDotaPracticeLobbyLaunchRequest(body, false, nullptr, false, 0u);

    TEST_ASSERT(result, "7041 custom launch handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 4u, "7041 custom launch should reset, publish, update rich presence, then build persona before setup flow handles it");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LaunchPeripheralReset, "7041 custom launch should reset launch peripheral first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "7041 custom launch should publish shared lobby before rich presence");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7041_launch_init", "7041 custom publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::RichPresenceUpdate, "7041 custom launch should update rich presence before setup flow return");
    TEST_ASSERT(tf.recorder.actions[2].status == "#DOTA_RP_INIT", "7041 custom rich presence status should reset to init");
    TEST_ASSERT(tf.recorder.actions[2].lobby_state == "SERVERSETUP", "7041 custom rich presence lobby state should be serversetup");
    TEST_ASSERT(!tf.recorder.actions[2].include_party, "7041 custom rich presence should clear party state");
    TEST_ASSERT(tf.recorder.actions[2].include_lobby, "7041 custom rich presence should include lobby");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::LaunchPersonaState, "7041 custom launch should build persona after rich presence");
    TEST_ASSERT(tf.recorder.actions[3].reason == "7041_custom_game_launch_init", "7041 custom persona reason should be preserved");

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
    tf.settings.set_lobby(CSteamID(lobby_id));

    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    shared.active = true;
    shared.lobby_id = lobby_id;
    shared.state = 2u;
    shared.game_state = 2u;
    GBE_GetSharedDotaLobbyStateStore().publish(shared);
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
    server_gc.test_set_active_server_lobby_id(0x5101u);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    setup_local_owner_postgame_observation_lobby(tf, 0x5101u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("host_client_postgame_observation_test");

    TEST_ASSERT(result, "host client postgame observation should report handled runtime change");
    const auto shared = GBE_GetSharedDotaLobbyStateStore().snapshot();
    TEST_ASSERT(shared.valid, "host client postgame observation should preserve server-owned shared state");
    TEST_ASSERT_EQ(shared.lobby_id, 0x5101u, "shared lobby id should remain server-owned lobby id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 3u, "local lobby should still observe postgame state");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "host client skip should only send refreshed details update");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "host client skip should push details update");

    ++g_tests_passed;
}

static void test_lobby_host_client_postgame_observation_ignores_mismatched_server_lobby()
{
    TestFixture tf;
    tf.reset();

    Steam_Game_Coordinator server_gc;
    server_gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
    server_gc.is_server = true;
    server_gc.test_set_active_server_lobby_id(0x9999u);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    setup_local_owner_postgame_observation_lobby(tf, 0x5105u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("host_client_mismatched_server_lobby_test");

    TEST_ASSERT(result, "mismatched server lobby should still report handled runtime change");
    TEST_ASSERT(!GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "mismatched server lobby should not preserve shared state");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "mismatched server lobby should run player cleanup");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 6u, "mismatched server lobby should run player cleanup sequence");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "mismatched server cleanup should push postgame details first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::RichPresenceClear, "mismatched server cleanup should clear rich presence after details update");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LaunchPeripheralReset, "mismatched server cleanup should reset launch peripheral after rich presence clear");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::DotaLobbyRuntimeClear, "mismatched server cleanup should clear shared/local runtime after launch reset");
    expect_push_action(tf.recorder.actions[4], GBE_kDotaCacheUnsubscribed, "mismatched server cleanup should push cache unsubscribe after cleanup");
    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::SettingsLobbyClear, "mismatched server cleanup should clear settings lobby after cache unsubscribe");

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
    TEST_ASSERT(!GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "player postgame cleanup should clear shared state");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "player postgame cleanup should clear local lobby");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 6u, "player postgame cleanup should push details, clear local state, then cache unsubscribe and settings lobby");
    expect_push_action(tf.recorder.actions[0], GBE_kDotaPracticeLobbyDetailsUpdate, "player cleanup should push postgame details first");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::RichPresenceClear, "player cleanup should clear rich presence after details update");
    TEST_ASSERT(tf.recorder.actions[1].reason == "clear_launch_rich_presence", "player cleanup rich presence clear reason should be recorded");
    TEST_ASSERT_EQ(tf.recorder.actions[2].type, GBE_DotaActionType::LaunchPeripheralReset, "player cleanup should reset launch peripheral state after rich presence clear");
    TEST_ASSERT_EQ(tf.recorder.actions[3].type, GBE_DotaActionType::DotaLobbyRuntimeClear, "player cleanup should clear shared/local runtime state after launch reset");
    TEST_ASSERT(tf.recorder.actions[3].reason == "player_postgame_observation_test", "player cleanup runtime clear reason should be recorded");
    expect_push_action(tf.recorder.actions[4], GBE_kDotaCacheUnsubscribed, "player cleanup should push cache unsubscribe after cleanup");
    TEST_ASSERT_EQ(tf.recorder.actions[5].type, GBE_DotaActionType::SettingsLobbyClear, "player cleanup should clear settings lobby after cache unsubscribe");
    TEST_ASSERT_EQ(tf.recorder.actions[5].item_id, 0x5102u, "player cleanup should clear the observed settings lobby id");

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
    TEST_ASSERT(GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "arcade active postgame observation should preserve shared state");
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
    server_gc.test_set_active_server_lobby_id(0x5104u);
    g_test_steam_client.steam_gameserver_game_coordinator = &server_gc;

    setup_local_owner_postgame_observation_lobby(tf, 0x5104u);
    setup_arcade_active_lobby(tf, 0x9001u, 0x7778u);
    queue_postgame_observation_capture(tf);

    const bool result = tf.gc.GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("host_client_arcade_postgame_observation_test");

    TEST_ASSERT(result, "host client arcade postgame observation should report handled runtime change");
    TEST_ASSERT(GBE_GetSharedDotaLobbyStateStore().snapshot().valid, "host client arcade postgame observation should preserve server-owned shared state");
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

static void test_misc_leaver_detected_publishes_before_details()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7072u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x707200u;
    const uint64_t leaver_steam_id = 0x110000100707200u;
    tf.gc.GBE_local_lobby.members.push_back(GBE_DotaLobbyMemberState{
        leaver_steam_id,
        0x7072u,
        GBE_kDotaTeamGoodGuys,
        1u,
        0u,
        true,
        0u});

    const std::string body = WireBodyBuilder()
        .varint(1u, leaver_steam_id)
        .varint(2u, 2u)
        .varint(6u, 123u)
        .take();
    bool result = tf.gc.GBE_HandleDotaLeaverDetectedRequest(reinterpret_cast<const uint8 *>(body.data()), body.size(), 0x7072ABCDu);

    TEST_ASSERT(result, "leaver detected handler should return true");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members.size(), 1u, "leaver detected should preserve member slot count");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[0].steam_id, leaver_steam_id, "leaver detected should target request steam id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.members[0].leaver_status, 2u, "leaver detected should update member leaver status before publishing");
    TEST_ASSERT(!tf.gc.GBE_local_lobby.members[0].connected, "leaver detected should mark member disconnected before publishing");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "leaver detected should only publish shared state directly");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "leaver detected should publish shared state before details update");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7072_leaver_detected", "leaver detected publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "leaver detected should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "leaver detected details update should happen after shared publish");
    TEST_ASSERT(!tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "leaver detected details update should preserve unwrapped flag in stub argument");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override.empty(), "leaver detected details update should use no session override");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7072_leaver_detected", "leaver detected details update reason should be preserved");

    ++g_tests_passed;
}

static void test_misc_lan_server_available_publishes_once_for_matching_lobby()
{
    TestFixture tf;
    tf.reset();
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x4511u;
    tf.gc.GBE_local_lobby.generic_lobby_id = 0x451100u;
    tf.gc.GBE_local_lobby.launch_4511_seen = false;

    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .take();
    bool first_result = tf.gc.GBE_HandleDotaLanServerAvailableRequest(4511u, reinterpret_cast<const uint8 *>(body.data()), body.size(), 0x4511ABCDu);

    TEST_ASSERT(first_result, "LAN server available handler should return true for matching lobby");
    TEST_ASSERT(tf.gc.GBE_local_lobby.launch_4511_seen, "LAN server available should mark 4511 seen before publishing");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "first matching LAN server available should publish shared state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "LAN server available should publish shared state directly");
    TEST_ASSERT(tf.recorder.actions[0].reason == "4511_lan_server_available_seen", "LAN server available publish reason should be preserved");

    bool second_result = tf.gc.GBE_HandleDotaLanServerAvailableRequest(4511u, reinterpret_cast<const uint8 *>(body.data()), body.size(), 0x4511ABCEu);

    TEST_ASSERT(second_result, "second LAN server available should return true for matching lobby");
    TEST_ASSERT(tf.gc.GBE_local_lobby.launch_4511_seen, "second LAN server available should keep 4511 seen");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "second matching LAN server available should not republish shared state");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 0u, "LAN server available should not send a details update directly");

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

static gbe::dota_gc_router::DotaGcRequestContext make_wrapped_custom_game_context(
    uint32_t inner_emsg, const std::string &body, JobID_t request_job_id = 0xC0FFEEu)
{
    gbe::dota_gc_router::DotaGcRequestContext context{};
    context.valid = true;
    context.inner_emsg = inner_emsg;
    context.body = body;
    context.has_request_job = true;
    context.request_job_id = request_job_id;
    context.outer_session_field_raw = "\x1A\x03sid";
    context.wrapped = true;
    context.path = gbe::dota_gc_router::DotaGcRequestPath::Wrapped;
    return context;
}

static gbe::dota_gc_router::DotaGcRequestContext make_direct_custom_game_context(
    uint32_t inner_emsg, const std::string &body, JobID_t request_job_id)
{
    gbe::dota_gc_router::DotaGcRequestContext context{};
    context.valid = true;
    context.inner_emsg = inner_emsg;
    context.body = body;
    context.has_request_job = true;
    context.request_job_id = request_job_id;
    context.path = gbe::dota_gc_router::DotaGcRequestPath::Direct;
    return context;
}

static void test_wrapped_custom_game_ready_up_returns_7170_and_preserves_session()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    const std::string body = WireBodyBuilder().varint(1u, 1u).take();
    const gbe::dota_gc_router::DotaGcRequestContext context =
        make_wrapped_custom_game_context(7070u, body, 0x7070ABCDu);
    const bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(context);

    TEST_ASSERT(result, "wrapped ready-up handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "wrapped ready-up should respond then publish state");
    TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[0]), 7170u, "wrapped ready-up should return 7170");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "wrapped ready-up response should retain wrapper flag");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == context.outer_session_field_raw, "wrapped ready-up should retain outer session");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7070_ready_up_status", "wrapped ready-up response reason should be preserved");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7070_wrapped_custom_game_ready_up_run_ack", "wrapped ready-up should publish its lifecycle transition");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 1u, "wrapped ready-up should advance game state");

    ++g_tests_passed;
}

static void test_wrapped_custom_game_started_loading_updates_state()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    const std::string body = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(2u, 0x8052u)
        .varint(4u, 12345u)
        .take();
    const bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_wrapped_custom_game_context(8052u, body));

    TEST_ASSERT(result, "wrapped started-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "wrapped 8052 should publish when runtime queue declines");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8052_wrapped_started_loading", "wrapped 8052 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "wrapped 8052 should send details update");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.custom_game.game_id, 0x8052u, "wrapped 8052 should update custom game id");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_start_time, 12345u, "wrapped 8052 should update game start time");

    ++g_tests_passed;
}

static void test_wrapped_custom_game_finished_loading_handles_success_and_failure()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    const std::string success = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(2u, 33u)
        .varint(3u, 0u)
        .varint(4u, 4u)
        .take();
    TEST_ASSERT(tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_wrapped_custom_game_context(8053u, success)), "wrapped 8053 success handler should return true");
    TEST_ASSERT_EQ(tf.recorder.runtime_states.size(), 1u, "wrapped 8053 success should update local member runtime state");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].steam_id, tf.settings.get_local_steam_id().ConvertToUint64(), "wrapped 8053 should update the local member");
    TEST_ASSERT(tf.recorder.runtime_states[0].connected, "wrapped 8053 should mark the local member connected");
    TEST_ASSERT_EQ(tf.recorder.runtime_states[0].action_sequence_index, 0u, "wrapped 8053 should update runtime state before publishing member data");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "wrapped 8053 success should publish local member data before shared state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "wrapped 8053 should publish local member data after runtime state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_wrapped_finished_loading", "wrapped 8053 local member reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "wrapped 8053 should publish shared state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "8053_wrapped_finished_loading", "wrapped 8053 shared state reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseLoaded, "wrapped 8053 success should mark launch loaded");

    tf.reset();
    setup_custom_game_lobby(tf);
    const std::string failure = WireBodyBuilder()
        .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
        .varint(3u, 2u)
        .bytes(4u, "#GameUI_Disconnect_Test")
        .take();
    TEST_ASSERT(tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_wrapped_custom_game_context(8053u, failure)), "wrapped 8053 failure handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "wrapped 8053 failure should publish state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_wrapped_load_failed", "wrapped 8053 failure reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "wrapped 8053 failure should retain queued launch phase");

    ++g_tests_passed;
}

static void test_match_ready_up_queues_7170_then_runtime_update()
{
    TestFixture tf;
    tf.reset();
    setup_custom_game_lobby(tf);

    const std::string body = WireBodyBuilder()
        .varint(1u, 1u)
        .take();
    bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_direct_custom_game_context(7070u, body, 0x7070u));

    TEST_ASSERT(result, "ready-up handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "ready-up should queue response then publish runtime state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::PushIncomingNow, "first action should be 7170 response");
    TEST_ASSERT_EQ(tf.recorder.actions[0].msg_type & ~Steam_Game_Coordinator::protobuf_mask, 7170u, "first response should be 7170");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "second action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7070_custom_game_ready_up_run_ack", "publish reason should identify ready-up ack");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "ready-up should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 2u, "ready-up details update should happen after 7170 response and shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7070_custom_game_ready_up_run_ack", "ready-up details update reason should be preserved");
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
    bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_direct_custom_game_context(8052u, body, 0x8052u));

    TEST_ASSERT(result, "started-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "8052 should publish one lobby state refresh when run advance stub declines");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "8052 action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8052_started_loading", "8052 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "8052 should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "8052 details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "8052_started_loading", "8052 details update reason should be preserved");
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
    bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_direct_custom_game_context(8053u, body, 0x8053u));

    TEST_ASSERT(result, "finished-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "8053 success should publish local member data before shared state");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbyLocalMemberData, "8053 should publish local member data first");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_finished_loading", "8053 local member data reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::LobbySnapshotRefresh, "8053 action should publish lobby state after local member data");
    TEST_ASSERT(tf.recorder.actions[1].reason == "8053_finished_loading", "8053 publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "8053 success should send one details update after publish");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 2u, "8053 success details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "8053_finished_loading", "8053 success details update reason should be preserved");
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
    bool result = tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_direct_custom_game_context(8053u, body, 0x8053u));

    TEST_ASSERT(result, "failed finished-loading handler should return true");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "8053 load failure should publish one lobby state refresh");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "8053 load failure action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "8053_load_failed", "8053 failure publish reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "8053 load failure should publish one details update");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 1u, "8053 failure details update should happen after shared publish");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "8053_load_failed", "8053 failure details update reason should be preserved");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "8053 failure should leave launch phase queued");

    ++g_tests_passed;
}

static bool invoke_custom_game_lifecycle(
    TestFixture &tf,
    bool wrapped,
    uint32_t emsg,
    const std::string &body,
    JobID_t source_job = 0x8053ABCDu)
{
    if (wrapped) {
        return tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
            make_wrapped_custom_game_context(emsg, body, source_job));
    }
    return tf.gc.GBE_HandleDotaCustomGameLifecycleRequest(
        make_direct_custom_game_context(emsg, body, source_job));
}

static void test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence()
{
    struct Case {
        bool load_failed;
        std::vector<std::string> expected_events;
    };
    const Case cases[] = {
        {false, {"runtime_state", "launch_phase", "local_member_publish", "shared_publish", "details_update"}},
        {true, {"shared_publish", "details_update"}},
    };

    for (const Case &test_case : cases) {
        std::vector<std::string> direct_events;
        for (bool wrapped : {false, true}) {
            TestFixture tf;
            tf.reset();
            setup_custom_game_lobby(tf);
            const std::string body = WireBodyBuilder()
                .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
                .varint(3u, test_case.load_failed ? 2u : 0u)
                .bytes(4u, test_case.load_failed ? "#GameUI_Disconnect_Test" : "")
                .take();

            TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, 8053u, body), "8053 equivalence case should be handled");
            TEST_ASSERT(tf.recorder.lifecycle_events == test_case.expected_events, "8053 lifecycle event order should match the shared executor contract");
            if (wrapped) {
                TEST_ASSERT(tf.recorder.lifecycle_events == direct_events, "direct and wrapped 8053 should produce equivalent domain event order");
                TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "wrapped 8053 should send one details update");
                TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "wrapped 8053 should preserve wrapper mode");
                TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override == "\x1A\x03sid", "wrapped 8053 should preserve outer session data");
            } else {
                direct_events = tf.recorder.lifecycle_events;
                TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 1u, "direct 8053 should send one details update");
                TEST_ASSERT(!tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "direct 8053 should use direct details mode");
                TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override.empty(), "direct 8053 should have no outer session data");
            }
        }
    }

    ++g_tests_passed;
}

static void test_custom_game_lifecycle_8052_direct_wrapped_action_sequence_equivalence()
{
    std::vector<std::string> direct_events;
    for (bool wrapped : {false, true}) {
        TestFixture tf;
        tf.reset();
        setup_custom_game_lobby(tf);
        tf.gc.GBE_local_lobby.match_id = 0x805200u;
        tf.gc.GBE_local_lobby.connect = "127.0.0.1:27015";
        tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
        const std::string body = WireBodyBuilder()
            .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
            .varint(2u, 0x8052u)
            .varint(4u, 12345u)
            .take();

        TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, 8052u, body), "8052 equivalence case should be handled");
        const std::vector<std::string> expected_events = {"launch_phase", "runtime_update"};
        TEST_ASSERT(tf.recorder.lifecycle_events == expected_events, "8052 should mark launch phase before queueing runtime update");
        if (wrapped) {
            TEST_ASSERT(tf.recorder.lifecycle_events == direct_events, "direct and wrapped 8052 should produce equivalent domain event order");
        } else {
            direct_events = tf.recorder.lifecycle_events;
        }
        TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 2u, "8052 runtime update should apply run state");
        TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 0u, "8052 runtime update should preserve pre-game state");
        TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "8052 should advance launch phase to run queued");
    }

    ++g_tests_passed;
}

static void test_custom_game_lifecycle_ignores_inactive_and_mismatched_lobbies()
{
    for (uint32_t emsg : {8052u, 8053u}) {
        for (bool wrapped : {false, true}) {
            TestFixture tf;
            tf.reset();
            setup_custom_game_lobby(tf);
            const uint64_t original_lobby_id = tf.gc.GBE_local_lobby.lobby_id;
            const uint64_t original_custom_game_id = tf.gc.GBE_local_lobby.custom_game.game_id;
            const std::string mismatched_body = WireBodyBuilder()
                .varint(1u, original_lobby_id + 1u)
                .varint(2u, 0xDEADu)
                .varint(3u, 0u)
                .varint(4u, 99999u)
                .take();

            TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, emsg, mismatched_body), "mismatched lifecycle request should be consumed");
            TEST_ASSERT(tf.recorder.lifecycle_events.empty(), "mismatched lifecycle request should produce no lifecycle events");
            TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.lobby_id, original_lobby_id, "mismatched lifecycle request should preserve lobby identity");
            TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.custom_game.game_id, original_custom_game_id, "mismatched lifecycle request should preserve custom game identity");
            TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "mismatched lifecycle request should preserve launch phase");

            tf.reset();
            const std::string inactive_body = WireBodyBuilder().varint(1u, original_lobby_id).take();
            TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, emsg, inactive_body), "inactive lifecycle request should be consumed");
            TEST_ASSERT(tf.recorder.lifecycle_events.empty(), "inactive lifecycle request should produce no lifecycle events");
            TEST_ASSERT(!tf.gc.GBE_local_lobby.active, "inactive lifecycle request should preserve inactive lobby state");
        }
    }

    ++g_tests_passed;
}

static void test_custom_game_lifecycle_duplicate_messages_are_deterministic()
{
    for (uint32_t emsg : {8052u, 8053u}) {
        for (bool wrapped : {false, true}) {
            TestFixture tf;
            tf.reset();
            setup_custom_game_lobby(tf);
            if (emsg == 8052u) {
                tf.gc.GBE_local_lobby.match_id = 0x805200u;
                tf.gc.GBE_local_lobby.connect = "127.0.0.1:27015";
                tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
            }
            const std::string body = WireBodyBuilder()
                .varint(1u, tf.gc.GBE_local_lobby.lobby_id)
                .varint(2u, 0x8052u)
                .varint(3u, 0u)
                .varint(4u, 12345u)
                .take();

            TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, emsg, body), "first lifecycle request should be handled");
            const std::vector<std::string> first_events = tf.recorder.lifecycle_events;
            tf.recorder.clear();
            TEST_ASSERT(invoke_custom_game_lifecycle(tf, wrapped, emsg, body), "duplicate lifecycle request should be handled");
            TEST_ASSERT(tf.recorder.lifecycle_events == first_events, "duplicate lifecycle request should preserve deterministic lifecycle ordering");
            if (emsg == 8053u) {
                TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseLoaded, "duplicate 8053 should preserve loaded phase");
                TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 1u, "duplicate 8053 should preserve monotonic game state");
            } else {
                TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.launch_phase, GBE_kDotaLaunchPhaseRunQueued, "duplicate 8052 should preserve run-queued phase");
                TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 0u, "duplicate 8052 should preserve pre-game state");
            }
        }
    }

    ++g_tests_passed;
}

static void test_lifecycle_executor_empty_and_conditional_actions()
{
    TestFixture tf;
    tf.reset();

    TEST_ASSERT(std::string(GBE_DescribeDotaActionType(GBE_DotaActionType::LobbyStateApply)) == "lobby_state_apply",
        "lifecycle state action diagnostic name should remain stable");
    TEST_ASSERT(std::string(GBE_DescribeDotaActionType(GBE_DotaActionType::RuntimeLobbyDetailsUpdate)) == "runtime_lobby_details_update",
        "lifecycle delayed action diagnostic name should remain stable");
    TEST_ASSERT(std::string(GBE_DescribeDotaActionType(GBE_DotaActionType::PushIncomingNow)) == "push_incoming_now",
        "lifecycle push action diagnostic name should remain stable");

    const gbe::dota_lifecycle::ExecutionResult empty_result =
        tf.gc.GBE_ExecuteDotaLifecycleActions({});
    TEST_ASSERT(empty_result.succeeded, "empty lifecycle action list should succeed");
    TEST_ASSERT(tf.recorder.actions.empty(), "empty lifecycle action list should record no actions");
    TEST_ASSERT(tf.recorder.lifecycle_events.empty(), "empty lifecycle action list should record no lifecycle events");

    tf.gc.test_set_member_runtime_result(false);
    const GBE_DotaActionList member_actions = gbe::dota_lifecycle::build_member_runtime_actions(
        700ull, true, 11u, true, "conditional_member_publish");
    const gbe::dota_lifecycle::ExecutionResult member_result =
        tf.gc.GBE_ExecuteDotaLifecycleActions(member_actions);
    TEST_ASSERT(!member_result.state_changed, "failed member mutation should report no state change");
    TEST_ASSERT(tf.recorder.lifecycle_events == std::vector<std::string>({"runtime_state"}),
        "failed member mutation should skip conditional shared publish");

    tf.reset();
    tf.gc.test_set_runtime_update_result(false);
    gbe::dota_lifecycle::TransitionEffects effects;
    effects.transition.queue_runtime_lobby_update = true;
    effects.transition.next_state = 2u;
    effects.transition.next_game_state = 1u;
    effects.transition.send_details_update = true;
    effects.transition.reason = "runtime_fallback";
    effects.fallback_publish_on_runtime_update_failure = true;
    const gbe::dota_lifecycle::ExecutionResult runtime_result =
        tf.gc.GBE_ExecuteDotaLifecycleActions(gbe::dota_lifecycle::build_transition_actions(effects));
    TEST_ASSERT(!runtime_result.runtime_update_queued, "failed runtime update should remain unqueued");
    TEST_ASSERT(runtime_result.details_update_sent, "failed runtime update should execute details fallback");
    TEST_ASSERT(tf.recorder.lifecycle_events == std::vector<std::string>({"runtime_update", "shared_publish", "details_update"}),
        "failed runtime update should execute ordered fallback actions");

    ++g_tests_passed;
}

static void test_lifecycle_executor_push_routes_and_failure_policy()
{
    TestFixture tf;
    tf.reset();

    GBE_DotaAction push;
    push.type = GBE_DotaActionType::PushIncomingNow;
    push.emsg = 7014u | GBE_kProtoMask;
    push.payload = "payload";
    push.reason = "action_reason";
    GBE_DotaAction follow_up;
    follow_up.type = GBE_DotaActionType::PendingResetAfterCacheUnsubscribed;
    follow_up.item_id = 55ull;
    const GBE_DotaActionList actions = {push, follow_up};

    std::string session = "session";
    gbe::dota_lifecycle::ExecutionOptions options;
    options.wrapped = true;
    options.outer_session_field_raw = &session;
    options.push_route = gbe::dota_lifecycle::PushRoute::DotaResponse;
    options.push_reason_override = "override_reason";
    tf.gc.test_set_dota_response_result(false);
    const gbe::dota_lifecycle::ExecutionResult continue_result =
        tf.gc.GBE_ExecuteDotaLifecycleActions(actions, options);
    TEST_ASSERT(continue_result.succeeded, "non-aborting push failure should continue execution");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "Dota response route should record one wrapped push");
    TEST_ASSERT(tf.recorder.actions[0].wrapped, "Dota response route should preserve wrapper mode");
    TEST_ASSERT(tf.recorder.actions[0].session_raw == session, "Dota response route should preserve session");
    TEST_ASSERT(tf.recorder.actions[0].reason == "override_reason", "push reason override should be applied");
    TEST_ASSERT(tf.gc.GBE_HasPendingResetAfterCacheUnsubscribed(), "non-aborting push failure should execute follow-up action");

    tf.reset();
    tf.gc.GBE_ClearPendingResetAfterCacheUnsubscribed();
    tf.gc.test_set_dota_response_result(false);
    options.push_route = gbe::dota_lifecycle::PushRoute::CacheUnsubscribedResponse;
    options.abort_on_push_failure = true;
    const gbe::dota_lifecycle::ExecutionResult abort_result =
        tf.gc.GBE_ExecuteDotaLifecycleActions(actions, options);
    TEST_ASSERT(!abort_result.succeeded, "aborting push failure should fail execution");
    TEST_ASSERT(!tf.gc.GBE_HasPendingResetAfterCacheUnsubscribed(), "aborting push failure should skip follow-up action");
    TEST_ASSERT_EQ(action_emsg(tf.recorder.actions[0]), GBE_kDotaCacheUnsubscribed,
        "cache-unsubscribed route should use message 25");

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
    tf.gc.is_server = true;
    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x703400u;
    tf.gc.GBE_local_lobby.match_id = 0x703401u;
    tf.gc.GBE_local_lobby.server_id = 0x703402u;
    tf.gc.GBE_local_lobby.owner_steam_id = owner_steam_id;
    tf.gc.GBE_local_lobby.owner_hero_id = 0u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 1u;

    Steam_Game_Coordinator client_gc;
    client_gc.items.push_back(Econ_Item{});
    g_test_steam_client.steam_game_coordinator = &client_gc;

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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 7u, "7034 should publish runtime state, replay host cache, update client items, refresh local wearables twice, queue runtime update, then respond");
    TEST_ASSERT_EQ(tf.recorder.actions[0].type, GBE_DotaActionType::LobbySnapshotRefresh, "first action should publish lobby state");
    TEST_ASSERT(tf.recorder.actions[0].reason == "7034_connected_player", "publish reason should identify connected player");
    TEST_ASSERT_EQ(tf.recorder.actions[1].type, GBE_DotaActionType::ServerGcForward, "second action should replay host equipped items after hero is known");
    TEST_ASSERT(tf.recorder.actions[1].reason == "7034_owner_hero_known_server", "cache replay reason should identify owner hero transition");
    TEST_ASSERT_EQ(tf.recorder.actions[2].msg_type, 26u, "third action should push current hero equipped cache update to client GC");
    TEST_ASSERT(tf.recorder.actions[2].reason == "7034_owner_hero_known_client", "client item update reason should identify owner hero transition");
    TEST_ASSERT_EQ(tf.recorder.actions[3].msg_type, 1029u, "fourth action should request host local wearable refresh");
    TEST_ASSERT_EQ(tf.recorder.actions[3].steam_id, owner_steam_id, "wearable refresh should target the lobby owner");
    TEST_ASSERT_EQ(tf.recorder.actions[3].delay, 0.1, "first wearable refresh should use the default hot-cache delay");
    TEST_ASSERT(tf.recorder.actions[3].server_gc, "first wearable refresh should be delivered by the server GC");
    TEST_ASSERT_EQ(tf.recorder.actions[4].msg_type, 1029u, "fifth action should request delayed cold-start wearable refresh");
    TEST_ASSERT_EQ(tf.recorder.actions[4].steam_id, owner_steam_id, "delayed wearable refresh should target the lobby owner");
    TEST_ASSERT_EQ(tf.recorder.actions[4].delay, 1.5, "second wearable refresh should cover cold-start entity initialization");
    TEST_ASSERT(tf.recorder.actions[4].server_gc, "delayed wearable refresh should be delivered by the server GC");
    expect_push_action(tf.recorder.actions[5], 26u, "sixth action should queue runtime update");
    expect_push_payload(tf.recorder.actions[6], 7034u, "seventh action should push 7034 response with payload");

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
    tf.gc.GBE_local_lobby.owner_hero_id = 0u;
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
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 2u, "first showcase request should repush server cache then respond while owner hero is unknown");
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
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates[0].action_sequence_index, 0u, "launch poll details update should happen before response action");
    TEST_ASSERT(!tf.recorder.practice_lobby_details_updates[0].preserve_server_id, "launch poll should not preserve server id in fallback details update");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].message_override.empty(), "launch poll should not pass a message override");
    TEST_ASSERT(tf.recorder.practice_lobby_details_updates[0].reason == "7034_launch_poll", "launch poll details update reason should be preserved");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "launch poll should still emit connected players response through recorder actions");
    expect_push_payload(tf.recorder.actions[0], 7034u, "recorded action should push 7034 response with payload");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.state, 1u, "launch poll should keep lobby state unchanged");
    TEST_ASSERT_EQ(tf.gc.GBE_local_lobby.game_state, 0u, "launch poll should keep game_state unchanged");

    ++g_tests_passed;
}

static void test_match_7034_terminal_launch_poll_skips_details_update()
{
    TestFixture tf;
    tf.reset();

    tf.gc.GBE_local_lobby.active = true;
    tf.gc.GBE_local_lobby.lobby_id = 0x7034F0u;
    tf.gc.GBE_local_lobby.match_id = 0x7034F1u;
    tf.gc.GBE_local_lobby.server_id = 0x7034F2u;
    tf.gc.GBE_local_lobby.state = 2u;
    tf.gc.GBE_local_lobby.game_state = 10u;
    tf.gc.GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseLoaded;

    std::string body;
    const bool result = tf.gc.GBE_HandleDotaDirect7034Request(
        7034u,
        reinterpret_cast<const uint8 *>(body.data()), body.size(), false, 0u);

    TEST_ASSERT(result, "terminal 7034 launch poll handler should return true");
    TEST_ASSERT_EQ(tf.recorder.practice_lobby_details_updates.size(), 0u, "terminal launch poll should skip details update");
    TEST_ASSERT_EQ(tf.recorder.actions.size(), 1u, "terminal launch poll should still emit 7034 response");
    expect_push_payload(tf.recorder.actions[0], 7034u, "terminal launch poll should preserve response");

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

    std::printf("[run] test_inventory_equip_inferred_hero_requires_valid_single_hero_items\n");
    RUN_TEST(test_inventory_equip_inferred_hero_requires_valid_single_hero_items);

    std::printf("[run] test_inventory_equip_same_hero_does_not_repeat_known_hero_replay\n");
    RUN_TEST(test_inventory_equip_same_hero_does_not_repeat_known_hero_replay);

    std::printf("[run] test_inventory_remote_cache_forward_preserves_aliased_items\n");
    RUN_TEST(test_inventory_remote_cache_forward_preserves_aliased_items);

    std::printf("[run] test_inventory_equip_planner_empty_body\n");
    RUN_TEST(test_inventory_equip_planner_empty_body);

    std::printf("[run] test_inventory_equip_planner_single_item_order\n");
    RUN_TEST(test_inventory_equip_planner_single_item_order);

    std::printf("[run] test_inventory_equip_planner_multi_item_order\n");
    RUN_TEST(test_inventory_equip_planner_multi_item_order);

    std::printf("[run] test_inventory_equip_planner_remote_client_broadcasts_without_server_gc\n");
    RUN_TEST(test_inventory_equip_planner_remote_client_broadcasts_without_server_gc);

    std::printf("[run] test_inventory_equip_planner_missing_item\n");
    RUN_TEST(test_inventory_equip_planner_missing_item);

    std::printf("[run] test_inventory_equip_planner_style_bitmask_input\n");
    RUN_TEST(test_inventory_equip_planner_style_bitmask_input);

    std::printf("[run] test_production_dispatcher_registry_contract\n");
    RUN_TEST(test_production_dispatcher_registry_contract);
    std::printf("[run] test_production_dispatcher_join_chat_modes_and_session\n");
    RUN_TEST(test_production_dispatcher_join_chat_modes_and_session);
    std::printf("[run] test_production_dispatcher_direct_only_and_fallbacks\n");
    RUN_TEST(test_production_dispatcher_direct_only_and_fallbacks);
    std::printf("[run] test_chat_join_channel\n");
    RUN_TEST(test_chat_join_channel);

    std::printf("[run] test_chat_leave_postgame_channel_order\n");
    RUN_TEST(test_chat_leave_postgame_channel_order);

    std::printf("[run] test_chat_leave_postgame_skips_stale_republish_after_shared_clear\n");
    RUN_TEST(test_chat_leave_postgame_skips_stale_republish_after_shared_clear);

    std::printf("[run] test_lobby_abandon_current_game_disconnect_queues_25\n");
    RUN_TEST(test_lobby_abandon_current_game_disconnect_queues_25);

    std::printf("[run] test_lobby_abandon_arcade_launch_failure_discards_before_25\n");
    RUN_TEST(test_lobby_abandon_arcade_launch_failure_discards_before_25);

    std::printf("[run] test_lobby_leave_queues_25_then_clears_local_lobby\n");
    RUN_TEST(test_lobby_leave_queues_25_then_clears_local_lobby);

    std::printf("[run] test_lobby_destroy_queues_25_then_8247_and_clears_lobby\n");
    RUN_TEST(test_lobby_destroy_queues_25_then_8247_and_clears_lobby);

    std::printf("[run] test_lobby_kick_removes_member_then_publishes_details\n");
    RUN_TEST(test_lobby_kick_removes_member_then_publishes_details);

    std::printf("[run] test_lobby_set_details_mutates_before_publish_and_details_update\n");
    RUN_TEST(test_lobby_set_details_mutates_before_publish_and_details_update);

    std::printf("[run] test_lobby_set_team_slot_publishes_before_details_and_ack\n");
    RUN_TEST(test_lobby_set_team_slot_publishes_before_details_and_ack);

    std::printf("[run] test_lobby_join_broadcast_publishes_before_details_and_ack\n");
    RUN_TEST(test_lobby_join_broadcast_publishes_before_details_and_ack);

    std::printf("[run] test_lobby_update_broadcast_publishes_before_details\n");
    RUN_TEST(test_lobby_update_broadcast_publishes_before_details);

    std::printf("[run] test_lobby_close_broadcast_publishes_before_details\n");
    RUN_TEST(test_lobby_close_broadcast_publishes_before_details);

    std::printf("[run] test_lobby_invite_to_lobby_preserves_wrapped_response\n");
    RUN_TEST(test_lobby_invite_to_lobby_preserves_wrapped_response);

    std::printf("[run] test_lobby_invite_response_decline_pushes_remove_then_unsubscribe\n");
    RUN_TEST(test_lobby_invite_response_decline_pushes_remove_then_unsubscribe);

    std::printf("[run] test_lobby_create_records_cache_subscription_before_pushes\n");
    RUN_TEST(test_lobby_create_records_cache_subscription_before_pushes);

    std::printf("[run] test_lobby_create_arcade_unsubscribes_previous_before_new_lobby\n");
    RUN_TEST(test_lobby_create_arcade_unsubscribes_previous_before_new_lobby);

    std::printf("[run] test_lobby_join_records_cache_subscription_before_pushes\n");
    RUN_TEST(test_lobby_join_records_cache_subscription_before_pushes);

    std::printf("[run] test_lobby_fast_leave_rejoin_same_id_rejects_old_generation_work\n");
    RUN_TEST(test_lobby_fast_leave_rejoin_same_id_rejects_old_generation_work);

    std::printf("[run] test_lobby_join_empty_local_lobby_generates_lobby_id_with_pass_key_only\n");
    RUN_TEST(test_lobby_join_empty_local_lobby_generates_lobby_id_with_pass_key_only);

    std::printf("[run] test_lobby_join_matched_generic_syncs_settings_before_publish\n");
    RUN_TEST(test_lobby_join_matched_generic_syncs_settings_before_publish);

    std::printf("[run] test_lobby_abandon_ready_teardown_queues_postgame_response\n");
    RUN_TEST(test_lobby_abandon_ready_teardown_queues_postgame_response);

    std::printf("[run] test_lobby_abandon_finalize_after_other_left_resets_state\n");
    RUN_TEST(test_lobby_abandon_finalize_after_other_left_resets_state);

    std::printf("[run] test_lobby_normal_signout_pending_clear_resets_state\n");
    RUN_TEST(test_lobby_normal_signout_pending_clear_resets_state);

    std::printf("[run] test_lobby_stale_postgame_task_is_rejected\n");
    RUN_TEST(test_lobby_stale_postgame_task_is_rejected);

    std::printf("[run] test_lobby_stale_delayed_runtime_task_is_rejected\n");
    RUN_TEST(test_lobby_stale_delayed_runtime_task_is_rejected);

    std::printf("[run] test_lobby_stale_generation_action_property\n");
    RUN_TEST(test_lobby_stale_generation_action_property);

    std::printf("[run] test_lobby_reset_retains_new_generation\n");
    RUN_TEST(test_lobby_reset_retains_new_generation);

    std::printf("[run] test_lobby_runtime_reset_clears_local_shared_and_last_launch_state\n");
    RUN_TEST(test_lobby_runtime_reset_clears_local_shared_and_last_launch_state);

    std::printf("[run] test_lobby_runtime_reset_preserves_newer_shared_generation\n");
    RUN_TEST(test_lobby_runtime_reset_preserves_newer_shared_generation);

    std::printf("[run] test_lobby_launch_state_push_smoke_action_sequence\n");
    RUN_TEST(test_lobby_launch_state_push_smoke_action_sequence);

    std::printf("[run] test_lobby_launch_state_push_smoke_duplicate_skip\n");
    RUN_TEST(test_lobby_launch_state_push_smoke_duplicate_skip);

    std::printf("[run] test_lobby_launch_updates_rich_presence_after_initial_details\n");
    RUN_TEST(test_lobby_launch_updates_rich_presence_after_initial_details);

    std::printf("[run] test_lobby_custom_launch_updates_rich_presence_before_setup_flow\n");
    RUN_TEST(test_lobby_custom_launch_updates_rich_presence_before_setup_flow);

    std::printf("[run] test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state\n");
    RUN_TEST(test_lobby_host_client_postgame_observation_preserves_server_owned_shared_state);

    std::printf("[run] test_lobby_host_client_postgame_observation_ignores_mismatched_server_lobby\n");
    RUN_TEST(test_lobby_host_client_postgame_observation_ignores_mismatched_server_lobby);

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

    std::printf("[run] test_misc_leaver_detected_publishes_before_details\n");
    RUN_TEST(test_misc_leaver_detected_publishes_before_details);

    std::printf("[run] test_misc_lan_server_available_publishes_once_for_matching_lobby\n");
    RUN_TEST(test_misc_lan_server_available_publishes_once_for_matching_lobby);

    std::printf("[run] test_misc_upload_rate\n");
    RUN_TEST(test_misc_upload_rate);

    std::printf("[run] test_misc_rank\n");
    RUN_TEST(test_misc_rank);

    std::printf("[run] test_match_ready_up_queues_7170_then_runtime_update\n");
    RUN_TEST(test_match_ready_up_queues_7170_then_runtime_update);

    std::printf("[run] test_wrapped_custom_game_ready_up_returns_7170_and_preserves_session\n");
    RUN_TEST(test_wrapped_custom_game_ready_up_returns_7170_and_preserves_session);

    std::printf("[run] test_wrapped_custom_game_started_loading_updates_state\n");
    RUN_TEST(test_wrapped_custom_game_started_loading_updates_state);

    std::printf("[run] test_wrapped_custom_game_finished_loading_handles_success_and_failure\n");
    RUN_TEST(test_wrapped_custom_game_finished_loading_handles_success_and_failure);

    std::printf("[run] test_match_started_loading_updates_custom_game_before_publish\n");
    RUN_TEST(test_match_started_loading_updates_custom_game_before_publish);

    std::printf("[run] test_match_finished_loading_marks_loaded_before_publish\n");
    RUN_TEST(test_match_finished_loading_marks_loaded_before_publish);

    std::printf("[run] test_match_finished_loading_failure_preserves_reason\n");
    RUN_TEST(test_match_finished_loading_failure_preserves_reason);

    std::printf("[run] test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence\n");
    RUN_TEST(test_custom_game_lifecycle_direct_wrapped_action_sequence_equivalence);

    std::printf("[run] test_custom_game_lifecycle_8052_direct_wrapped_action_sequence_equivalence\n");
    RUN_TEST(test_custom_game_lifecycle_8052_direct_wrapped_action_sequence_equivalence);

    std::printf("[run] test_custom_game_lifecycle_ignores_inactive_and_mismatched_lobbies\n");
    RUN_TEST(test_custom_game_lifecycle_ignores_inactive_and_mismatched_lobbies);

    std::printf("[run] test_custom_game_lifecycle_duplicate_messages_are_deterministic\n");
    RUN_TEST(test_custom_game_lifecycle_duplicate_messages_are_deterministic);

    std::printf("[run] test_lifecycle_executor_empty_and_conditional_actions\n");
    RUN_TEST(test_lifecycle_executor_empty_and_conditional_actions);

    std::printf("[run] test_lifecycle_executor_push_routes_and_failure_policy\n");
    RUN_TEST(test_lifecycle_executor_push_routes_and_failure_policy);

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

    std::printf("[run] test_match_7034_terminal_launch_poll_skips_details_update\n");
    RUN_TEST(test_match_7034_terminal_launch_poll_skips_details_update);

    std::printf("\n=== Results: %d passed, %d failed, %d total ===\n",
                g_tests_passed, g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}

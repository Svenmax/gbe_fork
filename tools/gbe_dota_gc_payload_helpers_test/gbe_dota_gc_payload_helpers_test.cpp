/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

// Unit tests for gbe_dota_gc_payload_helpers.cpp
//
// This TU was externalized from steam_game_coordinator.cpp and contains
// Free functions that perform protobuf wire-level payload patching,
// template replay, Hello/Welcome context extraction, and message construction.
//
// Most tests cover pure logic. Small coordinator stubs also verify focused
// message-construction helpers without loading the production runtime.
//
// Compilation: this file is compiled together with test_wrapper.cpp which
// includes the actual payload_helpers.cpp TU with stubbed SDK types.

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

// Provide stub types before including GBE headers
#include "stubs.h"

// Include GBE headers for function declarations
#include "dll/gbe_dota_gc_internal.h"
#include "dll/gbe_dota_lobby_state_store.h"
#include "dll/gbe_dota_payload_item_helpers.h"
#include "dll/gbe_dota_payload_lobby_helpers.h"
#include "dll/gbe_dota_payload_wire_helpers.h"
#include "dll/gbe_dota_protocol_constants.h"
#include "dll/gbe_dota_reconnect_context.h"
#include "dll/gbe_dota_request_router.h"
#include "dll/gbe_dota_runtime_state.h"
#include "dll/gbe_proto_buf_header.h"
#include "dll/gbe_dota_custom_game.h"
#include "dll/gbe_dota_custom_lobby_http.h"
#include "dll/gbe_dota_gc_router.h"
#include "dll/gbe_dota_gc_wire.h"
#include "dll/gbe_dota_lobby_flow.h"
#include "dll/gbe_gc_config.h"
#include "dll/gbe_gc_message_utils.h"
#include "dll/gbe_proto_wire.h"
#include "dll/dll/gbe_dota_reconnect_shared.h"
#include "dll/dll/gbe_dota_unlock_items.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <functional>

// =====================================================================
// Test infrastructure
// =====================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define EXPECT_EQ(a, b) do { \
    g_tests_run++; \
    if ((a) == (b)) { g_tests_passed++; } \
    else { g_tests_failed++; \
        std::fprintf(stderr, "FAIL %s:%d: EXPECT_EQ failed\n", __FILE__, __LINE__); } \
} while(0)

#define EXPECT_TRUE(a) do { \
    g_tests_run++; \
    if ((a)) { g_tests_passed++; } \
    else { g_tests_failed++; \
        std::fprintf(stderr, "FAIL %s:%d: EXPECT_TRUE(%s) failed\n", __FILE__, __LINE__, #a); } \
} while(0)

#define EXPECT_FALSE(a) do { \
    g_tests_run++; \
    if (!(a)) { g_tests_passed++; } \
    else { g_tests_failed++; \
        std::fprintf(stderr, "FAIL %s:%d: EXPECT_FALSE(%s) failed\n", __FILE__, __LINE__, #a); } \
} while(0)

#define EXPECT_STR_EQ(a, b) do { \
    g_tests_run++; \
    if (std::string(a) == std::string(b)) { g_tests_passed++; } \
    else { g_tests_failed++; \
        std::fprintf(stderr, "FAIL %s:%d: EXPECT_STR_EQ failed\n", __FILE__, __LINE__); } \
} while(0)

#define EXPECT_STR_CONTAINS(haystack, needle) do { \
    g_tests_run++; \
    if (std::string(haystack).find(needle) != std::string::npos) { g_tests_passed++; } \
    else { g_tests_failed++; \
        std::fprintf(stderr, "FAIL %s:%d: EXPECT_STR_CONTAINS failed\n", __FILE__, __LINE__); } \
} while(0)

#define TEST_CASE(name) static void name()

static std::string make_direct_proto_message(uint32_t emsg, const std::string &body, const std::string &header = std::string())
{
    std::string message;
    message.append(reinterpret_cast<const char *>(&emsg), sizeof(emsg));
    const uint32_t header_len = static_cast<uint32_t>(header.size());
    message.append(reinterpret_cast<const char *>(&header_len), sizeof(header_len));
    message.append(header);
    message.append(body);
    return message;
}

static std::string make_fixed64_bytes(uint64_t value)
{
    std::string bytes;
    bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
    return bytes;
}

static std::string make_fixed32_bytes(uint32_t value)
{
    std::string bytes;
    bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
    return bytes;
}

// =====================================================================
// Global state needed by payload_helpers TU
// =====================================================================

GBE_DotaLootListData GBE_vpk_loot_data;
bool GBE_recent_dota_reconnect_context_valid = false;
GBE_DotaReconnectContext GBE_recent_dota_reconnect_context{};
GBE_DotaServerHelloContext GBE_last_dota_server_hello_context{};
std::atomic<bool> GBE_dota_reconnect_eligible{true};

bool GBE_GetRecentDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out || !GBE_recent_dota_reconnect_context_valid)
        return false;
    *out = GBE_recent_dota_reconnect_context;
    return true;
}

void GBE_SetRecentDotaReconnectContext(const GBE_DotaReconnectContext &ctx)
{
    GBE_recent_dota_reconnect_context = ctx;
    GBE_recent_dota_reconnect_context_valid = true;
}

void GBE_ClearRecentDotaReconnectContext()
{
    GBE_recent_dota_reconnect_context_valid = false;
    GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
}

bool GBE_IsDotaReconnectEligible()
{
    return GBE_dota_reconnect_eligible.load();
}

void GBE_SetDotaReconnectEligible(bool eligible)
{
    GBE_dota_reconnect_eligible.store(eligible);
}

bool GBE_ConsumeDotaReconnectEligibility()
{
    bool expected = true;
    return GBE_dota_reconnect_eligible.compare_exchange_strong(expected, false);
}

// Stub for get_full_program_path (defined in dll/base.cpp which has heavy deps)
std::string get_full_program_path() { return "."; }

// =====================================================================
// Test: GBE_DescribeDotaLaunchPhase
// =====================================================================

TEST_CASE(test_csteamid_stub_behavior)
{
    CSteamID lobby_id(42u, k_EChatInstanceFlagLobby, k_EUniversePublic, k_EAccountTypeChat);
    CSteamID ordinary_chat_id(43u, 1u, k_EUniversePublic, k_EAccountTypeChat);
    CSteamID individual_id(44u, k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeIndividual);
    CSteamID console_user_id(45u, k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeConsoleUser);
    CSteamID game_server_id(46u, 1u, k_EUniversePublic, k_EAccountTypeGameServer);

    EXPECT_TRUE(lobby_id.IsLobby());
    EXPECT_FALSE(ordinary_chat_id.IsLobby());
    EXPECT_TRUE(individual_id.BIndividualAccount());
    EXPECT_TRUE(console_user_id.BIndividualAccount());
    EXPECT_FALSE(game_server_id.BIndividualAccount());
    EXPECT_TRUE(lobby_id.GetAccountID() == 42u);
}

TEST_CASE(test_player_item_cache_rebuild_advances_cache_version)
{
    auto &runtime_state = GBE_DotaRuntimeState();
    runtime_state.equip_cache_version = 29799760112000884ull;

    Steam_Game_Coordinator gc;
    Econ_Item item;
    item.id = 42ull;
    item.equip_states[1u] = 2u;

    EXPECT_TRUE(GBE_RebuildDotaPlayerItemsCacheToGC(
        &gc,
        CSteamID(76561198035005698ull),
        std::vector<Econ_Item>{item},
        "test"));
    EXPECT_EQ(runtime_state.equip_cache_version, 29799760112000885ull);
    EXPECT_EQ(gc.pushed_msg_type, GBE_kDotaCacheSubscribed | GBE_kProtoMask);
}

// =====================================================================
// Test: GBE_DescribeDotaLaunchPhase
// =====================================================================

TEST_CASE(test_describe_dota_launch_phase)
{
    EXPECT_STR_EQ("none", GBE_DescribeDotaLaunchPhase(0));
    EXPECT_STR_EQ("requested", GBE_DescribeDotaLaunchPhase(1));
    EXPECT_STR_EQ("serversetup_synced", GBE_DescribeDotaLaunchPhase(2));
    EXPECT_STR_EQ("run_queued", GBE_DescribeDotaLaunchPhase(3));
    EXPECT_STR_EQ("loaded", GBE_DescribeDotaLaunchPhase(4));
    EXPECT_STR_EQ("none", GBE_DescribeDotaLaunchPhase(999));
}

// =====================================================================
// Test: GBE_GetDotaReconnectContext
// =====================================================================

TEST_CASE(test_get_dota_reconnect_context)
{
    EXPECT_FALSE(GBE_GetDotaReconnectContext(nullptr));

    auto &shared_store = GBE_GetSharedDotaLobbyStateStore();
    shared_store.clear();
    GBE_ClearRecentDotaReconnectContext();

    GBE_DotaReconnectContext ctx{};
    EXPECT_FALSE(GBE_GetDotaReconnectContext(&ctx));

    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    shared.active = true;
    shared.generation = 77ull;
    shared.lobby_id = 67890ull;
    shared.state = 2;
    shared.game_state = 0;
    shared.connect = "127.0.0.1:27015";
    shared.server_id = 12345;
    shared.owner_steam_id = 76561198000000000ULL;
    shared.custom_game.game_id = 500ull;
    shared.owner_connected = true;
    shared.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
    shared_store.publish(shared);

    const auto source = gbe::dota_reconnect::source_from_shared_lobby_snapshot(
        shared_store.snapshot());
    EXPECT_TRUE(source.valid);
    EXPECT_TRUE(source.active);
    EXPECT_TRUE(source.generation == shared.generation);
    EXPECT_TRUE(source.lobby_id == shared.lobby_id);
    EXPECT_TRUE(source.lobby_state == shared.state);
    EXPECT_TRUE(source.game_state == shared.game_state);
    EXPECT_TRUE(source.server_id == shared.server_id);
    EXPECT_TRUE(source.custom_game_id == shared.custom_game.game_id);
    EXPECT_TRUE(source.owner_connected == shared.owner_connected);
    EXPECT_TRUE(source.launch_phase == shared.launch_phase);
    EXPECT_TRUE(source.owner_steam_id == shared.owner_steam_id);
    EXPECT_STR_CONTAINS(source.connect, "127.0.0.1");

    EXPECT_TRUE(GBE_GetDotaReconnectContext(&ctx));
    EXPECT_TRUE(ctx.generation == 77ull);
    EXPECT_TRUE(ctx.lobby_id == 67890ull);
    EXPECT_TRUE(ctx.server_id == 12345);
    EXPECT_TRUE(ctx.lobby_state == 2);
    EXPECT_TRUE(ctx.custom_game_id == 500ull);
    EXPECT_TRUE(ctx.owner_steam_id == 76561198000000000ULL);
    EXPECT_STR_CONTAINS(ctx.connect, "127.0.0.1");

    shared_store.clear();
    const auto empty_source = gbe::dota_reconnect::source_from_shared_lobby_snapshot(
        shared_store.snapshot());
    EXPECT_FALSE(empty_source.valid);
    EXPECT_FALSE(empty_source.active);
    EXPECT_TRUE(empty_source.connect.empty());
}

TEST_CASE(test_get_dota_reconnect_context_priority_and_fallback)
{
    auto &shared_store = GBE_GetSharedDotaLobbyStateStore();
    shared_store.clear();
    GBE_ClearRecentDotaReconnectContext();

    GBE_DotaReconnectContext recent{};
    recent.generation = 22ull;
    recent.lobby_id = 2001ull;
    recent.server_id = 2002ull;
    recent.lobby_state = 2u;
    recent.game_state = 2u;
    recent.custom_game_id = 2003ull;
    recent.owner_steam_id = 2004ull;
    std::strncpy(recent.connect, "10.2.0.1:27015 10.2.0.2:27015", sizeof(recent.connect) - 1);
    GBE_SetRecentDotaReconnectContext(recent);

    GBE_SharedDotaLobbyState shared;
    shared.valid = true;
    shared.active = true;
    shared.generation = 11ull;
    shared.lobby_id = 1001ull;
    shared.server_id = 1002ull;
    shared.state = 2u;
    shared.game_state = 2u;
    shared.custom_game.game_id = 1003ull;
    shared.owner_steam_id = 1004ull;
    shared.connect = "10.1.0.1:27015 10.1.0.2:27015";
    shared_store.publish(shared);

    GBE_DotaReconnectContext selected{};
    EXPECT_TRUE(GBE_GetDotaReconnectContext(&selected));
    EXPECT_TRUE(selected.generation == 11ull);
    EXPECT_TRUE(selected.lobby_id == 1001ull);
    EXPECT_TRUE(selected.server_id == 1002ull);
    EXPECT_TRUE(selected.custom_game_id == 1003ull);
    EXPECT_TRUE(selected.owner_steam_id == 1004ull);
    EXPECT_STR_EQ("10.1.0.1:27015", selected.connect);

    shared.active = false;
    shared_store.publish(shared);
    EXPECT_TRUE(GBE_GetDotaReconnectContext(&selected));
    EXPECT_TRUE(selected.generation == 22ull);
    EXPECT_TRUE(selected.lobby_id == 2001ull);
    EXPECT_TRUE(selected.server_id == 2002ull);
    EXPECT_TRUE(selected.custom_game_id == 2003ull);
    EXPECT_TRUE(selected.owner_steam_id == 2004ull);
    EXPECT_STR_EQ("10.2.0.1:27015", selected.connect);

    GBE_ClearRecentDotaReconnectContext();
    shared_store.clear();
}

// =====================================================================
// Test: shared lobby Store snapshots
// =====================================================================

TEST_CASE(test_shared_lobby_store_snapshot)
{
    auto &shared_store = GBE_GetSharedDotaLobbyStateStore();
    shared_store.clear();

    GBE_SharedDotaLobbyState state;
    state.valid = true;
    state.active = true;
    state.generation = 0x9903ull;
    state.lobby_id = 0x1234ull;
    state.generic_lobby_id = 0x5678ull;
    state.state = 2u;
    state.game_state = 3u;
    state.custom_game.game_id = 12345ull;
    shared_store.publish(state);

    const auto snapshot = shared_store.snapshot();
    EXPECT_TRUE(snapshot.valid);
    EXPECT_TRUE(snapshot.active);
    EXPECT_TRUE(snapshot.generation == 0x9903ull);
    EXPECT_TRUE(snapshot.lobby_id == 0x1234ull);
    EXPECT_TRUE(snapshot.generic_lobby_id == 0x5678ull);
    EXPECT_TRUE(snapshot.state == 2u);
    EXPECT_TRUE(snapshot.game_state == 3u);
    EXPECT_TRUE(snapshot.custom_game.game_id == 12345ull);
    EXPECT_TRUE(snapshot.valid && snapshot.active && snapshot.custom_game.game_id != 0ull);
}

TEST_CASE(test_shared_lobby_store_clear_property)
{
    auto &shared_store = GBE_GetSharedDotaLobbyStateStore();
    for (std::uint64_t seed = 1u; seed <= 64u; ++seed) {
        GBE_SharedDotaLobbyState state;
        state.valid = true;
        state.active = true;
        state.generation = seed;
        state.lobby_id = seed * 100u + 1u;
        state.generic_lobby_id = seed * 100u + 2u;
        shared_store.publish(state);

        shared_store.clear();

        const auto snapshot = shared_store.snapshot();
        EXPECT_FALSE(snapshot.valid);
        EXPECT_FALSE(snapshot.active);
        EXPECT_TRUE(snapshot.generation == 0ull);
        EXPECT_TRUE(snapshot.lobby_id == 0ull);
        EXPECT_TRUE(snapshot.generic_lobby_id == 0ull);
    }
}

// =====================================================================
// Test: GBE_DotaCustomGameDisplayName
// =====================================================================

TEST_CASE(test_dota_custom_game_display_name)
{
    GBE_DotaCustomGameDetails cg{};
    cg.game_id = 12345;
    std::string result = GBE_DotaCustomGameDisplayName(nullptr, cg, "fallback");
    EXPECT_TRUE(!result.empty());

    GBE_DotaCustomGameDetails cg_empty{};
    std::string result2 = GBE_DotaCustomGameDisplayName(nullptr, cg_empty, "fallback_value");
    EXPECT_STR_EQ("fallback_value", result2);
}

// =====================================================================
// Test: GBE_RewriteAccountIdVarintInDirectProtoBody
// =====================================================================

TEST_CASE(test_rewrite_account_id_varint)
{
    // Build a proper protobuf-framed GC message:
    // [u32 emsg][u32 header_len=0][body: field 1 varint = 12345]
    std::string message;
    uint32_t emsg = 4006u; // ClientHello
    uint32_t header_len = 0;
    message.append(reinterpret_cast<const char *>(&emsg), 4);
    message.append(reinterpret_cast<const char *>(&header_len), 4);
    // Body: field 1 (tag 0x08), varint 12345 (0xB9 0x60)
    message.push_back(static_cast<char>(0x08));
    message.push_back(static_cast<char>(0xB9));
    message.push_back(static_cast<char>(0x60));

    uint32 new_account_id = 54321;
    size_t replacement_count = 0;
    bool result = GBE_RewriteAccountIdVarintInDirectProtoBody(message, new_account_id, replacement_count);

    // Function should succeed (valid frame) even if no varint matched the old account_id pattern
    EXPECT_TRUE(result);

    // Empty message should fail (too small for header)
    std::string empty_body;
    size_t count2 = 0;
    EXPECT_FALSE(GBE_RewriteAccountIdVarintInDirectProtoBody(empty_body, new_account_id, count2));
}

// =====================================================================
// Test: GBE_TryPatchDotaAccountIdVarint
// =====================================================================

TEST_CASE(test_try_patch_dota_account_id_varint)
{
    std::string body;
    body.push_back(static_cast<char>(0x08));
    body.append(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size());
    std::string message = make_direct_proto_message(7009u | GBE_kProtoMask, body);

    bool result = GBE_TryPatchDotaAccountIdVarint(
        message, 54321u, "TEST", 7009u, 7009u,
        message.size() - 8, "test_patch_varint");

    EXPECT_TRUE(result);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size())) == std::string::npos);
    std::string encoded_account;
    gbe::proto_wire::append_varuint(encoded_account, 54321u);
    EXPECT_TRUE(message.find(encoded_account) != std::string::npos);
}

// =====================================================================
// Test: GBE_TryPatchDotaAccountIdFixed32
// =====================================================================

TEST_CASE(test_try_patch_dota_account_id_fixed32)
{
    std::string message(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdFixed32.data()), GBE_kOldDotaAccountIdFixed32.size());

    bool result = GBE_TryPatchDotaAccountIdFixed32(message, 54321u, "TEST");
    EXPECT_TRUE(result);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdFixed32.data()), GBE_kOldDotaAccountIdFixed32.size())) == std::string::npos);
    uint32_t patched_account = 0;
    std::memcpy(&patched_account, message.data(), sizeof(patched_account));
    EXPECT_TRUE(patched_account == 54321u);
}

// =====================================================================
// Test: GBE_PatchDotaLobbyTemplateIdentifiers
// =====================================================================

TEST_CASE(test_patch_dota_lobby_template_identifiers)
{
    std::string message;
    message.append(reinterpret_cast<const char *>(GBE_kOldDotaLobbyIdVarint.data()), GBE_kOldDotaLobbyIdVarint.size());
    message.append(make_fixed64_bytes(76561198000000000ULL));

    bool result = GBE_PatchDotaLobbyTemplateIdentifiers(
        message, 54321u, 76561198000000000ULL, 12345ULL);

    EXPECT_FALSE(result);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaLobbyIdVarint.data()), GBE_kOldDotaLobbyIdVarint.size())) != std::string::npos);
    EXPECT_TRUE(message.find(make_fixed64_bytes(76561198000000000ULL)) != std::string::npos);

    std::string exact_message = "prefix:";
    exact_message.append(reinterpret_cast<const char *>(GBE_kOldDotaLobbyIdVarint.data()), GBE_kOldDotaLobbyIdVarint.size());
    exact_message.append(":middle:");
    exact_message.append(reinterpret_cast<const char *>(GBE_kOldDotaSteamIdFixed64.data()), GBE_kOldDotaSteamIdFixed64.size());
    exact_message.append(":suffix");

    std::string expected = "prefix:";
    std::string encoded_lobby_id;
    gbe::proto_wire::append_varuint(encoded_lobby_id, 0x11223344556677ULL);
    EXPECT_TRUE(encoded_lobby_id.size() == GBE_kOldDotaLobbyIdVarint.size());
    expected.append(encoded_lobby_id);
    expected.append(":middle:");
    expected.append(make_fixed64_bytes(0x1122334455667788ULL));
    expected.append(":suffix");

    EXPECT_TRUE(GBE_PatchDotaLobbyTemplateIdentifiers(
        exact_message, 54321u, 0x1122334455667788ULL, 0x11223344556677ULL));
    EXPECT_TRUE(exact_message == expected);
}

// =====================================================================
// Test: GBE_PatchDotaTemplateIdentifiers (9-param version)
// =====================================================================

TEST_CASE(test_patch_dota_template_identifiers)
{
    std::string message;
    message.append(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size());
    message.append(make_fixed32_bytes(34567u));
    message.append(reinterpret_cast<const char *>(GBE_kOldDotaSteamIdVarint.data()), GBE_kOldDotaSteamIdVarint.size());

    bool result = GBE_PatchDotaTemplateIdentifiers(
        message, 54321u, 76561198000000000ULL,
        true, true, 7009u, 7009u,
        message.size() - 8, "test_template_patch");

    EXPECT_TRUE(result);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size())) != std::string::npos);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaSteamIdVarint.data()), GBE_kOldDotaSteamIdVarint.size())) == std::string::npos);
    EXPECT_TRUE(message.find(make_fixed32_bytes(34567u)) != std::string::npos);
}

// =====================================================================
// Test: GBE_ForceDotaLobbyUpdateOwnerSOID
// =====================================================================

TEST_CASE(test_force_dota_lobby_update_owner_soid)
{
    std::string message;
    // Build a minimal protobuf-framed message with header
    // [u32 emsg][u32 header_len=0][body]
    uint32_t emsg = 0;
    uint32_t header_len = 0;
    message.append(reinterpret_cast<const char *>(&emsg), 4);
    message.append(reinterpret_cast<const char *>(&header_len), 4);
    // Body: minimal SOMultipleObjects protobuf (just an empty message)
    message.push_back('\0');

    const std::string original = message;
    bool result = GBE_ForceDotaLobbyUpdateOwnerSOID(message, 12345ULL);
    EXPECT_TRUE(result);
    EXPECT_TRUE(message.size() >= original.size());
}

// =====================================================================
// Test: GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage
// =====================================================================

TEST_CASE(test_prepare_dota_practice_lobby_launch_peripheral)
{
    std::string message;
    const char *template_hex = "0800";

    bool result = GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(
        template_hex,
        76561198000000000ULL,
        12345ULL,
        54321ULL,
        true,
        message);

    EXPECT_FALSE(result);
    EXPECT_TRUE(!message.empty());
}

// =====================================================================
// Test: GBE_PrepareDotaPersonaStatePeripheralMessage
// =====================================================================

TEST_CASE(test_prepare_dota_persona_state_peripheral)
{
    std::string message;
    const char *template_hex = "0800";

    bool result = GBE_PrepareDotaPersonaStatePeripheralMessage(
        template_hex,
        76561198000000000ULL,
        12345ULL,
        message);

    EXPECT_FALSE(result);
    EXPECT_TRUE(!message.empty());
}

// =====================================================================
// Test: GBE_AdaptDotaJoinChatChannelResponsePayload
// =====================================================================

TEST_CASE(test_adapt_dota_join_chat_channel_response)
{
    std::vector<GBE_DotaLobbyMemberState> channel_members;
    std::string message;

    bool result = GBE_AdaptDotaJoinChatChannelResponsePayload(
        76561198000000000ULL,
        12345ULL,
        67890ULL,
        std::string("test_channel"),
        std::string("TestPlayer"),
        channel_members,
        76561198000000001ULL,
        std::string("Owner"),
        1u,
        message);

    EXPECT_TRUE(result);
    EXPECT_TRUE(!message.empty());
    EXPECT_TRUE(message.find("test_channel") != std::string::npos);
    EXPECT_TRUE(message.find("TestPlayer") != std::string::npos);
    EXPECT_TRUE(message.find("test_channel") != std::string::npos || message.find("TestPlayer") != std::string::npos);
}

// =====================================================================
// Test: GBE_LogDotaSOCacheSubscribedSummary (smoke test)
// =====================================================================

TEST_CASE(test_log_dota_socache_subscribed_summary)
{
    std::string message;
    for (int i = 0; i < 32; ++i)
        message.push_back(static_cast<char>(i));

    GBE_LogDotaSOCacheSubscribedSummary("TEST", "test_label", message);
    EXPECT_TRUE(message.size() == 32u);
}

// =====================================================================
// Test: GBE_LogDotaResponsePacket (smoke test)
// =====================================================================

TEST_CASE(test_log_dota_response_packet)
{
    std::string inner_payload = "08011204test";
    std::string outbound_payload = "08021204test";

    GBE_LogDotaResponsePacket("test_reason", 7009u, false,
        inner_payload, outbound_payload,
        12345ULL, 2u, 0u);
    EXPECT_TRUE(!inner_payload.empty());
    EXPECT_TRUE(!outbound_payload.empty());
}

// =====================================================================
// Test: Const data tables
// =====================================================================

TEST_CASE(test_const_data_tables)
{
    EXPECT_TRUE(GBE_kOldDotaAccountIdVarint.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaSteamIdVarint.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaLobbyIdVarint.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaSteamIdFixed64.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaAccountIdFixed32.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaPracticeLobbyMatchIdVarint.size() > 0);
    EXPECT_TRUE(GBE_kOldDotaPracticeLobbyServerIdFixed64.size() > 0);

    EXPECT_TRUE(GBE_kOldDotaPracticeLobbyLobbyIdText != nullptr);
    EXPECT_TRUE(strlen(GBE_kOldDotaPracticeLobbyLobbyIdText) > 0);
    EXPECT_TRUE(GBE_kOldDotaPracticeLobbyLobbyIdTextAlt != nullptr);
    EXPECT_TRUE(strlen(GBE_kOldDotaPracticeLobbyLobbyIdTextAlt) > 0);

    EXPECT_TRUE(GBE_kSteamTicketAuthComplete == 5429u);
    EXPECT_TRUE(GBE_kDotaAbandonPersonaStateInitHex != nullptr);
}

// =====================================================================
// Test: GBE_ExtractDotaHelloContext
// =====================================================================

TEST_CASE(test_extract_dota_hello_context)
{
    GBE_DotaHelloContext ctx{};
    std::string inner_body;
    gbe::proto_wire::append_varint_field(inner_body, 1, 123u);
    std::string inner_payload = make_direct_proto_message(GBE_kEMsgGCClientHello | GBE_kProtoMask, inner_body);
    std::string outer_header;
    const std::string session_raw("session-token");
    gbe::proto_wire::append_bytes_field(outer_header, 2u, session_raw);
    std::string outer_body;
    gbe::proto_wire::append_bytes_field(outer_body, 3u, inner_payload);
    std::string payload = make_direct_proto_message(GBE_kEMsgClientToGC | GBE_kProtoMask, outer_body, outer_header);

    bool result = GBE_ExtractDotaHelloContext(
        payload.data(), static_cast<uint32>(payload.size()), ctx);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ctx.valid);
    EXPECT_TRUE(ctx.version == 123u);
    EXPECT_TRUE(ctx.outer_session_field_raw == session_raw);
}

// =====================================================================
// Test: GBE_ExtractDirectDotaHelloContext
// =====================================================================

TEST_CASE(test_extract_direct_dota_hello_context)
{
    GBE_DotaHelloContext ctx{};
    std::string body;
    gbe::proto_wire::append_varint_field(body, 1, 456u);
    std::string payload = make_direct_proto_message(GBE_kEMsgGCClientHello | GBE_kProtoMask, body);

    bool result = GBE_ExtractDirectDotaHelloContext(
        GBE_kEMsgGCClientHello | GBE_kProtoMask, payload.data(), static_cast<uint32>(payload.size()), ctx);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ctx.valid);
    EXPECT_TRUE(ctx.version == 456u);
}

// =====================================================================
// Test: GBE_ExtractDirectDotaServerHelloContext
// =====================================================================

TEST_CASE(test_extract_direct_dota_server_hello_context)
{
    GBE_DotaServerHelloContext ctx{};
    std::string body;
    gbe::proto_wire::append_varint_field(body, 1, 789u);
    std::string payload = make_direct_proto_message(GBE_kEMsgGCServerHello | GBE_kProtoMask, body);

    bool result = GBE_ExtractDirectDotaServerHelloContext(
        GBE_kEMsgGCServerHello | GBE_kProtoMask, payload.data(), static_cast<uint32>(payload.size()), ctx);
    EXPECT_FALSE(result);
    EXPECT_FALSE(ctx.valid);
}

// =====================================================================
// Test: GBE_BuildDirectDotaClientWelcome
// =====================================================================

TEST_CASE(test_build_direct_dota_client_welcome)
{
    GBE_DotaHelloContext hello_ctx{};
    hello_ctx.valid = true;
    hello_ctx.version = 1682;

    std::string message;
    bool result = GBE_BuildDirectDotaClientWelcome(
        76561198000000000ULL, 570u, 12345u, hello_ctx, message);
    EXPECT_TRUE(result);
    EXPECT_TRUE(message.size() > sizeof(ProtoBufMsgHeader_t));
    uint32_t emsg = 0;
    std::memcpy(&emsg, message.data(), sizeof(emsg));
    EXPECT_TRUE(GBE_GC_MaskedEMsg(emsg) == GBE_kEMsgGCClientWelcome);
}

// =====================================================================
// Test: GBE_ComposeDotaClientWelcome
// =====================================================================

TEST_CASE(test_compose_dota_client_welcome)
{
    GBE_DotaHelloContext hello_ctx{};
    hello_ctx.valid = true;
    hello_ctx.version = 1682;
    hello_ctx.outer_session_field_raw = "session-token";

    std::string message;
    bool result = GBE_ComposeDotaClientWelcome(
        76561198000000000ULL, 570u, 12345u, hello_ctx, message);
    EXPECT_TRUE(result);
    EXPECT_TRUE(message.size() > 8u);
    uint32_t outer_emsg = 0;
    std::memcpy(&outer_emsg, message.data(), sizeof(outer_emsg));
    EXPECT_TRUE(GBE_GC_MaskedEMsg(outer_emsg) == GBE_kEMsgClientFromGC);
    EXPECT_TRUE(message.find("session-token") != std::string::npos);
}

// =====================================================================
// Test: GBE_BuildDirectDotaServerWelcome
// =====================================================================

TEST_CASE(test_build_direct_dota_server_welcome)
{
    GBE_DotaServerHelloContext ctx{};
    ctx.valid = true;
    ctx.active_version = 2345;
    ctx.min_allowed_version = 1234;

    std::string message;
    bool result = GBE_BuildDirectDotaServerWelcome(
        76561198000000000ULL, 570u, ctx, message);
    EXPECT_TRUE(result);
    EXPECT_TRUE(!message.empty());
}

// =====================================================================
// Test: GBE_IsDotaOtherLeftChannelPayloadForChannel
// =====================================================================

TEST_CASE(test_is_dota_other_left_channel_payload)
{
    std::string body;
    body.push_back(static_cast<char>(0x09));
    body.append(make_fixed64_bytes(12345ULL));
    std::string message = make_direct_proto_message(GBE_kDotaOtherLeftChannel | GBE_kProtoMask, body);

    bool result = GBE_IsDotaOtherLeftChannelPayloadForChannel(message, 12345ULL);
    EXPECT_TRUE(result);
    EXPECT_FALSE(GBE_IsDotaOtherLeftChannelPayloadForChannel(message, 54321ULL));
}

// =====================================================================
// Test: GBE_AdaptDotaTopCustomGamesListPayload
// =====================================================================

TEST_CASE(test_adapt_dota_top_custom_games_list_payload)
{
    std::string message;
    size_t game_count = 0;

    bool result = GBE_AdaptDotaTopCustomGamesListPayload(nullptr, message, game_count);
    EXPECT_TRUE(result);
    EXPECT_TRUE(game_count == 0u);
    EXPECT_TRUE(!message.empty());
}

// =====================================================================
// Test: GBE_PrepareDotaDirectReplayMessage
// =====================================================================

TEST_CASE(test_prepare_dota_direct_replay_message)
{
    std::string body;
    body.append(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size());
    body.append(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdFixed32.data()), GBE_kOldDotaAccountIdFixed32.size());
    body.append(reinterpret_cast<const char *>(GBE_kOldDotaSteamIdVarint.data()), GBE_kOldDotaSteamIdVarint.size());
    std::string template_message = make_direct_proto_message(7009u | GBE_kProtoMask, body);

    std::string message;
    bool result = GBE_PrepareDotaDirectReplayMessage(
        reinterpret_cast<const uint8 *>(template_message.data()), template_message.size(),
        54321u, 76561198000000000ULL,
        true, true, false, 0ULL,
        7009u, 7009u, 0, "test_direct_replay", message);

    EXPECT_TRUE(result);
    EXPECT_TRUE(message.size() >= 8u);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaAccountIdVarint.data()), GBE_kOldDotaAccountIdVarint.size())) != std::string::npos);
    EXPECT_TRUE(message.find(std::string(reinterpret_cast<const char *>(GBE_kOldDotaSteamIdVarint.data()), GBE_kOldDotaSteamIdVarint.size())) == std::string::npos);
}

// =====================================================================
// Test: GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState
// =====================================================================

TEST_CASE(test_patch_dota_practice_lobby_cache_subscribed_template_state)
{
    std::string message;
    for (int i = 0; i < 16; ++i)
        message.push_back(static_cast<char>(i));

    std::vector<GBE_DotaLobbyMemberState> members;
    std::string pass_key;
    const std::string original = message;

    bool result = GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message, 54321u, 76561198000000000ULL, 12345ULL,
        true, 2u, 0u, 54321ULL, 99999ULL, 0u,
        std::string("127.0.0.1:27015"), std::string("TestPlayer"),
        std::string("TestRoom"), 1u, 0u, false,
        std::string(""), false, false, false,
        0u, 0u, 0u, 0ULL, 0ULL, 0u, 0u, 0u,
        members, false, 0u, pass_key, nullptr);
    EXPECT_FALSE(result);
    EXPECT_TRUE(message == original);
}

// =====================================================================
// Test: GBE_PatchDotaPracticeLobbyLaunchTemplate
// =====================================================================

TEST_CASE(test_patch_dota_practice_lobby_launch_template)
{
    std::string message;
    for (int i = 0; i < 16; ++i)
        message.push_back(static_cast<char>(i));
    const std::string original = message;

    bool result = GBE_PatchDotaPracticeLobbyLaunchTemplate(
        message, 54321u, 76561198000000000ULL, 12345ULL,
        54321ULL, 99999ULL, 0u,
        std::string("127.0.0.1:27015"),
        true, true, true, "test_launch");
    EXPECT_TRUE(result);
    EXPECT_TRUE(message.size() >= original.size());
}

// =====================================================================
// Test: GBE_ParseDotaEquipOps
// =====================================================================

TEST_CASE(test_parse_dota_equip_ops)
{
    // Build a body with 2 equip ops:
    // field 1 (tag 0x0a), sub-message containing:
    //   field 1 (varint) = 123456789, field 2 (varint) = 1, field 3 (varint) = 0
    std::vector<uint8> body;

    // Op 1: item_id=123456789, class=1, slot=0, style=3
    {
        std::string sub;
        gbe::proto_wire::append_varint_field(sub, 1, 123456789ULL);
        gbe::proto_wire::append_varint_field(sub, 2, 1u);
        gbe::proto_wire::append_varint_field(sub, 3, 0u);
        gbe::proto_wire::append_varint_field(sub, 4, 3u);
        body.push_back(0x0a);
        body.push_back(static_cast<uint8>(sub.size()));
        body.insert(body.end(), sub.begin(), sub.end());
    }

    // Op 2: item_id=987654321, class=2, slot=1, no style (default 255)
    {
        std::string sub;
        gbe::proto_wire::append_varint_field(sub, 1, 987654321ULL);
        gbe::proto_wire::append_varint_field(sub, 2, 2u);
        gbe::proto_wire::append_varint_field(sub, 3, 1u);
        body.push_back(0x0a);
        body.push_back(static_cast<uint8>(sub.size()));
        body.insert(body.end(), sub.begin(), sub.end());
    }

    std::vector<GBE_DotaEquipOp> ops;
    bool result = GBE_ParseDotaEquipOps(body.data(), body.size(), ops);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ops.size() == 2);

    // Op 1
    EXPECT_TRUE(ops[0].item_id == 123456789ULL);
    EXPECT_TRUE(ops[0].new_class == 1u);
    EXPECT_TRUE(ops[0].new_slot == 0u);
    EXPECT_TRUE(ops[0].style_index == 3u);

    // Op 2 (style should default to 255)
    EXPECT_TRUE(ops[1].item_id == 987654321ULL);
    EXPECT_TRUE(ops[1].new_class == 2u);
    EXPECT_TRUE(ops[1].new_slot == 1u);
    EXPECT_TRUE(ops[1].style_index == 255u);

    // Empty body
    std::vector<GBE_DotaEquipOp> empty_ops;
    EXPECT_FALSE(GBE_ParseDotaEquipOps(nullptr, 0, empty_ops));

    // Malformed varint should fail instead of overflowing shifts.
    std::vector<uint8> malformed_varint = { 0x0a, 0x0b };
    for (int i = 0; i < 11; ++i)
        malformed_varint.push_back(0x80);
    EXPECT_FALSE(GBE_ParseDotaEquipOps(malformed_varint.data(), malformed_varint.size(), empty_ops));

    // Truncated length-delimited sub-message should fail.
    std::vector<uint8> truncated = { 0x0a, 0x05, 0x08, 0x01 };
    EXPECT_FALSE(GBE_ParseDotaEquipOps(truncated.data(), truncated.size(), empty_ops));

    // Missing required new_slot should fail rather than producing a default op.
    std::string missing_slot_sub;
    gbe::proto_wire::append_varint_field(missing_slot_sub, 1, 123ULL);
    gbe::proto_wire::append_varint_field(missing_slot_sub, 2, 1u);
    std::vector<uint8> missing_slot = { 0x0a, static_cast<uint8>(missing_slot_sub.size()) };
    missing_slot.insert(missing_slot.end(), missing_slot_sub.begin(), missing_slot_sub.end());
    EXPECT_FALSE(GBE_ParseDotaEquipOps(missing_slot.data(), missing_slot.size(), empty_ops));

    // Class/slot are stored as uint16 in Econ_Item and must reject narrowing.
    std::string class_overflow_sub;
    gbe::proto_wire::append_varint_field(class_overflow_sub, 1, 123ULL);
    gbe::proto_wire::append_varint_field(class_overflow_sub, 2, 70000u);
    gbe::proto_wire::append_varint_field(class_overflow_sub, 3, 1u);
    std::vector<uint8> class_overflow = { 0x0a, static_cast<uint8>(class_overflow_sub.size()) };
    class_overflow.insert(class_overflow.end(), class_overflow_sub.begin(), class_overflow_sub.end());
    EXPECT_FALSE(GBE_ParseDotaEquipOps(class_overflow.data(), class_overflow.size(), empty_ops));

    std::string slot_overflow_sub;
    gbe::proto_wire::append_varint_field(slot_overflow_sub, 1, 123ULL);
    gbe::proto_wire::append_varint_field(slot_overflow_sub, 2, 1u);
    gbe::proto_wire::append_varint_field(slot_overflow_sub, 3, 70000u);
    std::vector<uint8> slot_overflow = { 0x0a, static_cast<uint8>(slot_overflow_sub.size()) };
    slot_overflow.insert(slot_overflow.end(), slot_overflow_sub.begin(), slot_overflow_sub.end());
    EXPECT_FALSE(GBE_ParseDotaEquipOps(slot_overflow.data(), slot_overflow.size(), empty_ops));

    std::string style_overflow_sub;
    gbe::proto_wire::append_varint_field(style_overflow_sub, 1, 123ULL);
    gbe::proto_wire::append_varint_field(style_overflow_sub, 2, 1u);
    gbe::proto_wire::append_varint_field(style_overflow_sub, 3, 1u);
    gbe::proto_wire::append_varint_field(style_overflow_sub, 4, 300u);
    std::vector<uint8> style_overflow = { 0x0a, static_cast<uint8>(style_overflow_sub.size()) };
    style_overflow.insert(style_overflow.end(), style_overflow_sub.begin(), style_overflow_sub.end());
    EXPECT_FALSE(GBE_ParseDotaEquipOps(style_overflow.data(), style_overflow.size(), empty_ops));
}

// =====================================================================
// Test: GBE_ApplyDotaUnlockStyleBitmask
// =====================================================================

TEST_CASE(test_apply_dota_unlock_style_bitmask)
{
    // Case 1: Item without attr 400 -> creates with all bits set
    Econ_Item item1{};
    item1.id = 12345;
    bool result1 = GBE_ApplyDotaUnlockStyleBitmask(item1, 2u);
    EXPECT_TRUE(result1);
    EXPECT_TRUE(item1.style == 2);
    EXPECT_TRUE(item1.attributes.size() == 1);
    EXPECT_TRUE(item1.attributes[0].def == 400u);

    // Verify all bits set (0xFFFFFFFF)
    uint32_t val = 0;
    memcpy(&val, item1.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val == 0xFFFFFFFFu);

    // Case 2: Item with existing attr 400 -> OR in new bit
    Econ_Item item2{};
    item2.id = 67890;
    // Pre-set attr 400 with value 0x00000001 (bit 0 set)
    Econ_Item_Attribute attr;
    attr.def = 400u;
    uint32_t initial_val = 0x00000001u;
    attr.value_bytes.assign(reinterpret_cast<const char *>(&initial_val), 4);
    attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
    item2.attributes.push_back(attr);

    bool result2 = GBE_ApplyDotaUnlockStyleBitmask(item2, 3u);
    EXPECT_TRUE(result2);
    EXPECT_TRUE(item2.style == 3);
    EXPECT_TRUE(item2.attributes.size() == 1); // no new attr added

    // Verify bit 3 was OR-ed in: 0x00000001 | 0x00000008 = 0x00000009
    uint32_t val2 = 0;
    memcpy(&val2, item2.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val2 == 0x00000009u);

    // Case 3: Second unlock on same item should accumulate bits
    GBE_ApplyDotaUnlockStyleBitmask(item2, 5u);
    uint32_t val3 = 0;
    memcpy(&val3, item2.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val3 == 0x00000029u); // 0x09 | (1<<5) = 0x29

    // Invalid style indexes must fail without changing style or bitmask.
    EXPECT_FALSE(GBE_ApplyDotaUnlockStyleBitmask(item2, 32u));
    EXPECT_TRUE(item2.style == 5);
    uint32_t val4 = 0;
    memcpy(&val4, item2.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val4 == 0x00000029u);

    EXPECT_FALSE(GBE_ApplyDotaUnlockStyleBitmask(item2, 255u));
    EXPECT_TRUE(item2.style == 5);

    // Highest valid style index should set bit 31 and remain accepted.
    Econ_Item item3{};
    item3.id = 24680;
    bool result3 = GBE_ApplyDotaUnlockStyleBitmask(item3, 31u);
    EXPECT_TRUE(result3);
    EXPECT_TRUE(item3.style == 31);
    EXPECT_TRUE(item3.attributes.size() == 1);
    uint32_t val5 = 0;
    memcpy(&val5, item3.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val5 == 0xFFFFFFFFu);

    // Short attr 400 data should be treated as zero before OR-ing the bit.
    Econ_Item item4{};
    item4.id = 13579;
    Econ_Item_Attribute short_attr;
    short_attr.def = 400u;
    short_attr.value_bytes.assign(2, '\0');
    short_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
    item4.attributes.push_back(short_attr);
    bool result4 = GBE_ApplyDotaUnlockStyleBitmask(item4, 4u);
    EXPECT_TRUE(result4);
    EXPECT_TRUE(item4.style == 4);
    EXPECT_TRUE(item4.attributes.size() == 1);
    uint32_t val6 = 0;
    memcpy(&val6, item4.attributes[0].value_bytes.data(), 4);
    EXPECT_TRUE(val6 == 0x00000010u);
}

// =====================================================================
// Test: GBE_BuildSOSingleObjectFromItem
// =====================================================================

TEST_CASE(test_build_so_single_object_from_item)
{
    Econ_Item item{};
    item.id = 0x5000000100000001ULL;
    item.def = 1234;
    item.level = 5;
    item.quality = static_cast<EItemQuality>(4);
    item.inv_pos = 3;
    item.quantity = 1;
    item.flags = 0;
    item.origin = 2;
    item.in_use = false;
    item.original_id = 0x5000000100000001ULL;
    item.style = 0;
    item.custom_name = "custom name";
    item.custom_desc = "custom desc";
    item.equip_states.insert_or_assign(1, 2);
    Econ_Item_Attribute attr;
    attr.def = 400u;
    uint32_t attr_value = 0x00000003u;
    attr.value_bytes.assign(reinterpret_cast<const char *>(&attr_value), 4);
    attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
    item.attributes.push_back(attr);

    CSteamID steam_id(76561198000000000ULL);
    std::string output;
    bool result = GBE_BuildSOSingleObjectFromItem(item, steam_id, output);
    EXPECT_TRUE(result);
    EXPECT_TRUE(!output.empty());

    auto contains_u64 = [](const std::string &data, uint64_t value) {
        const char *needle = reinterpret_cast<const char *>(&value);
        return data.find(std::string(needle, sizeof(value))) != std::string::npos;
    };
    auto contains_u32 = [](const std::string &data, uint32_t value) {
        const char *needle = reinterpret_cast<const char *>(&value);
        return data.find(std::string(needle, sizeof(value))) != std::string::npos;
    };

    EXPECT_TRUE(contains_u64(output, steam_id.ConvertToUint64()));
    EXPECT_TRUE(contains_u64(output, item.id));
    EXPECT_TRUE(contains_u32(output, item.def));
    EXPECT_TRUE(output.find("custom name") != std::string::npos);
    EXPECT_TRUE(output.find("custom desc") != std::string::npos);
    EXPECT_TRUE(output.find(std::string(reinterpret_cast<const char *>(&attr_value), 4)) != std::string::npos);
}

TEST_CASE(test_parse_dota7034_runtime_request)
{
    const auto empty_request = gbe::proto_wire::parse_dota7034_runtime_request(nullptr, 0);
    EXPECT_FALSE(empty_request.has_connected_player);
    EXPECT_FALSE(empty_request.has_disconnected_player);
    EXPECT_FALSE(empty_request.has_game_state);
    EXPECT_FALSE(empty_request.has_draft);

    std::string connected_player;
    gbe::proto_wire::append_varint_field(connected_player, 1u, 76561198000000001ULL);
    gbe::proto_wire::append_varint_field(connected_player, 2u, 42u);

    std::string leaver_state;
    gbe::proto_wire::append_varint_field(leaver_state, 1u, 3u);
    gbe::proto_wire::append_varint_field(leaver_state, 2u, 10u);

    std::string disconnected_player;
    gbe::proto_wire::append_varint_field(disconnected_player, 1u, 76561198000000002ULL);
    gbe::proto_wire::append_bytes_field(disconnected_player, 3u, leaver_state);

    std::string draft;
    gbe::proto_wire::append_varint_field(draft, 1u, 76561198000000003ULL);
    gbe::proto_wire::append_varint_field(draft, 2u, 2u);
    gbe::proto_wire::append_varint_field(draft, 3u, 4u);

    std::string body;
    gbe::proto_wire::append_bytes_field(body, 1u, connected_player);
    gbe::proto_wire::append_varint_field(body, 2u, 4u);
    gbe::proto_wire::append_varint_field(body, 6u, 1u);
    gbe::proto_wire::append_bytes_field(body, 7u, disconnected_player);
    gbe::proto_wire::append_varint_field(body, 8u, 2u);
    gbe::proto_wire::append_varint_field(body, 11u, 12u);
    gbe::proto_wire::append_varint_field(body, 12u, 9u);
    gbe::proto_wire::append_varint_field(body, 14u, 3u);
    gbe::proto_wire::append_varint_field(body, 15u, 99u);
    gbe::proto_wire::append_bytes_field(body, 16u, draft);

    const auto request = gbe::proto_wire::parse_dota7034_runtime_request(
        reinterpret_cast<const std::uint8_t *>(body.data()),
        body.size());

    EXPECT_TRUE(request.has_connected_player);
    EXPECT_TRUE(request.connected_players.size() == 1u);
    EXPECT_TRUE(request.connected_players[0].has_steam_id);
    EXPECT_EQ(request.connected_players[0].steam_id, 76561198000000001ULL);
    EXPECT_TRUE(request.connected_players[0].has_hero_id);
    EXPECT_EQ(request.connected_players[0].hero_id, 42u);
    EXPECT_TRUE(request.has_disconnected_player);
    EXPECT_TRUE(request.disconnected_players.size() == 1u);
    EXPECT_EQ(request.disconnected_players[0].steam_id, 76561198000000002ULL);
    EXPECT_EQ(request.disconnected_players[0].lobby_state, 3u);
    EXPECT_EQ(request.disconnected_players[0].game_state, 10u);
    EXPECT_TRUE(request.has_game_state);
    EXPECT_EQ(request.game_state, 4u);
    EXPECT_TRUE(request.has_send_reason);
    EXPECT_EQ(request.send_reason, 2u);
    EXPECT_TRUE(request.has_first_blood_happened);
    EXPECT_EQ(request.first_blood_happened, 1u);
    EXPECT_TRUE(request.has_radiant_kills);
    EXPECT_EQ(request.radiant_kills, 12u);
    EXPECT_TRUE(request.has_dire_kills);
    EXPECT_EQ(request.dire_kills, 9u);
    EXPECT_TRUE(request.has_radiant_lead);
    EXPECT_EQ(request.radiant_lead, 3u);
    EXPECT_TRUE(request.has_building_state);
    EXPECT_EQ(request.building_state, 99u);
    EXPECT_TRUE(request.has_draft);
    EXPECT_TRUE(request.has_draft_steam_id);
    EXPECT_EQ(request.draft_steam_id, 76561198000000003ULL);
    EXPECT_TRUE(request.has_draft_team);
    EXPECT_EQ(request.draft_team, 2u);
    EXPECT_TRUE(request.has_draft_team_slot);
    EXPECT_EQ(request.draft_team_slot, 4u);

    const std::string truncated_nested_field{
        static_cast<char>((1u << 3) | 2u),
        static_cast<char>(4u),
        static_cast<char>((1u << 3) | 0u),
        static_cast<char>(0x80u)
    };
    const auto truncated_request = gbe::proto_wire::parse_dota7034_runtime_request(
        reinterpret_cast<const std::uint8_t *>(truncated_nested_field.data()),
        truncated_nested_field.size());
    EXPECT_FALSE(truncated_request.has_connected_player);
    EXPECT_FALSE(truncated_request.has_disconnected_player);
    EXPECT_FALSE(truncated_request.has_game_state);
    EXPECT_FALSE(truncated_request.has_draft);
}

// =====================================================================
// Main entry point
// =====================================================================

int main()
{
    std::printf("=== gbe_dota_gc_payload_helpers_test ===\n\n");

    std::printf("[1/27] CSteamID stub behavior...\n");
    test_csteamid_stub_behavior();

    std::printf("[2/27] Player item cache rebuild version...\n");
    test_player_item_cache_rebuild_advances_cache_version();

    std::printf("[3/27] GBE_DescribeDotaLaunchPhase...\n");
    test_describe_dota_launch_phase();

    std::printf("[3/26] GBE_GetDotaReconnectContext...\n");
    test_get_dota_reconnect_context();
    test_get_dota_reconnect_context_priority_and_fallback();

    std::printf("[4/26] shared lobby Store snapshots...\n");
    test_shared_lobby_store_snapshot();
    test_shared_lobby_store_clear_property();

    std::printf("[5/26] GBE_DotaCustomGameDisplayName...\n");
    test_dota_custom_game_display_name();

    std::printf("[6/26] GBE_RewriteAccountIdVarintInDirectProtoBody...\n");
    test_rewrite_account_id_varint();

    std::printf("[7/26] GBE_TryPatchDotaAccountIdVarint...\n");
    test_try_patch_dota_account_id_varint();

    std::printf("[8/26] GBE_TryPatchDotaAccountIdFixed32...\n");
    test_try_patch_dota_account_id_fixed32();

    std::printf("[9/26] GBE_PatchDotaLobbyTemplateIdentifiers...\n");
    test_patch_dota_lobby_template_identifiers();

    std::printf("[10/26] GBE_PatchDotaTemplateIdentifiers...\n");
    test_patch_dota_template_identifiers();

    std::printf("[11/26] GBE_ForceDotaLobbyUpdateOwnerSOID...\n");
    test_force_dota_lobby_update_owner_soid();

    std::printf("[12/26] GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage...\n");
    test_prepare_dota_practice_lobby_launch_peripheral();

    std::printf("[13/26] GBE_PrepareDotaPersonaStatePeripheralMessage...\n");
    test_prepare_dota_persona_state_peripheral();

    std::printf("[14/26] GBE_AdaptDotaJoinChatChannelResponsePayload...\n");
    test_adapt_dota_join_chat_channel_response();

    std::printf("[15/26] GBE_LogDotaSOCacheSubscribedSummary...\n");
    test_log_dota_socache_subscribed_summary();

    std::printf("[16/26] GBE_LogDotaResponsePacket...\n");
    test_log_dota_response_packet();

    std::printf("[17/26] Const data tables...\n");
    test_const_data_tables();

    std::printf("[18/26] GBE_ExtractDotaHelloContext...\n");
    test_extract_dota_hello_context();

    std::printf("[19/26] GBE_ExtractDirectDotaHelloContext...\n");
    test_extract_direct_dota_hello_context();

    std::printf("[20/26] GBE_ExtractDirectDotaServerHelloContext...\n");
    test_extract_direct_dota_server_hello_context();

    std::printf("[21/26] Hello/Welcome builders...\n");
    test_build_direct_dota_client_welcome();
    test_compose_dota_client_welcome();
    test_build_direct_dota_server_welcome();

    std::printf("[22/26] Payload adaptation functions...\n");
    test_is_dota_other_left_channel_payload();
    test_adapt_dota_top_custom_games_list_payload();
    test_prepare_dota_direct_replay_message();
    test_patch_dota_practice_lobby_cache_subscribed_template_state();
    test_patch_dota_practice_lobby_launch_template();

    std::printf("[23/26] GBE_ParseDotaEquipOps...\n");
    test_parse_dota_equip_ops();

    std::printf("[24/26] GBE_ApplyDotaUnlockStyleBitmask...\n");
    test_apply_dota_unlock_style_bitmask();

    std::printf("[25/26] GBE_BuildSOSingleObjectFromItem...\n");
    test_build_so_single_object_from_item();

    std::printf("[26/26] parse_dota7034_runtime_request...\n");
    test_parse_dota7034_runtime_request();

    std::printf("All payload helper tests complete.\n\n");

    std::printf("Results: %d/%d passed, %d failed\n",
        g_tests_passed, g_tests_run, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}

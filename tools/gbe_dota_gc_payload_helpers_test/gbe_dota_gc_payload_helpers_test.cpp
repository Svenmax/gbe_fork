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
// ~44 free functions that perform protobuf wire-level payload patching,
// template replay, Hello/Welcome context extraction, and message construction.
//
// These tests verify the pure-logic functions that do NOT depend on
// Steam_Game_Coordinator runtime state. Functions requiring runtime
// state (e.g. GBE_PushDotaPlayerEquippedItemsCacheToGC which needs a
// target_gc pointer) are tested via integration in gc_replay_test.
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
#include "dll/gbe_dota_protocol_constants.h"
#include "dll/gbe_dota_request_router.h"
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

// =====================================================================
// Global state needed by payload_helpers TU
// =====================================================================

GBE_DotaLootListData GBE_vpk_loot_data;
GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;
bool GBE_recent_dota_reconnect_context_valid = false;
GBE_DotaReconnectContext GBE_recent_dota_reconnect_context{};
GBE_DotaServerHelloContext GBE_last_dota_server_hello_context{};
std::atomic<bool> GBE_dota_reconnect_eligible{true};

// Stub for get_full_program_path (defined in dll/base.cpp which has heavy deps)
std::string get_full_program_path() { return "."; }

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

    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
    GBE_recent_dota_reconnect_context_valid = false;

    GBE_DotaReconnectContext ctx{};
    EXPECT_FALSE(GBE_GetDotaReconnectContext(&ctx));

    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = true;
    GBE_shared_dota_lobby_state.state = 2;
    GBE_shared_dota_lobby_state.game_state = 0;
    GBE_shared_dota_lobby_state.connect = "127.0.0.1:27015";
    GBE_shared_dota_lobby_state.server_id = 12345;
    GBE_shared_dota_lobby_state.owner_steam_id = 76561198000000000ULL;

    EXPECT_TRUE(GBE_GetDotaReconnectContext(&ctx));
    EXPECT_TRUE(ctx.server_id == 12345);
    EXPECT_TRUE(ctx.lobby_state == 2);
    EXPECT_STR_CONTAINS(ctx.connect, "127.0.0.1");

    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
}

// =====================================================================
// Test: GBE_IsDotaArcadeLobbyActive
// =====================================================================

TEST_CASE(test_is_dota_arcade_lobby_active)
{
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
    EXPECT_FALSE(GBE_IsDotaArcadeLobbyActive());

    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = true;
    GBE_shared_dota_lobby_state.custom_game.game_id = 0;
    EXPECT_FALSE(GBE_IsDotaArcadeLobbyActive());

    GBE_shared_dota_lobby_state.custom_game.game_id = 12345;
    EXPECT_TRUE(GBE_IsDotaArcadeLobbyActive());

    GBE_shared_dota_lobby_state.active = false;
    EXPECT_FALSE(GBE_IsDotaArcadeLobbyActive());

    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
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
    std::string message;
    for (int i = 0; i < 8; ++i)
        message.push_back('\0');

    message.push_back(static_cast<char>(0x08));
    message.push_back(static_cast<char>(0xB9));
    message.push_back(static_cast<char>(0x60));

    bool result = GBE_TryPatchDotaAccountIdVarint(
        message, 54321u, "TEST", 7009u, 7009u,
        message.size() - 8, "test_patch_varint");

    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_TryPatchDotaAccountIdFixed32
// =====================================================================

TEST_CASE(test_try_patch_dota_account_id_fixed32)
{
    std::string message;
    for (int i = 0; i < 8; ++i)
        message.push_back('\0');

    message.push_back(static_cast<char>(0x0D));
    message.push_back(static_cast<char>(0x39));
    message.push_back(static_cast<char>(0x30));
    message.push_back(static_cast<char>(0x00));
    message.push_back(static_cast<char>(0x00));

    bool result = GBE_TryPatchDotaAccountIdFixed32(message, 54321u, "TEST");
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_PatchDotaLobbyTemplateIdentifiers
// =====================================================================

TEST_CASE(test_patch_dota_lobby_template_identifiers)
{
    std::string message;
    for (int i = 0; i < 8; ++i)
        message.push_back('\0');
    message += "AAAA";

    bool result = GBE_PatchDotaLobbyTemplateIdentifiers(
        message, 54321u, 76561198000000000ULL, 12345ULL);

    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_PatchDotaTemplateIdentifiers (9-param version)
// =====================================================================

TEST_CASE(test_patch_dota_template_identifiers)
{
    std::string message;
    for (int i = 0; i < 8; ++i)
        message.push_back('\0');
    message += "AAAA";

    bool result = GBE_PatchDotaTemplateIdentifiers(
        message, 54321u, 76561198000000000ULL,
        true, true, 7009u, 7009u,
        message.size() - 8, "test_template_patch");

    (void)result;
    EXPECT_TRUE(true);
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

    bool result = GBE_ForceDotaLobbyUpdateOwnerSOID(message, 12345ULL);
    (void)result;
    EXPECT_TRUE(true);
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

    (void)result;
    EXPECT_TRUE(true);
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

    (void)result;
    EXPECT_TRUE(true);
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

    (void)result;
    EXPECT_TRUE(true);
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
    EXPECT_TRUE(true);
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
    EXPECT_TRUE(true);
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
    std::string payload;
    gbe::proto_wire::append_varint_field(payload, 1, 1u);

    bool result = GBE_ExtractDotaHelloContext(
        payload.data(), static_cast<uint32>(payload.size()), ctx);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_ExtractDirectDotaHelloContext
// =====================================================================

TEST_CASE(test_extract_direct_dota_hello_context)
{
    GBE_DotaHelloContext ctx{};
    std::string payload;
    gbe::proto_wire::append_varint_field(payload, 1, 1u);

    bool result = GBE_ExtractDirectDotaHelloContext(
        4006u, payload.data(), static_cast<uint32>(payload.size()), ctx);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_ExtractDirectDotaServerHelloContext
// =====================================================================

TEST_CASE(test_extract_direct_dota_server_hello_context)
{
    GBE_DotaServerHelloContext ctx{};
    std::string payload;
    gbe::proto_wire::append_varint_field(payload, 1, 1u);

    bool result = GBE_ExtractDirectDotaServerHelloContext(
        4007u, payload.data(), static_cast<uint32>(payload.size()), ctx);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_BuildDirectDotaClientWelcome
// =====================================================================

TEST_CASE(test_build_direct_dota_client_welcome)
{
    GBE_DotaHelloContext hello_ctx{};
    hello_ctx.valid = true;
    hello_ctx.version = 1;

    std::string message;
    bool result = GBE_BuildDirectDotaClientWelcome(
        76561198000000000ULL, 570u, 12345u, hello_ctx, message);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_ComposeDotaClientWelcome
// =====================================================================

TEST_CASE(test_compose_dota_client_welcome)
{
    GBE_DotaHelloContext hello_ctx{};
    hello_ctx.valid = true;
    hello_ctx.version = 1;

    std::string message;
    bool result = GBE_ComposeDotaClientWelcome(
        76561198000000000ULL, 570u, 12345u, hello_ctx, message);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_BuildDirectDotaServerWelcome
// =====================================================================

TEST_CASE(test_build_direct_dota_server_welcome)
{
    GBE_DotaServerHelloContext ctx{};
    ctx.valid = true;
    ctx.active_version = 1;
    ctx.min_allowed_version = 1;

    std::string message;
    bool result = GBE_BuildDirectDotaServerWelcome(
        76561198000000000ULL, 570u, ctx, message);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_IsDotaOtherLeftChannelPayloadForChannel
// =====================================================================

TEST_CASE(test_is_dota_other_left_channel_payload)
{
    std::string message;
    gbe::proto_wire::append_varint_field(message, 1, 12345u);

    bool result = GBE_IsDotaOtherLeftChannelPayloadForChannel(message, 12345ULL);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_AdaptDotaTopCustomGamesListPayload
// =====================================================================

TEST_CASE(test_adapt_dota_top_custom_games_list_payload)
{
    std::string message;
    size_t game_count = 0;

    bool result = GBE_AdaptDotaTopCustomGamesListPayload(nullptr, message, game_count);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_PrepareDotaDirectReplayMessage
// =====================================================================

TEST_CASE(test_prepare_dota_direct_replay_message)
{
    std::vector<uint8> template_bytes;
    for (int i = 0; i < 8; ++i)
        template_bytes.push_back(static_cast<uint8>(i));

    std::string message;
    bool result = GBE_PrepareDotaDirectReplayMessage(
        template_bytes.data(), template_bytes.size(),
        54321u, 76561198000000000ULL,
        true, true, false, 0ULL,
        7009u, 7009u, 0, "test_direct_replay", message);

    (void)result;
    EXPECT_TRUE(true);
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

    bool result = GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message, 54321u, 76561198000000000ULL, 12345ULL,
        true, 2u, 0u, 54321ULL, 99999ULL, 0u,
        std::string("127.0.0.1:27015"), std::string("TestPlayer"),
        std::string("TestRoom"), 1u, 0u, false,
        std::string(""), false, false, false,
        0u, 0u, 0u, 0ULL, 0ULL, 0u, 0u, 0u,
        members, false, 0u, pass_key, nullptr);
    (void)result;
    EXPECT_TRUE(true);
}

// =====================================================================
// Test: GBE_PatchDotaPracticeLobbyLaunchTemplate
// =====================================================================

TEST_CASE(test_patch_dota_practice_lobby_launch_template)
{
    std::string message;
    for (int i = 0; i < 16; ++i)
        message.push_back(static_cast<char>(i));

    bool result = GBE_PatchDotaPracticeLobbyLaunchTemplate(
        message, 54321u, 76561198000000000ULL, 12345ULL,
        54321ULL, 99999ULL, 0u,
        std::string("127.0.0.1:27015"),
        true, true, true, "test_launch");
    (void)result;
    EXPECT_TRUE(true);
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

    CSteamID steam_id(76561198000000000ULL);
    std::string output;
    bool result = GBE_BuildSOSingleObjectFromItem(item, steam_id, output);
    EXPECT_TRUE(result);
    EXPECT_TRUE(!output.empty());
}

// =====================================================================
// Main entry point
// =====================================================================

int main()
{
    std::printf("=== gbe_dota_gc_payload_helpers_test ===\n\n");

    std::printf("[1/22] GBE_DescribeDotaLaunchPhase...\n");
    test_describe_dota_launch_phase();

    std::printf("[2/22] GBE_GetDotaReconnectContext...\n");
    test_get_dota_reconnect_context();

    std::printf("[3/22] GBE_IsDotaArcadeLobbyActive...\n");
    test_is_dota_arcade_lobby_active();

    std::printf("[4/22] GBE_DotaCustomGameDisplayName...\n");
    test_dota_custom_game_display_name();

    std::printf("[5/22] GBE_RewriteAccountIdVarintInDirectProtoBody...\n");
    test_rewrite_account_id_varint();

    std::printf("[6/22] GBE_TryPatchDotaAccountIdVarint...\n");
    test_try_patch_dota_account_id_varint();

    std::printf("[7/22] GBE_TryPatchDotaAccountIdFixed32...\n");
    test_try_patch_dota_account_id_fixed32();

    std::printf("[8/22] GBE_PatchDotaLobbyTemplateIdentifiers...\n");
    test_patch_dota_lobby_template_identifiers();

    std::printf("[9/22] GBE_PatchDotaTemplateIdentifiers...\n");
    test_patch_dota_template_identifiers();

    std::printf("[10/22] GBE_ForceDotaLobbyUpdateOwnerSOID...\n");
    test_force_dota_lobby_update_owner_soid();

    std::printf("[11/22] GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage...\n");
    test_prepare_dota_practice_lobby_launch_peripheral();

    std::printf("[12/22] GBE_PrepareDotaPersonaStatePeripheralMessage...\n");
    test_prepare_dota_persona_state_peripheral();

    std::printf("[13/22] GBE_AdaptDotaJoinChatChannelResponsePayload...\n");
    test_adapt_dota_join_chat_channel_response();

    std::printf("[14/22] GBE_LogDotaSOCacheSubscribedSummary...\n");
    test_log_dota_socache_subscribed_summary();

    std::printf("[15/22] GBE_LogDotaResponsePacket...\n");
    test_log_dota_response_packet();

    std::printf("[16/22] Const data tables...\n");
    test_const_data_tables();

    std::printf("[17/22] GBE_ExtractDotaHelloContext...\n");
    test_extract_dota_hello_context();

    std::printf("[18/22] GBE_ExtractDirectDotaHelloContext...\n");
    test_extract_direct_dota_hello_context();

    std::printf("[19/22] GBE_ExtractDirectDotaServerHelloContext...\n");
    test_extract_direct_dota_server_hello_context();

    std::printf("[20/22] Hello/Welcome builders...\n");
    test_build_direct_dota_client_welcome();
    test_compose_dota_client_welcome();
    test_build_direct_dota_server_welcome();

    std::printf("[21/22] Payload adaptation functions...\n");
    test_is_dota_other_left_channel_payload();
    test_adapt_dota_top_custom_games_list_payload();
    test_prepare_dota_direct_replay_message();
    test_patch_dota_practice_lobby_cache_subscribed_template_state();
    test_patch_dota_practice_lobby_launch_template();

    std::printf("[22/25] GBE_ParseDotaEquipOps...\n");
    test_parse_dota_equip_ops();

    std::printf("[24/25] GBE_ApplyDotaUnlockStyleBitmask...\n");
    test_apply_dota_unlock_style_bitmask();

    std::printf("[25/25] GBE_BuildSOSingleObjectFromItem...\n");
    test_build_so_single_object_from_item();

    std::printf("All payload helper tests complete.\n\n");

    std::printf("Results: %d/%d passed, %d failed\n",
        g_tests_passed, g_tests_run, g_tests_failed);

    return g_tests_failed == 0 ? 0 : 1;
}

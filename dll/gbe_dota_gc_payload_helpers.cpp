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

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

void GBE_ComposeDotaPracticeLobbySOObjects(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    gbe::gc_message::DotaPracticeLobbyObjects &lobby_objects);

static const uint8 GBE_kDotaClientWelcomeTemplate[] = {
    0x4D, 0x15, 0x00, 0x80, 0x14, 0x00, 0x00, 0x00, 0x09, 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10,
    0x01, 0x10, 0xEB, 0xFC, 0x88, 0xA1, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x08, 0xBA, 0x04, 0x10,
    0xA4, 0x9F, 0x80, 0x80, 0x08, 0x1A, 0xD6, 0x06, 0xA4, 0x0F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x08, 0xEB, 0x34, 0x12, 0xB0, 0x03, 0x28, 0xBF, 0xA4, 0xE0, 0x86, 0x07, 0x38, 0x01, 0x68, 0xC6,
    0xCC, 0xBC, 0xEE, 0x0D, 0x88, 0x01, 0x00, 0x90, 0x01, 0x1B, 0xB0, 0x01, 0x09, 0xD2, 0x01, 0x32,
    0x08, 0x9F, 0x14, 0x12, 0x2D, 0x0A, 0x0D, 0x08, 0x80, 0xFB, 0xE0, 0xCE, 0x06, 0x10, 0x8A, 0xCF,
    0xB8, 0xE0, 0x82, 0x01, 0x0A, 0x0D, 0x08, 0x80, 0xA9, 0xC1, 0xCE, 0x06, 0x10, 0xEA, 0x98, 0xA6,
    0xD2, 0x82, 0x01, 0x0A, 0x0D, 0x08, 0x80, 0x84, 0xC8, 0xCD, 0x06, 0x10, 0xFE, 0xCA, 0x98, 0x8E,
    0x82, 0x01, 0xD2, 0x01, 0x47, 0x08, 0xD8, 0x3E, 0x12, 0x42, 0x08, 0xDF, 0xE5, 0xC7, 0x81, 0x06,
    0x08, 0x82, 0xB7, 0x99, 0xB9, 0x07, 0x08, 0xB8, 0x99, 0xE2, 0xCA, 0x0A, 0x08, 0xBB, 0x8B, 0xB4,
    0xAF, 0x0C, 0x08, 0x81, 0xFC, 0xE4, 0x9E, 0x0A, 0x08, 0xFB, 0x90, 0xE9, 0xDC, 0x0B, 0x08, 0x91,
    0xCE, 0xA9, 0xA4, 0x0A, 0x08, 0xC9, 0xAE, 0x89, 0xA0, 0x09, 0x08, 0xE7, 0xC4, 0xD1, 0xEF, 0x05,
    0x08, 0xA7, 0x83, 0xC7, 0xDA, 0x0C, 0x10, 0x84, 0x96, 0xCC, 0x8C, 0x0D, 0xD2, 0x01, 0x07, 0x08,
    0x8B, 0x3F, 0x12, 0x02, 0x08, 0x6B, 0xD2, 0x01, 0x7D, 0x08, 0xA9, 0x3A, 0x12, 0x78, 0x0A, 0x16,
    0x08, 0x02, 0x10, 0xA0, 0x98, 0xEA, 0xCE, 0x06, 0x18, 0xA4, 0x9F, 0xEA, 0xCE, 0x06, 0x20, 0xA0,
    0x8D, 0x8F, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16, 0x08, 0x03, 0x10, 0xB0, 0xC8, 0x8D, 0xCF, 0x06,
    0x18, 0xB4, 0xCF, 0x8D, 0xCF, 0x06, 0x20, 0xB0, 0xBD, 0xB2, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16,
    0x08, 0x04, 0x10, 0xC0, 0xEF, 0xE8, 0xCE, 0x06, 0x18, 0xC4, 0xF6, 0xE8, 0xCE, 0x06, 0x20, 0xC0,
    0xE4, 0x8D, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16, 0x08, 0x06, 0x10, 0x90, 0xDD, 0xEB, 0xCE, 0x06,
    0x18, 0x94, 0xE4, 0xEB, 0xCE, 0x06, 0x20, 0x90, 0xD2, 0x90, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16,
    0x08, 0x07, 0x10, 0xF0, 0xA4, 0xEB, 0xCE, 0x06, 0x18, 0xF4, 0xAB, 0xEB, 0xCE, 0x06, 0x20, 0xF0,
    0x99, 0x90, 0xCF, 0x06, 0x28, 0x43, 0xD2, 0x01, 0x07, 0x08, 0x83, 0x3F, 0x12, 0x02, 0x08, 0x01,
    0xD8, 0x01, 0xF5, 0xE8, 0xDB, 0xBF, 0x82, 0x01, 0xE0, 0x01, 0x00, 0xF0, 0x01, 0xB0, 0x18, 0x92,
    0x02, 0x71, 0x08, 0xA7, 0x14, 0x12, 0x6C, 0x0A, 0x2F, 0x31, 0x4F, 0x63, 0x95, 0x01, 0xAF, 0x01,
    0xC7, 0x01, 0xF9, 0x01, 0xAB, 0x02, 0x8F, 0x03, 0xF3, 0x03, 0xD7, 0x04, 0xBB, 0x05, 0x9F, 0x06,
    0x83, 0x07, 0xE7, 0x07, 0xCB, 0x08, 0xAF, 0x09, 0x93, 0x0A, 0xDB, 0x0B, 0xD5, 0x0D, 0xCF, 0x0F,
    0xC9, 0x11, 0x8B, 0x15, 0xAB, 0x1B, 0xE7, 0x20, 0x12, 0x39, 0x08, 0x1B, 0x12, 0x35, 0xAC, 0x02,
    0xD8, 0x04, 0xBC, 0x05, 0xE8, 0x07, 0xB0, 0x09, 0xF8, 0x0A, 0xA4, 0x0D, 0xB4, 0x10, 0xF0, 0x15,
    0xAC, 0x1B, 0xE8, 0x20, 0xC0, 0x25, 0xFC, 0x2A, 0xB8, 0x30, 0xF4, 0x35, 0xB0, 0x3B, 0xEC, 0x40,
    0xA8, 0x46, 0xBC, 0x50, 0xA8, 0x5F, 0xE8, 0x6B, 0xF0, 0x79, 0xA8, 0x91, 0x01, 0xEC, 0xBD, 0x01,
    0x80, 0xE1, 0x01, 0x98, 0x02, 0x37, 0x1A, 0xEA, 0x02, 0x12, 0xA1, 0x02, 0x08, 0xD2, 0x0F, 0x12,
    0x9B, 0x02, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x9A, 0x01, 0x20, 0xA0, 0x01, 0x60, 0x3A, 0x68,
    0x07, 0x70, 0x66, 0x78, 0x02, 0x90, 0x01, 0x00, 0xA0, 0x01, 0x00, 0xA8, 0x01, 0x00, 0xB0, 0x01,
    0xA5, 0xF1, 0xFA, 0xAA, 0x06, 0xB8, 0x01, 0x02, 0xC0, 0x01, 0x3E, 0xB0, 0x02, 0x00, 0xB8, 0x02,
    0x00, 0xC8, 0x02, 0xE5, 0xAF, 0xE4, 0xBF, 0x06, 0xD0, 0x02, 0x0C, 0x80, 0x03, 0x00, 0xB8, 0x03,
    0x01, 0xC8, 0x03, 0x00, 0xD0, 0x03, 0x01, 0xD8, 0x03, 0xCF, 0xD2, 0x9B, 0xA7, 0x05, 0xE0, 0x03,
    0xD5, 0x01, 0xE8, 0x03, 0x36, 0xF0, 0x03, 0x1C, 0x88, 0x04, 0x00, 0x98, 0x04, 0x07, 0xA0, 0x04,
    0x94, 0x01, 0xA8, 0x04, 0x03, 0xB0, 0x04, 0x43, 0xB8, 0x04, 0x95, 0x02, 0xC0, 0x04, 0xD2, 0x22,
    0xC8, 0x04, 0x01, 0xD0, 0x04, 0x03, 0xB0, 0x05, 0x00, 0xC0, 0x05, 0xA4, 0xC7, 0xC7, 0x94, 0x80,
    0xE3, 0xC8, 0xCE, 0x75, 0xC8, 0x05, 0xC4, 0xEF, 0x8F, 0xD0, 0x05, 0xD0, 0x05, 0xAC, 0xF1, 0xEE,
    0xBF, 0x06, 0xD8, 0x05, 0xEF, 0xDB, 0xEE, 0xBF, 0x06, 0xE0, 0x05, 0xBE, 0xC8, 0xEE, 0xBF, 0x06,
    0xB8, 0x06, 0xE1, 0xAC, 0x8B, 0x84, 0xD0, 0x85, 0x40, 0xC0, 0x06, 0xB4, 0xA7, 0xAC, 0x9D, 0x06,
    0xC8, 0x06, 0x80, 0x9A, 0x9A, 0x9B, 0x06, 0xD0, 0x06, 0xAC, 0xF1, 0xEE, 0xBF, 0x06, 0xD8, 0x06,
    0xEF, 0xDB, 0xEE, 0xBF, 0x06, 0xE0, 0x06, 0x99, 0xC1, 0xE4, 0xBF, 0x06, 0xE8, 0x06, 0x00, 0x90,
    0x07, 0x3C, 0x9A, 0x07, 0x07, 0x08, 0x01, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08,
    0x04, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08, 0x02, 0x15, 0x00, 0x00, 0x00, 0x00,
    0x9A, 0x07, 0x07, 0x08, 0x08, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08, 0x10, 0x15,
    0xCD, 0xCC, 0x4C, 0x3D, 0xC0, 0x07, 0x00, 0xC8, 0x07, 0xA0, 0x90, 0xDC, 0xB9, 0x06, 0xD0, 0x07,
    0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0x12, 0x22, 0x08,
    0xDC, 0x0F, 0x12, 0x1D, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x00, 0x18, 0x01, 0x20, 0x00, 0x28,
    0x00, 0x30, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x19, 0x2D, 0x4B, 0xA7, 0x55, 0x7F, 0xD5, 0x69, 0x00, 0x22, 0x0C, 0x08, 0x01, 0x10, 0xF5,
    0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01, 0x30, 0x01, 0x39, 0xB1, 0x4D, 0xA7, 0x55, 0x7F,
    0xD5, 0x69, 0x00, 0x2A, 0x0E, 0x0D, 0x48, 0xA1, 0x1F, 0x42, 0x15, 0x51, 0xCB, 0xE8, 0x42, 0x1A,
    0x02, 0x43, 0x4E, 0x48, 0x09, 0x52, 0x02, 0x43, 0x4E, 0x80, 0x01, 0x00, 0x88, 0x01, 0x00, 0x92,
    0x01, 0x0C, 0x08, 0xA7, 0x23, 0x12, 0x07, 0x0A, 0x05, 0x08, 0xA8, 0x23, 0x12, 0x00,
};
static const std::array<uint8, 2> GBE_kOldDotaVersionVarint = { 0xEB, 0x34 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyLobbyIdVarint = { 0x83, 0xCF, 0xA2, 0xB4, 0xA2, 0xFF, 0xF9, 0x34 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyGameStartTimeVarint = { 0xAE, 0xBB, 0xA3, 0xCF, 0x06 };
static constexpr const char *GBE_kOldDotaPracticeLobbyConnect = "117.157.79.194:27015 10.110.4.21:27015";
extern const char *GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba041098808080081ae9061800008000000000\n"
    "12bd0108d40f12b70108d6f9ac9f95a6fc3418012001310eac2b7cdec1400159f5b62108010010016001680070008201\n"
    "0531313131318a010240008a01024000a80100e00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f8\n"
    "0200800300980300a80300c80301f2030708f54412020800d80400900500b805f7e6cbcf06c00500e80503f00500f805\n"
    "00880600b80600c00637f00600880700c2071009f5b621080100100118003801800101c80700f807008008d5e6cbcf06\n"
    "120508dd0f1200121208de0f120d0a090a075376656e6d61781000129b0308df0f1295030a0012900308a545128a0308\n"
    "f5ed864112bc010a05080210c00c0a05080510c8010a04080a10640a04080b10640a05080c10de020a04082210640a04\n"
    "082310320a05082510ee050a05082810c00c0a04082a10320a05082c10db030a05082f10ac020a05083510de020a0408\n"
    "4510640a04084b10640a05085110d8040a05085310db030a05085410bd150a0508551096010a04086810320a0508c302\n"
    "10010a0508900310010a0508910310010a0508920310010a05089a0310060a0508cd0310030a0508ce0310080a0508cf\n"
    "0310161a060886011086011a0608d10f10d20f1a06088927108a271a0608914e10924e1a0608f95510fa551a0608e15d\n"
    "10e25d1a0808d1890210d289021a0808b9910210ba91021a080889a102108aa1021a0808c1b80210c2b8021a080891c8\n"
    "021092c8021a0808e1d70210e2d7021a080899ef02109aef021a0808899e03108a9e031a0808899b04108a9b041a0808\n"
    "f9c90410fac9041a0808e9f80410eaf8041a0808b9880510ba88051a0808a1900510a290051a0808899805108a98051a\n"
    "0808c1ac0610c2ac0612cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000\n"
    "e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038\n"
    "006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed86411000180020\n"
    "0138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838\n"
    "121908f5ed864110e8071800200138016000d00100d80100e0010019e0a6ef5331f16900220b080310d6f9ac9f95a6fc\n"
    "34";
extern const uint8 GBE_kDotaPracticeLobbyCacheSubscribedTemplate[] = {
    0x18, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x12, 0xC2, 0x01, 0x08, 0xD4, 0x0F, 0x12, 0xBC,
    0x01, 0x08, 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34, 0x18, 0x01, 0x20, 0x00, 0x59, 0xF5,
    0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01, 0x60, 0x01, 0x68, 0x00, 0x70, 0x01, 0x82, 0x01, 0x05,
    0x30, 0x30, 0x31, 0x32, 0x35, 0xA8, 0x01, 0x05, 0xE0, 0x01, 0x00, 0xF8, 0x01, 0x01, 0xA0, 0x02,
    0x04, 0xD0, 0x02, 0x00, 0xD8, 0x02, 0x00, 0xE0, 0x02, 0x00, 0xF0, 0x02, 0x00, 0xF8, 0x02, 0x00,
    0x80, 0x03, 0x00, 0x98, 0x03, 0x00, 0xA8, 0x03, 0x00, 0xC8, 0x03, 0x00, 0xF2, 0x03, 0x07, 0x08,
    0xF5, 0x44, 0x12, 0x02, 0x08, 0x00, 0xD8, 0x04, 0x00, 0x90, 0x05, 0x00, 0xC0, 0x05, 0x00, 0xE8,
    0x05, 0x04, 0xF0, 0x05, 0x8A, 0xB6, 0xFB, 0x8B, 0x0C, 0xF8, 0x05, 0xCD, 0xFB, 0x92, 0xDA, 0x0A,
    0x88, 0x06, 0x00, 0xF0, 0x06, 0x00, 0x88, 0x07, 0x00, 0xC2, 0x07, 0x10, 0x09, 0xF5, 0xB6, 0x21,
    0x08, 0x01, 0x00, 0x10, 0x01, 0x18, 0x00, 0x38, 0x01, 0x80, 0x01, 0x01, 0xC8, 0x07, 0x00, 0xE0,
    0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07,
    0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00,
    0xE0, 0x07, 0x00, 0xF8, 0x07, 0x00, 0x80, 0x08, 0xF2, 0xA7, 0xFE, 0xCE, 0x06, 0x12, 0x05, 0x08,
    0xDD, 0x0F, 0x12, 0x00, 0x12, 0x12, 0x08, 0xDE, 0x0F, 0x12, 0x0D, 0x0A, 0x09, 0x0A, 0x07, 0x53,
    0x76, 0x65, 0x6E, 0x6D, 0x61, 0x78, 0x10, 0x00, 0x12, 0x07, 0x08, 0xDF, 0x0F, 0x12, 0x02, 0x0A,
    0x00, 0x12, 0x2F, 0x08, 0xE0, 0x0F, 0x12, 0x2A, 0x0A, 0x23, 0x09, 0xF5, 0xB6, 0x21, 0x08, 0x01,
    0x00, 0x10, 0x01, 0x48, 0x00, 0x58, 0x00, 0x60, 0xE1, 0xAC, 0x8B, 0x84, 0xD0, 0x85, 0x40, 0x68,
    0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x15, 0x00, 0x00,
    0x00, 0x00, 0x19, 0x74, 0x1F, 0xDE, 0x53, 0xB9, 0xDE, 0x69, 0x00, 0x22, 0x0B, 0x08, 0x03, 0x10,
    0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34,
};
static bool GBE_PatchDotaWelcomeAccountObjects(std::string &inner_body, uint32 account_id)
{
    std::string rewritten_body;
    int patched_object_count = 0;
    size_t offset = 0;
    while (offset < inner_body.size()) {
        gbe::proto_wire::Field field{};
        size_t field_offset = 0;
        size_t field_end = 0;
        if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(inner_body.data()), inner_body.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number != 3u || field.wire_type != 2u) {
            rewritten_body.append(inner_body.data() + field_offset, field_end - field_offset);
            continue;
        }

        CMsgSOCacheSubscribed cache;
        if (!cache.ParseFromArray(inner_body.data() + field.value_offset, static_cast<int>(field.value_size)))
            return false;

        int cache_patch_count = 0;
        for (int object_index = 0; object_index < cache.objects_size(); ++object_index) {
            auto *object = cache.mutable_objects(object_index);
            const int type_id = object->type_id();
            if (type_id != 2002 && type_id != 2012)
                continue;

            for (int data_index = 0; data_index < object->object_data_size(); ++data_index) {
                std::string rewritten_object;
                if (!gbe::proto_wire::rewrite_dota_account_bound_object_data(object->object_data(data_index), type_id, account_id, rewritten_object))
                    return false;

                object->set_object_data(data_index, rewritten_object);
                ++patched_object_count;
                ++cache_patch_count;
            }
        }

        if (cache_patch_count != 0) {
            gbe::proto_wire::append_bytes_field(rewritten_body, 3u, cache.SerializeAsString());
            continue;
        }

        rewritten_body.append(inner_body.data() + field_offset, field_end - field_offset);
    }

    if (patched_object_count != 0) {
        inner_body.swap(rewritten_body);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "patched welcome account-bound objects count=%d account_id=%u", patched_object_count, account_id);
    }

    return true;
}
void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size)
{
    if (!scope || !direction || !data || size < sizeof(ProtoBufMsgHeader_t) || !gbe::gc_message::should_trace_dota_proto_boundary(emsg))
        return;

    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(data, size, context)) {
        GBE_GC_DebugLog(
            scope,
            "%s this=%p is_server=%u emsg=%u size=%u parse=0",
            direction,
            self,
            is_server ? 1u : 0u,
            emsg,
            size
        );
        return;
    }

    GBE_GC_DebugLog(
        scope,
        "%s this=%p is_server=%u emsg=%u size=%u ext=%u body=%zu has_job_src=%u job_src=%llu has_job_tgt=%u job_tgt=%llu client_steam_id=%llu session=%d app_id=%u",
        direction,
        self,
        is_server ? 1u : 0u,
        emsg,
        size,
        context.hdr.m_cubProtoBufExtHdr,
        context.body_size,
        context.protohdr.has_job_id_source() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_source() ? context.protohdr.job_id_source() : 0ull),
        context.protohdr.has_job_id_target() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_target() ? context.protohdr.job_id_target() : 0ull),
        static_cast<unsigned long long>(context.protohdr.has_client_steam_id() ? context.protohdr.client_steam_id() : 0ull),
        context.protohdr.has_client_session_id() ? context.protohdr.client_session_id() : 0,
        context.protohdr.has_source_app_id() ? context.protohdr.source_app_id() : 0u
    );
}
bool GBE_PatchDotaLobbyTemplateIdentifiers(std::string &message, uint32 account_id, uint64 steam_id, uint64 lobby_id)
{
    (void)account_id;
    const std::vector<uint8> old_lobby_id = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const std::vector<uint8> old_steam_id_fixed64 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    gbe::proto_wire::PatchTemplateIdentifierResult patch_result{};
    if (!gbe::proto_wire::patch_dota_lobby_template_identifiers(message, old_lobby_id, lobby_id, true, old_steam_id_fixed64, steam_id, true, patch_result)) {
        if (patch_result.lobby_id_match_count == 0)
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing lobby_id bytes lobby_id=%llu", static_cast<unsigned long long>(lobby_id));
        else
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing steam_id fixed64 bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched template LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        patch_result.lobby_id_match_count,
        patch_result.steam_id_fixed64_match_count,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}
static bool GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(std::string &message, uint64 steam_id, uint64 lobby_id)
{
    const std::vector<uint8> old_lobby_id = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const std::vector<uint8> old_steam_id_fixed64 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    gbe::proto_wire::PatchTemplateIdentifierResult patch_result{};
    if (!gbe::proto_wire::patch_dota_lobby_template_identifiers(message, old_lobby_id, lobby_id, false, old_steam_id_fixed64, steam_id, false, patch_result)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing optional template identifiers lobby_id=%llu steam_id=%llu", static_cast<unsigned long long>(lobby_id), static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched optional template identifiers LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        patch_result.lobby_id_match_count,
        patch_result.steam_id_fixed64_match_count,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}
static bool GBE_ForceDotaLobbyCacheOwnerSOID(std::string &message, uint64 lobby_id)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, proto_context.body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}
bool GBE_ForceDotaLobbyUpdateOwnerSOID(std::string &message, uint64 lobby_id)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOMultipleObjects protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, proto_context.body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}
bool GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    bool rewrite_runtime_fields,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const std::string &pass_key,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)account_id;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    const std::string body(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size);
    std::string rewritten_body;
    const uint32 scratch_startup_account_id = rewrite_2015 ? extra_startup_account_id : 0u;

    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        false,
        0u,
        std::string(),
        std::string(),
        std::string(),
        pass_key,
        scratch_startup_account_id,
        custom_game,
        lobby_objects);

    size_t offset = 0;
    while (offset < body.size()) {
        gbe::proto_wire::Field field{};
        size_t field_offset = 0;
        size_t field_end = 0;
        if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(body.data()), body.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number != 2u || field.wire_type != 2u) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const std::string subscribed = body.substr(field.value_offset, field.value_size);
        uint64 type_id = 0;
        if (!gbe::proto_wire::read_uint64_field(reinterpret_cast<const uint8 *>(subscribed.data()), subscribed.size(), 1u, type_id)) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const bool should_rewrite_type = rewrite_runtime_fields
            ? (type_id == 2004u || type_id == 2014u || type_id == 2015u || type_id == 2016u)
            : (type_id == 2004u || type_id == 2014u);
        if (!should_rewrite_type) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        std::string rewritten_subscribed;
        size_t subscribed_offset = 0;
        while (subscribed_offset < subscribed.size()) {
            gbe::proto_wire::Field subscribed_field{};
            size_t subscribed_field_offset = 0;
            size_t subscribed_field_end = 0;
            if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(subscribed.data()), subscribed.size(), subscribed_offset, subscribed_field, &subscribed_field_offset, &subscribed_field_end))
                return false;

            if (subscribed_field.number == 2u && subscribed_field.wire_type == 2u) {
                std::string rewritten_object;
                switch (type_id) {
                case 2004u:
                    rewritten_object = lobby_objects.object_2004;
                    break;
                case 2014u:
                    rewritten_object = lobby_objects.object_2014;
                    break;
                case 2015u:
                    rewritten_object = lobby_objects.object_2015;
                    break;
                case 2016u:
                    rewritten_object = lobby_objects.object_2016;
                    break;
                default:
                    rewritten_object.assign(subscribed.data() + subscribed_field.value_offset, subscribed_field.value_size);
                    break;
                }
                gbe::proto_wire::append_bytes_field(rewritten_subscribed, 2u, rewritten_object);
                continue;
            }

            rewritten_subscribed.append(subscribed.data() + subscribed_field_offset, subscribed_field_end - subscribed_field_offset);
        }

        gbe::proto_wire::append_bytes_field(rewritten_body, 2u, rewritten_subscribed);
    }

    message.resize(proto_context.body_offset);
    message.append(rewritten_body);
    return true;
}
bool GBE_PatchDotaPracticeLobbyLaunchTemplate(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    bool patch_server_id,
    bool patch_game_start_time,
    bool patch_connect,
    const char *stage_note)
{
    if (!GBE_TryPatchDotaAccountIdVarint(message, account_id, "GC_DOTA_PATCH", GBE_kDotaPracticeLobbyLaunch, GBE_kDotaPracticeLobbyDetailsUpdate, 0, stage_note)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch account_id varint patch failed stage=%s", stage_note ? stage_note : "");
        return false;
    }

    gbe::proto_wire::DotaPracticeLobbyLaunchTemplatePatchResult patch_result{};
    if (!gbe::proto_wire::patch_dota_practice_lobby_launch_template(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            steam_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyLobbyIdVarint.data(), GBE_kOldDotaPracticeLobbyLobbyIdVarint.size()),
            lobby_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyMatchIdVarint.data(), GBE_kOldDotaPracticeLobbyMatchIdVarint.size()),
            match_id,
            patch_server_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
            server_id,
            patch_game_start_time,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyGameStartTimeVarint.data(), GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size()),
            game_start_time,
            patch_connect,
            GBE_kOldDotaPracticeLobbyConnect,
            connect,
            patch_result)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch template patch failed stage=%s", stage_note ? stage_note : "");
        return false;
    }

    if (patch_result.steam_id_fixed64_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch steam_id fixed64 patch skipped stage=%s steam_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(steam_id)
        );
    }

    if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH")) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch account_id fixed32 patch skipped stage=%s account_id=%u; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            account_id
        );
    }

    if (!patch_result.lobby_id_size_ok) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id size mismatch skipped stage=%s lobby_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    } else if (patch_result.lobby_id_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id patch skipped stage=%s lobby_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    }

    if (!patch_result.match_id_size_ok) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id size mismatch skipped stage=%s match_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    } else if (patch_result.match_id_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id patch skipped stage=%s match_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    }

    if (patch_server_id && patch_result.server_id_fixed64_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch server_id patch skipped stage=%s server_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(server_id)
        );
    }

    if (patch_game_start_time) {
        if (!patch_result.game_start_time_size_ok) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time size mismatch skipped stage=%s game_start_time=%u; donor varint width differs",
                stage_note ? stage_note : "",
                game_start_time
            );
        } else if (patch_result.game_start_time_match_count == 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time patch skipped stage=%s game_start_time=%u; donor does not expose expected template bytes",
                stage_note ? stage_note : "",
                game_start_time
            );
        }
    }

    if (patch_connect) {
        if (!patch_result.connect_size_ok) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch connect size changed stage=%s size=%zu expected=%zu; skipping fixed-width overwrite and relying on proto rewrite",
                stage_note ? stage_note : "",
                connect.size(),
                std::strlen(GBE_kOldDotaPracticeLobbyConnect)
            );
        } else if (patch_result.connect_match_count == 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch connect patch skipped stage=%s connect=%s; donor does not expose expected template string",
                stage_note ? stage_note : "",
                connect.c_str()
            );
        }
    }

    return true;
}
bool GBE_AdaptDotaTopCustomGamesListPayload(class Settings *settings, std::string &message, size_t &game_count)
{
    std::vector<std::uint64_t> mod_ids;
    if (settings) {
        for (PublishedFileId_t mod_id : settings->modSet()) {
            if (mod_id == 0ull || mod_id == k_PublishedFileIdInvalid)
                continue;
            mod_ids.push_back(static_cast<std::uint64_t>(mod_id));
        }
    }
    return gbe::gc_message::build_dota_top_custom_games_list_payload(mod_ids, message, game_count);
}
bool GBE_IsDotaOtherLeftChannelPayloadForChannel(const std::string &message, uint64 channel_id)
{
    if (message.size() < 8u)
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(message.data());
    uint32 inner_emsg = 0;
    uint32 header_length = 0;
    std::memcpy(&inner_emsg, bytes, sizeof(inner_emsg));
    std::memcpy(&header_length, bytes + sizeof(inner_emsg), sizeof(header_length));
    if (GBE_GC_MaskedEMsg(inner_emsg) != GBE_kDotaOtherLeftChannel)
        return false;

    const size_t body_offset = 8u + header_length;
    if (body_offset + 9u > message.size())
        return false;

    const uint8 *body = bytes + body_offset;
    if (body[0] != 0x09u)
        return false;

    uint64 payload_channel_id = 0;
    std::memcpy(&payload_channel_id, body + 1u, sizeof(payload_channel_id));
    return payload_channel_id == channel_id;
}
void GBE_ComposeDotaPracticeLobbySOObjects(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    gbe::gc_message::DotaPracticeLobbyObjects &lobby_objects)
{
    static const uint8 GBE_kDotaLobbyField62Value[] = { 0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00 };

    const bool has_custom_game = custom_game && custom_game->game_id != 0ull;
    std::vector<GBE_DotaLobbyMemberState> effective_members = gbe::dota_lobby_flow::compose_lobby_members(
        steam_id,
        extra_startup_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        true,
        GBE_kDotaTeamPlayerPool,
        members);
    if (has_custom_game) {
        effective_members.erase(
            std::remove_if(effective_members.begin(), effective_members.end(), [](const GBE_DotaLobbyMemberState &member) { return member.steam_id == 0ull; }),
            effective_members.end());
    }

    const std::string normalized_connect = gbe::dota_custom_game::format_practice_lobby_connect_for_custom_game(connect, custom_game);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Building 2004 team details lobby_id=%llu state=%u game_state=%u custom_game_id=%llu team_details=%u",
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        static_cast<unsigned long long>(has_custom_game ? custom_game->game_id : 0ull),
        lobby_state != 0u ? 2u : 0u
    );

    gbe::gc_message::DotaCustomGameDetails parsed_custom_game{};
    const gbe::gc_message::DotaCustomGameDetails *parsed_custom_game_ptr = nullptr;
    if (custom_game) {
        parsed_custom_game.mode = custom_game->mode;
        parsed_custom_game.map_name = custom_game->map_name;
        parsed_custom_game.difficulty = custom_game->difficulty;
        parsed_custom_game.game_id = custom_game->game_id;
        parsed_custom_game.min_players = custom_game->min_players;
        parsed_custom_game.max_players = custom_game->max_players;
        parsed_custom_game.crc = custom_game->crc;
        parsed_custom_game.timestamp = custom_game->timestamp;
        parsed_custom_game.penalties = custom_game->penalties;
        parsed_custom_game_ptr = &parsed_custom_game;
    }

    std::vector<gbe::gc_message::DotaLobbyMemberObjectState> parsed_members;
    parsed_members.reserve(effective_members.size());
    std::vector<gbe::gc_message::DotaStaticLobbyMember> static_lobby_members;
    static_lobby_members.reserve(effective_members.size());
    for (const GBE_DotaLobbyMemberState &member : effective_members) {
        parsed_members.push_back(gbe::gc_message::DotaLobbyMemberObjectState{
            member.steam_id,
            member.team,
            member.slot,
            member.hero_id,
            member.connected,
            member.leaver_status});
        static_lobby_members.push_back(gbe::gc_message::DotaStaticLobbyMember{
            member.steam_id,
            member.account_id != 0u ? member.account_id : (member.steam_id != 0ull ? CSteamID((uint64)member.steam_id).GetAccountID() : 0u),
            member.connected});
    }

    gbe::gc_message::DotaPracticeLobbyObjectOptions lobby_object_options{};
    lobby_object_options.extra_startup_account_id = extra_startup_account_id;
    lobby_object_options.steam_id = steam_id;
    lobby_object_options.game_mode = game_mode;
    lobby_object_options.is_custom_game = has_custom_game;
    lobby_object_options.player_name = player_name;
    lobby_object_options.lobby_members = std::move(parsed_members);
    lobby_object_options.static_lobby_members = std::move(static_lobby_members);

    gbe::gc_message::DotaLobbyObject2004Options &object_2004_options = lobby_object_options.object_2004_options;
    object_2004_options.steam_id = steam_id;
    object_2004_options.lobby_id = lobby_id;
    object_2004_options.lobby_state = lobby_state;
    object_2004_options.lobby_game_state = lobby_game_state;
    object_2004_options.server_id = server_id;
    object_2004_options.match_id = match_id;
    object_2004_options.game_start_time = game_start_time;
    if (lobby_state == 3u && lobby_game_state == 6u && game_start_time != 0u) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        object_2004_options.elapsed_game_time = now > game_start_time ? (now - game_start_time) : 0u;
    }
    object_2004_options.connect = normalized_connect;
    object_2004_options.room_name = room_name;
    object_2004_options.game_mode = game_mode;
    object_2004_options.server_region = server_region;
    object_2004_options.lan = lan;
    object_2004_options.lan_host_ping_location = lan_host_ping_location;
    object_2004_options.allow_cheats = allow_cheats;
    object_2004_options.fill_with_bots = fill_with_bots;
    object_2004_options.allow_spectating = allow_spectating;
    object_2004_options.visibility = visibility;
    object_2004_options.bot_difficulty_radiant = bot_difficulty_radiant;
    object_2004_options.bot_difficulty_dire = bot_difficulty_dire;
    object_2004_options.bot_radiant = bot_radiant;
    object_2004_options.bot_dire = bot_dire;
    object_2004_options.has_broadcast_channel = has_broadcast_channel;
    object_2004_options.broadcast_channel_id = broadcast_channel_id;
    object_2004_options.broadcast_country_code = broadcast_country_code;
    object_2004_options.broadcast_description = broadcast_description;
    object_2004_options.broadcast_language_code = broadcast_language_code;
    object_2004_options.pass_key = pass_key;
    object_2004_options.custom_game = parsed_custom_game_ptr;

    gbe::gc_message::build_dota_practice_lobby_objects(lobby_object_options, lobby_objects);
}
bool GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_cache_subscribed_payload_from_objects(lobby_id, lobby_objects.object_2004, lobby_objects.object_2015, lobby_objects.object_2014, lobby_objects.object_2016, message);
}
bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    bool include_server_lobby_placeholder,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2015, lobby_objects.object_2004, lobby_objects.object_2016, include_server_lobby_placeholder, message);
}
bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
    const char *wrapped_template_hex,
    const char *template_note,
    bool require_lobby_identifiers,
    bool rewrite_runtime_fields,
    bool force_lobby_owner_soid,
    bool rewrite_2015,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    std::string wrapped_message;
    if (!wrapped_template_hex || !gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template decode failed note=%s has_template=%u", template_note ? template_note : "unknown", wrapped_template_hex ? 1u : 0u);
        return false;
    }

    if (!GBE_PatchDotaTemplateIdentifiers(
            wrapped_message,
            account_id,
            steam_id,
            true,
            true,
            GBE_kDotaPracticeLobbyLaunch,
            GBE_kDotaCacheSubscribed,
            0,
            template_note ? template_note : "practice lobby launch cache template")) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template identifier patch failed note=%s", template_note ? template_note : "unknown");
        return false;
    }

    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaCacheSubscribed, message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template inner extraction failed note=%s wrapped_size=%zu", template_note ? template_note : "unknown", wrapped_message.size());
        return false;
    }

    if (require_lobby_identifiers) {
        if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache required lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    } else {
        if (!GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(message, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache optional lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    }

    if (!rewrite_runtime_fields) {
        if (!force_lobby_owner_soid)
            return true;

        return GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id);
    }

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        std::vector<GBE_DotaLobbyMemberState>(),
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "launch cache runtime state patch failed note=%s lobby_id=%llu state=%u game_state=%u server_id=%llu match_id=%llu body_size=%zu",
            template_note ? template_note : "unknown",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            static_cast<unsigned long long>(server_id),
            static_cast<unsigned long long>(match_id),
            message.size());
        return false;
    }

    if (!GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache owner soid patch failed note=%s lobby_id=%llu body_size=%zu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id), message.size());
        return false;
    }

    return true;
}
bool GBE_ExtractDotaHelloContext(const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (!pubData || cubData < 8) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    const uint8 *bytes = reinterpret_cast<const uint8 *>(pubData);
    uint32 outer_raw_emsg = 0;
    uint32 outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientToGC) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected outer emsg=%u", GBE_GC_MaskedEMsg(outer_raw_emsg));
        return false;
    }

    const size_t outer_header_offset = 8;
    const size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > cubData) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "outer body offset overflow header_len=%u cubData=%u", outer_header_length, cubData);
        return false;
    }

    const uint8 *outer_header = bytes + outer_header_offset;
    const uint8 *outer_body = bytes + outer_body_offset;
    const size_t outer_body_size = cubData - outer_body_offset;

    if (!gbe::proto_wire::read_bytes_field(outer_header, outer_header_length, 2u, context.outer_session_field_raw)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "missing outer session field");
        return false;
    }

    std::string payload_raw;
    if (!gbe::proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, payload_raw) || payload_raw.size() < 8u) {
        GBE_GC_DebugLog(
            "GC_DOTA_HELLO",
            "invalid payload field size=%zu",
            payload_raw.size()
        );
        return false;
    }

    const uint8 *payload = reinterpret_cast<const uint8 *>(payload_raw.data());
    uint32 inner_raw_emsg = 0;
    uint32 inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    if (GBE_GC_MaskedEMsg(inner_raw_emsg) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected inner emsg=%u", GBE_GC_MaskedEMsg(inner_raw_emsg));
        return false;
    }

    const size_t inner_header_offset = 8;
    const size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_raw.size()) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "inner body offset overflow header_len=%u payload_size=%zu", inner_header_length, payload_raw.size());
        return false;
    }

    const uint8 *inner_header = payload + inner_header_offset;
    const uint8 *inner_body = payload + inner_body_offset;
    const size_t inner_body_size = payload_raw.size() - inner_body_offset;

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(inner_body, inner_body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "failed to extract version field");
        return false;
    }

    context.version = static_cast<uint32>(parsed_version);

    if (inner_header_length > 0) {
        uint64 source_job = 0;
        if (gbe::proto_wire::read_uint64_field(inner_header, inner_header_length, 11u, source_job)) {
            context.source_job_id = source_job;
            context.has_source_job = true;
        }
    }

    context.valid = true;
    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "parsed version=%u source_job=%llu has_source_job=%d session_raw_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        context.outer_session_field_raw.size()
    );
    return true;
}
bool GBE_ExtractDirectDotaHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(proto_context.body, proto_context.body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path failed to extract version field");
        return false;
    }

    context.valid = true;
    context.version = static_cast<uint32>(parsed_version);
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "direct path parsed version=%u source_job=%llu has_source_job=%d body_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        proto_context.body_size
    );
    return true;
}
bool GBE_ExtractDirectDotaServerHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaServerHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCServerHello) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    CMsgServerHello protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)) || !protomsg.has_version()) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed parsing CMsgServerHello body_size=%zu", proto_context.body_size);
        return false;
    }

    const uint32 version = protomsg.version();

    context.valid = true;
    context.active_version = version;
    context.min_allowed_version = version;
    context.compatibility_value = 0;
    context.universe = 0;
    if (proto_context.protohdr.has_client_steam_id()) {
        context.client_steam_id = proto_context.protohdr.client_steam_id();
        context.has_client_steam_id = true;
    }
    if (proto_context.protohdr.has_client_session_id()) {
        context.client_session_id = proto_context.protohdr.client_session_id();
        context.has_client_session_id = true;
    }
    if (proto_context.protohdr.has_source_app_id()) {
        context.source_app_id = proto_context.protohdr.source_app_id();
        context.has_source_app_id = true;
    }
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }
    if (proto_context.protohdr.has_gc_msg_src()) {
        context.gc_msg_src = static_cast<uint32>(proto_context.protohdr.gc_msg_src());
        context.has_gc_msg_src = true;
    }
    if (proto_context.protohdr.has_gc_dir_index_source()) {
        context.gc_dir_index_source = proto_context.protohdr.gc_dir_index_source();
        context.has_gc_dir_index_source = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "direct path parsed active_version=%u min_allowed=%u compat=%llu universe=%u source_job=%llu has_source_job=%d client_steam_id=%llu has_client_steam_id=%d client_session_id=%d has_client_session_id=%d source_app_id=%u has_source_app_id=%d gc_msg_src=%u has_gc_msg_src=%d gc_dir_index_source=%u has_gc_dir_index_source=%d body_size=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.compatibility_value),
        context.universe,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        static_cast<unsigned long long>(context.client_steam_id),
        context.has_client_steam_id ? 1 : 0,
        context.client_session_id,
        context.has_client_session_id ? 1 : 0,
        context.source_app_id,
        context.has_source_app_id ? 1 : 0,
        context.gc_msg_src,
        context.has_gc_msg_src ? 1 : 0,
        context.gc_dir_index_source,
        context.has_gc_dir_index_source ? 1 : 0,
        proto_context.body_size
    );
    return true;
}
static bool GBE_PrepareDotaWelcomeBody(uint64 steam_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &inner_body)
{
    if (!context.valid || GBE_kDotaWelcomeInnerBodyOffset >= sizeof(GBE_kDotaClientWelcomeTemplate)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "invalid context valid=%d offset=%zu template_size=%zu", context.valid ? 1 : 0, GBE_kDotaWelcomeInnerBodyOffset, sizeof(GBE_kDotaClientWelcomeTemplate));
        return false;
    }

    inner_body.assign(
        reinterpret_cast<const char *>(GBE_kDotaClientWelcomeTemplate + GBE_kDotaWelcomeInnerBodyOffset),
        sizeof(GBE_kDotaClientWelcomeTemplate) - GBE_kDotaWelcomeInnerBodyOffset
    );

    {
        size_t match_count = 0;
        bool size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(
                inner_body,
                gbe::proto_wire::vector_from_bytes(GBE_kOldDotaVersionVarint.data(), GBE_kOldDotaVersionVarint.size()),
                context.version,
                match_count,
                size_ok)) {
            if (!size_ok) {
                GBE_GC_DebugLog("GC_DOTA_WELCOME", "version varint size mismatch version=%u expected=%zu", context.version, GBE_kOldDotaVersionVarint.size());
                return false;
            }
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing version bytes version=%u", context.version);
            return false;
        }
    }

    if (!GBE_PatchDotaWelcomeAccountObjects(inner_body, account_id)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed patching welcome account-bound objects account_id=%u", account_id);
        return false;
    }

    {
        size_t match_count = 0;
        bool size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(
                inner_body,
                gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size()),
                steam_id,
                match_count,
                size_ok)) {
            if (!size_ok) {
                GBE_GC_DebugLog("GC_DOTA_WELCOME", "steam_id varint size mismatch steam_id=%llu expected=%zu", static_cast<unsigned long long>(steam_id), GBE_kOldDotaSteamIdVarint.size());
                return false;
            }
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing steam_id bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
            return false;
        }
    }

    GBE_GC_DebugLog("GC_DOTA_WELCOME", "prepared welcome body size=%zu", inner_body.size());
    return true;
}
bool GBE_BuildDirectDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string proto_header;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = GBE_kEMsgGCClientWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(steam_id);
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(app_id);
    if (context.has_source_job) {
        protohdr.set_job_id_target(context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "direct path mirroring source_job=%llu into target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);
    message.append(inner_body);

    GBE_GC_DebugLog("GC_DOTA_WELCOME", "built direct welcome header=%u body=%zu total=%zu", hdr.m_cubProtoBufExtHdr, inner_body.size(), message.size());
    return true;
}
bool GBE_ComposeDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string inner_header;
    if (context.has_source_job) {
        gbe::proto_wire::append_varint_field(inner_header, 10, context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "mirroring source_job=%llu into inner target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    std::string inner_payload;
    gbe::proto_wire::append_little_endian32(inner_payload, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(inner_payload, static_cast<uint32>(inner_header.size()));
    inner_payload.append(inner_header);
    inner_payload.append(inner_body);

    std::string outer_body;
    gbe::proto_wire::append_varint_field(outer_body, 1, app_id);
    gbe::proto_wire::append_varint_field(outer_body, 2, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_bytes_field(outer_body, 3, inner_payload);

    std::string outer_header;
    // The outer fixed64 steamid lives outside the inner template, so we rebuild that header directly.
    gbe::proto_wire::append_fixed64_field(outer_header, 1, steam_id);
    gbe::proto_wire::append_varuint(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(context.outer_session_field_raw);

    message.clear();
    gbe::proto_wire::append_little_endian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(message, static_cast<uint32>(outer_header.size()));
    message.append(outer_header);
    message.append(outer_body);
    GBE_GC_DebugLog(
        "GC_DOTA_WELCOME",
        "built welcome outer_size=%zu inner_header=%zu inner_body=%zu total=%zu",
        outer_header.size(),
        inner_header.size(),
        inner_body.size(),
        message.size()
    );
    return true;
}

// --- Phase 2.12: payload-rewrite free functions (moved from main file) ---

bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)extra_startup_account_id;

    return GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex,
        "practice lobby launch official cache template",
        false,
        true,
        true,
        false,
        account_id,
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)has_broadcast_channel;
    (void)broadcast_channel_id;
    (void)broadcast_country_code;
    (void)broadcast_description;
    (void)broadcast_language_code;

    const uint32 startup_account_id = gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(account_id, lobby_state, lobby_game_state);

    if (gbe::proto_wire::is_dota_practice_lobby_prelaunch_state(server_id, match_id, game_start_time, connect)) {
        gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;

        GBE_ComposeDotaPracticeLobbySOObjects(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            custom_game,
            lobby_objects);

        return gbe::gc_message::build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2016, lobby_objects.object_2015, lobby_objects.object_2004, message)
            && GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
    }

    if (GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            true,
            message,
            custom_game)) {
        return true;
    }

    return GBE_ReplayDotaPracticeLobbyOfficial26Payload(
        GBE_kDotaOfficial032PracticeLobby26Hex,
        "current direct 26 details update",
        account_id,
        steam_id,
        lobby_id,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        lobby_state,
        lobby_game_state,
        startup_account_id != 0u,
        startup_account_id,
        message,
        custom_game);
}

bool GBE_PrepareDotaDirectReplayMessage(
    const uint8 *template_bytes,
    size_t template_size,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    bool has_target_job,
    uint64 target_job,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note,
    std::string &message)
{
    message.assign(reinterpret_cast<const char *>(template_bytes), template_size);

    if (!GBE_PatchDotaTemplateIdentifiers(message, account_id, steam_id, replace_account, replace_steam_id, request_emsg, response_emsg, body_size, context_note))
        return false;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgProtoBufHeader protohdr = proto_context.protohdr;

    if (has_target_job) {
        protohdr.set_job_id_target(target_job);
    } else {
        protohdr.clear_job_id_target();
    }
    protohdr.clear_job_id_source();

    const char *body_ptr = reinterpret_cast<const char *>(proto_context.body);
    const size_t serialized_body_size = proto_context.body_size;

    std::string updated;
    ProtoBufMsgHeader_t hdr = proto_context.hdr;
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    ser_var<ProtoBufMsgHeader_t>(updated, hdr);
    protohdr.AppendToString(&updated);
    updated.append(body_ptr, serialized_body_size);
    message.swap(updated);
    return true;
}

bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message)
{
    if (!context.valid) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "invalid server hello context");
        return false;
    }

    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(context.has_client_steam_id ? context.client_steam_id : steam_id);
    if (context.has_client_session_id) {
        protohdr.set_client_session_id(context.client_session_id);
    } else {
        protohdr.set_client_session_id(1);
    }
    protohdr.set_source_app_id(context.has_source_app_id ? context.source_app_id : app_id);
    if (context.has_source_job)
        protohdr.set_job_id_target(context.source_job_id);
    if (context.has_gc_msg_src)
        protohdr.set_gc_msg_src(static_cast<GCProtoBufMsgSrc>(context.gc_msg_src));
    if (context.has_gc_dir_index_source)
        protohdr.set_gc_dir_index_source(context.gc_dir_index_source);

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(context.min_allowed_version);
    protomsg.set_active_version(context.active_version);
    protomsg.AppendToString(&message);

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "built direct ServerWelcome active_version=%u min_allowed=%u target_job=%llu client_steam_id=%llu client_session_id=%d source_app_id=%u gc_msg_src=%u gc_dir_index_source=%u total=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.has_source_job ? context.source_job_id : 0ull),
        static_cast<unsigned long long>(context.has_client_steam_id ? context.client_steam_id : steam_id),
        context.has_client_session_id ? context.client_session_id : 1,
        context.has_source_app_id ? context.source_app_id : app_id,
        context.has_gc_msg_src ? context.gc_msg_src : 0u,
        context.has_gc_dir_index_source ? context.gc_dir_index_source : 0u,
        message.size()
    );
    return true;
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_runtime_fields,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || account_id == 0 || lobby_id == 0)
        return false;

    if (!GBE_PrepareDotaDirectReplayMessage(
            GBE_kDotaPracticeLobbyCacheSubscribedTemplate,
            sizeof(GBE_kDotaPracticeLobbyCacheSubscribedTemplate),
            account_id,
            steam_id,
            false,
            false,
            false,
            0,
            7038u,
            24u,
            0,
            "practice lobby cache template",
            message)) {
        return false;
    }

    if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id))
        return false;

    return GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game);
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    return GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

// --- Externalized from steam_game_coordinator.cpp (helper cleanup phase) ---
// These file-scope helpers were previously defined in the GC main TU.
// They have no dependency on Steam_Game_Coordinator class internals.

bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out)
        return false;

    if (GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        (GBE_shared_dota_lobby_state.state >= 2u || GBE_shared_dota_lobby_state.game_state >= 2u) &&
        !GBE_shared_dota_lobby_state.connect.empty() &&
        GBE_shared_dota_lobby_state.server_id != 0) {
        out->server_id = GBE_shared_dota_lobby_state.server_id;
        out->lobby_state = GBE_shared_dota_lobby_state.state;
        out->game_state = GBE_shared_dota_lobby_state.game_state;
        out->custom_game_id = GBE_shared_dota_lobby_state.custom_game.game_id;
        const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_shared_dota_lobby_state.connect);
        std::strncpy(out->connect, endpoint.c_str(), sizeof(out->connect) - 1);
        out->connect[sizeof(out->connect) - 1] = '\0';
        out->owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
        return true;
    }

    if (GBE_recent_dota_reconnect_context_valid &&
        GBE_DotaReconnectContextIsStarted(GBE_recent_dota_reconnect_context) &&
        GBE_recent_dota_reconnect_context.connect[0] != '\0' &&
        GBE_recent_dota_reconnect_context.server_id != 0) {
        *out = GBE_recent_dota_reconnect_context;
        return true;
    }

    return false;
}

bool GBE_IsDotaArcadeLobbyActive()
{
    return GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        GBE_shared_dota_lobby_state.custom_game.game_id != 0ull;
}

bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64_t local_steam_id, GBE_DotaReconnectContext *out)
{
    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_game_coordinator)
        return false;

    return steam_client->steam_game_coordinator->GBE_TryRecoverDotaReconnectContextFromGenericLobbies(local_steam_id, out);
}
// --- End Dota reconnect shared state ---

const char *GBE_DescribeDotaLaunchPhase(uint32 phase)
{
    switch (phase) {
        case GBE_kDotaLaunchPhaseRequested:
            return "requested";
        case GBE_kDotaLaunchPhaseSetupSynced:
            return "serversetup_synced";
        case GBE_kDotaLaunchPhaseRunQueued:
            return "run_queued";
        case GBE_kDotaLaunchPhaseLoaded:
            return "loaded";
        default:
            return "none";
    }
}

extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34 };
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64 = { 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64 = { 0x91, 0x1D, 0xDF, 0x05, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt = "29822498642855090";
extern const uint32 GBE_kSteamTicketAuthComplete = 5429u;
extern const char *GBE_kDotaAbandonPersonaStateInitHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112a50209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131c1040000000000000000c9040000000000000000f80400800500880500980501";
extern const char *GBE_kDotaOfficial032PracticeLobby26Hex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04109a808080081a80041a00008000000000120508dd0f120012e90108d40f12e30108d6f9ac9f95a6fc34180120022a273138322e34322e3232342e31333a3237303135203139322e3136382e342e3136383a3237303135310eac2b7cdec1400159f5b621080100100160016800700082010531313131318a010240008a01024000a80100b0010ae00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f80200800300980300a80300c80301f2030708f54412020800880400d80400900500b805f7e6cbcf06c00500e80503f00500f80500880600b80600c00637f00600880700c2070d09f5b621080100100118003801c80700f807008008d5e6cbcf06121208de0f120d0a090a075376656e6d61781000120708df0f12020a0012cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed864110001800200138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838121908f5ed864110e8071800200138016000d00100d80100e00100190439f15331f16900320b080310d6f9ac9f95a6fc34";
// Removed: GBE_kDota7388Profile20Template (had owned=0, caused "Unavailable")
// Removed: GBE_kDota7388Profile37Template (had owned=1 but used hardcoded account_id)
// All 7387->7388 event queries now use gbe::gc_message::build_dota_7388_minimal_response_payload.

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    const char *log_scope = scope ? scope : "GC";
    if (std::strcmp(log_scope, "GC_SEND") == 0 ||
        std::strcmp(log_scope, "GC_SEND_DOTA") == 0 ||
        std::strcmp(log_scope, "GC_CONFIG") == 0 ||
        std::strcmp(log_scope, "GC_INIT") == 0 ||
        std::strcmp(log_scope, "CREATE_INTERFACE") == 0 ||
        std::strcmp(log_scope, "NETSOCK_CTOR") == 0) {
        return;
    }

    FILE *file = std::fopen(GBE_kGcDebugLogPath, "a");
    if (!file)
        return;

    std::fprintf(file, "[%s] ", log_scope);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);

    std::fprintf(file, "\n");
    std::fclose(file);
}

bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count)
{
    replacement_count = 0;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    const std::string body(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size);
    std::string rewritten_body;
    if (!gbe::proto_wire::rewrite_varint_bytes_recursive(
            body,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_body,
            replacement_count))
        return false;

    if (replacement_count == 0)
        return true;

    message.resize(proto_context.body_offset);
    message.append(rewritten_body);
    return true;
}

bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    std::string encoded_account_raw;
    gbe::proto_wire::append_varuint(encoded_account_raw, account_id);

    std::string rewritten_message;
    size_t replacement_count = 0;
    const bool full_message_parse_ok = gbe::proto_wire::rewrite_varint_bytes_recursive(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_message,
            replacement_count);
    if (full_message_parse_ok && replacement_count > 0) {
        message.swap(rewritten_message);
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=full_message",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            replacement_count,
            1u
        );
        return true;
    }

    size_t direct_body_replacements = 0;
    if (GBE_RewriteAccountIdVarintInDirectProtoBody(message, account_id, direct_body_replacements) && direct_body_replacements > 0) {
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=direct_body",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            direct_body_replacements,
            1u
        );
        return true;
    }

    if (!full_message_parse_ok) {
        GBE_GC_DebugLog(
            log_scope,
            "account_id semantic rewrite skipped full parse req=%u resp=%u note=%s account_id=%u encoded_size=%zu donor_size=%zu",
            request_emsg,
            response_emsg,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size()
        );
    }

    GBE_GC_DebugLog(
        log_scope,
        "no semantic account_id varint replacements req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu target_field=%u",
        request_emsg,
        response_emsg,
        body_size,
        context_note ? context_note : "",
        account_id,
        encoded_account_raw.size(),
        GBE_kOldDotaAccountIdVarint.size(),
        1u
    );
    return true;
}

bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope)
{
    const std::vector<uint8> old_account_id_fixed32 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdFixed32.data(), GBE_kOldDotaAccountIdFixed32.size());
    size_t match_count = 0;
    if (!gbe::proto_wire::patch_fixed32_template_value(message, old_account_id_fixed32, account_id, match_count)) {
        GBE_GC_DebugLog(log_scope, "failed replacing account_id fixed32 bytes account_id=%u matches=%zu", account_id, match_count);
        return false;
    }

    return true;
}

std::string GBE_DotaCustomGameDisplayName(class Settings *settings, const GBE_DotaCustomGameDetails &custom_game, const std::string &fallback)
{
    if (settings && custom_game.game_id != 0ull && settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id))) {
        Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
        std::string display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "display_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", "");
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
    }

    return gbe::dota_custom_game::custom_game_display_name_from_details(custom_game, fallback);
}

bool GBE_PatchDotaTemplateIdentifiers(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    if (replace_account) {
        if (!GBE_TryPatchDotaAccountIdVarint(message, account_id, "GC_DOTA_PATCH", request_emsg, response_emsg, body_size, context_note))
            return false;
        if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH"))
            return false;
    }

    if (replace_steam_id) {
        const std::vector<uint8> old_steam_id_varint = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size());
        size_t steam_id_match_count = 0;
        bool steam_id_size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(message, old_steam_id_varint, steam_id, steam_id_match_count, steam_id_size_ok)) {
            if (!steam_id_size_ok) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped due to size mismatch req=%u resp=%u note=%s steam_id=%llu encoded_expected=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
                return false;
            }

            if (steam_id_match_count == 0) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped; donor does not expose expected varint req=%u resp=%u note=%s steam_id=%llu expected_size=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
            } else {
                return false;
            }
        }
    }

    return true;
}

void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return;

    GBE_GC_DebugLog(
        tag,
        "%s owner_type=%u owner_id=%llu objects=%d version_present=%u version=%llu service_id_present=%u service_id=%u service_list_count=%d sync_version_present=%u sync_version=%llu",
        label ? label : "dota_cache_subscribed_summary",
        protomsg.has_owner_soid() ? protomsg.owner_soid().type() : 0u,
        static_cast<unsigned long long>(protomsg.has_owner_soid() ? protomsg.owner_soid().id() : 0ull),
        protomsg.objects_size(),
        protomsg.has_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_version() ? protomsg.version() : 0ull),
        protomsg.has_service_id() ? 1u : 0u,
        protomsg.has_service_id() ? protomsg.service_id() : 0u,
        protomsg.service_list_size(),
        protomsg.has_sync_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_sync_version() ? protomsg.sync_version() : 0ull)
    );

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        GBE_GC_DebugLog(
            tag,
            "%s object[%d] type=%d object_data_count=%d",
            label ? label : "dota_cache_subscribed_summary",
            object_index,
            object.type_id(),
            object.object_data_size()
        );

        for (int data_index = 0; data_index < object.object_data_size(); ++data_index) {
            const std::string &object_data = object.object_data(data_index);
            GBE_GC_DebugLog(
                tag,
                "%s object[%d] data[%d] size=%zu",
                label ? label : "dota_cache_subscribed_summary",
                object_index,
                data_index,
                object_data.size()
            );

            if (object.type_id() == 2004) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint64 lobby_id = 0;
                uint32 lobby_state = 0;
                std::string connect;
                uint64 server_id = 0;
                uint32 game_state = 0;
                uint64 match_id = 0;
                uint32 game_start_time = 0;
                const uint32 team_details_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 17u);
                std::string owner_state;
                const bool has_connect = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 5u, connect);
                const bool has_owner_state = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 120u, owner_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 1u, lobby_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 4u, lobby_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 6u, server_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 22u, game_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 30u, match_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 87u, game_start_time);
                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2004 lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu game_start_time=%u connect=%s team_details=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    static_cast<unsigned long long>(lobby_id),
                    lobby_state,
                    game_state,
                    static_cast<unsigned long long>(match_id),
                    static_cast<unsigned long long>(server_id),
                    game_start_time,
                    has_connect ? connect.c_str() : "",
                    team_details_count
                );
                if (has_owner_state) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2004 owner_state{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_lobby_member_state_summary(owner_state).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2014) {
                const uint32 member_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 1u);
                std::string first_member;
                if (!gbe::proto_wire::read_bytes_field(reinterpret_cast<const uint8 *>(object_data.data()), object_data.size(), 1u, first_member)) {
                    first_member.clear();
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2014 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2014 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_static_lobby_member_summary(first_member).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2016) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint32 member_count = 0;
                std::string first_member;
                size_t offset = 0;
                while (offset < object_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(object_bytes, object_size, offset, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 2u) {
                        ++member_count;
                        if (first_member.empty())
                            first_member.assign(object_data.data() + field.value_offset, field.value_size);
                    }
                    offset = field_end;
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2016 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2016 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_server_static_lobby_member_summary(first_member).c_str()
                    );
                }
            }
        }
    }
}

bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(
    const char *wrapped_template_hex,
    const char *stage_note,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 lobby_state,
    uint32 lobby_game_state,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (!wrapped_template_hex)
        return false;

    std::string wrapped_message;
    if (!gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message))
        return false;

    std::string inner_payload;
    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaPracticeLobbyDetailsUpdate, inner_payload))
        return false;

    if (!GBE_PatchDotaPracticeLobbyLaunchTemplate(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            server_id,
            match_id,
            game_start_time,
            connect,
            true,
            true,
            true,
            stage_note))
        return false;

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            true,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            std::vector<GBE_DotaLobbyMemberState>(),
            rewrite_2015,
            extra_startup_account_id,
            pass_key,
            custom_game))
        return false;

    message.swap(inner_payload);
    return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
}

bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    bool patch_server_id,
    std::string &message)
{
    if (!gbe::proto_wire::decode_hex_string(template_hex, message))
        return false;

    const std::vector<std::string> old_lobby_id_texts = {
        GBE_kOldDotaPracticeLobbyLobbyIdText,
        GBE_kOldDotaPracticeLobbyLobbyIdTextAlt,
    };
    gbe::proto_wire::DotaPracticeLobbyPeripheralTemplatePatchResult patch_result{};
    if (!gbe::proto_wire::patch_dota_practice_lobby_peripheral_template(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPersonaSteamIdFixed64.data(), GBE_kOldDotaPersonaSteamIdFixed64.size()),
            steam_id,
            patch_server_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
            server_id,
            old_lobby_id_texts,
            lobby_id,
            patch_result))
        return false;

    return true;
}

bool GBE_PrepareDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(template_hex, steam_id, lobby_id, 0u, false, message);
}

void GBE_LogDotaResponsePacket(
    const char *reason,
    uint32 inner_emsg,
    bool wrapped,
    const std::string &inner_payload,
    const std::string &outbound_payload,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state)
{
    const std::string body_prefix = gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(inner_payload.data()), inner_payload.size(), 32);
    const std::string packet_prefix = wrapped
        ? gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(outbound_payload.data()), outbound_payload.size(), 32)
        : "-";
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent response reason=%s path=%s inner_emsg=%u wrapped=%u payload_size=%zu lobby_id=%llu state=%u game_state=%u body_prefix=%s packet_prefix=%s",
        reason ? reason : "unknown",
        wrapped ? "wrapped" : "direct",
        inner_emsg,
        wrapped ? 1u : 0u,
        outbound_payload.size(),
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        body_prefix.c_str(),
        packet_prefix.c_str());
}

bool GBE_AdaptDotaJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 generic_lobby_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    uint64 owner_steam_id,
    const std::string &owner_name,
    uint32 channel_type,
    std::string &message)
{
    std::vector<GBE_DotaChatMemberState> resolved_remote_names;
    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == steam_id)
            continue;
        std::string generic_member_name;
        std::string friend_member_name;
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking && generic_lobby_id != 0ull) {
            CSteamID generic_lobby((uint64)generic_lobby_id);
            CSteamID member_id((uint64)channel_member.steam_id);
            if (generic_lobby.IsLobby() && member_id.IsValid()) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, member_id, GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    generic_member_name = generic_name;
            }
        }

        if (steam_client && steam_client->steam_friends) {
            const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)channel_member.steam_id));
            if (friend_name && friend_name[0] != '\0')
                friend_member_name = friend_name;
        }

        resolved_remote_names.push_back({
            channel_member.steam_id,
            gbe::dota_lobby_flow::resolve_chat_member_display_name(
                channel_member.steam_id,
                steam_id,
                player_name,
                owner_steam_id,
                owner_name,
                generic_member_name,
                friend_member_name,
                std::string()) });
    }

    const std::vector<GBE_DotaChatMemberState> chat_member_states = gbe::dota_lobby_flow::compose_join_chat_channel_members(
        steam_id,
        player_name,
        channel_members,
        owner_steam_id,
        owner_name,
        resolved_remote_names);
    std::vector<gbe::gc_message::DotaChatMember> chat_members;
    chat_members.reserve(chat_member_states.size());
    for (const GBE_DotaChatMemberState &chat_member : chat_member_states) {
        chat_members.push_back({ chat_member.steam_id, chat_member.name });
    }

    return gbe::gc_message::build_dota_join_chat_channel_response_payload(channel_id, channel_name, chat_members, channel_type, message);
}

bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason)
{
    if (!target_gc || !player_steam_id.BIndividualAccount())
        return false;

    std::vector<const Econ_Item *> equipped_items;
    for (const Econ_Item &item : source_items) {
        if (!item.equip_states.empty())
            equipped_items.push_back(&item);
    }

    if (equipped_items.empty())
        return false;

    const uint64 player_steam64 = player_steam_id.ConvertToUint64();

    if (unsubscribe_first) {
        std::string unsub_message;
        gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(1u, player_steam64, unsub_message);
        target_gc->push_incoming_message(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, unsub_message);
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "pushed player CacheUnsubscribed to target GC: steam64=%llu reason=%s message_size=%zu",
            static_cast<unsigned long long>(player_steam64),
            reason ? reason : "unknown",
            unsub_message.size()
        );
    }

    std::string owner_soid;
    gbe::proto_wire::append_varint_field(owner_soid, 1u, 1u);
    gbe::proto_wire::append_varint_field(owner_soid, 2u, player_steam64);

    std::string subscribed_type;
    gbe::proto_wire::append_varint_field(subscribed_type, 1u, 1u);
    for (const Econ_Item *ep : equipped_items) {
        std::string serialized = target_gc->serialize_item_to_gcprotobuf(*ep, player_steam_id);
        gbe::proto_wire::append_bytes_field(subscribed_type, 2u, serialized);
    }

    std::string cache_body;
    gbe::proto_wire::append_bytes_field(cache_body, 2u, subscribed_type);
    gbe::proto_wire::append_fixed64_field(cache_body, 3u, 1ull);
    gbe::proto_wire::append_bytes_field(cache_body, 4u, owner_soid);

    std::string cache_message;
    gbe::gc_message::build_dota_zero_header_payload(GBE_kDotaCacheSubscribed, cache_body, cache_message);
    target_gc->push_incoming_message(GBE_kDotaCacheSubscribed | GBE_kProtoMask, cache_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "pushed player item CacheSubscribed to target GC: steam64=%llu equipped_items=%zu reason=%s message_size=%zu unsub_first=%u",
        static_cast<unsigned long long>(player_steam64),
        equipped_items.size(),
        reason ? reason : "unknown",
        cache_message.size(),
        unsubscribe_first ? 1u : 0u
    );
    return true;
}

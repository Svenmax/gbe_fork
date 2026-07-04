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
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

// Type aliases for proto-wire request/response shapes. These mirror the
// aliases defined in steam_game_coordinator.cpp and are needed by the
// handlers below. They are alias-declarations (no linkage), so duplicating
// them across TUs is safe.
using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

using GBE_DotaPracticeLobbyBroadcastChannelRequest = gbe::proto_wire::DotaPracticeLobbyBroadcastChannelRequest;

using GBE_DotaJoinChatChannelRequest = gbe::proto_wire::DotaJoinChatChannelRequest;
using GBE_DotaLeaveChatChannelRequest = gbe::proto_wire::DotaLeaveChatChannelRequest;
using GBE_DotaChatMessageRequest = gbe::proto_wire::DotaChatMessageRequest;
using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;


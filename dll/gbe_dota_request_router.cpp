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

// Phase 1.1 of the Steam_Game_Coordinator refactor.
//
// Out-of-line implementation of GBE_ParseDirectProtoContext, separated from
// gbe_dota_request_router.h because it calls CMsgProtoBufHeader methods
// (Clear / ParseFromArray). Keeping the protobuf-generated header out of the
// router's public header avoids forcing every includer to compile the heavy
// steammessages.pb.h.

#include "gbe_dota_request_router.h"

#include <steammessages.pb.h>

bool GBE_ParseDirectProtoContext(const void *pubData, std::uint32_t cubData, GBE_DirectProtoContext &context)
{
    context = {};
    context.protohdr.Clear();

    if (!pubData || cubData < sizeof(ProtoBufMsgHeader_t))
        return false;

    const std::uint8 *bytes = reinterpret_cast<const std::uint8 *>(pubData);
    std::memcpy(&context.hdr, bytes, sizeof(context.hdr));

    const std::size_t body_offset = sizeof(context.hdr) + context.hdr.m_cubProtoBufExtHdr;
    if (body_offset > cubData)
        return false;

    if (context.hdr.m_cubProtoBufExtHdr != 0 && !context.protohdr.ParseFromArray(bytes + sizeof(context.hdr), context.hdr.m_cubProtoBufExtHdr))
        return false;

    context.body_offset = body_offset;
    context.body = bytes + body_offset;
    context.body_size = static_cast<std::size_t>(cubData - body_offset);
    return true;
}

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

// ProtoBufMsgHeader_t: the wire header that precedes a protobuf-framed GC
// message. Extracted to its own header so gbe_dota_request_router.h can hold
// it as a value member of GBE_DirectProtoContext without pulling the rest of
// steam_game_coordinator.cpp's internals into a public header.
//
// The struct is packed to 1-byte alignment, matching the original definition
// that lived inside a `#pragma pack(push, 1)` block in steam_game_coordinator.cpp.

#ifndef GBE_PROTO_BUF_HEADER_H
#define GBE_PROTO_BUF_HEADER_H

#include <cstdint>

#pragma pack( push, 1 )
struct ProtoBufMsgHeader_t
{
    std::uint32_t m_EMsgFlagged;       // High bit should be set to indicate this message header type is in use.  The rest of the bits indicate message type.
    std::uint32_t m_cubProtoBufExtHdr; // Size of the extended header which is a serialized protobuf object.  Indicates where it ends and the serialized body protobuf begins.
};
#pragma pack(pop)

#endif // GBE_PROTO_BUF_HEADER_H

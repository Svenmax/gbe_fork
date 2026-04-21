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
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

constexpr int GC_MIN_VERSION = 20091217;
static constexpr uint32 GBE_kProtoMask = 0x80000000u;
static constexpr uint32 GBE_kEMsgClientToGC = 5452u;
static constexpr uint32 GBE_kEMsgClientFromGC = 5453u;
static constexpr uint32 GBE_kEMsgGCClientHello = 4006u;
static constexpr uint32 GBE_kEMsgGCClientWelcome = 4004u;
static constexpr uint32 GBE_kDotaAppId = 570u;
static constexpr size_t GBE_kDotaWelcomeInnerBodyOffset = 48u;
static constexpr const char *GBE_kGcDebugLogPath = "C:\\Users\\Public\\gbe_gc_debug.log";

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
static const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
static const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };

#pragma pack( push, 1 )
//-----------------------------------------------------------------------------
// Purpose: Header for messages from a client or gameserver to or from the GC
//-----------------------------------------------------------------------------
struct GCMsgHdr_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
};

//-----------------------------------------------------------------------------
// Purpose: Header for messages from a client or gameserver to or from the GC
//          That contains source and destination jobs for the purpose of
//          replying messages.
//-----------------------------------------------------------------------------
struct GCMsgHdrEx_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
    uint16  m_nHdrVersion;
    JobID_t m_JobIDTarget;
    JobID_t m_JobIDSource;
};

struct ProtoBufMsgHeader_t
{
    uint32          m_EMsgFlagged;          // High bit should be set to indicate this message header type is in use.  The rest of the bits indicate message type.
    uint32          m_cubProtoBufExtHdr;    // Size of the extended header which is a serialized protobuf object.  Indicates where it ends and the serialized body protobuf begins.
};
#pragma pack(pop)

template <class T>
static void ser_var(std::string &buf, const T &input)
{
    buf.append(reinterpret_cast<const char *>(&input), sizeof(T));
}

static void ser_varstring(std::string &buf, const std::string &input)
{
    uint16 len = static_cast<uint16>(input.size());
    if (len != 0) {
        ser_var<uint16>(buf, len + 1);
        buf.append(input + '\0');
    } else {
        ser_var<uint16>(buf, 0);
    }
}

template <class T>
static T deser_var(const char *&p)
{
    T output;
    memcpy(&output, p, sizeof(T));
    p += sizeof(T);
    return output;
}

struct GBE_ProtoFieldView
{
    bool found{};
    uint32 field_number{};
    uint32 wire_type{};
    size_t value_offset{};
    size_t value_size{};
};

struct GBE_DotaHelloContext
{
    bool valid{};
    uint32 version{};
    std::string outer_session_field_raw;
    uint64 source_job_id{};
    bool has_source_job{};
};

static void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    FILE *file = std::fopen(GBE_kGcDebugLogPath, "a");
    if (!file)
        return;

    std::fprintf(file, "[%s] ", scope ? scope : "GC");

    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);

    std::fprintf(file, "\n");
    std::fclose(file);
}

static uint32 GBE_GC_MaskedEMsg(uint32 msg_type)
{
    return msg_type & (~GBE_kProtoMask);
}

static bool GBE_ReadVarUint64(const uint8 *data, size_t size, size_t &offset, uint64 &value, size_t *raw_begin = nullptr, size_t *raw_end = nullptr)
{
    if (!data || offset >= size)
        return false;

    size_t begin = offset;
    value = 0;
    int shift = 0;

    while (offset < size && shift <= 63) {
        const uint8 byte = data[offset++];
        value |= static_cast<uint64>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) {
            if (raw_begin)
                *raw_begin = begin;
            if (raw_end)
                *raw_end = offset;
            return true;
        }

        shift += 7;
    }

    return false;
}

static GBE_ProtoFieldView GBE_FindProtoField(const uint8 *data, size_t size, uint32 wanted_field)
{
    GBE_ProtoFieldView view{};
    size_t offset = 0;

    while (offset < size) {
        uint64 key = 0;
        if (!GBE_ReadVarUint64(data, size, offset, key))
            return {};

        const uint32 field_number = static_cast<uint32>(key >> 3);
        const uint32 wire_type = static_cast<uint32>(key & 0x7);
        size_t value_offset = offset;
        size_t value_size = 0;

        switch (wire_type) {
            case 0: {
                size_t raw_begin = offset;
                size_t raw_end = offset;
                uint64 ignored = 0;
                if (!GBE_ReadVarUint64(data, size, offset, ignored, &raw_begin, &raw_end))
                    return {};
                value_size = raw_end - raw_begin;
                break;
            }
            case 1:
                if (offset + 8 > size)
                    return {};
                offset += 8;
                value_size = 8;
                break;
            case 2: {
                uint64 length = 0;
                if (!GBE_ReadVarUint64(data, size, offset, length))
                    return {};
                if (offset + length > size)
                    return {};
                value_offset = offset;
                value_size = static_cast<size_t>(length);
                offset += static_cast<size_t>(length);
                break;
            }
            case 5:
                if (offset + 4 > size)
                    return {};
                offset += 4;
                value_size = 4;
                break;
            default:
                return {};
        }

        if (field_number == wanted_field) {
            view.found = true;
            view.field_number = field_number;
            view.wire_type = wire_type;
            view.value_offset = value_offset;
            view.value_size = value_size;
            return view;
        }
    }

    return {};
}

static void GBE_AppendVarUint64(std::string &buffer, uint64 value)
{
    do {
        uint8 byte = static_cast<uint8>(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buffer.push_back(static_cast<char>(byte));
    } while (value != 0);
}

static void GBE_AppendLittleEndian32(std::string &buffer, uint32 value)
{
    ser_var<uint32>(buffer, value);
}

static void GBE_AppendProtoVarIntField(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 0u);
    GBE_AppendVarUint64(buffer, value);
}

static void GBE_AppendProtoFixed64Field(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 1u);
    ser_var<uint64>(buffer, value);
}

static void GBE_AppendProtoBytesField(std::string &buffer, uint32 field_number, const std::string &value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 2u);
    GBE_AppendVarUint64(buffer, static_cast<uint64>(value.size()));
    buffer.append(value);
}

static std::vector<uint8> GBE_VectorFromBytes(const uint8 *data, size_t size)
{
    return std::vector<uint8>(data, data + size);
}

static bool GBE_FindAndReplaceBytes(std::string &buffer, const std::vector<uint8> &needle, const std::vector<uint8> &replacement)
{
    if (needle.empty() || needle.size() != replacement.size())
        return false;

    bool replaced = false;
    auto search_begin = buffer.begin();

    while (search_begin != buffer.end()) {
        auto found = std::search(search_begin, buffer.end(), needle.begin(), needle.end());
        if (found == buffer.end())
            break;

        std::copy(replacement.begin(), replacement.end(), found);
        search_begin = found + replacement.size();
        replaced = true;
    }

    return replaced;
}

static bool GBE_EncodeVarUint64WithExpectedSize(uint64 value, size_t expected_size, std::vector<uint8> &encoded)
{
    std::string encoded_raw;
    GBE_AppendVarUint64(encoded_raw, value);
    if (encoded_raw.size() != expected_size)
        return false;

    encoded.assign(encoded_raw.begin(), encoded_raw.end());
    return true;
}

static bool GBE_ExtractProtoFieldUint64(const uint8 *data, size_t size, const GBE_ProtoFieldView &view, uint64 &value)
{
    if (!view.found)
        return false;

    if (view.wire_type == 0) {
        size_t offset = view.value_offset;
        return GBE_ReadVarUint64(data, size, offset, value);
    }

    if (view.wire_type == 1 && view.value_size == 8) {
        std::memcpy(&value, data + view.value_offset, sizeof(value));
        return true;
    }

    return false;
}

static bool GBE_ExtractDotaHelloContext(const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
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

    GBE_ProtoFieldView outer_session_field = GBE_FindProtoField(outer_header, outer_header_length, 2);
    if (!outer_session_field.found) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "missing outer session field");
        return false;
    }
    context.outer_session_field_raw.assign(
        reinterpret_cast<const char *>(outer_header + outer_session_field.value_offset),
        outer_session_field.value_size
    );

    GBE_ProtoFieldView payload_field = GBE_FindProtoField(outer_body, outer_body_size, 3);
    if (!payload_field.found || payload_field.wire_type != 2 || payload_field.value_size < 8) {
        GBE_GC_DebugLog(
            "GC_DOTA_HELLO",
            "invalid payload field found=%d wire=%u size=%zu",
            payload_field.found ? 1 : 0,
            payload_field.wire_type,
            payload_field.value_size
        );
        return false;
    }

    const uint8 *payload = outer_body + payload_field.value_offset;
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
    if (inner_body_offset > payload_field.value_size) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "inner body offset overflow header_len=%u payload_size=%zu", inner_header_length, payload_field.value_size);
        return false;
    }

    const uint8 *inner_header = payload + inner_header_offset;
    const uint8 *inner_body = payload + inner_body_offset;
    const size_t inner_body_size = payload_field.value_size - inner_body_offset;

    GBE_ProtoFieldView version_field = GBE_FindProtoField(inner_body, inner_body_size, 1);
    uint64 parsed_version = 0;
    if (!GBE_ExtractProtoFieldUint64(inner_body, inner_body_size, version_field, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "failed to extract version field");
        return false;
    }

    context.version = static_cast<uint32>(parsed_version);

    if (inner_header_length > 0) {
        GBE_ProtoFieldView source_job_field = GBE_FindProtoField(inner_header, inner_header_length, 11);
        uint64 source_job = 0;
        if (GBE_ExtractProtoFieldUint64(inner_header, inner_header_length, source_job_field, source_job)) {
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

static bool GBE_BuildDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    if (!context.valid || GBE_kDotaWelcomeInnerBodyOffset >= sizeof(GBE_kDotaClientWelcomeTemplate)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "invalid context valid=%d offset=%zu template_size=%zu", context.valid ? 1 : 0, GBE_kDotaWelcomeInnerBodyOffset, sizeof(GBE_kDotaClientWelcomeTemplate));
        return false;
    }

    std::string inner_body(
        reinterpret_cast<const char *>(GBE_kDotaClientWelcomeTemplate + GBE_kDotaWelcomeInnerBodyOffset),
        sizeof(GBE_kDotaClientWelcomeTemplate) - GBE_kDotaWelcomeInnerBodyOffset
    );

    {
        std::vector<uint8> encoded_version;
        if (!GBE_EncodeVarUint64WithExpectedSize(context.version, GBE_kOldDotaVersionVarint.size(), encoded_version)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "version varint size mismatch version=%u expected=%zu", context.version, GBE_kOldDotaVersionVarint.size());
            return false;
        }
        if (!GBE_FindAndReplaceBytes(inner_body, GBE_VectorFromBytes(GBE_kOldDotaVersionVarint.data(), GBE_kOldDotaVersionVarint.size()), encoded_version)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing version bytes version=%u", context.version);
            return false;
        }
    }

    {
        std::vector<uint8> encoded_account;
        if (!GBE_EncodeVarUint64WithExpectedSize(account_id, GBE_kOldDotaAccountIdVarint.size(), encoded_account)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "account_id varint size mismatch account_id=%u expected=%zu", account_id, GBE_kOldDotaAccountIdVarint.size());
            return false;
        }
        if (!GBE_FindAndReplaceBytes(inner_body, GBE_VectorFromBytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()), encoded_account)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing account_id bytes account_id=%u", account_id);
            return false;
        }
    }

    {
        std::vector<uint8> encoded_steam_id;
        if (!GBE_EncodeVarUint64WithExpectedSize(steam_id, GBE_kOldDotaSteamIdVarint.size(), encoded_steam_id)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "steam_id varint size mismatch steam_id=%llu expected=%zu", static_cast<unsigned long long>(steam_id), GBE_kOldDotaSteamIdVarint.size());
            return false;
        }
        if (!GBE_FindAndReplaceBytes(inner_body, GBE_VectorFromBytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size()), encoded_steam_id)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing steam_id bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
            return false;
        }
    }

    std::string inner_header;
    if (context.has_source_job) {
        GBE_AppendProtoVarIntField(inner_header, 10, context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "mirroring source_job=%llu into inner target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    std::string inner_payload;
    GBE_AppendLittleEndian32(inner_payload, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    GBE_AppendLittleEndian32(inner_payload, static_cast<uint32>(inner_header.size()));
    inner_payload.append(inner_header);
    inner_payload.append(inner_body);

    std::string outer_body;
    GBE_AppendProtoVarIntField(outer_body, 1, app_id);
    GBE_AppendProtoVarIntField(outer_body, 2, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    GBE_AppendProtoBytesField(outer_body, 3, inner_payload);

    std::string outer_header;
    // The outer fixed64 steamid lives outside the inner template, so we rebuild that header directly.
    GBE_AppendProtoFixed64Field(outer_header, 1, steam_id);
    GBE_AppendVarUint64(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(context.outer_session_field_raw);

    message.clear();
    GBE_AppendLittleEndian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, static_cast<uint32>(outer_header.size()));
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

bool Steam_Game_Coordinator::gc_enabled()
{
    if (gc_profile == GC_PROFILE_DOTA2)
        return settings && settings->get_local_game_id().AppID() == GBE_kDotaAppId;

    return (gc_version >= GC_MIN_VERSION && gc_profile != GC_PROFILE_INVALID);
}

Steam_User_Items *Steam_Game_Coordinator::client_items()
{
    return get_steam_client()->steam_user_items;
}

Steam_GameServer_Items *Steam_Game_Coordinator::server_items()
{
    return get_steam_client()->steam_gameserver_items;
}

void Steam_Game_Coordinator::parse_gc_config()
{
    gc_profile = GC_PROFILE_INVALID;
    gc_version = 0;

    std::string file_path = Local_Storage::get_game_settings_path() + gc_config_file;
    nlohmann::json gc_json;
    if (!local_storage->load_json(file_path, gc_json)) {
        if (settings->get_local_game_id().AppID() == GBE_kDotaAppId) {
            gc_profile = GC_PROFILE_DOTA2;
            GBE_GC_DebugLog("GC_CONFIG", "auto-enabled Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
        }
        return;
    }

    try {
        std::string gc_profile_name = gc_json.value("gc_profile", std::string());
        std::transform(gc_profile_name.begin(), gc_profile_name.end(), gc_profile_name.begin(),
            [](auto c) { return std::tolower(c); });
        if (gc_profile_name == "tf2") {
            gc_profile = GC_PROFILE_TF2;
        } else if (gc_profile_name == "dota2" || gc_profile_name == "dota") {
            gc_profile = GC_PROFILE_DOTA2;
        } else if (gc_profile_name == "portal2") {
            // Portal 2 is pretty much entirely compatible with TF2 protobuf structs so we can just
            // make it an alias for TF2 profile.
            //gc_profile = GC_PROFILE_PORTAL2;
            gc_profile = GC_PROFILE_TF2;
            is_portal2 = true;
        } else {
            gc_profile = GC_PROFILE_INVALID;
        }

        gc_version = gc_json.value("gc_version", 0);
    } catch (std::exception &e) {
        const char *errorMessage = e.what();
        PRINT_DEBUG("error parsing GC config: %s", errorMessage);
        gc_version = 0;
        gc_profile = GC_PROFILE_INVALID;
    }

    if (gc_profile == GC_PROFILE_INVALID && settings->get_local_game_id().AppID() == GBE_kDotaAppId) {
        gc_profile = GC_PROFILE_DOTA2;
        GBE_GC_DebugLog("GC_CONFIG", "fell back to Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
    }
}

bool Steam_Game_Coordinator::is_welcome_message(const GC_Message &message)
{
    uint32 msg_type = message.msg_type & (~protobuf_mask);
    return (msg_type == 4004 ||
        msg_type == 4005);
}

void Steam_Game_Coordinator::push_incoming(uint32 msg_type, const std::string &message, double delay)
{
    PRINT_DEBUG("%u %.2f", msg_type, delay);

    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = delay;
    pending_messages.push_back(new_item);
}

void Steam_Game_Coordinator::push_incoming_now(uint32 msg_type, const std::string &message)
{
    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = 0.0;
    incoming_messages.push(new_item);

    GCMessageAvailable_t data{};
    data.m_nMessageSize = static_cast<uint32>(message.size());
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

    GBE_GC_DebugLog("GC_CALLBACK", "queued msg=%u size=%u and posted GCMessageAvailable_t", GBE_GC_MaskedEMsg(msg_type), static_cast<uint32>(message.size()));
}

std::string Steam_Game_Coordinator::build_msg_header(JobID_t target_job, JobID_t source_job)
{
    std::string message;
    GCMsgHdrEx_t hdr{};
    hdr.m_nHdrVersion = 1;
    hdr.m_JobIDTarget = target_job;
    hdr.m_JobIDSource = source_job;
    ser_var<GCMsgHdrEx_t>(message, hdr);
    return message.substr(sizeof(GCMsgHdr_t));
}

GCMsgHdrEx_t Steam_Game_Coordinator::parse_msg_header(const char *&p)
{
    size_t write_offset = sizeof(GCMsgHdr_t);
    size_t hdr_size = sizeof(GCMsgHdrEx_t) - write_offset;
    GCMsgHdrEx_t hdr{};
    memcpy(reinterpret_cast<char *>(&hdr) + write_offset, p, hdr_size);
    p += hdr_size;
    return hdr;
}

std::string Steam_Game_Coordinator::build_protomsg_header(uint32 msg_type, JobID_t target_job, JobID_t source_job)
{
    std::string message;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = msg_type;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(settings->get_local_steam_id().ConvertToUint64());
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(settings->get_local_game_id().AppID());
    protohdr.set_job_id_source(source_job);
    protohdr.set_job_id_target(target_job);
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    return message;
}

template <class T>
std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, T, bool> Steam_Game_Coordinator::parse_protomsg(const void *input, uint32 input_size)
{
    const char *p = reinterpret_cast<const char *>(input);
    const char *end = p + input_size;

    ProtoBufMsgHeader_t hdr{};
    CMsgProtoBufHeader protohdr;
    T protomsg;

    if (input_size < sizeof(ProtoBufMsgHeader_t))
        return { hdr, protohdr, protomsg, false };

    hdr = deser_var<ProtoBufMsgHeader_t>(p);

    if (!protohdr.ParseFromArray(p, hdr.m_cubProtoBufExtHdr))
        return { hdr, protohdr, protomsg, false };

    p += hdr.m_cubProtoBufExtHdr;

    int protomsg_size = static_cast<int>(end - p);
    if (!protomsg.ParseFromArray(p, protomsg_size))
        return { hdr, protohdr, protomsg, false };

    return { hdr, protohdr, protomsg, true };
}

uint64 Steam_Game_Coordinator::item_id_local_to_network(uint64 item_id)
{
    if (item_id == 0)
        return 0;

    // Add SteamID to item ID to avoid ID collisions in multiplayer games.
    uint32 account_id = settings->get_local_steam_id().GetAccountID();

    if (settings->use_32bit_inventory_item_ids) {
        // 32-bit mode
        item_id <<= 20ull;
        item_id |= static_cast<uint64>(account_id) & 0x000FFFFFull;
    } else {
        // 64-bit mode
        item_id <<= 32ull;
        item_id |= static_cast<uint64>(account_id);
    }

    return item_id;
}

uint64 Steam_Game_Coordinator::item_id_network_to_local(uint64 item_id)
{
    if (settings->use_32bit_inventory_item_ids) {
        // 32-bit mode
        item_id >>= 20ull;
    } else {
        // 64-bit mode
        item_id >>= 32ull;
    }

    return item_id;
}

std::string Steam_Game_Coordinator::item_to_gcstruct(const Econ_Item &item, CSteamID steam_id)
{
    std::string message;

    ser_var<uint64>(message, item.id);
    ser_var<uint32>(message, steam_id.GetAccountID());
    ser_var<uint16>(message, item.def);
    ser_var<uint8>(message, item.level);
    ser_var<uint8>(message, item.quality);
    ser_var<uint32>(message, item.inv_pos);
    ser_var<uint32>(message, item.quantity);

    if (gc_version >= 20100428) {
        // Strings are passed as UTF-8 which is good for us since we can just copy std::string as is.
        ser_varstring(message, item.custom_name);

        if (gc_version >= 20100930) {
            ser_var<uint8>(message, item.flags);

            if (gc_version >= 20101027) {
                ser_var<uint8>(message, item.origin);
                ser_varstring(message, item.custom_desc);
                ser_var<bool>(message, item.in_use);
            }
        }
    }

    ser_var<uint16>(message, static_cast<uint16>(item.attributes.size()));

    for (const Econ_Item_Attribute &attr : item.attributes) {
        ser_var<uint16>(message, attr.def);
        ser_var<float>(message, attr.value);
    }

    if (gc_version >= 20101217) {
        ser_var<uint64>(message, item.original_id);
    }

    return message;
}

std::string Steam_Game_Coordinator::item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id)
{
    CSOEconItem proto_item;
    proto_item.set_id(item.id);
    proto_item.set_account_id(steam_id.GetAccountID());
    proto_item.set_inventory(item.inv_pos);
    proto_item.set_def_index(item.def);
    proto_item.set_quantity(item.quantity);
    proto_item.set_level(item.level);
    proto_item.set_quality(item.quality);
    proto_item.set_flags(item.flags);
    proto_item.set_origin(item.origin);

    if (!item.custom_name.empty())
        proto_item.set_custom_name(item.custom_name);

    if (!item.custom_desc.empty())
        proto_item.set_custom_desc(item.custom_desc);

    proto_item.set_in_use(item.in_use);
    proto_item.set_style(item.style);
    proto_item.set_original_id(item.original_id);

    proto_item.set_contains_equipped_state(true);
    proto_item.set_contains_equipped_state_v2(true);

    for (const auto &[class_id, slot_id] : item.equip_states) {
        auto proto_equip = proto_item.add_equipped_state();
        proto_equip->set_new_class(class_id);
        proto_equip->set_new_slot(slot_id);
    }

    for (const Econ_Item_Attribute &attr : item.attributes) {
        auto proto_attr = proto_item.add_attribute();
        proto_attr->set_def_index(attr.def);
        if (gc_version < 20130319 || is_portal2) {
            // Derp.
            uint32 value;
            memcpy(&value, &attr.value, sizeof(uint32));
            proto_attr->set_value(value);
        } else {
            proto_attr->set_value_bytes(attr.value_bytes);
        }
    }

    return proto_item.SerializeAsString();
}

void Steam_Game_Coordinator::handle_set_item_pos(const void *input, uint32 input_size)
{
    if (is_server || input_size < 30)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    uint32 inv_pos = deser_var<uint32>(p);
    PRINT_DEBUG("%llu %u", item_id, inv_pos);

    if (const Econ_Item *item = set_item_pos(item_id, inv_pos, true)) {
        callback_item_updated(settings->get_local_steam_id(), *item);
    }
}

void Steam_Game_Coordinator::handle_delete_item(const void *input, uint32 input_size)
{
    if (is_server || input_size < 26)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    PRINT_DEBUG("%llu", item_id);

    if (delete_item(item_id, true)) {
        callback_item_deleted(settings->get_local_steam_id(), item_id);
    }
}

void Steam_Game_Coordinator::handle_motd_request(const void *input, uint32 input_size)
{
    if (is_server || input_size < 24)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint32 last_req_time = deser_var<uint32>(p);
    uint16 language = deser_var<uint16>(p);
    PRINT_DEBUG("%u %u", last_req_time, language);

    uint32 msg_type = EGCItemMsg::k_EMsgGCMOTDRequestResponse;
    std::string message = build_msg_header();
    uint16 num_entries = 0;
    ser_var<uint16>(message, num_entries);

    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::handle_respawn(const void *input, uint32 input_size)
{
    if (is_server || input_size < 19)
        return;

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Request_Respawn);
    gameserver_items_msg->set_is_gc(true);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    network->sendToAllGameservers(&msg, true);
}

void Steam_Game_Coordinator::handle_set_item_style(const void *input, uint32 input_size)
{
    if (is_server || input_size < 27)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    uint8 style = deser_var<uint8>(p);
    PRINT_DEBUG("%llu %u", item_id, style);

    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        item.style = style;
        save_items_to_file();

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item_id);
        inventory_msg->set_style(style);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(true);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        callback_item_updated(settings->get_local_steam_id(), item);
        break;
    }
}

void Steam_Game_Coordinator::handle_adjust_equip_state(const void *input, uint32 input_size)
{
    if (is_server)
        return;

    auto [hdr, protohdr, protomsg, success] = parse_protomsg<CMsgAdjustItemEquippedState>(input, input_size);
    if (!success)
        return;

    uint64 item_id = protomsg.item_id();
    uint32 new_class_id = protomsg.new_class();
    uint32 new_slot_id = protomsg.new_slot();
    PRINT_DEBUG("%llu %u %u", item_id, new_class_id, new_slot_id);

    for (Econ_Item &item : items) {
        if (item_id != UINT64_MAX && item.id == item_id) {
            // Equip the item into this slot.
            item.equip_states.insert_or_assign(new_class_id, new_slot_id);
        } else {
            // Unequip whatever else we had in this slot.
            auto it = item.equip_states.find(new_class_id);
            if (it == item.equip_states.end() || it->second != new_slot_id)
                continue;

            item.equip_states.erase(it);
        }

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item.id);
        inventory_msg->set_has_equip_states(true);
        for (const auto &[class_id, slot_id] : item.equip_states) {
            auto new_state = inventory_msg->add_equip_states();
            new_state->set_class_id(class_id);
            new_state->set_slot_id(slot_id);
        }

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(true);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        callback_item_updated(settings->get_local_steam_id(), item);
    }

    save_items_to_file();
}

void Steam_Game_Coordinator::handle_set_multiple_item_pos(const void *input, uint32 input_size)
{
    if (is_server)
        return;

    auto [hdr, protohdr, protomsg, success] = parse_protomsg<CMsgSetItemPositions>(input, input_size);
    if (!success)
        return;

    for (auto &entry : protomsg.item_positions()) {
        uint64 item_id = entry.item_id();
        uint32 inv_pos = entry.position();

        if (const Econ_Item *item = set_item_pos(item_id, inv_pos, true, false)) {
            callback_item_updated(settings->get_local_steam_id(), *item);
        }
    }

    save_items_to_file();
}

void Steam_Game_Coordinator::callback_client_welcome()
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCBaseClientMsg::k_EMsgGCClientWelcome | protobuf_mask;
    std::string message = build_protomsg_header(msg_type);

    CMsgClientWelcome protomsg;
    protomsg.set_version(0);

    protomsg.AppendToString(&message);
    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::callback_server_welcome()
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCBaseClientMsg::k_EMsgGCServerWelcome | protobuf_mask;
    std::string message = build_protomsg_header(msg_type);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(0);
    protomsg.set_active_version(0);

    protomsg.AppendToString(&message);
    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::callback_items_received(CSteamID steam_id, const std::vector<Econ_Item> &items)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheSubscribed;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint16 num_types = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint16>(message, num_types);

        // econ items (1)
        uint32 object_type = 1;
        uint16 num_objects = static_cast<uint16>(items.size());

        ser_var<uint32>(message, object_type);
        ser_var<uint16>(message, num_objects);

        for (const Econ_Item &item : items) {
            message.append(item_to_gcstruct(item, steam_id));
        }

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheSubscribed | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOCacheSubscribed protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        auto objects = protomsg.add_objects();
        objects->set_type_id(1);

        for (const Econ_Item &item : items) {
            objects->add_object_data(item_to_gcprotobuf(item, steam_id));
        }

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}

void Steam_Game_Coordinator::callback_items_removed(CSteamID steam_id)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheUnsubscribed;
        std::string message = build_msg_header();
        ser_var<uint64>(message, steam_id.ConvertToUint64());

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheUnsubscribed | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOCacheUnsubscribed protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}

void Steam_Game_Coordinator::callback_item_updated(CSteamID steam_id, const Econ_Item &item)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_Update;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint32 object_type = 1;
        uint8 num_fields = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint32>(message, object_type);
        ser_var<uint64>(message, item.id);
        ser_var<uint8>(message, num_fields);

        uint8 field_idx = 5;
        ser_var<uint8>(message, field_idx);
        ser_var<uint32>(message, item.inv_pos);

        if (gc_version >= 20101027) {
            ser_var<bool>(message, item.in_use);
        }

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_Update | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOSingleObject protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        protomsg.set_type_id(1);
        protomsg.set_object_data(item_to_gcprotobuf(item, steam_id));

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}

void Steam_Game_Coordinator::callback_item_deleted(CSteamID steam_id, uint64 item_id)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_Destroy;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint32 object_type = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint32>(message, object_type);
        ser_var<uint64>(message, item_id);

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_Destroy | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOSingleObject protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        protomsg.set_type_id(1);

        CSOEconItem proto_item;
        proto_item.set_id(item_id);
        protomsg.set_object_data(proto_item.SerializeAsString());

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}

void Steam_Game_Coordinator::callback_respawn_request(CSteamID steam_id)
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange;
    std::string message = build_msg_header();
    ser_var<uint64>(message, steam_id.ConvertToUint64());

    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::steam_network_callback(void *object, Common_Message *msg)
{
    //PRINT_DEBUG_ENTRY();

    auto inst = (Steam_Game_Coordinator *)object;
    inst->network_callback(msg);
}

void Steam_Game_Coordinator::steam_run_every_runcb(void *object)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Game_Coordinator *steam_gamecoordinator = (Steam_Game_Coordinator *)object;
    steam_gamecoordinator->RunCallbacks();
}

Steam_Game_Coordinator::Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, bool is_server)
{
    this->settings = settings;
    this->network = network;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;
    this->is_server = is_server;

    this->network->setCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->add(&Steam_Game_Coordinator::steam_run_every_runcb, this);

    parse_gc_config();
}

Steam_Game_Coordinator::~Steam_Game_Coordinator()
{
    this->network->rmCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->remove(&Steam_Game_Coordinator::steam_run_every_runcb, this);
}

void Steam_Game_Coordinator::initialize_gc()
{
    if (!gc_enabled() || gc_initialized)
        return;

    gc_initialized = true;

    if (gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog("GC_INIT", "initialized Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
        return;
    }

    if (is_server) {
        callback_server_welcome();
    } else {
        callback_client_welcome();

        // Load user's items.
        const auto &items = load_items_from_file();
        callback_items_received(settings->get_local_steam_id(), items);
    }

    // Wait a bit until after the game has received welcome message from us before posting anything else.
    // This avoids a race condition that can cause the game to receive inventory before parsing item schema.
    // For old versions of TF2, we can't do that because they instead have a different bug where receiving
    // the inventory late causes erroneous "new items" notifications.
    if (gc_version >= 20110414) {
        delay_init = true;
    }
}

void Steam_Game_Coordinator::shutdown_gc()
{
    if (!gc_initialized)
        return;

    items_loaded = false;
    items.clear();
    all_user_items.clear();
    pending_items_requests.clear();
    pending_messages.clear();
    while (incoming_messages.size())
        incoming_messages.pop();

    welcome_received = false;
    delay_init = false;
    gc_initialized = false;
}

const std::vector<Econ_Item> &Steam_Game_Coordinator::load_items_from_file()
{
    if (items_loaded)
        return items;

    items_loaded = true;

    nlohmann::json items_json;
    if (!local_storage->load_json_file("", items_user_file, items_json))
        return items;

    for (auto it = items_json.begin(); it != items_json.end(); it++) {
        Econ_Item new_item{};
        try {
            new_item.id = std::stoull(it.key());
        } catch (...) {
            continue;
        }
        if (new_item.id == 0)
            continue;

        try {
            new_item.def = it->value("definition", 0u); // 0 is a valid item definition
            new_item.level = it->value("level", 1u);
            new_item.quality = static_cast<EItemQuality>(it->value("quality", 0));
            new_item.inv_pos = it->value("inventory_pos", 0u);
            new_item.quantity = it->value("quantity", 1u);
            new_item.flags = it->value("flags", 0u);
            new_item.origin = it->value("origin", 0u);
            new_item.custom_name = it->value("custom_name", std::string());
            new_item.custom_desc = it->value("custom_desc", std::string());
            new_item.original_id = it->value("original_id", 0ull);
            new_item.style = it->value("style", 0u);
            new_item.in_use = false;

            if (it->contains("equip_states")) {
                for (const auto &equip : it->at("equip_states")) {
                    uint32 class_id = equip.value("class", 0u);
                    uint32 slot_id = equip.value("slot", 0u);

                    new_item.equip_states.insert({ class_id, slot_id });
                }
            }

            if (it->contains("attributes")) {
                for (const auto &attr : it->at("attributes")) {
                    Econ_Item_Attribute new_attr{};
                    new_attr.def = attr.value("definition", 0u);
                    if (new_attr.def == 0) // 0 is not a valid attribute definition, however
                        continue;

                    if (attr.contains("value")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_DEFAULT;
                        float value = attr.value("value", 0.0f);
                        ser_var<float>(new_attr.value_bytes, value);
                        new_attr.value = value;
                    } else if (attr.contains("value_float")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_FLOAT;
                        float value = attr.value("value_float", 0.0f);
                        ser_var<float>(new_attr.value_bytes, value);
                        new_attr.value = value;
                    } else if (attr.contains("value_int")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
                        uint32 value = attr.value("value_int", 0u);
                        ser_var<uint32>(new_attr.value_bytes, value);
                        memcpy(&new_attr.value, &value, sizeof(float));
                    } else if (attr.contains("value_string")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_STRING;
                        std::string value = attr.value("value_string", std::string());
                        new_attr.value_bytes = value + '\0';
                        new_attr.value = 0.0f;
                    } else {
                        continue;
                    }

                    new_item.attributes.push_back(new_attr);
                }
            }
        } catch (std::exception &e) {
            const char *errorMessage = e.what();
            PRINT_DEBUG("error parsing item %llu: %s", new_item.id, errorMessage);
            continue;
        }

        new_item.id = item_id_local_to_network(new_item.id);
        new_item.original_id = item_id_local_to_network(new_item.original_id);

        // Check custom name and custom description limits.
        if (!check_econ_item_name(new_item.custom_name)) {
            new_item.custom_name.clear();
        }

        if (!check_econ_item_desc(new_item.custom_desc)) {
            new_item.custom_desc.clear();
        }

        items.push_back(new_item);
    }

    return items;
}

void Steam_Game_Coordinator::save_items_to_file()
{
    nlohmann::json items_json;

    for (const Econ_Item &item : items) {
        uint64 item_id = item_id_network_to_local(item.id);

        nlohmann::json json_item;
        json_item["definition"] = item.def;
        json_item["level"] = item.level;
        json_item["quality"] = item.quality;
        json_item["inventory_pos"] = item.inv_pos;
        json_item["quantity"] = item.quantity;
        json_item["flags"] = item.flags;
        json_item["origin"] = item.origin;
        json_item["custom_name"] = item.custom_name;
        json_item["custom_desc"] = item.custom_desc;
        json_item["original_id"] = item_id_network_to_local(item.original_id);
        json_item["style"] = item.style;

        for (auto &[class_id, slot_id] : item.equip_states) {
            nlohmann::json json_equip;
            json_equip["class"] = class_id;
            json_equip["slot"] = slot_id;
            json_item["equip_states"].push_back(json_equip);
        }

        for (const Econ_Item_Attribute &attr : item.attributes) {
            nlohmann::json json_attr;
            json_attr["definition"] = attr.def;

            switch (attr.type) {
                case Econ_Item_Attribute::ATTR_TYPE_DEFAULT: {
                    float value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(float));
                    json_attr["value"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_FLOAT: {
                    float value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(float));
                    json_attr["value_float"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_INT: {
                    uint32 value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(uint32));
                    json_attr["value_int"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_STRING: {
                    const char *value = attr.value_bytes.c_str();
                    json_attr["value_string"] = value;
                    break;
                }
            }

            json_item["attributes"].push_back(json_attr);
        }

        items_json[std::to_string(item_id)] = json_item;
    }

    local_storage->write_json_file("", items_user_file, items_json);
}

const Econ_Item *Steam_Game_Coordinator::set_item_pos(uint64 item_id, uint32 inv_pos, bool is_gc, bool save)
{
    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        item.inv_pos = inv_pos;
        if (save) {
            save_items_to_file();
        }

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item_id);
        inventory_msg->set_inv_pos(inv_pos);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(is_gc);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        return &item;
    }

    return nullptr;
}

bool Steam_Game_Coordinator::delete_item(uint64 item_id, bool is_gc)
{
    for (auto it = items.begin(); it != items.end(); it++) {
        if (it->id != item_id)
            continue;

        items.erase(it);
        save_items_to_file();

        // Let the others know, too.
        auto delete_msg = new GameServer_Items_Messages::ItemDeletion();
        delete_msg->set_item_id(item_id);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_DeleteItem);
        gameserver_items_msg->set_is_gc(is_gc);
        gameserver_items_msg->set_allocated_item_deletion(delete_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        return true;
    }

    return false;
}

void Steam_Game_Coordinator::request_user_items(CSteamID steam_id, SteamAPICall_t api_call, bool is_gc)
{
    RequestInventory new_request{};
    new_request.created = std::chrono::high_resolution_clock::now();
    new_request.steam_id = steam_id;
    new_request.steam_api_call = api_call;
    new_request.is_gc = is_gc;
    pending_items_requests.push_back(new_request);

    auto request_msg = new GameServer_Items_Messages::InventoryRequest();
    request_msg->set_steam_api_call(new_request.steam_api_call);

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Request_Inventory);
    gameserver_items_msg->set_is_gc(is_gc);
    gameserver_items_msg->set_allocated_inventory_request(request_msg);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id(steam_id.ConvertToUint64());
    network->sendTo(&msg, true);
}

SteamAPICall_t Steam_Game_Coordinator::find_items_request(CSteamID steam_id)
{
    auto it = std::find_if(
        pending_items_requests.begin(), pending_items_requests.end(),
        [=](const RequestInventory &item) {
            return item.steam_id == steam_id;
        }
    );

    if (it == pending_items_requests.end())
        return k_uAPICallInvalid;

    return it->steam_api_call;
}

void Steam_Game_Coordinator::remove_user_items(CSteamID steam_id)
{
    all_user_items.erase(steam_id);

    // Clean up any pending requests we have.
    for (auto it = pending_items_requests.begin(); it != pending_items_requests.end();) {
        if (it->steam_id == steam_id) {
            it = pending_items_requests.erase(it);
        } else {
            it++;
        }
    }

    callback_items_removed(steam_id);
}

void Steam_Game_Coordinator::on_client_connected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    if (gc_initialized) {
        request_user_items(steam_id, generate_steam_api_call_id(), true);
    }
}

void Steam_Game_Coordinator::on_client_disconnected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    remove_user_items(steam_id);
}

bool Steam_Game_Coordinator::handle_dota_client_message(uint32 unMsgType, const void *pubData, uint32 cubData)
{
    const uint32 masked_emsg = GBE_GC_MaskedEMsg(unMsgType);
    GBE_GC_DebugLog("GC_SEND_DOTA", "outer_emsg=%u len=%u", masked_emsg, cubData);

    if (masked_emsg != GBE_kEMsgClientToGC)
        return false;

    GBE_DotaHelloContext hello_context{};
    if (!GBE_ExtractDotaHelloContext(pubData, cubData, hello_context)) {
        GBE_GC_DebugLog("GC_SEND_DOTA", "ignored ClientToGC payload because it was not a valid Dota ClientHello");
        return false;
    }

    std::string welcome_message;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();
    const uint32 app_id = settings->get_local_game_id().AppID();

    if (!GBE_BuildDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message)) {
        GBE_GC_DebugLog(
            "GC_SEND_DOTA",
            "failed to build ClientWelcome version=%u steamid=%llu accountid=%u",
            hello_context.version,
            static_cast<unsigned long long>(steam_id),
            account_id
        );
        return true;
    }

    GBE_GC_DebugLog(
        "GC_SEND_DOTA",
        "replaying ClientWelcome version=%u steamid=%llu accountid=%u target_job=%llu",
        hello_context.version,
        static_cast<unsigned long long>(steam_id),
        account_id,
        static_cast<unsigned long long>(hello_context.has_source_job ? hello_context.source_job_id : 0ull)
    );

    push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, welcome_message);
    return true;
}

// sends a message to the Game Coordinator
EGCResults Steam_Game_Coordinator::SendMessage_( uint32 unMsgType, const void *pubData, uint32 cubData )
{
    PRINT_DEBUG("0x%08X %u len %u", unMsgType, (~protobuf_mask) & unMsgType, cubData);
    GBE_GC_DebugLog("GC_SEND", "outer_emsg=%u len=%u", GBE_GC_MaskedEMsg(unMsgType), cubData);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!gc_initialized && gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog("GC_SEND", "initializing GC lazily for Dota2 profile");
        initialize_gc();
    }

    if (!gc_initialized) {
        GBE_GC_DebugLog("GC_SEND", "gc not initialized, swallowing msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    if (gc_profile == GC_PROFILE_DOTA2 && handle_dota_client_message(unMsgType, pubData, cubData)) {
        GBE_GC_DebugLog("GC_SEND", "handled by Dota2 replay path msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    switch (unMsgType) {
        case EGCItemMsg::k_EMsgGCSetSingleItemPosition:
            PRINT_DEBUG("k_EMsgGCSetSingleItemPosition");
            handle_set_item_pos(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCDelete:
            PRINT_DEBUG("k_EMsgGCDelete");
            handle_delete_item(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCMOTDRequest:
            PRINT_DEBUG("k_EMsgGCMOTDRequest");
            handle_motd_request(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange:
            PRINT_DEBUG("k_EMsgGCRespawnPostLoadoutChange");
            handle_respawn(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemStyle:
            PRINT_DEBUG("k_EMsgGCSetItemStyle");
            handle_set_item_style(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCAdjustItemEquippedState | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCAdjustItemEquippedState");
            handle_adjust_equip_state(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemPositions | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCSetItemPositions");
            handle_set_multiple_item_pos(pubData, cubData);
            break;
        default:
            break;
    }

    return k_EGCResultOK;
}

// returns true if there is a message waiting from the game coordinator
bool Steam_Game_Coordinator::IsMessageAvailable( uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!gc_initialized || incoming_messages.empty()) {
        *pcubMsgSize = 0;
        return false;
    }

    GC_Message &message = incoming_messages.front();
    *pcubMsgSize = static_cast<uint32>(message.msg_body.size());

    return true;
}

// fills the provided buffer with the first message in the queue and returns k_EGCResultOK or 
// returns k_EGCResultNoMessage if there is no message waiting. pcubMsgSize is filled with the message size.
// If the provided buffer is not large enough to fit the entire message, k_EGCResultBufferTooSmall is returned
// and the message remains at the head of the queue.
EGCResults Steam_Game_Coordinator::RetrieveMessage( uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    const uint32 queued_emsg = incoming_messages.empty() ? 0u : GBE_GC_MaskedEMsg(incoming_messages.front().msg_type);
    GBE_GC_DebugLog("GC_RETRIEVE", "queued_emsg=%u queue_size=%zu cubDest=%u", queued_emsg, incoming_messages.size(), cubDest);

    if (!gc_initialized || incoming_messages.empty()) {
        *pcubMsgSize = 0;
        return k_EGCResultNoMessage;
    }

    GC_Message &message = incoming_messages.front();

    uint32 outsize = static_cast<uint32>(message.msg_body.size());
    if (outsize > cubDest) {
        return k_EGCResultBufferTooSmall;
    }

    if (is_welcome_message(message)) {
        welcome_received = true;
        welcome_time = std::chrono::high_resolution_clock::now();
    }

    *punMsgType = message.msg_type;
    *pcubMsgSize = outsize;
    message.msg_body.copy(reinterpret_cast<char *>(pubDest), cubDest);
    incoming_messages.pop();

    GBE_GC_DebugLog("GC_RETRIEVE", "returned_emsg=%u size=%u", GBE_GC_MaskedEMsg(*punMsgType), outsize);

    return k_EGCResultOK;
}

// server requested our inventory
void Steam_Game_Coordinator::network_callback_inventory_request(Common_Message *msg)
{
    // Server instance should never receive this.
    if (is_server)
        return;

    uint64 server_steamid = msg->source_id();

    if (!msg->gameserver_items_messages().has_inventory_request()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &request_msg = msg->gameserver_items_messages().inventory_request();
    auto response_msg = new GameServer_Items_Messages::InventoryResponse();
    response_msg->set_steam_api_call(request_msg.steam_api_call());

    for (const Econ_Item &item : items) {
        auto new_item = response_msg->add_items();
        new_item->set_id(item.id);
        new_item->set_def(item.def);
        new_item->set_level(item.level);
        new_item->set_quality(static_cast<int32>(item.quality));
        new_item->set_inv_pos(item.inv_pos);
        new_item->set_quantity(item.quantity);
        new_item->set_flags(item.flags);
        new_item->set_origin(item.origin);
        new_item->set_custom_name(item.custom_name);
        new_item->set_custom_desc(item.custom_desc);
        new_item->set_original_id(item.original_id);
        new_item->set_in_use(item.in_use);
        new_item->set_style(item.style);

        for (const auto &[class_id, slot_id] : item.equip_states) {
            auto new_state = new_item->add_equip_states();
            new_state->set_class_id(class_id);
            new_state->set_slot_id(slot_id);
        }

        for (const Econ_Item_Attribute &attr : item.attributes) {
            auto new_attr = new_item->add_attributes();
            new_attr->set_def(attr.def);
            new_attr->set_value(attr.value);
            new_attr->set_value_bytes(attr.value_bytes);
        }
    }

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Response_Inventory);
    gameserver_items_msg->set_is_gc(is_gc);
    gameserver_items_msg->set_allocated_inventory_response(response_msg);

    Common_Message new_msg{};
    new_msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    new_msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    new_msg.set_dest_id(server_steamid);
    network->sendTo(&new_msg, true);

    PRINT_DEBUG("server requested inventory, sent %u items", static_cast<uint32>(items.size()));
}

// user sent their inventory
void Steam_Game_Coordinator::network_callback_inventory_response(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player sent their inventory %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_inventory_response()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &response_msg = msg->gameserver_items_messages().inventory_response();
    SteamAPICall_t api_call = response_msg.steam_api_call();

    // Find this pending request.
    auto it = std::find_if(
        pending_items_requests.begin(), pending_items_requests.end(),
        [=](const RequestInventory &item) {
            return item.steam_api_call == response_msg.steam_api_call() &&
                item.steam_id == user_steamid;
        }
    );
    if (pending_items_requests.end() == it) {
        PRINT_DEBUG("error got player inventory but pending request timedout/removed (doesn't exist)");
        return;
    }
    pending_items_requests.erase(it);

    auto &items = all_user_items[user_steamid];
    items.clear();

    for (const auto &item : response_msg.items()) {
        Econ_Item new_item{};
        new_item.id = item.id();
        new_item.def = item.def();
        new_item.level = item.level();
        new_item.quality = static_cast<EItemQuality>(item.quality());
        new_item.inv_pos = item.inv_pos();
        new_item.quantity = item.quantity();
        new_item.flags = item.flags();
        new_item.origin = item.origin();
        new_item.custom_name = item.custom_name();
        new_item.custom_desc = item.custom_desc();
        new_item.original_id = item.original_id();
        new_item.in_use = item.in_use();
        new_item.style = item.style();
        if (new_item.id == 0)
            continue;

        for (const auto &state : item.equip_states()) {
            new_item.equip_states.insert({ state.class_id(), state.slot_id() });
        }

        for (const auto &attr : item.attributes()) {
            Econ_Item_Attribute new_attr{};
            new_attr.def = attr.def();
            new_attr.value = attr.value();
            new_attr.value_bytes = attr.value_bytes();
            if (new_attr.def == 0)
                continue;

            new_item.attributes.push_back(new_attr);
        }

        // Check custom name and custom description limits.
        if (!check_econ_item_name(new_item.custom_name)) {
            new_item.custom_name.clear();
        }

        if (!check_econ_item_desc(new_item.custom_desc)) {
            new_item.custom_desc.clear();
        }

        items.push_back(new_item);
    }

    if (is_gc) {
        callback_items_received(user_steamid, items);
    } else {
        server_items()->callback_items_received(user_steamid, items.size(), api_call, true);
    }

    PRINT_DEBUG("got player inventory: %u items", response_msg.items_size());
}

// user updated an item
void Steam_Game_Coordinator::network_callback_item_update(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player updated an item %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_item_update()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    if (!all_user_items.count(user_steamid)) {
        PRINT_DEBUG("error no inventory for player", user_steamid);
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &inventory_msg = msg->gameserver_items_messages().item_update();
    uint64 item_id = inventory_msg.id();

    auto &items = all_user_items.at(user_steamid);

    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        if (inventory_msg.has_inv_pos()) {
            item.inv_pos = inventory_msg.inv_pos();
            PRINT_DEBUG("got updated item inventory pos: %llu 0x%08X", item_id, inventory_msg.inv_pos());
        }

        if (inventory_msg.has_style()) {
            item.style = inventory_msg.style();
            PRINT_DEBUG("got updated item style: %llu %u", item_id, inventory_msg.style());
        }

        if (inventory_msg.has_equip_states()) {
            item.equip_states.clear();
            for (const auto &state : inventory_msg.equip_states()) {
                item.equip_states.insert({ state.class_id(), state.slot_id() });
            }
            PRINT_DEBUG("got updated item equip states: %llu", item_id);
        }

        if (is_gc) {
            callback_item_updated(user_steamid, item);
        } else if (inventory_msg.has_inv_pos()) {
            server_items()->callback_item_pos_updated(user_steamid, item_id, inventory_msg.inv_pos());
        }

        return;
    }

    PRINT_DEBUG("error item %llu not found", item_id);
}

// user deleted an item
void Steam_Game_Coordinator::network_callback_item_deletion(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player deleted inventory item %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_item_deletion()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    if (!all_user_items.count(user_steamid)) {
        PRINT_DEBUG("error no inventory for player", user_steamid);
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &delete_msg = msg->gameserver_items_messages().item_deletion();
    uint64 item_id = delete_msg.item_id();

    auto &items = all_user_items.at(user_steamid);

    for (auto it = items.begin(); it != items.end(); it++) {
        if (it->id != item_id)
            continue;

        items.erase(it);

        if (is_gc) {
            callback_item_deleted(user_steamid, item_id);
        } else {
            server_items()->callback_item_deleted(user_steamid, item_id);
        }

        PRINT_DEBUG("deleted player's inventory item: %llu", item_id);
        return;
    }

    PRINT_DEBUG("error item %llu not found", item_id);
}

// user wants to respawn after loadout change
void Steam_Game_Coordinator::network_callback_respawn_request(Common_Message *msg)
{
    if (!is_server)
        return;

    uint64 user_steamid = msg->source_id();
    if (!all_user_items.count(user_steamid))
        return;

    callback_respawn_request(user_steamid);
}

// only triggered when we have a message
void Steam_Game_Coordinator::network_callback(Common_Message *msg)
{
    if (msg->source_id() == settings->get_local_steam_id().ConvertToUint64()) return;

    if (msg->has_gameserver_items_messages()) {
        switch (msg->gameserver_items_messages().type()) {
        // server requested our inventory
        case GameServer_Items_Messages::Request_Inventory:
            network_callback_inventory_request(msg);
        break;

        // user sent their inventory
        case GameServer_Items_Messages::Response_Inventory:
            network_callback_inventory_response(msg);
        break;

        // user updated an item
        case GameServer_Items_Messages::Request_UpdateItem:
            network_callback_item_update(msg);
        break;

        // user deleted an item
        case GameServer_Items_Messages::Request_DeleteItem:
            network_callback_item_deletion(msg);
        break;

        // user wants to respawn after loadout change
        case GameServer_Items_Messages::Request_Respawn:
            network_callback_respawn_request(msg);
        break;

        default:
            PRINT_DEBUG("unhandled type %i", (int)msg->gameserver_items_messages().type());
        break;
        }
    } else if (msg->has_low_level()) {
        if (!is_server && gc_initialized) {
            CSteamID user_steamid;
            user_steamid.SetFromUint64(msg->source_id());

            if (user_steamid.BIndividualAccount()) {
                // Client needs to know other players' inventories as well since the game uses them to
                // validate cosmetic items.
                switch (msg->low_level().type()) {
                case Low_Level::CONNECT:
                    request_user_items(user_steamid, generate_steam_api_call_id(), true);
                break;

                case Low_Level::DISCONNECT:
                    remove_user_items(user_steamid);
                break;
                }
            }
        }
    }
}

void Steam_Game_Coordinator::RunCallbacks()
{
    if (delay_init && welcome_received && check_timedout(welcome_time, 0.2)) {
        delay_init = false;
    }

    for (auto it = pending_messages.begin(); it != pending_messages.end();) {
        if (delay_init && !is_welcome_message(*it)) {
            it++;
            continue;
        }

        if (check_timedout(it->created, it->post_in)) {
            incoming_messages.push(*it);

            GCMessageAvailable_t data{};
            data.m_nMessageSize = static_cast<uint32>(it->msg_body.size());
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

            it = pending_messages.erase(it);
        } else {
            it++;
        }
    }

    for (auto it = pending_items_requests.begin(); it != pending_items_requests.end();) {
        if (check_timedout(it->created, 7.0)) {
            if (!it->is_gc) {
                server_items()->callback_items_received(it->steam_id, items.size(), it->steam_api_call, false);
            }

            PRINT_DEBUG("player inventory request timeout %llu", it->steam_id);
            it = pending_items_requests.erase(it);
        } else {
            it++;
        }
    }
}

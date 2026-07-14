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

#include "dll/steam_unified_messages.h"

static void GBE_UM_AppendVarint(std::string &buffer, uint64 value)
{
    while (value >= 0x80) {
        buffer.push_back(static_cast<char>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    buffer.push_back(static_cast<char>(value));
}

static void GBE_UM_AppendLittleEndian64(std::string &buffer, uint64 value)
{
    for (unsigned i = 0; i < 8; ++i) {
        buffer.push_back(static_cast<char>((value >> (i * 8)) & 0xff));
    }
}

static void GBE_UM_AppendVarintField(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_UM_AppendVarint(buffer, (static_cast<uint64>(field_number) << 3) | 0u);
    GBE_UM_AppendVarint(buffer, value);
}

static void GBE_UM_AppendFixed64Field(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_UM_AppendVarint(buffer, (static_cast<uint64>(field_number) << 3) | 1u);
    GBE_UM_AppendLittleEndian64(buffer, value);
}

static void GBE_UM_AppendBytesField(std::string &buffer, uint32 field_number, const std::string &value)
{
    GBE_UM_AppendVarint(buffer, (static_cast<uint64>(field_number) << 3) | 2u);
    GBE_UM_AppendVarint(buffer, value.size());
    buffer.append(value);
}

static uint64 GBE_UM_ReadLittleEndian64(const uint8 *bytes)
{
    uint64 value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= static_cast<uint64>(bytes[i]) << (i * 8);
    }
    return value;
}

static bool GBE_UM_ReadVarint(const uint8 *data, uint32 size, uint32 &offset, uint64 &value)
{
    value = 0;
    unsigned shift = 0;
    while (offset < size && shift < 64) {
        const uint8 byte = data[offset++];
        value |= static_cast<uint64>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return true;
        shift += 7;
    }
    return false;
}

static std::vector<PublishedFileId_t> GBE_UM_ParsePublishedFileDetailsRequest(const void *buffer, uint32 size)
{
    std::vector<PublishedFileId_t> ids{};
    if (!buffer || !size) return ids;

    const uint8 *data = static_cast<const uint8 *>(buffer);
    uint32 offset = 0;
    while (offset < size) {
        uint64 tag = 0;
        if (!GBE_UM_ReadVarint(data, size, offset, tag)) break;
        const uint32 field_number = static_cast<uint32>(tag >> 3);
        const uint32 wire_type = static_cast<uint32>(tag & 0x7);
        if (field_number == 1 && wire_type == 1 && offset + 8 <= size) {
            ids.push_back(static_cast<PublishedFileId_t>(GBE_UM_ReadLittleEndian64(data + offset)));
            offset += 8;
        } else if (wire_type == 0) {
            uint64 ignored = 0;
            if (!GBE_UM_ReadVarint(data, size, offset, ignored)) break;
        } else if (wire_type == 1) {
            if (offset + 8 > size) break;
            offset += 8;
        } else if (wire_type == 2) {
            uint64 length = 0;
            if (!GBE_UM_ReadVarint(data, size, offset, length)) break;
            if (length > size - offset) break;
            offset += static_cast<uint32>(length);
        } else if (wire_type == 5) {
            if (offset + 4 > size) break;
            offset += 4;
        } else {
            break;
        }
    }
    return ids;
}

static std::vector<std::string> GBE_UM_SplitTags(const std::string &tags)
{
    std::vector<std::string> result{};
    size_t start = 0;
    while (start <= tags.size()) {
        const size_t end = tags.find(',', start);
        std::string tag = tags.substr(start, end == std::string::npos ? std::string::npos : end - start);
        while (!tag.empty() && (tag.front() == ' ' || tag.front() == '\t')) tag.erase(tag.begin());
        while (!tag.empty() && (tag.back() == ' ' || tag.back() == '\t')) tag.pop_back();
        if (!tag.empty()) result.push_back(tag);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

static std::string GBE_UM_BuildPublishedFileTag(const std::string &tag)
{
    std::string output{};
    GBE_UM_AppendBytesField(output, 1u, tag);
    GBE_UM_AppendBytesField(output, 3u, tag);
    return output;
}

static std::string GBE_UM_BuildPublishedFileDetails(Settings *settings, PublishedFileId_t id)
{
    std::string output{};
    GBE_UM_AppendVarintField(output, 2u, id);

    if (!settings || !settings->isModInstalled(id)) {
        GBE_UM_AppendVarintField(output, 1u, k_EResultFail);
        return output;
    }

    const Mod_entry mod = settings->getMod(id);
    GBE_UM_AppendVarintField(output, 1u, k_EResultOK);
    GBE_UM_AppendFixed64Field(output, 3u, mod.steamIDOwner ? mod.steamIDOwner : settings->get_local_steam_id().ConvertToUint64());
    GBE_UM_AppendVarintField(output, 4u, settings->get_local_game_id().AppID());
    GBE_UM_AppendVarintField(output, 5u, settings->get_local_game_id().AppID());
    GBE_UM_AppendVarintField(output, 8u, static_cast<uint64>(std::max<int32>(0, mod.primaryFileSize)));
    GBE_UM_AppendVarintField(output, 9u, static_cast<uint64>(std::max<int32>(0, mod.previewFileSize)));
    if (!mod.previewURL.empty()) GBE_UM_AppendBytesField(output, 11u, mod.previewURL);
    if (!mod.workshopItemURL.empty()) GBE_UM_AppendBytesField(output, 13u, mod.workshopItemURL);
    GBE_UM_AppendFixed64Field(output, 14u, mod.handleFile);
    GBE_UM_AppendFixed64Field(output, 15u, mod.handlePreviewFile);
    GBE_UM_AppendBytesField(output, 16u, mod.title.empty() ? std::to_string(id) : mod.title);
    if (!mod.description.empty()) GBE_UM_AppendBytesField(output, 17u, mod.description);
    GBE_UM_AppendVarintField(output, 19u, mod.timeCreated);
    GBE_UM_AppendVarintField(output, 20u, mod.timeUpdated);
    GBE_UM_AppendVarintField(output, 21u, mod.visibility);
    GBE_UM_AppendVarintField(output, 23u, 1u);
    GBE_UM_AppendVarintField(output, 24u, mod.acceptedForUse ? 1u : 0u);
    GBE_UM_AppendVarintField(output, 28u, mod.banned ? 1u : 0u);
    GBE_UM_AppendVarintField(output, 31u, 1u);
    GBE_UM_AppendBytesField(output, 33u, "Dota 2");
    GBE_UM_AppendVarintField(output, 34u, mod.fileType);
    GBE_UM_AppendVarintField(output, 35u, 1u);
    GBE_UM_AppendVarintField(output, 36u, mod.votesUp);
    GBE_UM_AppendVarintField(output, 37u, mod.votesDown);
    for (const auto &tag : GBE_UM_SplitTags(mod.tags.empty() ? std::string("Custom Game") : mod.tags)) {
        GBE_UM_AppendBytesField(output, 52u, GBE_UM_BuildPublishedFileTag(tag));
    }
    GBE_UM_AppendVarintField(output, 56u, mod.timeAddedToUserList);
    if (!mod.metadata.empty()) GBE_UM_AppendBytesField(output, 58u, mod.metadata);
    GBE_UM_AppendVarintField(output, 61u, 0u);
    GBE_UM_AppendVarintField(output, 67u, 1u);
    GBE_UM_AppendVarintField(output, 68u, 1u);
    GBE_UM_AppendVarintField(output, 69u, 2u);
    GBE_UM_AppendVarintField(output, 69u, 4u);
    GBE_UM_AppendVarintField(output, 71u, 5u);
    return output;
}

static std::string GBE_UM_BuildPublishedFileDetailsResponse(Settings *settings, const std::vector<PublishedFileId_t> &ids)
{
    std::string output{};
    for (PublishedFileId_t id : ids) {
        GBE_UM_AppendBytesField(output, 1u, GBE_UM_BuildPublishedFileDetails(settings, id));
    }
    return output;
}

static bool GBE_UM_IsPublishedFileGetDetails(const char *method)
{
    if (!method) return false;
    const std::string name(method);
    return name == "PublishedFile.GetDetails"
        || name == "PublishedFile.GetDetails#1"
        || name.find("PublishedFile.GetDetails") != std::string::npos;
}

void Steam_Unified_Messages::network_callback(void *object, Common_Message *msg)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Unified_Messages *steam_steamunifiedmessages = (Steam_Unified_Messages *)object;
    steam_steamunifiedmessages->Callback(msg);
}

void Steam_Unified_Messages::steam_runcb(void *object)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Unified_Messages *steam_steamunifiedmessages = (Steam_Unified_Messages *)object;
    steam_steamunifiedmessages->RunCallbacks();
}


Steam_Unified_Messages::Steam_Unified_Messages(class Settings *settings, class Networking *network, class SteamCallResults *callback_results, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb)
{
    this->settings = settings;
    this->network = network;
    this->callback_results = callback_results;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;

    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Unified_Messages::network_callback, this);
    this->run_every_runcb->add(&Steam_Unified_Messages::steam_runcb, this);

}

Steam_Unified_Messages::~Steam_Unified_Messages()
{
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Unified_Messages::network_callback, this);
    this->run_every_runcb->remove(&Steam_Unified_Messages::steam_runcb, this);
}

// Sends a service method (in binary serialized form) using the Steam Client.
// Returns a unified message handle (k_InvalidUnifiedMessageHandle if could not send the message).
ClientUnifiedMessageHandle Steam_Unified_Messages::SendMethod( const char *pchServiceMethod, const void *pRequestBuffer, uint32 unRequestBufferSize, uint64 unContext )
{
    PRINT_DEBUG("%s %p %u %llu", pchServiceMethod ? pchServiceMethod : "", pRequestBuffer, unRequestBufferSize, unContext);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!GBE_UM_IsPublishedFileGetDetails(pchServiceMethod)) {
        PRINT_DEBUG_TODO();
        return ISteamUnifiedMessages::k_InvalidUnifiedMessageHandle;
    }

    std::vector<PublishedFileId_t> ids = GBE_UM_ParsePublishedFileDetailsRequest(pRequestBuffer, unRequestBufferSize);
    if (ids.empty()) return ISteamUnifiedMessages::k_InvalidUnifiedMessageHandle;

    ClientUnifiedMessageHandle handle = next_handle++;
    if (handle == ISteamUnifiedMessages::k_InvalidUnifiedMessageHandle) handle = next_handle++;
    responses[handle] = GBE_UM_BuildPublishedFileDetailsResponse(settings, ids);

    SteamUnifiedMessagesSendMethodResult_t data{};
    data.m_hHandle = handle;
    data.m_unContext = unContext;
    data.m_eResult = k_EResultOK;
    data.m_unResponseSize = static_cast<uint32>(responses[handle].size());
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return handle;
}


// Gets the size of the response and the EResult. Returns false if the response is not ready yet.
bool Steam_Unified_Messages::GetMethodResponseInfo( ClientUnifiedMessageHandle hHandle, uint32 *punResponseSize, EResult *peResult )
{
    PRINT_DEBUG("%llu %p %p", hHandle, punResponseSize, peResult);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    auto response = responses.find(hHandle);
    if (response == responses.end()) return false;

    if (punResponseSize) *punResponseSize = static_cast<uint32>(response->second.size());
    if (peResult) *peResult = k_EResultOK;
    return true;
}


// Gets a response in binary serialized form (and optionally release the corresponding allocated memory).
bool Steam_Unified_Messages::GetMethodResponseData( ClientUnifiedMessageHandle hHandle, void *pResponseBuffer, uint32 unResponseBufferSize, bool bAutoRelease )
{
    PRINT_DEBUG("%llu %p %u %u", hHandle, pResponseBuffer, unResponseBufferSize, bAutoRelease);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    auto response = responses.find(hHandle);
    if (response == responses.end() || !pResponseBuffer || unResponseBufferSize < response->second.size()) return false;

    std::memcpy(pResponseBuffer, response->second.data(), response->second.size());
    if (bAutoRelease) responses.erase(response);
    return true;
}


// Releases the message and its corresponding allocated memory.
bool Steam_Unified_Messages::ReleaseMethod( ClientUnifiedMessageHandle hHandle )
{
    PRINT_DEBUG("%llu", hHandle);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return responses.erase(hHandle) > 0;
}


// Sends a service notification (in binary serialized form) using the Steam Client.
// Returns true if the notification was sent successfully.
bool Steam_Unified_Messages::SendNotification( const char *pchServiceNotification, const void *pNotificationBuffer, uint32 unNotificationBufferSize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return false;
}


void Steam_Unified_Messages::RunCallbacks()
{
}

void Steam_Unified_Messages::Callback(Common_Message *msg)
{
    if (msg->has_low_level()) {
        if (msg->low_level().type() == Low_Level::CONNECT) {
            
        }

        if (msg->low_level().type() == Low_Level::DISCONNECT) {

        }
    }
}

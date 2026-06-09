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

#include "dll/steam_networking_socketsserialized.h"
#include "dll/steam_networking_sockets.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/steam_client.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <ctime>
#include <vector>
#include <cstdlib>

namespace {

constexpr int GBE_kSerializedRendezvousPort = -5434;

template <typename T>
const T *GBE_AsSerializedCallbackPayload(const void *data, uint32 size)
{
    if (!data)
        return nullptr;

    if (size == sizeof(T))
        return static_cast<const T *>(data);

    if (size == sizeof(int) + sizeof(T)) {
        const auto *bytes = static_cast<const uint8_t *>(data);
        int callback_id = 0;
        std::memcpy(&callback_id, bytes, sizeof(callback_id));
        if (callback_id == T::k_iCallback)
            return reinterpret_cast<const T *>(bytes + sizeof(callback_id));
    }

    return nullptr;
}

template <typename T>
bool GBE_PostSerializedCallbackPayload(SteamCallBacks *callbacks, const void *data, uint32 size, const char *name)
{
    const T *payload = GBE_AsSerializedCallbackPayload<T>(data, size);
    if (!payload)
        return false;

    callbacks->addCBResult(T::k_iCallback, const_cast<T *>(payload), sizeof(T));
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "queued serialized callback id=%d type=%s size=%u",
        T::k_iCallback,
        name ? name : "unknown",
        size
    );
    return true;
}

void GBE_LogSerializedNetSockTrace(const char *scope, uint64 local_id, uint64 remote_id, uint32 connection_id, uint32 size_or_reason)
{
    FILE *file = std::fopen("C:\\Users\\Public\\gbe_gc_debug.log", "a");
    if (!file)
        return;

    std::fprintf(
        file,
        "[%s] local_id=%llu remote_id=%llu connection_id=%u size_or_reason=%u\n",
        scope ? scope : "NETSOCK_SERIALIZED_TRACE",
        (unsigned long long)local_id,
        (unsigned long long)remote_id,
        connection_id,
        size_or_reason
    );
    std::fclose(file);
}

std::string GBE_FormatPayloadPrefix(const void *data, uint32 size)
{
    if (!data || size == 0)
        return "";

    const uint32 prefix_size = std::min<uint32>(size, 16u);
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    char buffer[(16u * 3u) + 1u] = {};
    size_t offset = 0;
    for (uint32 i = 0; i < prefix_size && offset < sizeof(buffer); ++i) {
        const int written = std::snprintf(buffer + offset, sizeof(buffer) - offset, "%s%02X", i == 0 ? "" : " ", bytes[i]);
        if (written <= 0)
            break;
        offset += static_cast<size_t>(written);
    }
    return buffer;
}

std::string GBE_FormatSerializedPayloadFields(const void *data, uint32 size)
{
    if (!data || size < 5)
        return "";

    const auto *bytes = static_cast<const uint8_t *>(data);
    uint32 offset = 0;
    uint32 fields_logged = 0;
    std::string fields;
    while (offset < size) {
        uint64 key = 0;
        uint32 shift = 0;
        while (offset < size && shift < 64) {
            const uint8_t byte = bytes[offset++];
            key |= static_cast<uint64>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0)
                break;
            shift += 7;
        }

        const uint32 field_number = static_cast<uint32>(key >> 3);
        const uint32 wire_type = static_cast<uint32>(key & 0x7);
        char field[96] = {};

        if (wire_type == 0) {
            uint64 value = 0;
            shift = 0;
            while (offset < size) {
                const uint8_t byte = bytes[offset++];
                value |= static_cast<uint64>(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0)
                    break;
                shift += 7;
            }
            std::snprintf(field, sizeof(field), "f%u:varint=%llu", field_number, (unsigned long long)value);
        } else if (wire_type == 1) {
            if (offset + 8 > size)
                return fields;
            uint64 value = 0;
            std::memcpy(&value, bytes + offset, sizeof(value));
            offset += 8;
            std::snprintf(field, sizeof(field), "f%u:fixed64=%llu", field_number, (unsigned long long)value);
        } else if (wire_type == 2) {
            uint64 length = 0;
            shift = 0;
            while (offset < size && shift < 64) {
                const uint8_t byte = bytes[offset++];
                length |= static_cast<uint64>(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0)
                    break;
                shift += 7;
            }
            if (length > size - offset)
                return fields;
            offset += static_cast<uint32>(length);
            std::snprintf(field, sizeof(field), "f%u:len=%llu", field_number, (unsigned long long)length);
        } else if (wire_type == 5) {
            if (offset + 4 > size)
                return fields;
            uint32 value = 0;
            std::memcpy(&value, bytes + offset, sizeof(value));
            offset += 4;
            std::snprintf(field, sizeof(field), "f%u:fixed32=%u", field_number, value);
        } else {
            return fields;
        }

        if (field[0] != '\0') {
            if (!fields.empty())
                fields += " ";
            fields += field;
            ++fields_logged;
            if (fields_logged >= 8)
                break;
        }
    }

    return fields;
}

bool GBE_GetSerializedFixed32Field(const void *data, uint32 size, uint32 wanted_field, uint32 *out)
{
    if (!data || !out)
        return false;

    const auto *bytes = static_cast<const uint8_t *>(data);
    uint32 offset = 0;
    while (offset < size) {
        uint64 key = 0;
        uint32 shift = 0;
        while (offset < size && shift < 64) {
            const uint8_t byte = bytes[offset++];
            key |= static_cast<uint64>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0)
                break;
            shift += 7;
        }

        const uint32 field_number = static_cast<uint32>(key >> 3);
        const uint32 wire_type = static_cast<uint32>(key & 0x7);
        if (wire_type == 0) {
            while (offset < size) {
                const uint8_t byte = bytes[offset++];
                if ((byte & 0x80) == 0)
                    break;
            }
        } else if (wire_type == 1) {
            if (offset + 8 > size)
                return false;
            offset += 8;
        } else if (wire_type == 2) {
            uint64 length = 0;
            shift = 0;
            while (offset < size && shift < 64) {
                const uint8_t byte = bytes[offset++];
                length |= static_cast<uint64>(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0)
                    break;
                shift += 7;
            }
            if (length > size - offset)
                return false;
            offset += static_cast<uint32>(length);
        } else if (wire_type == 5) {
            if (offset + 4 > size)
                return false;
            uint32 value = 0;
            std::memcpy(&value, bytes + offset, sizeof(value));
            offset += 4;
            if (field_number == wanted_field) {
                *out = value;
                return true;
            }
        } else {
            return false;
        }
    }

    return false;
}

uint64 GBE_last_post_connection_state_server_id = 0;
uint32 GBE_post_connection_state_retry_count = 0;
uint32 GBE_last_post_connection_state_size = 0;
uint64 GBE_last_direct_connect_server_id = 0;
std::string GBE_last_direct_connect_endpoint;

static constexpr uint8_t GBE_kSerializedPublicKey[32] = {
    0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
    0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
    0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
    0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
};

static constexpr uint8_t GBE_kSerializedPrivateKey[32] = {
    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
    0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
    0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
};

int GBE_CopySerializedNetworkingJson(const char *json, void *buf, uint32 cbBuf)
{
    if (!json)
        json = "{}";

    const size_t required = std::strlen(json) + 1;
    if (buf && cbBuf > 0) {
        const size_t to_copy = std::min<size_t>(required, cbBuf);
        std::memcpy(buf, json, to_copy);
        reinterpret_cast<char *>(buf)[to_copy - 1] = '\0';
    }

    return static_cast<int>(required);
}

const char *GBE_GetSerializedNetworkingConfigJSON()
{
    return
        "{\"revision\":1778707800,\"pops\":{"
        "\"sgp\":{\"desc\":\"Singapore\",\"geo\":[103.83,1.28],\"partners\":3,\"tier\":0,"
        "\"relays\":[{\"ipv4\":\"103.10.124.116\",\"port_range\":[27015,27060]}]}},"
        "\"certs\":["
        "\"Ii4IARIgSJbwDpn/07/GHiGMKio0Vh18VN3D/hKzQGh6n0Yx9qpF2uYEak3a6dB8KT6tfJIvitz8MkADsyKTi+VwxYwR6npnpWPd42q0AYGT5GY9Fje8AbrTFvsRwS6tNRX/1JQqpZZYhC/drdKjYcIPKUxa1KEgS/QO\","
        "\"Ii4IARIgmuygThdRzmJo1WkALKHh+hstvCbTa06joAg603KCm4RF2uYEak3a6dB8KT6tfJIvitz8MkDiEksgJ+a351sr1F+N+GTvtboavJFRg/M2/x1B6Biro/BEgHskHZlIQJULbpvkDCvzBHBiZ+1L59hmdj32aucK\""
        "],\"p2p_share_ip\":{\"default\":40,\"cn\":20,\"ru\":20},"
        "\"relay_public_key\":\"5AC884C1045BA0FF44142AC8DCA51B8A98C8F1CB4FEE36284AFBE92FCF594932\","
        "\"revoked_keys\":[\"11146342570456886677\"],\"typical_pings\":[],\"success\":true}";
}

void GBE_AppendVarint(std::vector<uint8_t> &out, uint64_t value)
{
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

void GBE_AppendFixed32(std::vector<uint8_t> &out, uint32_t value)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
    }
}

void GBE_AppendFixed64(std::vector<uint8_t> &out, uint64_t value)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
    }
}

void GBE_AppendBytes(std::vector<uint8_t> &out, uint32_t field, const void *data, size_t size)
{
    GBE_AppendVarint(out, (static_cast<uint64_t>(field) << 3) | 2);
    GBE_AppendVarint(out, size);
    const auto *bytes = reinterpret_cast<const uint8_t *>(data);
    out.insert(out.end(), bytes, bytes + size);
}

std::vector<uint8_t> GBE_BuildSerializedNetworkingCert(CSteamID steam_id, uint32 app_id)
{
    const uint32 now = static_cast<uint32>(std::time(nullptr));
    const uint32 expiry = now + 24u * 60u * 60u;
    const uint64 steam_id64 = steam_id.ConvertToUint64();
    const std::string identity = std::string("steamid:") + std::to_string(steam_id64);
    std::vector<uint8_t> identity_binary;
    GBE_AppendVarint(identity_binary, (16u << 3) | 1u);
    GBE_AppendFixed64(identity_binary, steam_id64);

    std::vector<uint8_t> cert;
    cert.reserve(128);
    GBE_AppendVarint(cert, (1u << 3) | 0u);
    GBE_AppendVarint(cert, 1);
    GBE_AppendBytes(cert, 2, GBE_kSerializedPublicKey, sizeof(GBE_kSerializedPublicKey));
    GBE_AppendVarint(cert, (4u << 3) | 1u);
    GBE_AppendFixed64(cert, steam_id64);
    GBE_AppendVarint(cert, (8u << 3) | 5u);
    GBE_AppendFixed32(cert, now);
    GBE_AppendVarint(cert, (9u << 3) | 5u);
    GBE_AppendFixed32(cert, expiry);
    GBE_AppendVarint(cert, (10u << 3) | 0u);
    GBE_AppendVarint(cert, app_id);
    GBE_AppendBytes(cert, 11, identity_binary.data(), identity_binary.size());
    GBE_AppendBytes(cert, 12, identity.data(), identity.size());
    return cert;
}

bool GBE_ParseIPv4Endpoint(const std::string &endpoint, SteamNetworkingIPAddr *address)
{
    if (!address)
        return false;

    const size_t port_pos = endpoint.find(':');
    if (port_pos == std::string::npos)
        return false;

    unsigned long octets[4] = {};
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const size_t end = endpoint.find(i == 3 ? ':' : '.', start);
        if (end == std::string::npos || end <= start)
            return false;

        const std::string segment = endpoint.substr(start, end - start);
        char *parse_end = nullptr;
        octets[i] = std::strtoul(segment.c_str(), &parse_end, 10);
        if (!parse_end || *parse_end != '\0' || octets[i] > 255)
            return false;

        start = end + 1;
    }

    if (start != port_pos + 1)
        return false;

    char *port_end = nullptr;
    const unsigned long port = std::strtoul(endpoint.c_str() + port_pos + 1, &port_end, 10);
    if (!port_end || *port_end != '\0' || port == 0 || port > 65535)
        return false;

    const uint32 ip = static_cast<uint32>((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
    address->SetIPv4(ip, static_cast<uint16>(port));
    return true;
}

void GBE_SendSerializedRendezvous(Networking *network, uint64 local_id, uint64 remote_id, uint32 connection_id, const void *data, uint32 size)
{
    if (!network || remote_id == 0 || remote_id == local_id || !data || size == 0)
        return;

    Common_Message msg;
    msg.set_source_id(local_id);
    msg.set_dest_id(remote_id);
    msg.set_allocated_networking_sockets(new Networking_Sockets);
    msg.mutable_networking_sockets()->set_type(Networking_Sockets::DATA);
    msg.mutable_networking_sockets()->set_real_port(GBE_kSerializedRendezvousPort);
    msg.mutable_networking_sockets()->set_connection_id(connection_id);
    msg.mutable_networking_sockets()->set_data(data, size);
    network->sendTo(&msg, true);
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_FORWARD_RENDEZVOUS", local_id, remote_id, connection_id, size);
}

}


void Steam_Networking_Sockets_Serialized::steam_callback(void *object, Common_Message *msg)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Networking_Sockets_Serialized *steam_networkingsockets = (Steam_Networking_Sockets_Serialized *)object;
    steam_networkingsockets->Callback(msg);
}

void Steam_Networking_Sockets_Serialized::steam_run_every_runcb(void *object)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Networking_Sockets_Serialized *steam_networkingsockets = (Steam_Networking_Sockets_Serialized *)object;
    steam_networkingsockets->RunCallbacks();
}

Steam_Networking_Sockets_Serialized::Steam_Networking_Sockets_Serialized(class Settings *settings, class Networking *network, class SteamCallResults *callback_results, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, class Steam_Networking_Sockets *direct_sockets)
{
    this->settings = settings;
    this->network = network;
    this->callback_results = callback_results;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;
    this->direct_sockets = direct_sockets;

    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->network->setCallback(CALLBACK_ID_NETWORKING_SOCKETS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->run_every_runcb->add(&Steam_Networking_Sockets_Serialized::steam_run_every_runcb, this);

}

Steam_Networking_Sockets_Serialized::~Steam_Networking_Sockets_Serialized()
{
    this->network->rmCallback(CALLBACK_ID_NETWORKING_SOCKETS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->run_every_runcb->remove(&Steam_Networking_Sockets_Serialized::steam_run_every_runcb, this);
}

void Steam_Networking_Sockets_Serialized::SendP2PRendezvous( CSteamID steamIDRemote, uint32 unConnectionIDSrc, const void *pMsgRendezvous, uint32 cbRendezvous )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    const uint64 local_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 remote_id = steamIDRemote.ConvertToUint64();
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_SEND_RENDEZVOUS", local_id, remote_id, unConnectionIDSrc, cbRendezvous);

    // --- Dota 2 LAN reconnect / initial connect interception ---
    // When Dota tries to connect via P2P relay using the server's SteamID,
    // it fails on LAN (Cert failure 3). We intercept this call and fire
    // GameServerChangeRequested_t with the LAN IP instead, causing Dota's
    // engine to execute "connect <LAN IP>" which works.
    //
    // This covers two scenarios:
    //   a) Initial connect: Dota gets lobby with server_id, tries P2P first
    //   b) Reconnect after disconnect: Dota retries P2P to same server_id
    //
    // Gates:
    //   1. remote_id matches lobby server_id
    //   2. game_state >= 2 (game started)
    //   3. connect endpoint available (LAN IP)
    //   4. One-shot per connection_id to avoid firing repeatedly
    {
        GBE_DotaReconnectContext ctx{};
        const bool has_ctx = GBE_GetDotaReconnectContext(&ctx);
        const bool remote_matches = has_ctx && remote_id == ctx.server_id;
        const bool state_ready = has_ctx && ctx.game_state >= 2;
        const bool has_connect = has_ctx && ctx.connect[0] != '\0';
        const bool eligible_before = GBE_dota_reconnect_eligible.load();
        GBE_ReconnectLog(
            "GBE_RECONNECT_DIAG",
            "SendP2PRendezvous gate local_id=%llu remote_id=%llu connection_id=%u size=%u has_ctx=%u server_id=%llu game_state=%u remote_matches=%u state_ready=%u has_connect=%u eligible=%u endpoint=%s",
            (unsigned long long)local_id,
            (unsigned long long)remote_id,
            unConnectionIDSrc,
            cbRendezvous,
            has_ctx ? 1u : 0u,
            (unsigned long long)ctx.server_id,
            ctx.game_state,
            remote_matches ? 1u : 0u,
            state_ready ? 1u : 0u,
            has_connect ? 1u : 0u,
            eligible_before ? 1u : 0u,
            ctx.connect
        );
        if (remote_matches && state_ready && has_connect) {
            bool expected = true;
            if (GBE_dota_reconnect_eligible.compare_exchange_strong(expected, false)) {
                GBE_ReconnectLog("GBE_RECONNECT",
                    "Intercepted SendP2PRendezvous: remote_id=%llu matches server_id=%llu, firing GameServerChangeRequested_t endpoint=%s",
                    (unsigned long long)remote_id,
                    (unsigned long long)ctx.server_id,
                    ctx.connect);

                GameServerChangeRequested_t server_change{};
                std::strncpy(server_change.m_rgchServer, ctx.connect, sizeof(server_change.m_rgchServer) - 1);
                server_change.m_rgchServer[sizeof(server_change.m_rgchServer) - 1] = '\0';
                callbacks->addCBResult(server_change.k_iCallback, &server_change, sizeof(server_change), 0.0);
                GBE_ReconnectLog(
                    "GBE_RECONNECT_DIAG",
                    "queued callback id=%d type=GameServerChangeRequested delay=0.00 source=SendP2PRendezvous remote_id=%llu connection_id=%u endpoint=%s",
                    server_change.k_iCallback,
                    (unsigned long long)remote_id,
                    unConnectionIDSrc,
                    ctx.connect
                );

                std::string connect_command = std::string("+connect ") + ctx.connect;
                GameRichPresenceJoinRequested_t rich_join{};
                rich_join.m_steamIDFriend = CSteamID(static_cast<uint64>(ctx.owner_steam_id));
                std::strncpy(rich_join.m_rgchConnect, connect_command.c_str(), sizeof(rich_join.m_rgchConnect) - 1);
                rich_join.m_rgchConnect[sizeof(rich_join.m_rgchConnect) - 1] = '\0';
                callbacks->addCBResult(rich_join.k_iCallback, &rich_join, sizeof(rich_join), 0.25);
                GBE_ReconnectLog(
                    "GBE_RECONNECT_DIAG",
                    "queued callback id=%d type=GameRichPresenceJoinRequested delay=0.25 source=SendP2PRendezvous remote_id=%llu connection_id=%u command=%s owner=%llu",
                    rich_join.k_iCallback,
                    (unsigned long long)remote_id,
                    unConnectionIDSrc,
                    connect_command.c_str(),
                    (unsigned long long)ctx.owner_steam_id
                );
            } else {
                GBE_ReconnectLog(
                    "GBE_RECONNECT_DIAG",
                    "skipped intercept source=SendP2PRendezvous reason=not_eligible remote_id=%llu connection_id=%u endpoint=%s",
                    (unsigned long long)remote_id,
                    unConnectionIDSrc,
                    ctx.connect
                );
            }
        }
    }
    // --- End Dota 2 LAN connect interception ---

    if (!steamIDRemote.IsValid() || !pMsgRendezvous || cbRendezvous == 0)
        return;

    Common_Message msg;
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id(steamIDRemote.ConvertToUint64());
    msg.set_allocated_networking_sockets(new Networking_Sockets);
    msg.mutable_networking_sockets()->set_type(Networking_Sockets::DATA);
    msg.mutable_networking_sockets()->set_real_port(GBE_kSerializedRendezvousPort);
    msg.mutable_networking_sockets()->set_connection_id(unConnectionIDSrc);
    msg.mutable_networking_sockets()->set_data(pMsgRendezvous, cbRendezvous);
    network->sendTo(&msg, true);
}

void Steam_Networking_Sockets_Serialized::SendP2PConnectionFailure( CSteamID steamIDRemote, uint32 unConnectionIDDest, uint32 nReason, const char *pszReason )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_SEND_FAILURE", settings->get_local_steam_id().ConvertToUint64(), steamIDRemote.ConvertToUint64(), unConnectionIDDest, nReason);

    if (!steamIDRemote.IsValid())
        return;

    SteamNetworkingSocketsRecvP2PFailure_t failure = {};
    failure.steamIDRemote = settings->get_local_steam_id().ConvertToUint64();
    failure.unConnectionIDDest = unConnectionIDDest;
    failure.nReason = nReason;
    if (pszReason) {
        std::strncpy(failure.pszReason, pszReason, sizeof(failure.pszReason) - 1);
    }

    Common_Message msg;
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id(steamIDRemote.ConvertToUint64());
    msg.set_allocated_networking_sockets(new Networking_Sockets);
    msg.mutable_networking_sockets()->set_type(Networking_Sockets::CONNECTION_END);
    msg.mutable_networking_sockets()->set_real_port(GBE_kSerializedRendezvousPort);
    msg.mutable_networking_sockets()->set_connection_id(unConnectionIDDest);
    msg.mutable_networking_sockets()->set_message_number(nReason);
    msg.mutable_networking_sockets()->set_data(&failure, sizeof(failure));
    network->sendTo(&msg, true);
}

SteamAPICall_t Steam_Networking_Sockets_Serialized::GetCertAsync()
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct SteamNetworkingSocketsCert_t data = {};
    GBE_DotaReconnectContext ctx{};
    const bool has_ctx = GBE_GetDotaReconnectContext(&ctx);
    const bool state_ready = has_ctx && ctx.game_state >= 2;
    const bool has_connect = has_ctx && ctx.connect[0] != '\0';
    const bool cert_ready = state_ready && has_connect;
    data.m_eResult = cert_ready ? k_EResultOK : k_EResultNoConnection;
    if (cert_ready) {
        const auto cert = GBE_BuildSerializedNetworkingCert(settings->get_local_steam_id(), settings->get_local_game_id().AppID());
        data.m_cbCert = static_cast<uint32>(std::min<size_t>(cert.size(), sizeof(data.m_certOrMsg)));
        if (data.m_cbCert) {
            std::memcpy(data.m_certOrMsg, cert.data(), data.m_cbCert);
        }

        uint8_t private_key[sizeof(GBE_kSerializedPublicKey) + sizeof(GBE_kSerializedPrivateKey)] = {};
        std::memcpy(private_key, GBE_kSerializedPublicKey, sizeof(GBE_kSerializedPublicKey));
        std::memcpy(private_key + sizeof(GBE_kSerializedPublicKey), GBE_kSerializedPrivateKey, sizeof(GBE_kSerializedPrivateKey));
        data.m_cbPrivKey = sizeof(private_key);
        std::memcpy(data.m_privKey, private_key, sizeof(private_key));
    }
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "GetCertAsync result=%d cert_size=%u privkey_size=%u has_ctx=%u server_id=%llu game_state=%u state_ready=%u has_connect=%u endpoint=%s",
        data.m_eResult,
        data.m_cbCert,
        data.m_cbPrivKey,
        has_ctx ? 1u : 0u,
        (unsigned long long)ctx.server_id,
        ctx.game_state,
        state_ready ? 1u : 0u,
        has_connect ? 1u : 0u,
        ctx.connect
    );

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

int Steam_Networking_Sockets_Serialized::GetNetworkConfigJSON( void *buf, uint32 cbBuf, const char *pszLauncherPartner )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    (void)pszLauncherPartner;
    const int required = GBE_CopySerializedNetworkingJson(GBE_GetSerializedNetworkingConfigJSON(), buf, cbBuf);
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "GetNetworkConfigJSON cbBuf=%u required=%d partner=%s",
        cbBuf,
        required,
        pszLauncherPartner ? pszLauncherPartner : ""
    );
    return required;
}

int Steam_Networking_Sockets_Serialized::GetNetworkConfigJSON( void *buf, uint32 cbBuf )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return GetNetworkConfigJSON(buf, cbBuf, "");
}

void Steam_Networking_Sockets_Serialized::CacheRelayTicket( const void *pTicket, uint32 cbTicket )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
}

uint32 Steam_Networking_Sockets_Serialized::GetCachedRelayTicketCount()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return 0;
}

int Steam_Networking_Sockets_Serialized::GetCachedRelayTicket( uint32 idxTicket, void *buf, uint32 cbBuf )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return 0;
}

void Steam_Networking_Sockets_Serialized::PostConnectionStateMsg( const void *pMsg, uint32 cbMsg )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    const std::string payload_prefix = GBE_FormatPayloadPrefix(pMsg, cbMsg);
    const std::string payload_fields = GBE_FormatSerializedPayloadFields(pMsg, cbMsg);

    if (GBE_PostSerializedCallbackPayload<SteamNetworkingSocketsConfigUpdated_t>(callbacks, pMsg, cbMsg, "SteamNetworkingSocketsConfigUpdated") ||
        GBE_PostSerializedCallbackPayload<SteamNetworkingSocketsCert_t>(callbacks, pMsg, cbMsg, "SteamNetworkingSocketsCert") ||
        GBE_PostSerializedCallbackPayload<SteamNetworkingSocketsRecvP2PFailure_t>(callbacks, pMsg, cbMsg, "SteamNetworkingSocketsRecvP2PFailure") ||
        GBE_PostSerializedCallbackPayload<SteamNetworkingSocketsRecvP2PRendezvous_t>(callbacks, pMsg, cbMsg, "SteamNetworkingSocketsRecvP2PRendezvous")) {
        return;
    }

    GBE_DotaReconnectContext ctx{};
    const bool has_ctx = GBE_GetDotaReconnectContext(&ctx);
    const bool state_ready = has_ctx && ctx.game_state >= 2;
    const bool has_connect = has_ctx && ctx.connect[0] != '\0';
    const bool eligible_before = GBE_dota_reconnect_eligible.load();
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "PostConnectionStateMsg gate size=%u prefix=%s fields=%s has_ctx=%u server_id=%llu game_state=%u state_ready=%u has_connect=%u eligible=%u endpoint=%s",
        cbMsg,
        payload_prefix.c_str(),
        payload_fields.c_str(),
        has_ctx ? 1u : 0u,
        (unsigned long long)ctx.server_id,
        ctx.game_state,
        state_ready ? 1u : 0u,
        has_connect ? 1u : 0u,
        eligible_before ? 1u : 0u,
        ctx.connect
    );

    if (!state_ready || !has_connect)
        return;

    if (GBE_last_post_connection_state_server_id != ctx.server_id) {
        GBE_last_post_connection_state_server_id = ctx.server_id;
        GBE_post_connection_state_retry_count = 0;
        GBE_last_post_connection_state_size = 0;
    }

    const std::string endpoint(ctx.connect);
    if (direct_sockets && (GBE_last_direct_connect_server_id != ctx.server_id || GBE_last_direct_connect_endpoint != endpoint)) {
        SteamNetworkingIPAddr address{};
        if (GBE_ParseIPv4Endpoint(endpoint, &address)) {
            SteamNetworkingConfigValue_t options[3] = {};
            options[0].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 2);
            options[1].SetInt32(k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth, 2);
            options[2].SetInt32(k_ESteamNetworkingConfig_Unencrypted, 2);
            const HSteamNetConnection connection = direct_sockets->ConnectByIPAddress(address, 3, options);
            GBE_last_direct_connect_server_id = ctx.server_id;
            GBE_last_direct_connect_endpoint = endpoint;
            GBE_ReconnectLog(
                "GBE_RECONNECT_DIAG",
                "direct ConnectByIPAddress source=PostConnectionStateMsg server_id=%llu endpoint=%s connection=%u options=IP_AllowWithoutAuth:2,IPLocalHost_AllowWithoutAuth:2,Unencrypted:2",
                (unsigned long long)ctx.server_id,
                endpoint.c_str(),
                connection
            );
        } else {
            GBE_ReconnectLog(
                "GBE_RECONNECT_DIAG",
                "skipped direct ConnectByIPAddress reason=parse_failed server_id=%llu endpoint=%s",
                (unsigned long long)ctx.server_id,
                endpoint.c_str()
            );
        }
    }

    ++GBE_post_connection_state_retry_count;
    const bool size_changed = cbMsg != GBE_last_post_connection_state_size;
    GBE_last_post_connection_state_size = cbMsg;
    const bool should_queue = GBE_post_connection_state_retry_count == 1u || size_changed || (GBE_post_connection_state_retry_count % 10u) == 0u;
    if (!should_queue) {
        GBE_ReconnectLog(
            "GBE_RECONNECT_DIAG",
            "skipped intercept source=PostConnectionStateMsg reason=retry_throttle retry=%u server_id=%llu endpoint=%s",
            GBE_post_connection_state_retry_count,
            (unsigned long long)ctx.server_id,
            ctx.connect
        );
        return;
    }

    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "skipped synthetic callback id=%d source=PostConnectionStateMsg reason=outgoing_state_blob retry=%u server_id=%llu size=%u prefix=%s fields=%s",
        SteamNetworkingSocketsRecvP2PRendezvous_t::k_iCallback,
        GBE_post_connection_state_retry_count,
        (unsigned long long)ctx.server_id,
        cbMsg,
        payload_prefix.c_str(),
        payload_fields.c_str()
    );

    uint32 rendezvous_connection_id = GBE_post_connection_state_retry_count;
    (void)GBE_GetSerializedFixed32Field(pMsg, cbMsg, 3, &rendezvous_connection_id);
    const uint64 local_id = settings->get_local_steam_id().ConvertToUint64();
    uint64 rendezvous_remote_id = ctx.server_id;
    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->settings_server) {
        const uint64 local_server_id = steam_client->settings_server->get_local_steam_id().ConvertToUint64();
        if (local_server_id != 0 && local_server_id != local_id)
            rendezvous_remote_id = local_server_id;
    }
    GBE_SendSerializedRendezvous(
        network,
        local_id,
        rendezvous_remote_id,
        rendezvous_connection_id,
        pMsg,
        cbMsg
    );

    GameServerChangeRequested_t server_change{};
    std::strncpy(server_change.m_rgchServer, ctx.connect, sizeof(server_change.m_rgchServer) - 1);
    server_change.m_rgchServer[sizeof(server_change.m_rgchServer) - 1] = '\0';
    callbacks->addCBResult(server_change.k_iCallback, &server_change, sizeof(server_change), 0.0);
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "queued callback id=%d type=GameServerChangeRequested delay=0.00 source=PostConnectionStateMsg retry=%u keep_eligible=1 server_id=%llu endpoint=%s",
        server_change.k_iCallback,
        GBE_post_connection_state_retry_count,
        (unsigned long long)ctx.server_id,
        ctx.connect
    );

    std::string connect_command = std::string("+connect ") + ctx.connect;
    GameRichPresenceJoinRequested_t rich_join{};
    rich_join.m_steamIDFriend = CSteamID(static_cast<uint64>(ctx.owner_steam_id));
    std::strncpy(rich_join.m_rgchConnect, connect_command.c_str(), sizeof(rich_join.m_rgchConnect) - 1);
    rich_join.m_rgchConnect[sizeof(rich_join.m_rgchConnect) - 1] = '\0';
    callbacks->addCBResult(rich_join.k_iCallback, &rich_join, sizeof(rich_join), 0.25);
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "queued callback id=%d type=GameRichPresenceJoinRequested delay=0.25 source=PostConnectionStateMsg retry=%u keep_eligible=1 command=%s owner=%llu",
        rich_join.k_iCallback,
        GBE_post_connection_state_retry_count,
        connect_command.c_str(),
        (unsigned long long)ctx.owner_steam_id
    );
}

bool Steam_Networking_Sockets_Serialized::GetSTUNServer(int dont_know, char *buf, unsigned int len)
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (buf && len > 0)
        buf[0] = '\0';
    return false;
}

bool Steam_Networking_Sockets_Serialized::BAllowDirectConnectToPeer(SteamNetworkingIdentity const &identity)
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_ALLOW_DIRECT", settings->get_local_steam_id().ConvertToUint64(), identity.GetSteamID64(), 0, 1);
    return true;
}

int Steam_Networking_Sockets_Serialized::BeginAsyncRequestFakeIP(int a)
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return true;
}

void Steam_Networking_Sockets_Serialized::RunCallbacks()
{
    
}

void Steam_Networking_Sockets_Serialized::Callback(Common_Message *msg)
{
    if (msg->has_low_level()) {
        if (msg->low_level().type() == Low_Level::CONNECT) {
            
        }

        if (msg->low_level().type() == Low_Level::DISCONNECT) {

        }
    }

    if (msg->has_networking_sockets() && msg->networking_sockets().real_port() == GBE_kSerializedRendezvousPort) {
        const Networking_Sockets &net_msg = msg->networking_sockets();
        if (net_msg.type() == Networking_Sockets::DATA) {
            SteamNetworkingSocketsRecvP2PRendezvous_t data = {};
            data.steamIDRemote = msg->source_id();
            data.unConnectionIDSrc = static_cast<uint32>(net_msg.connection_id());
            data.m_cbRendezvous = static_cast<uint32>(std::min<size_t>(net_msg.data().size(), sizeof(data.m_MsgRendezvous)));
            if (data.m_cbRendezvous > 0)
                std::memcpy(data.m_MsgRendezvous, net_msg.data().data(), data.m_cbRendezvous);

            GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_RECV_RENDEZVOUS", settings->get_local_steam_id().ConvertToUint64(), msg->source_id(), data.unConnectionIDSrc, data.m_cbRendezvous);
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        } else if (net_msg.type() == Networking_Sockets::CONNECTION_END) {
            SteamNetworkingSocketsRecvP2PFailure_t data = {};
            if (net_msg.data().size() >= sizeof(data)) {
                std::memcpy(&data, net_msg.data().data(), sizeof(data));
            } else {
                data.steamIDRemote = msg->source_id();
                data.unConnectionIDDest = static_cast<uint32>(net_msg.connection_id());
                data.nReason = static_cast<uint32>(net_msg.message_number());
            }

            GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_RECV_FAILURE", settings->get_local_steam_id().ConvertToUint64(), msg->source_id(), data.unConnectionIDDest, data.nReason);
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        }
    }
}

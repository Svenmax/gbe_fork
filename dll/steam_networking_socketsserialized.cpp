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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

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
    static constexpr uint8_t key_data[32] = {
        0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
        0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
        0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
    };

    const uint32 now = static_cast<uint32>(std::time(nullptr));
    const uint32 expiry = now + 24u * 60u * 60u;
    const uint64 steam_id64 = steam_id.ConvertToUint64();
    const std::string identity = std::string("steamid:") + std::to_string(steam_id64);

    std::vector<uint8_t> cert;
    cert.reserve(128);
    GBE_AppendVarint(cert, (1u << 3) | 0u);
    GBE_AppendVarint(cert, 1); // CMsgSteamDatagramCertificate.ED25519
    GBE_AppendBytes(cert, 2, key_data, sizeof(key_data));
    GBE_AppendVarint(cert, (4u << 3) | 1u);
    GBE_AppendFixed64(cert, steam_id64);
    GBE_AppendVarint(cert, (8u << 3) | 5u);
    GBE_AppendFixed32(cert, now);
    GBE_AppendVarint(cert, (9u << 3) | 5u);
    GBE_AppendFixed32(cert, expiry);
    GBE_AppendVarint(cert, (10u << 3) | 0u);
    GBE_AppendVarint(cert, app_id);
    GBE_AppendBytes(cert, 12, identity.data(), identity.size());
    return cert;
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

Steam_Networking_Sockets_Serialized::Steam_Networking_Sockets_Serialized(class Settings *settings, class Networking *network, class SteamCallResults *callback_results, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb)
{
    this->settings = settings;
    this->network = network;
    this->callback_results = callback_results;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;

    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->run_every_runcb->add(&Steam_Networking_Sockets_Serialized::steam_run_every_runcb, this);

}

Steam_Networking_Sockets_Serialized::~Steam_Networking_Sockets_Serialized()
{
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Networking_Sockets_Serialized::steam_callback, this);
    this->run_every_runcb->remove(&Steam_Networking_Sockets_Serialized::steam_run_every_runcb, this);
}

void Steam_Networking_Sockets_Serialized::SendP2PRendezvous( CSteamID steamIDRemote, uint32 unConnectionIDSrc, const void *pMsgRendezvous, uint32 cbRendezvous )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
}

void Steam_Networking_Sockets_Serialized::SendP2PConnectionFailure( CSteamID steamIDRemote, uint32 unConnectionIDDest, uint32 nReason, const char *pszReason )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
}

SteamAPICall_t Steam_Networking_Sockets_Serialized::GetCertAsync()
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct SteamNetworkingSocketsCert_t data = {};
    data.m_eResult = k_EResultOK;

    const auto cert = GBE_BuildSerializedNetworkingCert(settings->get_local_steam_id(), settings->get_local_game_id().AppID());
    data.m_cbCert = static_cast<uint32>(std::min<size_t>(cert.size(), sizeof(data.m_certOrMsg)));
    if (data.m_cbCert) {
        std::memcpy(data.m_certOrMsg, cert.data(), data.m_cbCert);
    }

    data.m_caKeyID = 0;
    data.m_cbSignature = 0;
    static constexpr uint8_t private_key[32] = {
        0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
        0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
        0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
        0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
    };
    data.m_cbPrivKey = sizeof(private_key);
    std::memcpy(data.m_privKey, private_key, sizeof(private_key));

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

int Steam_Networking_Sockets_Serialized::GetNetworkConfigJSON( void *buf, uint32 cbBuf, const char *pszLauncherPartner )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return GBE_CopySerializedNetworkingJson("{}", buf, cbBuf);
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
}

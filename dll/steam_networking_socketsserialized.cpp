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
#include "dll/gbe_ed25519.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <mbedtls/sha256.h>

#if defined(__has_include)
#if __has_include(<psa/crypto.h>)
#define GBE_HAS_PSA_CRYPTO 1
#include <psa/crypto.h>
#endif
#endif

namespace {

constexpr int GBE_kSerializedRendezvousPort = -5434;

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
    std::vector<uint8_t> identity_binary;
    GBE_AppendVarint(identity_binary, (16u << 3) | 1u);
    GBE_AppendFixed64(identity_binary, steam_id64);

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
    GBE_AppendBytes(cert, 11, identity_binary.data(), identity_binary.size());
    GBE_AppendBytes(cert, 12, identity.data(), identity.size());
    return cert;
}

uint64_t GBE_CalculateSteamNetworkingPublicKeyID(const uint8_t *public_key, size_t public_key_size)
{
    if (!public_key || public_key_size != 32)
        return 0;

    uint8_t digest[32] = {};
    if (mbedtls_sha256(public_key, public_key_size, digest, 0) != 0)
        return 0;

    uint64_t key_id = 0;
    for (int i = 0; i < 8; ++i) {
        key_id |= static_cast<uint64_t>(digest[i]) << (i * 8);
    }
    return key_id;
}

bool GBE_SignSerializedNetworkingCert(const std::vector<uint8_t> &cert, const uint8_t *private_key, size_t private_key_size, uint8_t *signature, size_t signature_size)
{
    if (cert.empty() || !private_key || private_key_size != 32 || !signature || signature_size < 64)
        return false;

    if (gbe_ed25519_sign(signature, cert.data(), cert.size(), private_key) == 0)
        return true;

#if defined(GBE_HAS_PSA_CRYPTO) && defined(PSA_ALG_PURE_EDDSA) && defined(PSA_ECC_FAMILY_TWISTED_EDWARDS)
    if (psa_crypto_init() != PSA_SUCCESS)
        return false;

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
    psa_set_key_bits(&attributes, 255);

    psa_key_id_t key = 0;
    psa_status_t status = psa_import_key(&attributes, private_key, private_key_size, &key);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS)
        return false;

    size_t signature_len = 0;
    status = psa_sign_message(key, PSA_ALG_PURE_EDDSA, cert.data(), cert.size(), signature, signature_size, &signature_len);
    psa_destroy_key(key);

    return status == PSA_SUCCESS && signature_len == 64;
#else
    (void)cert;
    (void)private_key;
    (void)private_key_size;
    (void)signature;
    (void)signature_size;
    return false;
#endif
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
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_SEND_RENDEZVOUS", settings->get_local_steam_id().ConvertToUint64(), steamIDRemote.ConvertToUint64(), unConnectionIDSrc, cbRendezvous);

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
    data.m_eResult = k_EResultOK;

    static constexpr uint8_t public_key[32] = {
        0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
        0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
        0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a,
    };
    static constexpr uint8_t private_key[32] = {
        0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
        0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
        0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
        0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60,
    };

    const auto cert = GBE_BuildSerializedNetworkingCert(settings->get_local_steam_id(), settings->get_local_game_id().AppID());
    data.m_cbCert = static_cast<uint32>(std::min<size_t>(cert.size(), sizeof(data.m_certOrMsg)));
    if (data.m_cbCert) {
        std::memcpy(data.m_certOrMsg, cert.data(), data.m_cbCert);
    }

    /* Retail SteamNetworkingSockets has a hardcoded root CA and rejects extra
       self-signed CA roots from SDR config.  Return an unsigned direct-IP cert
       instead so LAN connections use the IP_AllowWithoutAuth path. */
    data.m_caKeyID = 0;
    data.m_cbSignature = 0;
    data.m_cbPrivKey = sizeof(private_key) + sizeof(public_key);
    std::memcpy(data.m_privKey, public_key, sizeof(public_key));
    std::memcpy(data.m_privKey + sizeof(public_key), private_key, sizeof(private_key));

    {
        FILE *dbg = std::fopen("C:\\Users\\Public\\gbe_gc_debug.log", "a");
        if (dbg) {
            std::fprintf(dbg, "[NETSOCK_SERIALIZED_GET_CERT] eResult=%d cbCert=%u cbSig=%u cbPrivKey=%u caKeyID=%llu\n",
                (int)data.m_eResult, data.m_cbCert, data.m_cbSignature, data.m_cbPrivKey, (unsigned long long)data.m_caKeyID);
            std::fprintf(dbg, "[NETSOCK_SERIALIZED_GET_CERT] privKey[0..7]=");
            for (int i = 0; i < 8 && i < (int)data.m_cbPrivKey; ++i)
                std::fprintf(dbg, "%02x", (unsigned char)data.m_privKey[i]);
            std::fprintf(dbg, " certKeyData[0..7]=");
            /* field 2 in protobuf cert starts after field 1 (varint tag+val = 2 bytes), then tag for field 2 (1 byte 0x12), len (1 byte 0x20), then 32 bytes key_data */
            if (data.m_cbCert > 4) {
                /* Scan for field 2 tag byte 0x12 followed by length 0x20 */
                for (uint32 off = 0; off + 34 <= data.m_cbCert; ++off) {
                    if ((unsigned char)data.m_certOrMsg[off] == 0x12 && (unsigned char)data.m_certOrMsg[off+1] == 0x20) {
                        for (int i = 0; i < 8; ++i)
                            std::fprintf(dbg, "%02x", (unsigned char)data.m_certOrMsg[off+2+i]);
                        break;
                    }
                }
            }
            std::fprintf(dbg, "\n");
            std::fclose(dbg);
        }
    }

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_GET_CERT", settings->get_local_steam_id().ConvertToUint64(), 0, data.m_cbCert, data.m_eResult);
    return ret;
}

int Steam_Networking_Sockets_Serialized::GetNetworkConfigJSON( void *buf, uint32 cbBuf, const char *pszLauncherPartner )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    (void)pszLauncherPartner;
    const char *json = GBE_GetSerializedNetworkingConfigJSON();
    const int required = GBE_CopySerializedNetworkingJson(json, buf, cbBuf);
    GBE_LogSerializedNetSockTrace("NETSOCK_SERIALIZED_GET_CONFIG", settings->get_local_steam_id().ConvertToUint64(), 0, cbBuf, static_cast<uint32>(required));
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

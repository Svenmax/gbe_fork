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
#include "dll/gbe_dota_reconnect_shared.h"

#include <cstdio>
#include <cstring>

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
    data.m_eResult = (state_ready && has_connect) ? k_EResultOK : k_EResultNoConnection;
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "GetCertAsync result=%d has_ctx=%u server_id=%llu game_state=%u state_ready=%u has_connect=%u endpoint=%s",
        data.m_eResult,
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
    if (buf && cbBuf > 0)
        reinterpret_cast<char *>(buf)[0] = '\0';
    return 0;
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
    (void)pMsg;

    GBE_DotaReconnectContext ctx{};
    const bool has_ctx = GBE_GetDotaReconnectContext(&ctx);
    const bool state_ready = has_ctx && ctx.game_state >= 2;
    const bool has_connect = has_ctx && ctx.connect[0] != '\0';
    const bool eligible_before = GBE_dota_reconnect_eligible.load();
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "PostConnectionStateMsg gate size=%u has_ctx=%u server_id=%llu game_state=%u state_ready=%u has_connect=%u eligible=%u endpoint=%s",
        cbMsg,
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

    bool expected = true;
    if (!GBE_dota_reconnect_eligible.compare_exchange_strong(expected, false)) {
        GBE_ReconnectLog(
            "GBE_RECONNECT_DIAG",
            "skipped intercept source=PostConnectionStateMsg reason=not_eligible server_id=%llu endpoint=%s",
            (unsigned long long)ctx.server_id,
            ctx.connect
        );
        return;
    }

    GameServerChangeRequested_t server_change{};
    std::strncpy(server_change.m_rgchServer, ctx.connect, sizeof(server_change.m_rgchServer) - 1);
    server_change.m_rgchServer[sizeof(server_change.m_rgchServer) - 1] = '\0';
    callbacks->addCBResult(server_change.k_iCallback, &server_change, sizeof(server_change), 0.0);
    GBE_ReconnectLog(
        "GBE_RECONNECT_DIAG",
        "queued callback id=%d type=GameServerChangeRequested delay=0.00 source=PostConnectionStateMsg server_id=%llu endpoint=%s",
        server_change.k_iCallback,
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
        "queued callback id=%d type=GameRichPresenceJoinRequested delay=0.25 source=PostConnectionStateMsg command=%s owner=%llu",
        rich_join.k_iCallback,
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

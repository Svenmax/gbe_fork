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
#include "dll/steam_networking_utils.h"


namespace {

template <typename T>
const T *as_serialized_callback_payload(const void *pMsg, uint32 cbMsg)
{
    if (!pMsg) return nullptr;

    if (cbMsg == sizeof(T)) {
        return static_cast<const T *>(pMsg);
    }

    if (cbMsg == sizeof(int) + sizeof(T)) {
        const auto *bytes = static_cast<const uint8_t *>(pMsg);
        int callback_id = 0;
        memcpy(&callback_id, bytes, sizeof(callback_id));
        if (callback_id == T::k_iCallback) {
            return reinterpret_cast<const T *>(bytes + sizeof(callback_id));
        }
    }

    return nullptr;
}

template <typename T>
bool post_serialized_callback(SteamCallBacks *callbacks, const void *pMsg, uint32 cbMsg)
{
    const T *payload = as_serialized_callback_payload<T>(pMsg, cbMsg);
    if (!payload) return false;

    callbacks->addCBResult(T::k_iCallback, const_cast<T *>(payload), sizeof(T));
    return true;
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
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct SteamNetworkingSocketsCert_t data = {};
    data.m_eResult = k_EResultOK;

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

int Steam_Networking_Sockets_Serialized::GetNetworkConfigJSON( void *buf, uint32 cbBuf, const char *pszLauncherPartner )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
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
    PRINT_DEBUG("cbMsg=%u", cbMsg);

    FnSteamNetConnectionStatusChanged connection_status_changed = nullptr;
    FnSteamNetAuthenticationStatusChanged auth_status_changed = nullptr;
    SteamNetConnectionStatusChangedCallback_t connection_status{};
    SteamNetAuthenticationStatus_t auth_status{};
    bool have_connection_status = false;
    bool have_auth_status = false;

    {
        std::lock_guard<std::recursive_mutex> lock(global_mutex);

        if (const auto *payload = as_serialized_callback_payload<SteamNetConnectionStatusChangedCallback_t>(pMsg, cbMsg)) {
            connection_status = *payload;
            have_connection_status = true;
            callbacks->addCBResult(connection_status.k_iCallback, &connection_status, sizeof(connection_status));
            connection_status_changed = Steam_Networking_Utils::get_global_connection_status_changed_callback();
        } else if (const auto *payload = as_serialized_callback_payload<SteamNetAuthenticationStatus_t>(pMsg, cbMsg)) {
            auth_status = *payload;
            have_auth_status = true;
            callbacks->addCBResult(auth_status.k_iCallback, &auth_status, sizeof(auth_status));
            auth_status_changed = Steam_Networking_Utils::get_global_auth_status_changed_callback();
        } else if (post_serialized_callback<SteamNetworkingSocketsConfigUpdated_t>(callbacks, pMsg, cbMsg) ||
                   post_serialized_callback<SteamNetworkingSocketsRecvP2PFailure_t>(callbacks, pMsg, cbMsg) ||
                   post_serialized_callback<SteamNetworkingSocketsRecvP2PRendezvous_t>(callbacks, pMsg, cbMsg) ||
                   post_serialized_callback<SteamNetworkingSocketsCert_t>(callbacks, pMsg, cbMsg)) {
            return;
        } else {
            PRINT_DEBUG("unhandled serialized networking callback payload size=%u", cbMsg);
            return;
        }
    }

    if (have_connection_status && connection_status_changed) {
        connection_status_changed(&connection_status);
    }

    if (have_auth_status && auth_status_changed) {
        auth_status_changed(&auth_status);
    }
}

bool Steam_Networking_Sockets_Serialized::GetSTUNServer(int dont_know, char *buf, unsigned int len)
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
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

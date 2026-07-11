#ifndef GBE_DOTA_HANDLER_TEST_FIXTURE_H
#define GBE_DOTA_HANDLER_TEST_FIXTURE_H

#include "stubs.h"

struct TestFixture
{
    Steam_Game_Coordinator gc;
    Settings settings;
    Networking network;
    SteamCallBacks callbacks;
    ActionRecorder recorder;

    TestFixture()
    {
        gc.settings = &settings;
        gc.network = &network;
        gc.callbacks = &callbacks;
        gc.gc_profile = Steam_Game_Coordinator::GC_PROFILE_DOTA2;
        gc.is_server = false;
        g_action_recorder = &recorder;
    }

    ~TestFixture()
    {
        g_action_recorder = nullptr;
    }

    void reset()
    {
        recorder.clear();
        gc.items.clear();
        gc.pending_messages.clear();
        while (!gc.incoming_messages.empty())
            gc.incoming_messages.pop();
        gc.GBE_local_lobby = GBE_LocalLobby{};
        gc.GBE_dota_lobby_generation_counter = gbe::dota_lobby_generation::Counter{};
        gc.GBE_ClearPendingDotaAbandonFinalizeAfterOtherLeftChannel();
        gc.GBE_ClearPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed();
        gc.GBE_ClearPendingResetAfterCacheUnsubscribed();
        gc.GBE_ClearDotaLoginSyncSent();
        gc.GBE_ClearDotaPrivateLobbySnapshotReplayed();
        gc.GBE_ClearDotaHostShowcaseEquipPushed();
        gc.GBE_ClearLastDotaLaunchStatePushedGameState();
        gc.GBE_ClearLastDotaLaunchPersonaSignature();
        gc.GBE_ClearLastDotaDirectConnectCallbackKey();
        gc.test_set_active_server_lobby(false);
        gc.test_clear_next_lobby_capture();
        gc.test_set_dota_response_result(true);
        gc.test_set_member_runtime_result(true);
        gc.test_set_runtime_update_result(true);
        GBE_GetSharedDotaLobbyStateStore().clear();
        g_test_steam_client.steam_matchmaking = nullptr;
        g_test_steam_client.steam_game_coordinator = nullptr;
        g_test_steam_client.steam_gameserver_game_coordinator = nullptr;
    }

    Econ_Item &add_item(uint64_t id, uint32_t def_index = 100)
    {
        Econ_Item item;
        item.id = id;
        item.def = def_index;
        item.level = 1;
        item.quantity = 1;
        item.style = 0;
        gc.items.push_back(std::move(item));
        return gc.items.back();
    }
};

#endif

#ifndef __INCLUDED_GBE_DOTA_PAYLOAD_WIRE_HELPERS_H__
#define __INCLUDED_GBE_DOTA_PAYLOAD_WIRE_HELPERS_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <steam/steamtypes.h>

#include "gbe_dota_types.h"

struct GBE_DotaServerHelloContext
{
    bool valid{};
    uint32 active_version{};
    uint32 min_allowed_version{};
    uint64 compatibility_value{};
    uint32 universe{};
    uint64 source_job_id{};
    bool has_source_job{};
    uint64 client_steam_id{};
    bool has_client_steam_id{};
    int32 client_session_id{};
    bool has_client_session_id{};
    uint32 source_app_id{};
    bool has_source_app_id{};
    uint32 gc_msg_src{};
    bool has_gc_msg_src{};
    uint32 gc_dir_index_source{};
    bool has_gc_dir_index_source{};
};

struct GBE_DotaHelloContext
{
    bool valid{};
    uint32 version{};
    std::string outer_session_field_raw;
    uint64 source_job_id{};
    bool has_source_job{};
    uint64 target_job_id{};
    bool has_target_job{};
    uint64 client_steam_id{};
    bool has_client_steam_id{};
    int32 client_session_id{};
    bool has_client_session_id{};
    uint32 source_app_id{};
    bool has_source_app_id{};
    uint32 gc_msg_src{};
    bool has_gc_msg_src{};
    uint32 gc_dir_index_source{};
    bool has_gc_dir_index_source{};
};

bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count);

bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note);

bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope);

bool GBE_PatchDotaTemplateIdentifiers(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note);

bool GBE_PrepareDotaDirectReplayMessage(
    const uint8 *template_bytes,
    size_t template_size,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    bool has_target_job,
    uint64 target_job,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note,
    std::string &message);

bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message);

bool GBE_PatchDotaLobbyTemplateIdentifiers(std::string &message, uint32 account_id, uint64 steam_id, uint64 lobby_id);

bool GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(std::string &message, uint64 steam_id, uint64 lobby_id);

bool GBE_ForceDotaLobbyCacheOwnerSOID(std::string &message, uint64 lobby_id);

bool GBE_ForceDotaLobbyUpdateOwnerSOID(std::string &message, uint64 lobby_id);

bool GBE_PatchDotaPracticeLobbyLaunchTemplate(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    bool patch_server_id,
    bool patch_game_start_time,
    bool patch_connect,
    const char *stage_note);

bool GBE_PrepareDotaWelcomeBody(uint64 steam_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &inner_body);

bool GBE_ExtractDotaHelloContext(const void *pubData, uint32 cubData, GBE_DotaHelloContext &context);

bool GBE_ExtractDirectDotaHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaHelloContext &context);

bool GBE_ExtractDirectDotaServerHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaServerHelloContext &context);

bool GBE_BuildDirectDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message);

bool GBE_ComposeDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message);

#endif // __INCLUDED_GBE_DOTA_PAYLOAD_WIRE_HELPERS_H__

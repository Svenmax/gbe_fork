// Auto-generated stub for offline testing
// Provides CMsgProtoBufHeader used by gbe_dota_request_router.h
// Note: in the real SDK this is in the global namespace (not gamecoordinator::tf2)
#pragma once
#include <cstdint>
#include <string>

// GCProtoBufMsgSrc - defined in steammessages.proto (no package = global namespace)
enum GCProtoBufMsgSrc {
    GCProtoBufMsgSrc_Unspecified = 0,
    GCProtoBufMsgSrc_FromSystem = 1,
    GCProtoBufMsgSrc_FromSteamID = 2,
    GCProtoBufMsgSrc_FromGC = 3,
    GCProtoBufMsgSrc_ReplySystem = 4,
};

// CMsgProtoBufHeader - used by GBE_DirectProtoContext as a value member
class CMsgProtoBufHeader
{
public:
    // Getters
    bool has_job_id_source() const { return false; }
    uint64_t job_id_source() const { return 0; }
    bool has_job_id_target() const { return false; }
    uint64_t job_id_target() const { return 0; }
    bool has_client_steam_id() const { return false; }
    uint64_t client_steam_id() const { return 0; }
    bool has_client_session_id() const { return false; }
    uint32_t client_session_id() const { return 0; }
    bool has_source_app_id() const { return false; }
    uint32_t source_app_id() const { return 0; }
    bool has_gc_msg_src() const { return false; }
    uint32_t gc_msg_src() const { return 0; }
    bool has_gc_dir_index_source() const { return false; }
    uint32_t gc_dir_index_source() const { return 0; }

    // Setters (no-ops, just store values)
    void set_job_id_source(uint64_t v) { (void)v; }
    void set_job_id_target(uint64_t v) { (void)v; }
    void set_client_steam_id(uint64_t v) { (void)v; }
    void set_client_session_id(uint32_t v) { (void)v; }
    void set_source_app_id(uint32_t v) { (void)v; }
    void set_gc_msg_src(GCProtoBufMsgSrc v) { (void)v; }
    void set_gc_dir_index_source(uint32_t v) { (void)v; }

    void Clear() {}
    bool ParseFromArray(const void *data, int size) { (void)data; (void)size; return false; }
    std::string SerializeAsString() const { return {}; }
    bool SerializeToString(std::string *output) const { (void)output; return true; }
    bool AppendToString(std::string *output) const { (void)output; return true; }
    size_t ByteSizeLong() const { return 0; }
    void clear_job_id_target() {}
    void clear_job_id_source() {}
    void clear_client_steam_id() {}
    void clear_client_session_id() {}
    void clear_source_app_id() {}
};

// CMsgServerHello - used by GBE_ExtractDirectDotaServerHelloContext
class CMsgServerHello
{
public:
    bool has_version() const { return false; }
    uint32_t version() const { return 0; }
    bool ParseFromArray(const void *data, int size) { (void)data; (void)size; return false; }
};

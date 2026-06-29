// Auto-generated stub for offline testing
// Provides SO cache types used by gbe_dota_gc_payload_helpers.cpp
// These are in the gamecoordinator::tf2 namespace as expected by the TU
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace gamecoordinator { namespace tf2 {

class CMsgSOIDOwner
{
public:
    void set_type(uint32_t type) { (void)type; }
    void set_id(uint64_t id) { (void)id; }
    uint32_t type() const { return 0; }
    uint64_t id() const { return 0; }
};

class CMsgSOCacheSubscribed_Object
{
public:
    uint32_t type_id() const { return 0; }
    int object_data_size() const { return 0; }
    std::string object_data() const { return m_data; }
    std::string object_data(int index) const { (void)index; return m_data; }
    std::string *mutable_object_data(int index) { (void)index; return &m_data; }
    void set_object_data(int index, const std::string &data) { (void)index; m_data = data; }
    std::string m_data;
};

class CMsgSOCacheSubscribed
{
public:
    // Mutators
    CMsgSOIDOwner *mutable_owner_soid() { return &m_owner_soid; }
    void clear_owner() {}
    void clear_objects() {}
    int objects_size() const { return 0; }
    CMsgSOCacheSubscribed_Object *mutable_objects(int index) { (void)index; static CMsgSOCacheSubscribed_Object obj; return &obj; }

    // Accessors used by GBE_LogDotaSOCacheSubscribedSummary
    bool has_owner_soid() const { return false; }
    const CMsgSOIDOwner &owner_soid() const { return m_owner_soid; }
    bool has_version() const { return false; }
    uint64_t version() const { return 0; }
    bool has_service_id() const { return false; }
    uint32_t service_id() const { return 0; }
    int service_list_size() const { return 0; }
    bool has_sync_version() const { return false; }
    uint64_t sync_version() const { return 0; }
    const CMsgSOCacheSubscribed_Object &objects(int index) const { (void)index; static CMsgSOCacheSubscribed_Object obj; return obj; }

    // Serialization
    std::string SerializeAsString() const { return m_data; }
    bool ParseFromString(const std::string &data) { m_data = data; return true; }
    bool ParseFromArray(const void *data, int size) { m_data.assign(static_cast<const char*>(data), size); return true; }
    bool AppendToString(std::string *output) const { *output += m_data; return true; }

    CMsgSOIDOwner m_owner_soid;
    std::string m_data;
};

class CMsgSOMultipleObjects_Object
{
public:
    uint32_t type_id() const { return 0; }
    int object_data_size() const { return 0; }
    std::string object_data() const { return m_data; }
    std::string object_data(int index) const { (void)index; return m_data; }
    std::string *mutable_object_data(int index) { (void)index; return &m_data; }
    void set_object_data(int index, const std::string &data) { (void)index; m_data = data; }
    std::string m_data;
};

class CMsgSOMultipleObjects
{
public:
    CMsgSOIDOwner *mutable_owner_soid() { return &m_owner_soid; }
    void clear_owner() {}
    void clear_objects() {}
    int objects_size() const { return 0; }
    CMsgSOMultipleObjects_Object *mutable_objects(int index) { (void)index; static CMsgSOMultipleObjects_Object obj; return &obj; }
    std::string SerializeAsString() const { return m_data; }
    bool ParseFromString(const std::string &data) { m_data = data; return true; }
    bool ParseFromArray(const void *data, int size) { m_data.assign(static_cast<const char*>(data), size); return true; }
    bool AppendToString(std::string *output) const { *output += m_data; return true; }

    CMsgSOIDOwner m_owner_soid;
    std::string m_data;
};

// CMsgServerWelcome - used by GBE_BuildDirectDotaServerWelcome()
class CMsgServerWelcome
{
public:
    void set_min_allowed_version(uint32_t v) { (void)v; }
    void set_active_version(uint32_t v) { (void)v; }
    bool AppendToString(std::string *output) const { (void)output; return true; }
    std::string SerializeAsString() const { return {}; }
    bool ParseFromArray(const void *data, int size) { (void)data; (void)size; return false; }
    bool ParseFromString(const std::string &data) { (void)data; return false; }
};

}} // namespace gamecoordinator::tf2

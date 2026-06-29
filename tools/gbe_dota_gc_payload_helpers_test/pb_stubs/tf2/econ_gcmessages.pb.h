// Auto-generated stub for offline testing
// Provides CSOEconItem used by GBE_BuildSOSingleObjectFromItem()
#pragma once
#include <cstdint>
#include <string>

namespace gamecoordinator { namespace tf2 {

class CSOEconItem
{
public:
    void set_id(uint64_t v) { id = v; }
    void set_account_id(uint32_t v) { account_id = v; }
    void set_def_index(uint32_t v) { def_index = v; }
    void set_inventory(uint32_t v) { inventory = v; }
    void set_quantity(uint32_t v) { quantity = v; }
    void set_level(uint32_t v) { level = v; }
    void set_quality(uint32_t v) { quality = v; }
    void set_flags(uint32_t v) { flags = v; }
    void set_origin(uint32_t v) { origin = v; }
    void set_in_use(bool v) { in_use = v; }
    void set_style(uint32_t v) { style = v; }
    void set_original_id(uint64_t v) { original_id = v; }

    std::string SerializeAsString() const {
        std::string out;
        out.reserve(64);
        // Minimal protobuf: just encode a few fields so output is non-empty
        // field 1 (varint) = id, field 2 (varint) = account_id, etc.
        // For test purposes, just produce a non-empty deterministic string
        for (size_t i = 0; i < 8; ++i)
            out.push_back(static_cast<char>(i + 1));
        return out;
    }
    bool ParseFromArray(const void *data, int size) { m_data.assign(static_cast<const char*>(data), size); return true; }
    bool AppendToString(std::string *output) const { *output += m_data; return true; }

    uint64_t id{};
    uint32_t account_id{};
    uint32_t def_index{};
    uint32_t inventory{};
    uint32_t quantity{};
    uint32_t level{};
    uint32_t quality{};
    uint32_t flags{};
    uint32_t origin{};
    bool in_use{};
    uint32_t style{};
    uint64_t original_id{};
    std::string m_data;
};

}} // namespace gamecoordinator::tf2

// Auto-generated stub for offline testing
// Provides CSOEconItem used by GBE_BuildSOSingleObjectFromItem()
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

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
    void set_custom_name(const std::string &v) { custom_name = v; }
    void set_custom_desc(const std::string &v) { custom_desc = v; }
    void set_contains_equipped_state(bool v) { contains_equipped_state = v; }
    void set_contains_equipped_state_v2(bool v) { contains_equipped_state_v2 = v; }

    class EquippedState
    {
    public:
        void set_new_class(uint32_t v) { new_class = v; }
        void set_new_slot(uint32_t v) { new_slot = v; }

        uint32_t new_class{};
        uint32_t new_slot{};
    };

    class Attribute
    {
    public:
        void set_def_index(uint32_t v) { def_index = v; }
        void set_value(uint32_t v) { value = v; }
        void set_value_bytes(const std::string &v) { value_bytes = v; }

        uint32_t def_index{};
        uint32_t value{};
        std::string value_bytes;
    };

    EquippedState *add_equipped_state() { equipped_states.emplace_back(); return &equipped_states.back(); }
    Attribute *add_attribute() { attributes.emplace_back(); return &attributes.back(); }

    std::string SerializeAsString() const {
        std::string out;
        out.append(reinterpret_cast<const char *>(&id), sizeof(id));
        out.append(reinterpret_cast<const char *>(&account_id), sizeof(account_id));
        out.append(reinterpret_cast<const char *>(&def_index), sizeof(def_index));
        out.append(reinterpret_cast<const char *>(&inventory), sizeof(inventory));
        out.append(reinterpret_cast<const char *>(&quantity), sizeof(quantity));
        out.append(reinterpret_cast<const char *>(&level), sizeof(level));
        out.append(reinterpret_cast<const char *>(&quality), sizeof(quality));
        out.append(reinterpret_cast<const char *>(&flags), sizeof(flags));
        out.append(reinterpret_cast<const char *>(&origin), sizeof(origin));
        out.push_back(in_use ? 1 : 0);
        out.append(reinterpret_cast<const char *>(&style), sizeof(style));
        out.append(reinterpret_cast<const char *>(&original_id), sizeof(original_id));
        out.push_back(contains_equipped_state ? 1 : 0);
        out.push_back(contains_equipped_state_v2 ? 1 : 0);
        const uint32_t equipped_count = static_cast<uint32_t>(equipped_states.size());
        out.append(reinterpret_cast<const char *>(&equipped_count), sizeof(equipped_count));
        for (const auto &state : equipped_states) {
            out.append(reinterpret_cast<const char *>(&state.new_class), sizeof(state.new_class));
            out.append(reinterpret_cast<const char *>(&state.new_slot), sizeof(state.new_slot));
        }
        const uint32_t attr_count = static_cast<uint32_t>(attributes.size());
        out.append(reinterpret_cast<const char *>(&attr_count), sizeof(attr_count));
        for (const auto &attr : attributes) {
            out.append(reinterpret_cast<const char *>(&attr.def_index), sizeof(attr.def_index));
            out.append(reinterpret_cast<const char *>(&attr.value), sizeof(attr.value));
            const uint32_t value_bytes_size = static_cast<uint32_t>(attr.value_bytes.size());
            out.append(reinterpret_cast<const char *>(&value_bytes_size), sizeof(value_bytes_size));
            out.append(attr.value_bytes);
        }
        const uint32_t custom_name_size = static_cast<uint32_t>(custom_name.size());
        out.append(reinterpret_cast<const char *>(&custom_name_size), sizeof(custom_name_size));
        out.append(custom_name);
        const uint32_t custom_desc_size = static_cast<uint32_t>(custom_desc.size());
        out.append(reinterpret_cast<const char *>(&custom_desc_size), sizeof(custom_desc_size));
        out.append(custom_desc);
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
    std::string custom_name;
    std::string custom_desc;
    bool contains_equipped_state{};
    bool contains_equipped_state_v2{};
    std::vector<EquippedState> equipped_states;
    std::vector<Attribute> attributes;
    std::string m_data;
};

}} // namespace gamecoordinator::tf2

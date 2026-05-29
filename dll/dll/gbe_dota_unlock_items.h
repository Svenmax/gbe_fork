// gbe_dota_unlock_items.h
// Reads items_game.txt from Dota 2 VPK, parses VDF, and generates CSOEconItem
// protobuf entries for all cosmetic items (unlock all).

#ifndef GBE_DOTA_UNLOCK_ITEMS_H
#define GBE_DOTA_UNLOCK_ITEMS_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <filesystem>

// ============================================================================
// VPK v2 Reader (minimal: extract a single file by path)
// ============================================================================

struct GBE_VpkDirectoryEntry {
    uint32_t crc;
    uint16_t preload_bytes;
    uint16_t archive_index;
    uint32_t entry_offset;
    uint32_t entry_length;
};

static bool GBE_VpkExtractFile(const std::string &vpk_dir_path, const std::string &target_extension,
                               const std::string &target_path, const std::string &target_filename,
                               std::string &out_data)
{
    std::ifstream f(vpk_dir_path, std::ios::binary);
    if (!f.is_open()) return false;

    // Read header
    uint32_t signature, version, tree_size;
    f.read(reinterpret_cast<char *>(&signature), 4);
    f.read(reinterpret_cast<char *>(&version), 4);
    f.read(reinterpret_cast<char *>(&tree_size), 4);

    if (signature != 0x55AA1234) return false;
    if (version != 2 && version != 1) return false;

    // v2 has extra header fields
    uint32_t file_data_section_size = 0;
    uint32_t archive_md5_section_size = 0;
    uint32_t other_md5_section_size = 0;
    uint32_t signature_section_size = 0;
    if (version == 2) {
        f.read(reinterpret_cast<char *>(&file_data_section_size), 4);
        f.read(reinterpret_cast<char *>(&archive_md5_section_size), 4);
        f.read(reinterpret_cast<char *>(&other_md5_section_size), 4);
        f.read(reinterpret_cast<char *>(&signature_section_size), 4);
    }

    const std::streampos tree_start = f.tellg();
    const std::streampos tree_end = tree_start + static_cast<std::streamoff>(tree_size);

    // Parse tree: extensions -> paths -> filenames
    auto read_null_string = [&](std::string &s) -> bool {
        s.clear();
        char c;
        while (f.get(c)) {
            if (c == '\0') return true;
            s += c;
        }
        return false;
    };

    std::string ext, path, filename;
    while (true) {
        if (!read_null_string(ext)) return false;
        if (ext.empty()) break; // end of tree

        while (true) {
            if (!read_null_string(path)) return false;
            if (path.empty()) break;

            while (true) {
                if (!read_null_string(filename)) return false;
                if (filename.empty()) break;

                // Read directory entry
                GBE_VpkDirectoryEntry entry{};
                f.read(reinterpret_cast<char *>(&entry.crc), 4);
                f.read(reinterpret_cast<char *>(&entry.preload_bytes), 2);
                f.read(reinterpret_cast<char *>(&entry.archive_index), 2);
                f.read(reinterpret_cast<char *>(&entry.entry_offset), 4);
                f.read(reinterpret_cast<char *>(&entry.entry_length), 4);

                uint16_t terminator;
                f.read(reinterpret_cast<char *>(&terminator), 2);

                // Read preload data
                std::string preload_data;
                if (entry.preload_bytes > 0) {
                    preload_data.resize(entry.preload_bytes);
                    f.read(preload_data.data(), entry.preload_bytes);
                }

                // Check if this is our target file
                if (ext == target_extension && path == target_path && filename == target_filename) {
                    out_data = preload_data;

                    if (entry.entry_length > 0) {
                        std::string archive_data;
                        archive_data.resize(entry.entry_length);

                        if (entry.archive_index == 0x7FFF) {
                            // Data is in the _dir.vpk itself, after the tree
                            std::streampos data_pos = tree_end + static_cast<std::streamoff>(entry.entry_offset);
                            auto cur_pos = f.tellg();
                            f.seekg(data_pos);
                            f.read(archive_data.data(), entry.entry_length);
                            f.seekg(cur_pos);
                        } else {
                            // Data is in a numbered archive file
                            std::string base = vpk_dir_path;
                            // Replace _dir.vpk with _XXX.vpk
                            size_t dir_pos = base.rfind("_dir.vpk");
                            if (dir_pos == std::string::npos) return false;
                            char archive_suffix[16];
                            snprintf(archive_suffix, sizeof(archive_suffix), "_%03u.vpk", entry.archive_index);
                            std::string archive_path = base.substr(0, dir_pos) + archive_suffix;

                            std::ifstream af(archive_path, std::ios::binary);
                            if (!af.is_open()) return false;
                            af.seekg(entry.entry_offset);
                            af.read(archive_data.data(), entry.entry_length);
                            if (!af.good()) return false;
                        }

                        out_data += archive_data;
                    }

                    return true;
                }
            }
        }
    }

    return false;
}

// ============================================================================
// VDF Parser (minimal: parse Valve KeyValues text format)
// ============================================================================

struct GBE_VdfNode {
    std::string key;
    std::string value; // leaf value (empty if has children)
    std::vector<GBE_VdfNode> children;

    const GBE_VdfNode *find(const std::string &k) const {
        for (const auto &c : children) {
            if (c.key == k) return &c;
        }
        return nullptr;
    }

    std::string get_string(const std::string &k, const std::string &def = "") const {
        const GBE_VdfNode *n = find(k);
        if (n && n->children.empty()) return n->value;
        return def;
    }
};

class GBE_VdfParser {
public:
    static bool Parse(const std::string &text, GBE_VdfNode &root) {
        size_t pos = 0;
        root.key = "root";
        root.children.clear();
        while (pos < text.size()) {
            skip_whitespace_and_comments(text, pos);
            if (pos >= text.size()) break;
            GBE_VdfNode child;
            if (!parse_node(text, pos, child)) return false;
            root.children.push_back(std::move(child));
        }
        return true;
    }

private:
    static void skip_whitespace_and_comments(const std::string &text, size_t &pos) {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                pos++;
            } else if (c == '/' && pos + 1 < text.size() && text[pos + 1] == '/') {
                // Line comment
                pos += 2;
                while (pos < text.size() && text[pos] != '\n') pos++;
            } else {
                break;
            }
        }
    }

    static bool parse_quoted_string(const std::string &text, size_t &pos, std::string &out) {
        if (pos >= text.size() || text[pos] != '"') return false;
        pos++; // skip opening quote
        out.clear();
        while (pos < text.size()) {
            char c = text[pos];
            if (c == '"') {
                pos++;
                return true;
            }
            if (c == '\\' && pos + 1 < text.size()) {
                pos++;
                c = text[pos];
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case '\\': out += '\\'; break;
                    case '"': out += '"'; break;
                    default: out += '\\'; out += c; break;
                }
            } else {
                out += c;
            }
            pos++;
        }
        return false; // unterminated
    }

    static bool parse_unquoted_string(const std::string &text, size_t &pos, std::string &out) {
        out.clear();
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' || c == '}' || c == '"')
                break;
            out += c;
            pos++;
        }
        return !out.empty();
    }

    static bool parse_string(const std::string &text, size_t &pos, std::string &out) {
        skip_whitespace_and_comments(text, pos);
        if (pos >= text.size()) return false;
        if (text[pos] == '"')
            return parse_quoted_string(text, pos, out);
        return parse_unquoted_string(text, pos, out);
    }

    static bool parse_node(const std::string &text, size_t &pos, GBE_VdfNode &node) {
        if (!parse_string(text, pos, node.key)) return false;
        skip_whitespace_and_comments(text, pos);
        if (pos >= text.size()) return false;

        if (text[pos] == '{') {
            // Subsection
            pos++; // skip {
            while (true) {
                skip_whitespace_and_comments(text, pos);
                if (pos >= text.size()) return false;
                if (text[pos] == '}') {
                    pos++;
                    return true;
                }
                GBE_VdfNode child;
                if (!parse_node(text, pos, child)) return false;
                node.children.push_back(std::move(child));
            }
        } else {
            // Key-value pair
            if (!parse_string(text, pos, node.value)) return false;
            return true;
        }
    }
};

// ============================================================================
// Dota 2 Item Extraction
// ============================================================================

struct GBE_DotaItemDef {
    uint32_t def_index;
    uint8_t num_styles; // total style count; 0 means 1 default style
    // Bitmask of styles that have "additional_hidden" and need unlock attributes.
    // Bit N set = style N requires unlock. Style 0 is never locked.
    uint32_t locked_styles_mask;
    // True if this item has prefab containing "sticker" - needs quality variants injected
    bool is_sticker;
};

// Resolved style unlock attribute def_index values.
// In items_game.txt attributes section, these are named "unlock_style_#".
// We search for them dynamically; fallback to hardcoded range if not found.
struct GBE_DotaStyleUnlockInfo {
    // attr_def_index[N] = the attribute def_index that unlocks style N (1-based)
    // Index 0 is unused (style 0 never needs unlock)
    uint32_t attr_def_index[32];
    bool found;
};

static GBE_DotaStyleUnlockInfo GBE_FindStyleUnlockAttributes(const GBE_VdfNode &root)
{
    GBE_DotaStyleUnlockInfo info{};
    info.found = false;

    const GBE_VdfNode *items_game = root.find("items_game");
    if (!items_game) {
        for (const auto &c : root.children) {
            if (c.key == "items_game") { items_game = &c; break; }
        }
    }
    if (!items_game) return info;

    const GBE_VdfNode *attributes = items_game->find("attributes");
    if (!attributes) return info;

    // Search for attributes named "unlock_style_#" or "style_#_unlock_date"
    for (const auto &attr_node : attributes->children) {
        std::string attr_name = attr_node.get_string("name");
        uint32_t attr_id = 0;
        try {
            attr_id = static_cast<uint32_t>(std::stoul(attr_node.key));
        } catch (...) {
            continue;
        }
        if (attr_id == 0) continue;

        // Pattern: "unlock_style_1", "unlock_style_2", etc.
        if (attr_name.rfind("unlock_style_", 0) == 0) {
            std::string num_str = attr_name.substr(13); // after "unlock_style_"
            try {
                uint32_t style_idx = static_cast<uint32_t>(std::stoul(num_str));
                if (style_idx > 0 && style_idx < 32) {
                    info.attr_def_index[style_idx] = attr_id;
                    info.found = true;
                }
            } catch (...) {}
        }
        // Pattern: "style_1_unlock_date", "style_2_unlock_date", etc.
        else if (attr_name.rfind("style_", 0) == 0 && attr_name.find("_unlock") != std::string::npos) {
            // Extract style number
            size_t num_start = 6; // after "style_"
            size_t num_end = attr_name.find('_', num_start);
            if (num_end != std::string::npos) {
                std::string num_str = attr_name.substr(num_start, num_end - num_start);
                try {
                    uint32_t style_idx = static_cast<uint32_t>(std::stoul(num_str));
                    if (style_idx > 0 && style_idx < 32) {
                        info.attr_def_index[style_idx] = attr_id;
                        info.found = true;
                    }
                } catch (...) {}
            }
        }
    }

    return info;
}

// Prefabs that represent equippable cosmetic items
static bool GBE_IsCosmeticPrefab(const std::string &prefab) {
    // Whitelist approach: known cosmetic/equippable prefabs
    static const std::unordered_set<std::string> cosmetic_prefabs = {
        "wearable", "courier", "ward", "loading_screen", "taunt",
        "terrain", "hero_effigy_block", "announcer", "announcer_pack",
        "music", "pennant", "cursor_pack",
        "weather", "emoticon", "spray", "emblem",
        "hero_statue",
        // World items: creeps, towers, HUD skins, kill effects, etc.
        "creep", "creep_skin",
        "tower", "tower_skin",
        "radiantcreeps", "direcreeps",
        "radianttowers", "diretowers",
        "radiantsiegecreeps", "diresiegecreeps",
        "hud_skin",
        "kill_effect", "kill_streak_effect",
        "blink_effect", "teleport_effect", "tp_effect",
        "versus_screen",
        "map_effect",
        "courier_effect",
        // Roshan, Tormentor, Ancient
        "roshan", "tormentor", "ancient",
        // Interface items: stickers, emoticons, gems, bundles, tools
        "sticker", "sticker_capsule", "socket_gem",
        "emoticon_tool", "bundle", "tool",
        // Treasury items: treasure chests, keys
        "treasure_chest", "retired_treasure_chest", "key",
        // Misc items with player_loadout (streak effects, etc.)
        "streak_effect", "dynamic_recipe",
        "courier_wearable", "showcase_decoration",
        "default_item",
        "summons", "head_effect", "death_effect",
        // Misc items: charms, collector pins, relics, etc.
        "misc",
    };

    // Blacklist: known non-cosmetic prefabs that should never be injected
    static const std::unordered_set<std::string> excluded_prefabs = {
        "recipe",
        "tournament", "player_card", "retired_item",
        "ticket", "league", "passport", "gem",
        "supply_crate",
    };

    // Check if any token in the prefab string matches
    size_t start = 0;
    bool has_excluded = false;
    bool has_cosmetic = false;
    while (start < prefab.size()) {
        size_t end = prefab.find(' ', start);
        if (end == std::string::npos) end = prefab.size();
        std::string token = prefab.substr(start, end - start);
        if (excluded_prefabs.count(token)) has_excluded = true;
        if (cosmetic_prefabs.count(token)) has_cosmetic = true;
        start = end + 1;
    }
    // Excluded prefabs take priority over cosmetic ones
    if (has_excluded) return false;
    return has_cosmetic;
}

// Diagnostics from style parsing
struct GBE_DotaStyleDiag {
    uint32_t multi_style_items;
    uint32_t items_with_locked_styles;
    std::unordered_map<std::string, uint32_t> style_field_counts;
    // Samples: first few multi-style items
    struct Sample {
        uint32_t def_index;
        uint8_t num_styles;
        std::string style1_fields; // concatenated fields from style[1]
    };
    std::vector<Sample> samples;
};

static std::vector<GBE_DotaItemDef> GBE_ExtractDotaItemDefs(const GBE_VdfNode &root, GBE_DotaStyleDiag *diag = nullptr)
{
    std::vector<GBE_DotaItemDef> result;

    // Navigate to items_game -> items
    const GBE_VdfNode *items_game = root.find("items_game");
    if (!items_game) {
        // Try direct children
        for (const auto &c : root.children) {
            if (c.key == "items_game") { items_game = &c; break; }
        }
    }
    if (!items_game) return result;

    const GBE_VdfNode *items = items_game->find("items");
    if (!items) return result;

    // Also load prefabs for inheritance
    const GBE_VdfNode *prefabs_node = items_game->find("prefabs");
    std::unordered_map<std::string, std::string> prefab_inherits;
    if (prefabs_node) {
        for (const auto &p : prefabs_node->children) {
            std::string base = p.get_string("prefab");
            prefab_inherits[p.key] = base;
        }
    }

    auto resolve_prefab = [&](const std::string &prefab) -> bool {
        if (GBE_IsCosmeticPrefab(prefab)) return true;
        // Check inheritance chain (up to 3 levels)
        std::string current = prefab;
        for (int depth = 0; depth < 3; depth++) {
            auto it = prefab_inherits.find(current);
            if (it == prefab_inherits.end()) break;
            if (GBE_IsCosmeticPrefab(it->second)) return true;
            current = it->second;
        }
        return false;
    };

    // Diagnostics: track style field names for debugging
    uint32_t multi_style_items = 0;
    uint32_t items_with_locked_styles = 0;
    std::unordered_map<std::string, uint32_t> style_field_counts;

    for (const auto &item_node : items->children) {
        // Skip non-numeric keys (like "default")
        uint32_t def_index = 0;
        try {
            def_index = static_cast<uint32_t>(std::stoul(item_node.key));
        } catch (...) {
            continue;
        }

        if (def_index == 0) continue; // skip item 0 (invalid)

        std::string prefab = item_node.get_string("prefab");
        if (prefab.empty()) continue;

        // Items with player_loadout=1 are equippable cosmetics (e.g. weather effects
        // with prefab=misc), but still respect the excluded prefabs blacklist.
        bool has_player_loadout = (item_node.get_string("player_loadout") == "1");
        bool is_excluded = GBE_IsCosmeticPrefab(prefab) == false && 
                           prefab.find("league") != std::string::npos; // crude check
        
        // Use resolve_prefab for normal path; player_loadout bypasses prefab whitelist
        // but excluded prefabs (tool, bundle, league, etc.) still block injection.
        if (!resolve_prefab(prefab) && !has_player_loadout) continue;
        if (has_player_loadout) {
            // Still check blacklist
            bool blacklisted = false;
            static const std::unordered_set<std::string> bl = {
                "recipe",
                "tournament", "player_card", "retired_item",
                "ticket", "league", "passport", "gem",
                "supply_crate",
            };
            size_t s = 0;
            while (s < prefab.size()) {
                size_t e = prefab.find(' ', s);
                if (e == std::string::npos) e = prefab.size();
                if (bl.count(prefab.substr(s, e - s))) { blacklisted = true; break; }
                s = e + 1;
            }
            if (blacklisted) continue;
        }

        // Count styles and detect locked ones
        uint8_t num_styles = 0;
        uint32_t locked_styles_mask = 0;
        const GBE_VdfNode *styles_node = item_node.find("styles");
        if (styles_node) {
            num_styles = static_cast<uint8_t>(styles_node->children.size());

            if (num_styles > 1) {
                multi_style_items++;

                for (uint8_t si = 0; si < num_styles && si < 32; si++) {
                    const auto &style_node = styles_node->children[si];

                    // A style is locked if it has an "unlock" child node (sub-tree)
                    const GBE_VdfNode *unlock_node = style_node.find("unlock");
                    if (unlock_node && !unlock_node->children.empty()) {
                        locked_styles_mask |= (1u << si);
                    }

                    // Collect field names for diagnostics
                    for (const auto &child : style_node.children) {
                        style_field_counts[child.key]++;
                    }
                }

                // Collect samples
                if (diag && diag->samples.size() < 5) {
                    GBE_DotaStyleDiag::Sample sample;
                    sample.def_index = def_index;
                    sample.num_styles = num_styles;
                    if (styles_node->children.size() > 1) {
                        for (const auto &style_child : styles_node->children[1].children) {
                            sample.style1_fields += style_child.key + "=" + style_child.value + " ";
                        }
                    }
                    diag->samples.push_back(sample);
                }
            }
        }

        if (locked_styles_mask != 0) items_with_locked_styles++;

        // Detect sticker items by prefab - they need quality variant injection
        bool is_sticker = (prefab.find("sticker") != std::string::npos &&
                           prefab.find("sticker_capsule") == std::string::npos);

        result.push_back({ def_index, num_styles, locked_styles_mask, is_sticker });
    }

    // Populate diagnostics
    if (diag) {
        diag->multi_style_items = multi_style_items;
        diag->items_with_locked_styles = items_with_locked_styles;
        diag->style_field_counts = style_field_counts;
    }

    return result;
}

// ============================================================================
// VPK Path Discovery
// ============================================================================

static std::string GBE_FindDotaVpkPath()
{
    // Strategy: GBE DLL runs inside dota2.exe process.
    // Working directory is typically: <dota 2 beta>/game/bin/win64/
    // VPK is at: <dota 2 beta>/game/dota/pak01_dir.vpk

    // Try relative from CWD
    std::vector<std::string> candidates = {
        "../../dota/pak01_dir.vpk",         // from game/bin/win64/
        "../dota/pak01_dir.vpk",            // from game/bin/
        "dota/pak01_dir.vpk",               // from game/
        "../game/dota/pak01_dir.vpk",       // from dota 2 beta/
        "pak01_dir.vpk",                    // from game/dota/
    };

    for (const auto &c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec)) {
            return std::filesystem::absolute(c, ec).string();
        }
    }

    // Fallback: try from program path
    // get_full_program_path() is available globally
    extern std::string get_full_program_path();
    std::string prog_path = get_full_program_path();
    // prog_path ends with separator, points to DLL directory
    std::vector<std::string> prog_candidates = {
        prog_path + "../../dota/pak01_dir.vpk",
        prog_path + "../../../dota/pak01_dir.vpk",
        prog_path + "../../../../game/dota/pak01_dir.vpk",
    };

    for (const auto &c : prog_candidates) {
        std::error_code ec;
        auto p = std::filesystem::absolute(c, ec);
        if (!ec && std::filesystem::exists(p, ec)) {
            return p.string();
        }
    }

    return "";
}

// ============================================================================
// Loot List / Treasure / Bundle Data
// ============================================================================

struct GBE_DotaLootListData {
    // name -> def_index mapping (from items section, key=item "name" field)
    std::unordered_map<std::string, uint32_t> name_to_def;
    // loot_lists: loot_list_name -> list of item/set names (normal drops only, no escalating)
    std::unordered_map<std::string, std::vector<std::string>> loot_lists;
    // treasure def_index -> loot_list_name (from "treasure loot list" static attribute)
    std::unordered_map<uint32_t, std::string> treasure_to_loot_list;
    // bundle def_index -> list of contained item names (from "bundle" sub-node)
    std::unordered_map<uint32_t, std::vector<std::string>> bundle_contents;
};

static GBE_DotaLootListData GBE_ExtractLootListData(const GBE_VdfNode &root)
{
    GBE_DotaLootListData data;

    const GBE_VdfNode *items_game = root.find("items_game");
    if (!items_game) {
        for (const auto &c : root.children) {
            if (c.key == "items_game") { items_game = &c; break; }
        }
    }
    if (!items_game) return data;

    // 1. Build name -> def_index map from items section
    const GBE_VdfNode *items_node = items_game->find("items");
    if (items_node) {
        for (const auto &item : items_node->children) {
            uint32_t def_index = 0;
            try { def_index = static_cast<uint32_t>(std::stoul(item.key)); } catch (...) { continue; }
            if (def_index == 0) continue;

            std::string name = item.get_string("name");
            if (!name.empty()) {
                data.name_to_def[name] = def_index;
            }

            // Check for treasure loot list attribute
            const GBE_VdfNode *static_attrs = item.find("static_attributes");
            if (static_attrs) {
                std::string loot_list_name = static_attrs->get_string("treasure loot list");
                if (!loot_list_name.empty()) {
                    data.treasure_to_loot_list[def_index] = loot_list_name;
                }
            }

            // Check for bundle contents
            const GBE_VdfNode *bundle_node = item.find("bundle");
            if (bundle_node) {
                std::vector<std::string> contents;
                for (const auto &entry : bundle_node->children) {
                    if (!entry.key.empty()) {
                        contents.push_back(entry.key);
                    }
                }
                if (!contents.empty()) {
                    data.bundle_contents[def_index] = std::move(contents);
                }
            }
        }
    }

    // 2. Parse loot_lists section
    const GBE_VdfNode *loot_lists_node = items_game->find("loot_lists");
    if (loot_lists_node) {
        for (const auto &ll : loot_lists_node->children) {
            std::vector<std::string> entries;
            for (const auto &entry : ll.children) {
                // Skip special sub-nodes like "additional_drop", "escalating_chance_drop_by_rarity"
                if (entry.children.empty() && !entry.key.empty()) {
                    // Normal drop entry: key is item/set name, value is weight
                    entries.push_back(entry.key);
                }
            }
            if (!entries.empty()) {
                data.loot_lists[ll.key] = std::move(entries);
            }
        }
    }

    return data;
}

// ============================================================================
// Main Entry Point: Load All Dota Items from VPK
// ============================================================================

struct GBE_DotaVpkData {
    std::vector<GBE_DotaItemDef> item_defs;
    GBE_DotaStyleUnlockInfo style_unlock;
    GBE_DotaStyleDiag style_diag;
    GBE_DotaLootListData loot_data;
};

static GBE_DotaVpkData GBE_LoadAllDotaItemsFromVpk()
{
    std::string vpk_path = GBE_FindDotaVpkPath();
    if (vpk_path.empty()) {
        // Try environment variable as last resort
        const char *env = std::getenv("GBE_DOTA_VPK_PATH");
        if (env && env[0]) vpk_path = env;
    }

    if (vpk_path.empty()) return {};

    // Extract items_game.txt from VPK
    // In VPK tree: extension="txt", path="scripts/items", filename="items_game"
    std::string items_game_text;
    if (!GBE_VpkExtractFile(vpk_path, "txt", "scripts/items", "items_game", items_game_text)) {
        // Try alternate path structure
        if (!GBE_VpkExtractFile(vpk_path, "txt", "scripts/npc", "items_game", items_game_text)) {
            return {};
        }
    }

    if (items_game_text.empty()) return {};

    // Parse VDF
    GBE_VdfNode root;
    if (!GBE_VdfParser::Parse(items_game_text, root)) return {};

    GBE_DotaVpkData data;
    data.item_defs = GBE_ExtractDotaItemDefs(root, &data.style_diag);
    data.style_unlock = GBE_FindStyleUnlockAttributes(root);
    data.loot_data = GBE_ExtractLootListData(root);
    return data;
}

#endif // GBE_DOTA_UNLOCK_ITEMS_H

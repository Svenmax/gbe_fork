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
    uint8_t num_styles; // 0 means 1 default style
};

// Prefabs that represent equippable cosmetic items
static bool GBE_IsCosmeticPrefab(const std::string &prefab) {
    // Some items have compound prefabs like "wearable default_item"
    static const std::unordered_set<std::string> cosmetic_prefabs = {
        "wearable", "courier", "ward", "loading_screen", "taunt",
        "terrain", "hero_effigy_block", "announcer", "announcer_pack",
        "music", "pennant", "blink_dagger", "cursor_pack",
        "weather", "emoticon", "spray", "emblem",
        "hero_statue", "misc", "tool", "default_item"
    };

    // Check if any token in the prefab string matches
    size_t start = 0;
    while (start < prefab.size()) {
        size_t end = prefab.find(' ', start);
        if (end == std::string::npos) end = prefab.size();
        std::string token = prefab.substr(start, end - start);
        if (cosmetic_prefabs.count(token)) return true;
        start = end + 1;
    }
    return false;
}

static std::vector<GBE_DotaItemDef> GBE_ExtractDotaItemDefs(const GBE_VdfNode &root)
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
        // Check inheritance chain (1 level)
        auto it = prefab_inherits.find(prefab);
        if (it != prefab_inherits.end() && GBE_IsCosmeticPrefab(it->second))
            return true;
        return false;
    };

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

        if (!resolve_prefab(prefab)) continue;

        // Count styles
        uint8_t num_styles = 0;
        const GBE_VdfNode *styles_node = item_node.find("styles");
        if (styles_node) {
            num_styles = static_cast<uint8_t>(styles_node->children.size());
        }

        result.push_back({ def_index, num_styles });
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
// Main Entry Point: Load All Dota Items from VPK
// ============================================================================

static std::vector<GBE_DotaItemDef> GBE_LoadAllDotaItemsFromVpk()
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

    // Extract item definitions
    return GBE_ExtractDotaItemDefs(root);
}

#endif // GBE_DOTA_UNLOCK_ITEMS_H

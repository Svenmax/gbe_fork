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

#include "dll/steam_ugc.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game.h"

#include <cctype>
#include <fstream>

static std::string GBE_DotaWorkshopParentPath(const std::string &path)
{
    if (path.empty()) return {};

    std::filesystem::path fs_path = std::filesystem::u8path(path);
    std::filesystem::path parent = fs_path.parent_path();
    if (parent.empty() || parent == fs_path) return {};
    return canonical_path(parent.u8string());
}

static void GBE_DotaAppendUniqueSeedPath(std::vector<std::string> &seed_paths, std::set<std::string> &seen_paths, const std::string &path)
{
    const std::string normalized = canonical_path(path);
    if (normalized.empty()) return;
    if (seen_paths.insert(normalized).second) seed_paths.push_back(normalized);
}

static bool GBE_DotaParseWorkshopId(const std::string &folder_name, PublishedFileId_t &workshop_id)
{
    if (folder_name.empty()) return false;

    try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(folder_name, &consumed, 10);
        if (consumed != folder_name.size() || parsed == 0ull) return false;
        workshop_id = static_cast<PublishedFileId_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

static PublishedFileId_t GBE_DotaLocalAddonPublishedFileId(const std::string &addon_path)
{
    constexpr uint64 base_id = 570000000ull;
    constexpr uint64 hash_range = 3000000000ull;
    uint64 hash = 14695981039346656037ull;

    for (unsigned char ch : canonical_path(addon_path)) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }

    PublishedFileId_t id = static_cast<PublishedFileId_t>(base_id + (hash % hash_range));
    return id == k_PublishedFileIdInvalid ? base_id + 1ull : id;
}

static bool GBE_DotaIsLocalAddonFolder(const std::string &addon_path)
{
    if (addon_path.empty()) return false;

    const std::filesystem::path root = std::filesystem::u8path(addon_path);
    return common_helpers::file_exist(root / "addoninfo.txt")
        || common_helpers::file_exist(root / "addoninfo.gi")
        || common_helpers::dir_exist(root / "maps")
        || common_helpers::dir_exist(root / "scripts");
}

static std::string GBE_DotaLocalAddonMapName(const std::string &addon_path, const std::string &fallback)
{
    const std::filesystem::path maps_path = std::filesystem::u8path(addon_path) / "maps";
    try {
        if (common_helpers::dir_exist(maps_path)) {
            for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(maps_path, std::filesystem::directory_options::follow_directory_symlink)) {
                if (!std::filesystem::is_regular_file(dir_entry))
                    continue;

                const std::filesystem::path map_path = dir_entry.path();
                const std::string extension = common_helpers::to_lower(map_path.extension().u8string());
                if (extension == ".vmap" || extension == ".vmap_c" || extension == ".bsp" || extension == ".vpk")
                    return map_path.stem().u8string();
            }
        }
    } catch (...) { }

    return fallback;
}

static bool GBE_DotaIsMapResourcePath(const std::filesystem::path &path)
{
    const std::string extension = common_helpers::to_lower(path.extension().u8string());
    if (extension != ".vmap" && extension != ".vmap_c" && extension != ".bsp") return false;

    const std::string generic_path = common_helpers::to_lower(path.generic_u8string());
    return generic_path.find("maps/") != std::string::npos || generic_path.find("maps\\") != std::string::npos;
}

static std::string GBE_DotaReadVpkCString(std::ifstream &input)
{
    std::string value;
    char ch = 0;
    while (input.read(&ch, 1) && ch != '\0') value.push_back(ch);
    return value;
}

static bool GBE_DotaReadVpkUint16(std::ifstream &input, uint16 &value)
{
    unsigned char bytes[2]{};
    if (!input.read(reinterpret_cast<char *>(bytes), sizeof(bytes))) return false;
    value = static_cast<uint16>(bytes[0] | (bytes[1] << 8));
    return true;
}

static bool GBE_DotaReadVpkUint32(std::ifstream &input, uint32 &value)
{
    unsigned char bytes[4]{};
    if (!input.read(reinterpret_cast<char *>(bytes), sizeof(bytes))) return false;
    value = static_cast<uint32>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
    return true;
}

static std::string GBE_DotaMapNameFromVpk(const std::filesystem::path &vpk_path)
{
    std::ifstream input(vpk_path, std::ios::binary);
    if (!input.is_open()) return {};

    uint32 signature = 0;
    uint32 version = 0;
    uint32 tree_size = 0;
    if (!GBE_DotaReadVpkUint32(input, signature) || !GBE_DotaReadVpkUint32(input, version) || !GBE_DotaReadVpkUint32(input, tree_size)) return {};
    if (signature != 0x55aa1234u || tree_size == 0u) return {};
    if (version >= 2u) {
        uint32 ignored = 0;
        for (int i = 0; i < 4; ++i) {
            if (!GBE_DotaReadVpkUint32(input, ignored)) return {};
        }
    }

    while (input.good()) {
        const std::string extension = GBE_DotaReadVpkCString(input);
        if (extension.empty()) break;

        while (input.good()) {
            const std::string path = GBE_DotaReadVpkCString(input);
            if (path.empty()) break;

            while (input.good()) {
                const std::string filename = GBE_DotaReadVpkCString(input);
                if (filename.empty()) break;

                uint32 crc = 0;
                uint16 preload_bytes = 0;
                uint16 archive_index = 0;
                uint32 offset = 0;
                uint32 length = 0;
                uint16 terminator = 0;
                if (!GBE_DotaReadVpkUint32(input, crc) || !GBE_DotaReadVpkUint16(input, preload_bytes) || !GBE_DotaReadVpkUint16(input, archive_index) || !GBE_DotaReadVpkUint32(input, offset) || !GBE_DotaReadVpkUint32(input, length) || !GBE_DotaReadVpkUint16(input, terminator)) return {};

                if (preload_bytes > 0) input.seekg(preload_bytes, std::ios::cur);

                const std::string full_path = path + "/" + filename + "." + extension;
                const std::filesystem::path resource_path = std::filesystem::u8path(full_path);
                if (GBE_DotaIsMapResourcePath(resource_path)) return resource_path.stem().u8string();
            }
        }
    }

    return {};
}

static std::string GBE_DotaWorkshopModMapName(const std::string &mod_path, const std::string &fallback)
{
    try {
        const std::filesystem::path root = std::filesystem::u8path(mod_path);
        if (common_helpers::dir_exist(root)) {
            for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink)) {
                if (!std::filesystem::is_regular_file(dir_entry)) continue;
                if (GBE_DotaIsMapResourcePath(dir_entry.path())) return dir_entry.path().stem().u8string();
            }
            for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink)) {
                if (!std::filesystem::is_regular_file(dir_entry)) continue;
                const std::string extension = common_helpers::to_lower(dir_entry.path().extension().u8string());
                if (extension != ".vpk") continue;
                std::string map_name = GBE_DotaMapNameFromVpk(dir_entry.path());
                if (!map_name.empty()) return map_name;
            }
        }
    } catch (...) { }

    return fallback;
}

static bool GBE_DotaIsNumericString(const std::string &value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

static bool GBE_DotaIsReadableAddonName(const std::string &value)
{
    return !value.empty()
        && !GBE_DotaIsNumericString(value)
        && value != "dota"
        && value != "publish_data"
        && value != "addoninfo"
        && value != "preview"
        && value != "thumbnail";
}

static std::string GBE_DotaWorkshopFallbackDisplayName(const std::string &workshop_id)
{
    if (GBE_DotaIsNumericString(workshop_id))
        return "Workshop " + workshop_id;
    return workshop_id;
}

static std::string GBE_DotaExtractAddonInfoValue(const std::string &line, const std::string &key);

static void GBE_DotaAddUniquePath(std::vector<std::filesystem::path> &paths, std::set<std::string> &seen, const std::filesystem::path &path)
{
    const std::string normalized = common_helpers::to_lower(path.u8string());
    if (!normalized.empty() && seen.insert(normalized).second)
        paths.push_back(path);
}

static std::string GBE_DotaReadableTitleNearWorkshopId(const std::filesystem::path &path, const std::string &workshop_id)
{
    try {
        if (!common_helpers::file_exist(path)) return {};
        const auto file_size = std::filesystem::file_size(path);
        if (file_size == 0 || file_size > 16u * 1024u * 1024u) return {};

        std::ifstream input(path);
        if (!input.is_open()) return {};

        std::vector<std::string> recent_lines;
        recent_lines.reserve(80);
        bool near_item = false;
        size_t remaining = 0;
        for (std::string line; std::getline(input, line); ) {
            const std::string stripped = common_helpers::string_strip(line);
            if (stripped.find(workshop_id) != std::string::npos) {
                for (const std::string &recent_line : recent_lines) {
                    for (const char *key : { "title", "name", "display_name" }) {
                        const std::string value = GBE_DotaExtractAddonInfoValue(recent_line, key);
                        if (GBE_DotaIsReadableAddonName(value)) return value;
                    }
                }
                near_item = true;
                remaining = 160;
            }

            if (!near_item) {
                recent_lines.push_back(stripped);
                if (recent_lines.size() > 80) recent_lines.erase(recent_lines.begin());
                continue;
            }

            for (const char *key : { "title", "name", "display_name" }) {
                const std::string value = GBE_DotaExtractAddonInfoValue(stripped, key);
                if (GBE_DotaIsReadableAddonName(value)) return value;
            }

            if (remaining == 0) {
                near_item = false;
                continue;
            }
            --remaining;
            recent_lines.push_back(stripped);
            if (recent_lines.size() > 80) recent_lines.erase(recent_lines.begin());
        }
    } catch (...) { }

    return {};
}

static std::string GBE_DotaWorkshopCachedTitle(const std::string &mod_path, const std::string &workshop_id)
{
    if (workshop_id.empty()) return {};

    std::vector<std::filesystem::path> candidates;
    std::set<std::string> seen;

    try {
        std::filesystem::path cursor = std::filesystem::u8path(mod_path);
        for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
            GBE_DotaAddUniquePath(candidates, seen, cursor / "appworkshop_570.acf");
            GBE_DotaAddUniquePath(candidates, seen, cursor / "workshop" / "appworkshop_570.acf");
            GBE_DotaAddUniquePath(candidates, seen, cursor / "steamapps" / "workshop" / "appworkshop_570.acf");
            GBE_DotaAddUniquePath(candidates, seen, cursor / "appcache" / "workshop" / "appworkshop_570.acf");
            GBE_DotaAddUniquePath(candidates, seen, cursor / "appcache" / "workshop" / (workshop_id + ".acf"));
            GBE_DotaAddUniquePath(candidates, seen, cursor / "appcache" / "workshop" / (workshop_id + ".json"));
            cursor = cursor.parent_path();
        }

        const std::filesystem::path root = std::filesystem::u8path(mod_path);
        if (common_helpers::dir_exist(root)) {
            for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink)) {
                if (!std::filesystem::is_regular_file(dir_entry)) continue;
                const std::string filename = common_helpers::to_lower(dir_entry.path().filename().u8string());
                if (filename == "publish_data" || filename == "publish_data.txt" || filename == "addoninfo.txt" || filename == "addoninfo.gi")
                    GBE_DotaAddUniquePath(candidates, seen, dir_entry.path());
            }
        }

        for (const auto &candidate : candidates) {
            const std::string title = GBE_DotaReadableTitleNearWorkshopId(candidate, workshop_id);
            if (GBE_DotaIsReadableAddonName(title)) return title;
        }
    } catch (...) { }

    return {};
}

static std::string GBE_DotaWorkshopFileStemName(const std::string &mod_path)
{
    try {
        const std::filesystem::path root = std::filesystem::u8path(mod_path);
        if (!common_helpers::dir_exist(root)) return {};

        for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::follow_directory_symlink)) {
            if (!std::filesystem::is_regular_file(dir_entry)) continue;

            const std::string extension = common_helpers::to_lower(dir_entry.path().extension().u8string());
            if (extension != ".vpk" && extension != ".vmap" && extension != ".vmap_c" && extension != ".bsp") continue;

            const std::string stem = dir_entry.path().stem().u8string();
            if (GBE_DotaIsReadableAddonName(stem)) return stem;
        }
    } catch (...) { }

    return {};
}

static std::string GBE_DotaWorkshopManifestTitle(const std::string &mod_path, const std::string &workshop_id)
{
    if (workshop_id.empty()) return {};

    try {
        std::filesystem::path cursor = std::filesystem::u8path(mod_path);
        for (int depth = 0; depth < 6 && !cursor.empty(); ++depth) {
            const std::filesystem::path manifest_path = cursor / "appworkshop_570.acf";
            if (common_helpers::file_exist(manifest_path)) {
                std::ifstream input(manifest_path);
                bool in_item = false;
                int block_depth = 0;
                for (std::string line; std::getline(input, line); ) {
                    const std::string stripped = common_helpers::string_strip(line);
                    if (stripped.find('"' + workshop_id + '"') != std::string::npos) {
                        in_item = true;
                        block_depth = 0;
                    }
                    if (in_item) {
                        std::string title = GBE_DotaExtractAddonInfoValue(stripped, "title");
                        if (GBE_DotaIsReadableAddonName(title)) return title;
                        block_depth += static_cast<int>(std::count(stripped.begin(), stripped.end(), '{'));
                        block_depth -= static_cast<int>(std::count(stripped.begin(), stripped.end(), '}'));
                        if (block_depth < 0 || (block_depth == 0 && stripped.find('}') != std::string::npos)) in_item = false;
                    }
                }
            }
            cursor = cursor.parent_path();
        }
    } catch (...) { }

    return {};
}

static std::string GBE_DotaExtractAddonInfoValue(const std::string &line, const std::string &key)
{
    const std::string lower_line = common_helpers::to_lower(line);

    std::vector<std::string> quoted_tokens;
    for (size_t pos = 0; pos < line.size(); ) {
        const size_t first_quote = line.find('"', pos);
        if (first_quote == std::string::npos) break;
        const size_t second_quote = line.find('"', first_quote + 1);
        if (second_quote == std::string::npos) break;
        quoted_tokens.push_back(line.substr(first_quote + 1, second_quote - first_quote - 1));
        pos = second_quote + 1;
    }
    if (quoted_tokens.size() >= 2 && common_helpers::to_lower(quoted_tokens[0]) == key)
        return quoted_tokens[1];

    const size_t key_pos = lower_line.find(key);
    if (key_pos == std::string::npos) return {};
    const bool left_boundary = key_pos == 0 || (!std::isalnum(static_cast<unsigned char>(lower_line[key_pos - 1])) && lower_line[key_pos - 1] != '_');
    const size_t key_end = key_pos + key.size();
    const bool right_boundary = key_end >= lower_line.size() || (!std::isalnum(static_cast<unsigned char>(lower_line[key_end])) && lower_line[key_end] != '_');
    if (!left_boundary || !right_boundary) return {};

    if (!quoted_tokens.empty() && key_pos < line.find('"')) {
        return quoted_tokens[0];
    }

    size_t value_pos = line.find('=', key_pos + key.size());
    if (value_pos == std::string::npos)
        value_pos = key_pos + key.size();
    std::string value = common_helpers::string_strip(line.substr(value_pos + 1));
    if (!value.empty() && value.front() == '"') value.erase(value.begin());
    if (!value.empty() && value.back() == '"') value.pop_back();
    return common_helpers::string_strip(value);
}

static std::string GBE_DotaLocalAddonDisplayName(const std::string &addon_path, const std::string &fallback)
{
    static constexpr const char *addoninfo_files[] = { "addoninfo.txt", "addoninfo.gi" };
    static constexpr const char *title_keys[] = { "addontitle", "addon_title", "title", "name" };

    for (const char *addoninfo_file : addoninfo_files) {
        std::ifstream input(std::filesystem::u8path(addon_path) / addoninfo_file);
        if (!input.is_open()) continue;

        for (std::string line; std::getline(input, line); ) {
            for (const char *title_key : title_keys) {
                std::string value = GBE_DotaExtractAddonInfoValue(line, title_key);
                if (!value.empty()) return value;
            }
        }
    }

    return fallback;
}

static std::string GBE_DotaWorkshopDisplayName(const std::string &mod_path, const std::string &fallback)
{
    const std::string cached_title = GBE_DotaWorkshopCachedTitle(mod_path, fallback);
    if (!cached_title.empty()) return cached_title;

    const std::string manifest_title = GBE_DotaWorkshopManifestTitle(mod_path, fallback);
    if (!manifest_title.empty()) return manifest_title;

    std::ifstream input(std::filesystem::u8path(mod_path) / "publish_data");
    if (!input.is_open())
        input.open(std::filesystem::u8path(mod_path) / "publish_data.txt");
    if (!input.is_open()) {
        const std::string file_stem = GBE_DotaWorkshopFileStemName(mod_path);
        return file_stem.empty() ? fallback : file_stem;
    }

    for (std::string line; std::getline(input, line); ) {
        std::string value = GBE_DotaExtractAddonInfoValue(line, "title");
        if (GBE_DotaIsReadableAddonName(value)) return value;
    }

    const std::string file_stem = GBE_DotaWorkshopFileStemName(mod_path);
    return file_stem.empty() ? fallback : file_stem;
}

static std::string GBE_DotaModMetadataValue(const Mod_entry &mod, const char *key, const std::string &fallback)
{
    if (mod.metadata.empty()) return fallback;

    try {
        nlohmann::json metadata = nlohmann::json::parse(mod.metadata);
        return metadata.value(key, fallback);
    } catch (...) {
        return fallback;
    }
}

static std::vector<std::pair<std::string, std::string>> GBE_DotaModKeyValueTags(const Mod_entry &mod)
{
    std::vector<std::pair<std::string, std::string>> tags;
    auto add_tag = [&tags](const std::string &key, const std::string &value) {
        if (!key.empty() && !value.empty()) tags.emplace_back(key, value);
    };

    const std::string addon_name = GBE_DotaModMetadataValue(mod, "addon_name", mod.title);
    const std::string display_name = GBE_DotaModMetadataValue(mod, "display_name", mod.title);
    const std::string map_name = GBE_DotaModMetadataValue(mod, "map_name", addon_name);
    add_tag("addon_name", addon_name);
    add_tag("display_name", display_name);
    add_tag("map_name", map_name);
    add_tag("custom_map_name", map_name);
    add_tag("custom_game_mode", addon_name);
    add_tag("launch_command", "dota_launch_custom_game " + addon_name + " " + map_name);

    if (!mod.metadata.empty()) {
        try {
            nlohmann::json metadata = nlohmann::json::parse(mod.metadata);
            if (metadata.is_object()) {
                for (auto it = metadata.begin(); it != metadata.end(); ++it) {
                    if (it.value().is_string()) add_tag(it.key(), it.value().get<std::string>());
                }
            }
        } catch (...) { }
    }

    std::sort(tags.begin(), tags.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
    tags.erase(std::unique(tags.begin(), tags.end(), [](const auto &left, const auto &right) { return left.first == right.first; }), tags.end());
    return tags;
}

static void GBE_DotaEnsureWorkshopModsForUGC(class Settings *settings, class Ugc_Remote_Storage_Bridge *ugc_bridge)
{
    if (!settings || !ugc_bridge || settings->get_local_game_id().AppID() != 570u) return;

    std::vector<std::string> seed_paths;
    std::set<std::string> seen_seed_paths;

    std::string app_install_path;
    if (settings->getAppInstallPath(570u, app_install_path) && !app_install_path.empty())
        GBE_DotaAppendUniqueSeedPath(seed_paths, seen_seed_paths, app_install_path);

    const std::string steam_path = get_env_variable("SteamPath");
    if (!steam_path.empty())
        GBE_DotaAppendUniqueSeedPath(seed_paths, seen_seed_paths, steam_path);

    GBE_DotaAppendUniqueSeedPath(seed_paths, seen_seed_paths, get_full_program_path());

    std::vector<std::string> candidate_roots;
    std::set<std::string> seen_roots;

    for (const std::string &seed_path : seed_paths) {
        std::string cursor = canonical_path(seed_path);
        for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
            const std::string root = cursor + PATH_SEPARATOR + "steamapps" + PATH_SEPARATOR + "workshop" + PATH_SEPARATOR + "content" + PATH_SEPARATOR + "570";
            if (seen_roots.insert(root).second) candidate_roots.push_back(root);

            const std::string adjacent_root = cursor + PATH_SEPARATOR + "workshop" + PATH_SEPARATOR + "content" + PATH_SEPARATOR + "570";
            if (seen_roots.insert(adjacent_root).second) candidate_roots.push_back(adjacent_root);

            cursor = GBE_DotaWorkshopParentPath(cursor);
        }
    }

    size_t added = 0;
    for (const std::string &candidate_root : candidate_roots) {
        const std::vector<std::string> workshop_folders = Local_Storage::get_folders_path(candidate_root);
        if (workshop_folders.empty()) continue;

        PRINT_DEBUG("[DOTA_UGC] scanning workshop root '%s' folders=%zu", candidate_root.c_str(), workshop_folders.size());
        for (const std::string &workshop_folder : workshop_folders) {
            PublishedFileId_t workshop_id = 0;
            if (!GBE_DotaParseWorkshopId(workshop_folder, workshop_id)) continue;

            const std::string mod_path = candidate_root + PATH_SEPARATOR + workshop_folder;
            const std::string detected_name = GBE_DotaWorkshopDisplayName(mod_path, workshop_folder);
            const std::string display_name = GBE_DotaIsReadableAddonName(detected_name)
                ? detected_name
                : GBE_DotaWorkshopFallbackDisplayName(workshop_folder);
            const std::string map_name = GBE_DotaWorkshopModMapName(mod_path, "");
            if (map_name.empty() || gbe::dota_custom_game::is_guide_only_workshop_mod({}, display_name, detected_name, mod_path)) {
                PRINT_DEBUG("[DOTA_UGC] skipping workshop folder '%s' invalid map='%s' path='%s'", workshop_folder.c_str(), map_name.c_str(), mod_path.c_str());
                continue;
            }
            Mod_entry mod{};
            mod.id = workshop_id;
            mod.title = display_name;
            mod.path = mod_path;
            mod.fileType = k_EWorkshopFileTypeCommunity;
            mod.description = "auto-detected Dota2 workshop mod #" + workshop_folder;
            mod.steamIDOwner = settings->get_local_steam_id().ConvertToUint64();
            mod.timeCreated = 1554997000u;
            mod.timeUpdated = 1555601800u;
            mod.timeAddedToUserList = 1556206600u;
            mod.visibility = k_ERemoteStoragePublishedFileVisibilityPublic;
            mod.acceptedForUse = true;
            mod.workshopItemURL = "https://steamcommunity.com/sharedfiles/filedetails/?id=" + workshop_folder;
            mod.votesUp = 500u;
            mod.votesDown = 12u;
            mod.score = 0.97f;
            if (!map_name.empty()) {
                mod.tags = "Dota,Custom Game,Workshop";
                nlohmann::json metadata = nlohmann::json::object();
                metadata["addon_name"] = workshop_folder;
                metadata["display_name"] = display_name;
                metadata["map_name"] = map_name;
                metadata["launch_command"] = "dota_launch_custom_game " + workshop_folder + " " + map_name;
                mod.metadata = metadata.dump();
                mod.description = mod.metadata;
            }

            const std::vector<std::string> primary_files = Local_Storage::get_filenames_path(mod.path);
            if (!primary_files.empty()) {
                mod.primaryFileName = primary_files[0];
                size_t primary_file_size = 0;
                if (common_helpers::file_size(std::filesystem::u8path(mod.path) / mod.primaryFileName, primary_file_size)) {
                    mod.primaryFileSize = static_cast<int32>(primary_file_size);
                }
                mod.total_files_sizes = mod.primaryFileSize;
            }

            const bool already_installed = settings->isModInstalled(workshop_id);
            settings->addMod(mod.id, mod.title, mod.path);
            settings->addModDetails(mod.id, mod);
            PRINT_DEBUG("[DOTA_UGC] auto-detected workshop mod '%s' title='%s' map='%s' path='%s'", workshop_folder.c_str(), mod.title.c_str(), GBE_DotaModMetadataValue(mod, "map_name", "").c_str(), mod.path.c_str());
            if (!already_installed) {
                ++added;
            }

            ugc_bridge->add_subbed_mod(workshop_id);
        }

        if (added > 0) break;
    }

    PRINT_DEBUG("[DOTA_UGC] workshop ensure complete candidates=%zu added=%zu subscribed=%zu", candidate_roots.size(), added, ugc_bridge->subbed_mods_count());
}

UGCQueryHandle_t Steam_UGC::new_ugc_query(EQueryType query_type, bool return_all_subscribed, uint32 page, bool next_cursor, const std::set<PublishedFileId_t> &return_only)
{
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    ++handle;
    if ((handle == 0) || (handle == k_UGCQueryHandleInvalid)) handle = 50;

    struct UGC_query query{};
    query.handle = handle;
    query.return_all_subscribed = return_all_subscribed;
    query.page = page;
    query.next_cursor = next_cursor;
    query.query_type = query_type;
    query.return_only = return_only;
    ugc_queries.push_back(query);
    PRINT_DEBUG("new request handle = %llu", query.handle);
    return query.handle;
}

std::optional<Mod_entry> Steam_UGC::get_query_ugc(UGCQueryHandle_t handle, uint32 index)
{
    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return std::nullopt;
    if (index >= request->results.size()) return std::nullopt;

    auto it = request->results.begin();
    std::advance(it, index);
    
    PublishedFileId_t file_id = *it;
    if (!settings->isModInstalled(file_id)) return std::nullopt;

    return settings->getMod(file_id);
}

std::vector<std::string> Steam_UGC::get_query_ugc_tags(UGCQueryHandle_t handle, uint32 index)
{
    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return {};

    std::string tmp = res.value().tags;

    auto tags_tokens = std::vector<std::string>{};
    size_t start = 0;
    while (true) {
        auto end = tmp.find(',', start);
        if (end == std::string::npos) break;

        tags_tokens.push_back(tmp.substr(start, end - start));
        start = end + 1;
    }

    tags_tokens.push_back(tmp.substr(start));

    return tags_tokens;

}

static uint64 GBE_UGCStatisticValue(const Mod_entry &mod, EItemStatistic eStatType)
{
    switch (eStatType) {
        case k_EItemStatistic_NumSubscriptions:
        case k_EItemStatistic_NumUniqueSubscriptions:
            return mod.votesUp;

        case k_EItemStatistic_NumFavorites:
        case k_EItemStatistic_NumUniqueFavorites:
            return std::max<uint32>(1u, mod.votesUp / 10u);

        case k_EItemStatistic_NumFollowers:
        case k_EItemStatistic_NumUniqueFollowers:
            return std::max<uint32>(1u, mod.votesUp / 20u);

        case k_EItemStatistic_NumUniqueWebsiteViews:
            return std::max<uint32>(1u, mod.votesUp + mod.votesDown);

        case k_EItemStatistic_ReportScore:
            return static_cast<uint64>(std::max(0.0f, mod.score) * 100000.0f);

        case k_EItemStatistic_NumSecondsPlayed:
        case k_EItemStatistic_NumSecondsPlayedDuringTimePeriod:
            return static_cast<uint64>(std::max<uint32>(1u, mod.votesUp)) * 60ull;

        case k_EItemStatistic_NumPlaytimeSessions:
        case k_EItemStatistic_NumPlaytimeSessionsDuringTimePeriod:
            return std::max<uint32>(1u, mod.votesUp / 5u);

        case k_EItemStatistic_NumComments:
            return std::max<uint32>(1u, mod.votesDown);

        default:
            return 0ull;
    }
}

static std::string GBE_DotaReadableModTitle(const Mod_entry &mod)
{
    if (GBE_DotaIsReadableAddonName(mod.title)) return mod.title;

    const std::string display_name = GBE_DotaModMetadataValue(mod, "display_name", "");
    if (GBE_DotaIsReadableAddonName(display_name)) return display_name;

    const std::string map_name = GBE_DotaModMetadataValue(mod, "map_name", "");
    if (GBE_DotaIsReadableAddonName(map_name)) return map_name;

    const std::string addon_name = GBE_DotaModMetadataValue(mod, "addon_name", "");
    if (GBE_DotaIsReadableAddonName(addon_name)) return addon_name;

    const std::string file_stem = GBE_DotaWorkshopFileStemName(mod.path);
    if (GBE_DotaIsReadableAddonName(file_stem)) return file_stem;

    if (GBE_DotaIsNumericString(addon_name)) return GBE_DotaWorkshopFallbackDisplayName(addon_name);

    if (GBE_DotaIsNumericString(mod.title)) return GBE_DotaWorkshopFallbackDisplayName(mod.title);

    return mod.title;
}

void Steam_UGC::set_details(PublishedFileId_t id, SteamUGCDetails_t *pDetails, IUgcItfVersion ver)
{
    if (pDetails) {
        pDetails->m_nPublishedFileId = id;

        if (settings->isModInstalled(id)) {
            PRINT_DEBUG("  mod is installed, setting details");
            pDetails->m_eResult = k_EResultOK;

            auto mod = settings->getMod(id);
            pDetails->m_bAcceptedForUse = mod.acceptedForUse;
            pDetails->m_bBanned = mod.banned;
            pDetails->m_bTagsTruncated = mod.tagsTruncated;
            pDetails->m_eFileType = mod.fileType;
            pDetails->m_eVisibility = mod.visibility;
            pDetails->m_hFile = mod.handleFile;
            pDetails->m_hPreviewFile = mod.handlePreviewFile;
            pDetails->m_nConsumerAppID = settings->get_local_game_id().AppID();
            pDetails->m_nCreatorAppID = settings->get_local_game_id().AppID();
            pDetails->m_nFileSize = mod.primaryFileSize;
            pDetails->m_nPreviewFileSize = mod.previewFileSize;
            pDetails->m_rtimeCreated = mod.timeCreated;
            pDetails->m_rtimeUpdated = mod.timeUpdated;
            pDetails->m_ulSteamIDOwner = mod.steamIDOwner;

            pDetails->m_rtimeAddedToUserList = mod.timeAddedToUserList;
            pDetails->m_unVotesUp = mod.votesUp;
            pDetails->m_unVotesDown = mod.votesDown;
            pDetails->m_flScore = mod.score;

            // real steamclient64.dll may set this to null! (ex: item id 3366485326)
            auto copied_chars = mod.primaryFileName.copy(pDetails->m_pchFileName, sizeof(pDetails->m_pchFileName) - 1);
            pDetails->m_pchFileName[copied_chars] = 0;

            const std::string details_description = settings->get_local_game_id().AppID() == 570u && !mod.metadata.empty()
                ? mod.metadata
                : mod.description;
            copied_chars = details_description.copy(pDetails->m_rgchDescription, sizeof(pDetails->m_rgchDescription) - 1);
            pDetails->m_rgchDescription[copied_chars] = 0;

            copied_chars = mod.tags.copy(pDetails->m_rgchTags, sizeof(pDetails->m_rgchTags) - 1);
            pDetails->m_rgchTags[copied_chars] = 0;

            const std::string details_title = settings->get_local_game_id().AppID() == 570u
                ? GBE_DotaReadableModTitle(mod)
                : mod.title;
            copied_chars = details_title.copy(pDetails->m_rgchTitle, sizeof(pDetails->m_rgchTitle) - 1);
            pDetails->m_rgchTitle[copied_chars] = 0;

            // real steamclient64.dll may set this to null! (ex: item id 3366485326)
            copied_chars = mod.workshopItemURL.copy(pDetails->m_rgchURL, sizeof(pDetails->m_rgchURL) - 1);
            pDetails->m_rgchURL[copied_chars] = 0;

            // TODO should we enable this?
            // pDetails->m_unNumChildren = mod.numChildren;

            if (ver >= IUgcItfVersion::v020) {
                // TODO make sure the filesize is good
                pDetails->m_ulTotalFilesSize = mod.total_files_sizes;
            }
        } else {
            PRINT_DEBUG("  mod isn't installed, returning failure");
            pDetails->m_eResult = k_EResultFail;
        }
    }
}

void Steam_UGC::read_ugc_favorites()
{
    if (!local_storage->file_exists("", ugc_favorits_file)) return;

    unsigned int size = local_storage->file_size("", ugc_favorits_file);
    if (!size) return;

    std::string data(size, '\0');
    int read = local_storage->get_data("", ugc_favorits_file, &data[0], (unsigned int)data.size());
    if ((size_t)read != data.size()) return;

    std::stringstream ss(data);
    std::string line{};
    while (std::getline(ss, line)) {
        try
        {
            unsigned long long fav_id = std::stoull(line);
            favorites.insert(fav_id);
            PRINT_DEBUG("added item to favorites %llu", fav_id);
        } catch(...) { }
    }
    
}

bool Steam_UGC::write_ugc_favorites()
{
    std::stringstream ss{};
    for (auto id : favorites) {
        ss << id << "\n";
        ss.flush();
    }
    auto file_data = ss.str();
    int stored = local_storage->store_data("", ugc_favorits_file, &file_data[0], static_cast<unsigned int>(file_data.size()));
    return (size_t)stored == file_data.size();
}

bool Steam_UGC::internal_GetQueryUGCResult( UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails, IUgcItfVersion ver )
{
    PRINT_DEBUG("%llu [%u] %p <%u>", handle, index, pDetails, (unsigned)ver);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    // some apps (like appid 588650) ignore the return of this function, especially for builtin mods
    if (pDetails) {
        pDetails->m_nPublishedFileId = k_PublishedFileIdInvalid;
        pDetails->m_eResult = k_EResultFail;
        pDetails->m_bAcceptedForUse = false;
        pDetails->m_hFile = k_UGCHandleInvalid;
        pDetails->m_hPreviewFile = k_UGCHandleInvalid;
        pDetails->m_unNumChildren = 0;
    }

    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) {
        return false;
    }

    if (index >= request->results.size()) {
        return false;
    }

    auto it = request->results.begin();
    std::advance(it, index);
    PublishedFileId_t file_id = *it;
    set_details(file_id, pDetails, ver);
    return true;
}

SteamAPICall_t Steam_UGC::internal_RequestUGCDetails( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds, IUgcItfVersion ver )
{
    PRINT_DEBUG("%llu %u <%u>", nPublishedFileID, unMaxAgeSeconds, (unsigned)ver);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (ver <= IUgcItfVersion::v018) { // <= SDK 1.59
        SteamUGCRequestUGCDetailsResult018_t data{};
        data.m_bCachedData = false;
        set_details(nPublishedFileID, reinterpret_cast<SteamUGCDetails_t *>(&data.m_details), ver);
        
        auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        return ret;
    } else { // >= SDK 1.60
        SteamUGCRequestUGCDetailsResult_t data{};
        data.m_bCachedData = false;
        set_details(nPublishedFileID, &data.m_details, ver);
        
        auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        return ret;
    }
}


Steam_UGC::Steam_UGC(class Settings *settings, class Ugc_Remote_Storage_Bridge *ugc_bridge, class Local_Storage *local_storage, class SteamCallResults *callback_results, class SteamCallBacks *callbacks)
{
    this->settings = settings;
    this->ugc_bridge = ugc_bridge;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->callback_results = callback_results;

    GBE_DotaEnsureWorkshopModsForUGC(settings, ugc_bridge);

    read_ugc_favorites();
}


// Query UGC associated with a user. Creator app id or consumer app id must be valid and be set to the current running app. unPage should start at 1.
UGCQueryHandle_t Steam_UGC::CreateQueryUserUGCRequest( AccountID_t unAccountID, EUserUGCList eListType, EUGCMatchingUGCType eMatchingUGCType, EUserUGCListSortOrder eSortOrder, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage )
{
    PRINT_DEBUG("%u %i %i %i %u %u %u", unAccountID, eListType, eMatchingUGCType, eSortOrder, nCreatorAppID, nConsumerAppID, unPage);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    // TODO: more info needed to decide which UGCs will be returned
    // if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (unPage < 1) return k_UGCQueryHandleInvalid;
    if (eListType < 0) return k_UGCQueryHandleInvalid;
    if (unAccountID != settings->get_local_steam_id().GetAccountID()) return k_UGCQueryHandleInvalid;
    
    // TODO
    return new_ugc_query(eUserUGCRequest, eListType == k_EUserUGCList_Subscribed || eListType == k_EUserUGCList_Published, unPage);
}


// Query for all matching UGC. Creator app id or consumer app id must be valid and be set to the current running app. unPage should start at 1.
UGCQueryHandle_t Steam_UGC::CreateQueryAllUGCRequest( EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage )
{
    PRINT_DEBUG("page");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (unPage < 1) return k_UGCQueryHandleInvalid;
    if (eQueryType < 0) return k_UGCQueryHandleInvalid;
    
    // TODO
    return new_ugc_query(eAllUGCRequestPage, true, unPage);
}

// Query for all matching UGC using the new deep paging interface. Creator app id or consumer app id must be valid and be set to the current running app. pchCursor should be set to NULL or "*" to get the first result set.
UGCQueryHandle_t Steam_UGC::CreateQueryAllUGCRequest( EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, const char *pchCursor )
{
    PRINT_DEBUG("cursor");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nCreatorAppID != settings->get_local_game_id().AppID() || nConsumerAppID != settings->get_local_game_id().AppID()) return k_UGCQueryHandleInvalid;
    if (eQueryType < 0) return k_UGCQueryHandleInvalid;
    
    // TODO: totally don't know what does pchCursor mean, we currently emu it to be a string of page number instead
    uint32 page = 0;
    bool next_cursor = true;
    std::string cursor = pchCursor != NULL ? std::string(pchCursor) : std::string("*");

    try {
        if (cursor == std::string("*")) {
            page = 1;
        }
        else if (cursor == std::string("")) {
            page = 1;            // Tested on real steam, "" is a valid cursor, which seems to be always page 1.
            next_cursor = false; // However, under this condition, next cursor will still be "", so we flag it here
        }
        else {
            page = std::stoul(cursor);
        }
    }
    catch (const std::exception &e) {
        PRINT_DEBUG("Conversion error, reason: %s. Is this a valid cursor?", e.what());
        page = 0;
    }

    // TODO
    return new_ugc_query(eAllUGCRequestCursor, true, page, next_cursor);
}

// Query for the details of the given published file ids (the RequestUGCDetails call is deprecated and replaced with this)
UGCQueryHandle_t Steam_UGC::CreateQueryUGCDetailsRequest( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG("%p, max file IDs = [%u]", pvecPublishedFileID, unNumPublishedFileIDs);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (!pvecPublishedFileID) return k_UGCQueryHandleInvalid;
    if (unNumPublishedFileIDs < 1) return k_UGCQueryHandleInvalid;

    // TODO
    std::set<PublishedFileId_t> only(pvecPublishedFileID, pvecPublishedFileID + unNumPublishedFileIDs);
    
#ifndef EMU_RELEASE_BUILD
    for (const auto &id : only) {
        PRINT_DEBUG("  requesting details for file ID = %llu", id);
    }
#endif

    return new_ugc_query(eUGCDetailsRequest, false, 0, true, only);
}


// Send the query to Steam
STEAM_CALL_RESULT( SteamUGCQueryCompleted_t )
SteamAPICall_t Steam_UGC::SendQueryUGCRequest( UGCQueryHandle_t handle )
{
    PRINT_DEBUG("%llu", handle);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    const auto trigger_failure = [handle, this](){
        SteamUGCQueryCompleted_t data{};
        data.m_handle = handle;
        data.m_eResult = k_EResultFail;
        data.m_unNumResultsReturned = 0;
        data.m_unTotalMatchingResults = 0;
        data.m_bCachedData = false;
        
        auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        return ret;
    };
    
    if (handle == k_UGCQueryHandleInvalid) return trigger_failure();

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return trigger_failure();

    SteamUGCQueryCompleted_t data{};
    data.m_handle = handle;
    data.m_eResult = k_EResultOK;
    data.m_bCachedData = false;

    std::set<PublishedFileId_t> all_subscribed = std::set<PublishedFileId_t>(ugc_bridge->subbed_mods_itr_begin(), ugc_bridge->subbed_mods_itr_end());

    if (request->query_type == eUserUGCRequest) {
        if (request->return_all_subscribed) {
            if (request->page > 0) {
                uint32 beg_item = (request->page - 1) * kNumUGCResultsPerPage;
                if (beg_item < all_subscribed.size()) {
                    auto sub = all_subscribed.begin();
                    std::advance(sub, beg_item);
                    for (uint32 i = 0; sub != all_subscribed.end() && i < kNumUGCResultsPerPage; ++sub, ++i) {
                        request->results.insert(*sub);
                    }
                }
            }

            data.m_unNumResultsReturned = static_cast<uint32>(request->results.size());
            data.m_unTotalMatchingResults = static_cast<uint32>(all_subscribed.size());
        }
        else {
            data.m_unNumResultsReturned = 0;
            data.m_unTotalMatchingResults = 0;
        }
    }
    else if (request->query_type == eAllUGCRequestPage || request->query_type == eAllUGCRequestCursor) {
        if (request->page > 0) {
            uint32 beg_item = (request->page - 1) * kNumUGCResultsPerPage;
            if (beg_item < all_subscribed.size()) {
                auto sub = all_subscribed.begin();
                std::advance(sub, beg_item);
                for (uint32 i = 0; sub != all_subscribed.end() && i < kNumUGCResultsPerPage; ++sub, ++i) {
                    request->results.insert(*sub);
                }

                data.m_unNumResultsReturned = static_cast<uint32>(request->results.size());
                data.m_unTotalMatchingResults = static_cast<uint32>(all_subscribed.size());
                if (request->query_type == eAllUGCRequestCursor) {
                    std::string next_page_cursor = request->next_cursor ? std::to_string(request->page + 1) : std::string("");
                    next_page_cursor.copy(data.m_rgchNextCursor, sizeof(data.m_rgchNextCursor) - 1);
                }
            }
            else {
                data.m_eResult = k_EResultInvalidParam;
                data.m_unNumResultsReturned = 0;
                data.m_unTotalMatchingResults = 0;
                if (request->query_type == eAllUGCRequestCursor)
                    data.m_rgchNextCursor[0] = '\0';
            }
        }
        else { // impossible to meet this condition when query_type is eAllUGCRequestPage though
            data.m_eResult = request->query_type == eAllUGCRequestCursor ? k_EResultFail : k_EResultInvalidParam;
            data.m_unNumResultsReturned = 0;
            data.m_unTotalMatchingResults = 0;
            if (request->query_type == eAllUGCRequestCursor)
                data.m_rgchNextCursor[0] = '\0';
        }
    }
    else if (request->query_type == eUGCDetailsRequest) {
        if (request->return_only.size()) {
            for (auto & s : request->return_only) {
                if (ugc_bridge->has_subbed_mod(s)) {
                    request->results.insert(s);
                }
            }
        }

        data.m_unNumResultsReturned = static_cast<uint32>(request->results.size());
        data.m_unTotalMatchingResults = static_cast<uint32>(request->results.size());
    }

    // send these handles to steam_remote_storage since the game will later
    // call Steam_Remote_Storage::UGCDownload() with these files handles (primary + preview)
    for (auto fileid : request->results) {
        auto mod = settings->getMod(fileid);
        ugc_bridge->add_ugc_query_result(mod.handleFile, fileid, true);
        ugc_bridge->add_ugc_query_result(mod.handlePreviewFile, fileid, false);
    }
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


// Retrieve an individual result after receiving the callback for querying UGC
bool Steam_UGC::GetQueryUGCResult( UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails )
{
    PRINT_DEBUG("%llu %u %p", handle, index, pDetails);
    return internal_GetQueryUGCResult(handle, index, pDetails, IUgcItfVersion::v020);
}

bool Steam_UGC::GetQueryUGCResult_old( UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails )
{
    PRINT_DEBUG("%llu %u %p", handle, index, pDetails);
    return internal_GetQueryUGCResult(handle, index, pDetails, IUgcItfVersion::v018);
}

std::optional<std::string> Steam_UGC::get_query_ugc_tag(UGCQueryHandle_t handle, uint32 index, uint32 indexTag)
{
    auto res = get_query_ugc_tags(handle, index);
    if (res.empty()) return std::nullopt;
    if (indexTag >= res.size()) return std::nullopt;

    std::string tmp = res[indexTag];
    if (!tmp.empty() && tmp.back() == ',') {
        tmp = tmp.substr(0, tmp.size() - 1);
    }
    return tmp;
}

uint32 Steam_UGC::GetQueryUGCNumTags( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;
    
    auto res = get_query_ugc_tags(handle, index);
    return static_cast<uint32>(res.size());
}

bool Steam_UGC::GetQueryUGCTag( UGCQueryHandle_t handle, uint32 index, uint32 indexTag, STEAM_OUT_STRING_COUNT( cchValueSize ) char* pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchValue || !cchValueSize) return false;

    auto res = get_query_ugc_tag(handle, index, indexTag);
    if (!res.has_value()) return false;

    memset(pchValue, 0, cchValueSize);
    res.value().copy(pchValue, cchValueSize - 1);
    return true;
}

bool Steam_UGC::GetQueryUGCTagDisplayName( UGCQueryHandle_t handle, uint32 index, uint32 indexTag, STEAM_OUT_STRING_COUNT( cchValueSize ) char* pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    // TODO is this correct?
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchValue || !cchValueSize) return false;

    auto res = get_query_ugc_tag(handle, index, indexTag);
    if (!res.has_value()) return false;

    memset(pchValue, 0, cchValueSize);
    res.value().copy(pchValue, cchValueSize - 1);
    return true;
}

bool Steam_UGC::GetQueryUGCPreviewURL( UGCQueryHandle_t handle, uint32 index, STEAM_OUT_STRING_COUNT(cchURLSize) char *pchURL, uint32 cchURLSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    //TODO: escape simulator tries downloading this url and unsubscribes if it fails
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchURL || !cchURLSize) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto &mod = res.value();
    PRINT_DEBUG("Steam_UGC:GetQueryUGCPreviewURL: '%s'", mod.previewURL.c_str());
    memset(pchURL, 0, cchURLSize);
    mod.previewURL.copy(pchURL, cchURLSize - 1);
    return true;
}


bool Steam_UGC::GetQueryUGCMetadata( UGCQueryHandle_t handle, uint32 index, STEAM_OUT_STRING_COUNT(cchMetadatasize) char *pchMetadata, uint32 cchMetadatasize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchMetadata || !cchMetadatasize) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto &mod = res.value();
    PRINT_DEBUG("Steam_UGC:GetQueryUGCMetadata: '%s'", mod.metadata.c_str());
    memset(pchMetadata, 0, cchMetadatasize);
    mod.metadata.copy(pchMetadata, cchMetadatasize - 1);
    return true;
}


bool Steam_UGC::GetQueryUGCChildren( UGCQueryHandle_t handle, uint32 index, PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}


bool Steam_UGC::GetQueryUGCStatistic( UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint64 *pStatValue )
{
    PRINT_DEBUG("%llu %u %i %p", handle, index, static_cast<int>(eStatType), pStatValue);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pStatValue) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    *pStatValue = GBE_UGCStatisticValue(res.value(), eStatType);
    PRINT_DEBUG("Steam_UGC:GetQueryUGCStatistic: file=%llu stat=%i value=%llu", res.value().id, static_cast<int>(eStatType), *pStatValue);
    return true;
}

bool Steam_UGC::GetQueryUGCStatistic( UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint32 *pStatValue )
{
    PRINT_DEBUG("%llu %u %i %p", handle, index, static_cast<int>(eStatType), pStatValue);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pStatValue) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    const uint64 stat_value = GBE_UGCStatisticValue(res.value(), eStatType);
    *pStatValue = stat_value > UINT32_MAX ? UINT32_MAX : static_cast<uint32>(stat_value);
    PRINT_DEBUG("Steam_UGC:GetQueryUGCStatistic: file=%llu stat=%i value=%u", res.value().id, static_cast<int>(eStatType), *pStatValue);
    return true;
}

uint32 Steam_UGC::GetQueryUGCNumAdditionalPreviews( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return 0;
    
    return 0;
}


bool Steam_UGC::GetQueryUGCAdditionalPreview( UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, STEAM_OUT_STRING_COUNT(cchURLSize) char *pchURLOrVideoID, uint32 cchURLSize, STEAM_OUT_STRING_COUNT(cchOriginalFileNameSize) char *pchOriginalFileName, uint32 cchOriginalFileNameSize, EItemPreviewType *pPreviewType )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

bool Steam_UGC::GetQueryUGCAdditionalPreview( UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, char *pchURLOrVideoID, uint32 cchURLSize, bool *hz )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return false;
}

uint32 Steam_UGC::GetQueryUGCNumKeyValueTags( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return 0;

    auto key_value_tags = GBE_DotaModKeyValueTags(res.value());
    PRINT_DEBUG("Steam_UGC:GetQueryUGCNumKeyValueTags: %u", static_cast<uint32>(key_value_tags.size()));
    return static_cast<uint32>(key_value_tags.size());
}


bool Steam_UGC::GetQueryUGCKeyValueTag( UGCQueryHandle_t handle, uint32 index, uint32 keyValueTagIndex, STEAM_OUT_STRING_COUNT(cchKeySize) char *pchKey, uint32 cchKeySize, STEAM_OUT_STRING_COUNT(cchValueSize) char *pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchKey || !cchKeySize || !pchValue || !cchValueSize) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto key_value_tags = GBE_DotaModKeyValueTags(res.value());
    if (keyValueTagIndex >= key_value_tags.size()) return false;

    const auto &key_value = key_value_tags[keyValueTagIndex];
    memset(pchKey, 0, cchKeySize);
    memset(pchValue, 0, cchValueSize);
    key_value.first.copy(pchKey, cchKeySize - 1);
    key_value.second.copy(pchValue, cchValueSize - 1);
    PRINT_DEBUG("Steam_UGC:GetQueryUGCKeyValueTag: [%u] '%s'='%s'", keyValueTagIndex, key_value.first.c_str(), key_value.second.c_str());
    return true;
}

bool Steam_UGC::GetQueryUGCKeyValueTag( UGCQueryHandle_t handle, uint32 index, const char *pchKey, STEAM_OUT_STRING_COUNT(cchValueSize) char *pchValue, uint32 cchValueSize )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;
    if (!pchKey || !pchValue || !cchValueSize) return false;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto key_value_tags = GBE_DotaModKeyValueTags(res.value());
    auto tag = std::find_if(key_value_tags.begin(), key_value_tags.end(), [pchKey](const auto &item) { return item.first == pchKey; });
    if (tag == key_value_tags.end()) return false;

    memset(pchValue, 0, cchValueSize);
    tag->second.copy(pchValue, cchValueSize - 1);
    PRINT_DEBUG("Steam_UGC:GetQueryUGCKeyValueTag: '%s'='%s'", tag->first.c_str(), tag->second.c_str());
    return true;
}

// TODO no public docs
// Some items can specify that they have a version that is valid for a range of game versions (Steam branch)
uint32 Steam_UGC::GetNumSupportedGameVersions( UGCQueryHandle_t handle, uint32 index )
{
    PRINT_DEBUG("%llu %u // TODO", handle, index);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return 0;
    
    return 1;
}

// TODO no public docs
bool Steam_UGC::GetSupportedGameVersionData( UGCQueryHandle_t handle, uint32 index, uint32 versionIndex, STEAM_OUT_STRING_COUNT( cchGameBranchSize ) char *pchGameBranchMin, STEAM_OUT_STRING_COUNT( cchGameBranchSize ) char *pchGameBranchMax, uint32 cchGameBranchSize )
{
    PRINT_DEBUG("%llu %u %u // TODO", handle, index, versionIndex);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    if (versionIndex != 0) { // TODO I assume this is supposed to be an index in the range [ 0, GetNumSupportedGameVersions() )
        return false;
    }
    
    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return false;

    auto &mod = res.value();

    // TODO I assume each mod/workshop item has a min version and max version for the game
    if (pchGameBranchMin && static_cast<size_t>(cchGameBranchSize) > mod.min_game_branch.size()) {
        memset(pchGameBranchMin, 0, cchGameBranchSize);
        memcpy(pchGameBranchMin, mod.min_game_branch.c_str(), mod.min_game_branch.size());
    }
    if (pchGameBranchMax && static_cast<size_t>(cchGameBranchSize) > mod.max_game_branch.size()) {
        memset(pchGameBranchMax, 0, cchGameBranchSize);
        memcpy(pchGameBranchMax, mod.max_game_branch.c_str(), mod.max_game_branch.size());
    }

    return true;
}

uint32 Steam_UGC::GetQueryUGCContentDescriptors( UGCQueryHandle_t handle, uint32 index, EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return 0;

    auto res = get_query_ugc(handle, index);
    if (!res.has_value()) return 0;

    return 0;
}

// Release the request to free up memory, after retrieving results
bool Steam_UGC::ReleaseQueryUGCRequest( UGCQueryHandle_t handle )
{
    PRINT_DEBUG("%llu", handle);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;

    ugc_queries.erase(request);
    return true;
}


// Options to set for querying UGC
bool Steam_UGC::AddRequiredTag( UGCQueryHandle_t handle, const char *pTagName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::AddRequiredTagGroup( UGCQueryHandle_t handle, const SteamParamStringArray_t *pTagGroups )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::AddExcludedTag( UGCQueryHandle_t handle, const char *pTagName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnOnlyIDs( UGCQueryHandle_t handle, bool bReturnOnlyIDs )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnKeyValueTags( UGCQueryHandle_t handle, bool bReturnKeyValueTags )
{
    PRINT_DEBUG_TODO();
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnLongDescription( UGCQueryHandle_t handle, bool bReturnLongDescription )
{
    PRINT_DEBUG_TODO();
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnMetadata( UGCQueryHandle_t handle, bool bReturnMetadata )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnChildren( UGCQueryHandle_t handle, bool bReturnChildren )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnAdditionalPreviews( UGCQueryHandle_t handle, bool bReturnAdditionalPreviews )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnTotalOnly( UGCQueryHandle_t handle, bool bReturnTotalOnly )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetReturnPlaytimeStats( UGCQueryHandle_t handle, uint32 unDays )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetLanguage( UGCQueryHandle_t handle, const char *pchLanguage )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetAllowCachedResponse( UGCQueryHandle_t handle, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

// TODO no public docs
// allow ISteamUGC to be used in a tools like environment for users who have the appropriate privileges for the calling appid
bool Steam_UGC::SetAdminQuery( UGCUpdateHandle_t handle, bool bAdminQuery )
{
    PRINT_DEBUG("%llu %i // TODO", handle, (int)bAdminQuery);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    request->admin_query = bAdminQuery;
    return true;
}


// Options only for querying user UGC
bool Steam_UGC::SetCloudFileNameFilter( UGCQueryHandle_t handle, const char *pMatchCloudFileName )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


// Options only for querying all UGC
bool Steam_UGC::SetMatchAnyTag( UGCQueryHandle_t handle, bool bMatchAnyTag )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetSearchText( UGCQueryHandle_t handle, const char *pSearchText )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::SetRankedByTrendDays( UGCQueryHandle_t handle, uint32 unDays )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}


bool Steam_UGC::AddRequiredKeyValueTag( UGCQueryHandle_t handle, const char *pKey, const char *pValue )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::SetTimeCreatedDateRange( UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

bool Steam_UGC::SetTimeUpdatedDateRange( UGCQueryHandle_t handle, RTime32 rtStart, RTime32 rtEnd )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    return true;
}

// DEPRECATED - Use CreateQueryUGCDetailsRequest call above instead!
SteamAPICall_t Steam_UGC::RequestUGCDetails( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    return internal_RequestUGCDetails(nPublishedFileID, unMaxAgeSeconds, IUgcItfVersion::v020);
}
 
SteamAPICall_t Steam_UGC::RequestUGCDetails_old( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    return internal_RequestUGCDetails(nPublishedFileID, unMaxAgeSeconds, IUgcItfVersion::v018);
}

SteamAPICall_t Steam_UGC::RequestUGCDetails( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("old");
    return RequestUGCDetails_old(nPublishedFileID, 0);
}


// Steam Workshop Creator API
STEAM_CALL_RESULT( CreateItemResult_t )
SteamAPICall_t Steam_UGC::CreateItem( AppId_t nConsumerAppId, EWorkshopFileType eFileType )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_uAPICallInvalid;
}
 // create new item for this app with no content attached yet


UGCUpdateHandle_t Steam_UGC::StartItemUpdate( AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_UGCUpdateHandleInvalid;
}
 // start an UGC item update. Set changed properties before commiting update with CommitItemUpdate()


bool Steam_UGC::SetItemTitle( UGCUpdateHandle_t handle, const char *pchTitle )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the title of an UGC item


bool Steam_UGC::SetItemDescription( UGCUpdateHandle_t handle, const char *pchDescription )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the description of an UGC item


bool Steam_UGC::SetItemUpdateLanguage( UGCUpdateHandle_t handle, const char *pchLanguage )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // specify the language of the title or description that will be set


bool Steam_UGC::SetItemMetadata( UGCUpdateHandle_t handle, const char *pchMetaData )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the metadata of an UGC item (max = k_cchDeveloperMetadataMax)


bool Steam_UGC::SetItemVisibility( UGCUpdateHandle_t handle, ERemoteStoragePublishedFileVisibility eVisibility )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the visibility of an UGC item


bool Steam_UGC::SetItemTags( UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags )
{
    PRINT_DEBUG("old");
    return SetItemTags(updateHandle, pTags, false);
}

bool Steam_UGC::SetItemTags( UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags, bool bAllowAdminTags )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // change the tags of an UGC item

bool Steam_UGC::SetItemContent( UGCUpdateHandle_t handle, const char *pszContentFolder )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // update item content from this local folder


bool Steam_UGC::SetItemPreview( UGCUpdateHandle_t handle, const char *pszPreviewFile )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  change preview image file for this item. pszPreviewFile points to local image file, which must be under 1MB in size

bool Steam_UGC::SetAllowLegacyUpload( UGCUpdateHandle_t handle, bool bAllowLegacyUpload )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

bool Steam_UGC::RemoveAllItemKeyValueTags( UGCUpdateHandle_t handle )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove all existing key-value tags (you can add new ones via the AddItemKeyValueTag function)

bool Steam_UGC::RemoveItemKeyValueTags( UGCUpdateHandle_t handle, const char *pchKey )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove any existing key-value tags with the specified key


bool Steam_UGC::AddItemKeyValueTag( UGCUpdateHandle_t handle, const char *pchKey, const char *pchValue )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // add new key-value tags for the item. Note that there can be multiple values for a tag.


bool Steam_UGC::AddItemPreviewFile( UGCUpdateHandle_t handle, const char *pszPreviewFile, EItemPreviewType type )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  add preview file for this item. pszPreviewFile points to local file, which must be under 1MB in size


bool Steam_UGC::AddItemPreviewVideo( UGCUpdateHandle_t handle, const char *pszVideoID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  add preview video for this item


bool Steam_UGC::UpdateItemPreviewFile( UGCUpdateHandle_t handle, uint32 index, const char *pszPreviewFile )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  updates an existing preview file for this item. pszPreviewFile points to local file, which must be under 1MB in size


bool Steam_UGC::UpdateItemPreviewVideo( UGCUpdateHandle_t handle, uint32 index, const char *pszVideoID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 //  updates an existing preview video for this item


bool Steam_UGC::RemoveItemPreview( UGCUpdateHandle_t handle, uint32 index )
{
    PRINT_DEBUG("%llu %u", handle, index);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}
 // remove a preview by index starting at 0 (previews are sorted)

bool Steam_UGC::AddContentDescriptor( UGCUpdateHandle_t handle, EUGCContentDescriptorID descid )
{
    PRINT_DEBUG("%llu %u", handle, descid);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

bool Steam_UGC::RemoveContentDescriptor( UGCUpdateHandle_t handle, EUGCContentDescriptorID descid )
{
    PRINT_DEBUG("%llu %u", handle, descid);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

// TODO no public docs
bool Steam_UGC::SetRequiredGameVersions( UGCUpdateHandle_t handle, const char *pszGameBranchMin, const char *pszGameBranchMax )
{
    PRINT_DEBUG("%llu '%s' '%s' // TODO", handle, pszGameBranchMin, pszGameBranchMax);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (handle == k_UGCQueryHandleInvalid) return false;

    auto request = std::find_if(ugc_queries.begin(), ugc_queries.end(), [&handle](struct UGC_query const& item) { return item.handle == handle; });
    if (ugc_queries.end() == request) return false;
    
    if (pszGameBranchMin) request->min_branch = pszGameBranchMin;
    if (pszGameBranchMax) request->max_branch = pszGameBranchMax;
    return true;
}

STEAM_CALL_RESULT( SubmitItemUpdateResult_t )
SteamAPICall_t Steam_UGC::SubmitItemUpdate( UGCUpdateHandle_t handle, const char *pchChangeNote )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_uAPICallInvalid;
}
 // commit update process started with StartItemUpdate()


EItemUpdateStatus Steam_UGC::GetItemUpdateProgress( UGCUpdateHandle_t handle, uint64 *punBytesProcessed, uint64* punBytesTotal )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return k_EItemUpdateStatusInvalid;
}


// Steam Workshop Consumer API

STEAM_CALL_RESULT( SetUserItemVoteResult_t )
SteamAPICall_t Steam_UGC::SetUserItemVote( PublishedFileId_t nPublishedFileID, bool bVoteUp )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct
    
    auto mod  = settings->getMod(nPublishedFileID);
    if (bVoteUp) {
        ++mod.votesUp;
    } else {
        ++mod.votesDown;
    }
    settings->addModDetails(nPublishedFileID, mod);
    
    SetUserItemVoteResult_t data{};
    data.m_eResult = EResult::k_EResultOK;
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bVoteUp = bVoteUp;

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


STEAM_CALL_RESULT( GetUserItemVoteResult_t )
SteamAPICall_t Steam_UGC::GetUserItemVote( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    auto mod  = settings->getMod(nPublishedFileID);
    GetUserItemVoteResult_t data{};
    data.m_eResult = EResult::k_EResultOK;
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bVotedDown = mod.votesDown;
    data.m_bVotedUp = mod.votesUp;
    data.m_bVoteSkipped = true;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


STEAM_CALL_RESULT( UserFavoriteItemsListChanged_t )
SteamAPICall_t Steam_UGC::AddItemToFavorites( AppId_t nAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%u %llu", nAppId, nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nAppId == k_uAppIdInvalid || nAppId != settings->get_local_game_id().AppID()) return k_uAPICallInvalid; // TODO is this correct
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    UserFavoriteItemsListChanged_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bWasAddRequest = true;

    auto add = favorites.insert(nPublishedFileID);
    if (add.second) { // if new insertion
        PRINT_DEBUG(" adding new item to favorites");
        bool ok = write_ugc_favorites();
        data.m_eResult = ok ? EResult::k_EResultOK : EResult::k_EResultFail;
    } else { // nPublishedFileID already exists
        data.m_eResult = EResult::k_EResultOK;
    }
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


STEAM_CALL_RESULT( UserFavoriteItemsListChanged_t )
SteamAPICall_t Steam_UGC::RemoveItemFromFavorites( AppId_t nAppId, PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (nAppId == k_uAppIdInvalid || nAppId != settings->get_local_game_id().AppID()) return k_uAPICallInvalid; // TODO is this correct
    if (nPublishedFileID == k_PublishedFileIdInvalid || !settings->isModInstalled(nPublishedFileID)) return k_uAPICallInvalid; // TODO is this correct

    UserFavoriteItemsListChanged_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    data.m_bWasAddRequest = false;

    auto removed = favorites.erase(nPublishedFileID);
    if (removed) {
        PRINT_DEBUG(" removing item from favorites");
        bool ok = write_ugc_favorites();
        data.m_eResult = ok ? EResult::k_EResultOK : EResult::k_EResultFail;
    } else { // nPublishedFileID didn't exist
        data.m_eResult = EResult::k_EResultOK;
    }
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


STEAM_CALL_RESULT( RemoteStorageSubscribePublishedFileResult_t )
SteamAPICall_t Steam_UGC::SubscribeItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    RemoteStorageSubscribePublishedFileResult_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    if (settings->isModInstalled(nPublishedFileID)) {
        data.m_eResult = k_EResultOK;
        ugc_bridge->add_subbed_mod(nPublishedFileID);
    } else {
        data.m_eResult = k_EResultFail;
    }
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}
 // subscribe to this item, will be installed ASAP

STEAM_CALL_RESULT( RemoteStorageUnsubscribePublishedFileResult_t )
SteamAPICall_t Steam_UGC::UnsubscribeItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    RemoteStorageUnsubscribePublishedFileResult_t data{};
    data.m_nPublishedFileId = nPublishedFileID;
    if (!ugc_bridge->has_subbed_mod(nPublishedFileID)) {
        data.m_eResult = k_EResultFail; //TODO: check if this is accurate
    } else {
        data.m_eResult = k_EResultOK;
        ugc_bridge->remove_subbed_mod(nPublishedFileID);
    }

    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}
 // unsubscribe from this item, will be uninstalled after game quits

uint32 Steam_UGC::GetNumSubscribedItems( bool bIncludeLocallyDisabled )
{
    PRINT_DEBUG(" %d", (int)bIncludeLocallyDisabled);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    std::set<PublishedFileId_t> subscribed_enabled = std::set<PublishedFileId_t>(ugc_bridge->subbed_mods_itr_begin(), ugc_bridge->subbed_mods_itr_end());
    if (!bIncludeLocallyDisabled) {
        for (auto &sd : subscribed_disabled) {
            subscribed_enabled.erase(sd);
        }
    }

    size_t count = subscribed_enabled.size();
    PRINT_DEBUG("  Steam_UGC::GetNumSubscribedItems = %zu", count);
    return (uint32)count;
}

uint32 Steam_UGC::GetNumSubscribedItems()
{
    PRINT_DEBUG_ENTRY();
    return GetNumSubscribedItems(false);
}
 // number of subscribed items 

uint32 Steam_UGC::GetSubscribedItems( PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries, bool bIncludeLocallyDisabled )
{
    PRINT_DEBUG("%p %u %d", pvecPublishedFileID, cMaxEntries, (int)bIncludeLocallyDisabled);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    std::set<PublishedFileId_t> subscribed_enabled = std::set<PublishedFileId_t>(ugc_bridge->subbed_mods_itr_begin(), ugc_bridge->subbed_mods_itr_end());
    if (!bIncludeLocallyDisabled) {
        for (auto &sd : subscribed_disabled) {
            subscribed_enabled.erase(sd);
        }
    }

    size_t count = std::min<size_t>(subscribed_enabled.size(), cMaxEntries);
    std::copy_n(subscribed_enabled.begin(), count, pvecPublishedFileID);
    return (uint32)count;
}

uint32 Steam_UGC::GetSubscribedItems( PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries )
{
    PRINT_DEBUG("old %p %u", pvecPublishedFileID, cMaxEntries);
    return GetSubscribedItems(pvecPublishedFileID, cMaxEntries, false);
}
 // all subscribed item PublishFileIDs

// get EItemState flags about item on this client
uint32 Steam_UGC::GetItemState( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (!settings->isModInstalled(nPublishedFileID)) {
        PRINT_DEBUG("  mod isn't found");
        return k_EItemStateNone;
    }

    if (ugc_bridge->has_subbed_mod(nPublishedFileID)) {
        if (subscribed_disabled.count(nPublishedFileID)) {
            PRINT_DEBUG("  mod is subscribed but disabled");
            return k_EItemStateDisabledLocally | k_EItemStateSubscribed;
        }
        else {
            PRINT_DEBUG("  mod is subscribed and installed");
            return k_EItemStateInstalled | k_EItemStateSubscribed;
        }
    }


    PRINT_DEBUG("  mod is not subscribed");
    return k_EItemStateDisabledLocally;
}


// get info about currently installed content on disc for items that have k_EItemStateInstalled set
// if k_EItemStateLegacyItem is set, pchFolder contains the path to the legacy file itself (not a folder)
bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, STEAM_OUT_STRING_COUNT( cchFolderSize ) char *pchFolder, uint32 cchFolderSize, uint32 *punTimeStamp )
{
    PRINT_DEBUG("%llu %p %p [%u] %p", nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, punTimeStamp);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!cchFolderSize) return false;
    if (!settings->isModInstalled(nPublishedFileID)) return false;

    auto mod = settings->getMod(nPublishedFileID);
    
    // I don't know if this is accurate behavior, but to avoid returning true with invalid data
    if ((cchFolderSize - 1) < mod.path.size()) { // -1 because the last char is reserved for null terminator
        PRINT_DEBUG("  ERROR mod path: '%s' [%zu bytes] cannot fit into the given buffer", mod.path.c_str(), mod.path.size());
        return false;
    }

    if (punSizeOnDisk) *punSizeOnDisk = mod.primaryFileSize;
    if (punTimeStamp) *punTimeStamp = mod.timeAddedToUserList;
    if (pchFolder && cchFolderSize) {
        // human fall flat doesn't send a nulled buffer, and won't recognize the proper mod path because of that
        memset(pchFolder, 0, cchFolderSize);
        mod.path.copy(pchFolder, cchFolderSize - 1);
        PRINT_DEBUG("  final mod path: '%s'", pchFolder);
    }

    return true;
}


// get info about pending update for items that have k_EItemStateNeedsUpdate set. punBytesTotal will be valid after download started once
bool Steam_UGC::GetItemDownloadInfo( PublishedFileId_t nPublishedFileID, uint64 *punBytesDownloaded, uint64 *punBytesTotal )
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!settings->isModInstalled(nPublishedFileID)) return false;

    auto mod = settings->getMod(nPublishedFileID);
    if (punBytesDownloaded) *punBytesDownloaded = mod.primaryFileSize;
    if (punBytesTotal) *punBytesTotal = mod.primaryFileSize;
    return true;
}

bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, STEAM_OUT_STRING_COUNT( cchFolderSize ) char *pchFolder, uint32 cchFolderSize, bool *pbLegacyItem ) // returns true if item is installed
{
    PRINT_DEBUG("old");
    return GetItemInstallInfo(nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, (uint32*) nullptr);
}

bool Steam_UGC::GetItemUpdateInfo( PublishedFileId_t nPublishedFileID, bool *pbNeedsUpdate, bool *pbIsDownloading, uint64 *punBytesDownloaded, uint64 *punBytesTotal )
{
    PRINT_DEBUG("old");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    bool res = GetItemDownloadInfo(nPublishedFileID, punBytesDownloaded, punBytesTotal);
    if (res) {
        if (pbNeedsUpdate) *pbNeedsUpdate = false;
        if (pbIsDownloading) *pbIsDownloading = false;
    }
    return res;
}

bool Steam_UGC::GetItemInstallInfo( PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, char *pchFolder, uint32 cchFolderSize ) // returns true if item is installed
{
    PRINT_DEBUG("older");
    return GetItemInstallInfo(nPublishedFileID, punSizeOnDisk, pchFolder, cchFolderSize, (uint32*) nullptr);
}


// download new or update already installed item. If function returns true, wait for DownloadItemResult_t. If the item is already installed,
// then files on disk should not be used until callback received. If item is not subscribed to, it will be cached for some time.
// If bHighPriority is set, any other item download will be suspended and this item downloaded ASAP.
bool Steam_UGC::DownloadItem( PublishedFileId_t nPublishedFileID, bool bHighPriority )
{
    PRINT_DEBUG("%llu %i // TODO", nPublishedFileID, (int)bHighPriority);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (!settings->isModInstalled(nPublishedFileID)) {
        DownloadItemResult_t data_fail{};
        data_fail.m_eResult = EResult::k_EResultFail;
        data_fail.m_nPublishedFileId = nPublishedFileID;
        data_fail.m_unAppID = settings->get_local_game_id().AppID();
        callbacks->addCBResult(data_fail.k_iCallback, &data_fail, sizeof(data_fail), 0.050);
        return false;
    }

    {
        DownloadItemResult_t data{};
        data.m_eResult = EResult::k_EResultOK;
        data.m_nPublishedFileId = nPublishedFileID;
        data.m_unAppID = settings->get_local_game_id().AppID();
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.1);
    }

    {
        ItemInstalled_t data{};
        data.m_hLegacyContent = nPublishedFileID;
        data.m_nPublishedFileId = nPublishedFileID;
        data.m_unAppID = settings->get_local_game_id().AppID();
        data.m_unManifestID = 123; // TODO
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.15);
    }

    PRINT_DEBUG("downloaded!");
    return true;
}


// game servers can set a specific workshop folder before issuing any UGC commands.
// This is helpful if you want to support multiple game servers running out of the same install folder
bool Steam_UGC::BInitWorkshopForGameServer( DepotId_t unWorkshopDepotID, const char *pszFolder )
{
    PRINT_DEBUG_TODO();
    PRINT_DEBUG("[%u] '%s'", unWorkshopDepotID, pszFolder);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    // Space Engineers Dedicated Server (appid 298740) expects this to be true
    return true;
}


// SuspendDownloads( true ) will suspend all workshop downloads until SuspendDownloads( false ) is called or the game ends
void Steam_UGC::SuspendDownloads( bool bSuspend )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
}


// usage tracking
STEAM_CALL_RESULT( StartPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StartPlaytimeTracking( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

STEAM_CALL_RESULT( StopPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StopPlaytimeTracking( PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

STEAM_CALL_RESULT( StopPlaytimeTrackingResult_t )
SteamAPICall_t Steam_UGC::StopPlaytimeTrackingForAllItems()
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    StopPlaytimeTrackingResult_t data;
    data.m_eResult = k_EResultOK;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}


// parent-child relationship or dependency management
STEAM_CALL_RESULT( AddUGCDependencyResult_t )
SteamAPICall_t Steam_UGC::AddDependency( PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nParentPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

STEAM_CALL_RESULT( RemoveUGCDependencyResult_t )
SteamAPICall_t Steam_UGC::RemoveDependency( PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nParentPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}


// add/remove app dependence/requirements (usually DLC)
STEAM_CALL_RESULT( AddAppDependencyResult_t )
SteamAPICall_t Steam_UGC::AddAppDependency( PublishedFileId_t nPublishedFileID, AppId_t nAppID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

STEAM_CALL_RESULT( RemoveAppDependencyResult_t )
SteamAPICall_t Steam_UGC::RemoveAppDependency( PublishedFileId_t nPublishedFileID, AppId_t nAppID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

// request app dependencies. note that whatever callback you register for GetAppDependenciesResult_t may be called multiple times
// until all app dependencies have been returned
STEAM_CALL_RESULT( GetAppDependenciesResult_t )
SteamAPICall_t Steam_UGC::GetAppDependencies( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}


// delete the item without prompting the user
STEAM_CALL_RESULT( DeleteItemResult_t )
SteamAPICall_t Steam_UGC::DeleteItem( PublishedFileId_t nPublishedFileID )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    if (nPublishedFileID == k_PublishedFileIdInvalid) return k_uAPICallInvalid;
    
    return k_uAPICallInvalid;
}

// Show the app's latest Workshop EULA to the user in an overlay window, where they can accept it or not
bool Steam_UGC::ShowWorkshopEULA()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return false;
}

// Retrieve information related to the user's acceptance or not of the app's specific Workshop EULA
STEAM_CALL_RESULT( WorkshopEULAStatus_t )
SteamAPICall_t Steam_UGC::GetWorkshopEULAStatus()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    WorkshopEULAStatus_t data{};
    data.m_eResult = k_EResultOK;
    data.m_nAppID = settings->get_local_game_id().AppID();
    data.m_unVersion = 0; // TODO
    data.m_rtAction = (RTime32)std::chrono::duration_cast<std::chrono::seconds>(startup_time.time_since_epoch()).count();
    data.m_bAccepted = true;
    data.m_bNeedsAction = false;
    
    auto ret = callback_results->addCallResult(data.k_iCallback, &data, sizeof(data));
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return ret;
}

// Return the user's community content descriptor preferences
uint32 Steam_UGC::GetUserContentDescriptorPreferences( EUGCContentDescriptorID *pvecDescriptors, uint32 cMaxEntries )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    
    return 0;
}

// Sets whether the item should be disabled locally or not. This means that it will not be returned in GetSubscribedItems() by default.
bool Steam_UGC::SetItemsDisabledLocally( PublishedFileId_t *pvecPublishedFileIDs, uint32 unNumPublishedFileIDs, bool bDisabledLocally )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    // TODO: save results to disk because real steam can remember them even when restarting the app
    if (!unNumPublishedFileIDs)
        return false;
    if (!pvecPublishedFileIDs)
        return false; // real steam crashes the app

    bool modified = false;
    std::set<PublishedFileId_t> all_subscribed = std::set<PublishedFileId_t>(ugc_bridge->subbed_mods_itr_begin(), ugc_bridge->subbed_mods_itr_end());

    for (uint32 i = 0; i < unNumPublishedFileIDs; ++i) {
        PublishedFileId_t id = pvecPublishedFileIDs[i];

        if (!all_subscribed.count(id))
            continue;

        if (bDisabledLocally) {
            if (subscribed_disabled.insert(id).second)
                modified = true;
        }
        else {
            if (subscribed_disabled.erase(id))
                modified = true;
        }
    }

    return modified;
}

// Set the local load order for these items. If there are any items not in the given list, they will sort by the time subscribed.
bool Steam_UGC::SetSubscriptionsLoadOrder( PublishedFileId_t *pvecPublishedFileIDs, uint32 unNumPublishedFileIDs )
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    // TODO
    if (!unNumPublishedFileIDs)
        return false;

    return true;
}

// Tells the client to no longer try to keep the item in its local cache, unless it was subscribed to by other users on this machine
bool Steam_UGC::MarkDownloadedItemAsUnused(PublishedFileId_t nPublishedFileID)
{
    PRINT_DEBUG("%llu", nPublishedFileID);
    // we don't really have to do anything here, leaving this TODO
    // in case we need to keep track of these marked items later
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return true;
}

// Returns the number of items actually downloaded locally
uint32 Steam_UGC::GetNumDownloadedItems()
{
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    // https://partner.steamgames.com/doc/api/ISteamUGC#GetDownloadedItems
    // "Returns 0 if called from a game server"
    if (get_steam_client()->settings_server == settings) {
        return 0;
    }

    return (uint32)settings->modSet().size(); // not sure if returning all mods is correct
}

// Returns the ids of the items downloaded
uint32 Steam_UGC::GetDownloadedItems(PublishedFileId_t* pvecPublishedFileIDs, uint32 cMaxEntries)
{
    PRINT_DEBUG("%p [%u]", pvecPublishedFileIDs, cMaxEntries);
    PRINT_DEBUG_TODO();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!pvecPublishedFileIDs || cMaxEntries == 0) {
        return 0;
    }

    // https://partner.steamgames.com/doc/api/ISteamUGC#GetDownloadedItems
    // "Returns 0 if called from a game server"
    if (get_steam_client()->settings_server == settings) {
        return 0;
    }

    const auto all_mods = settings->modSet(); // not sure if using all mods is correct
    uint32 count = std::min<uint32>((uint32)all_mods.size(), cMaxEntries);
    std::copy_n(all_mods.cbegin(), count, pvecPublishedFileIDs);

    PRINT_DEBUG("  copied count = %u", count);
    return count;
}

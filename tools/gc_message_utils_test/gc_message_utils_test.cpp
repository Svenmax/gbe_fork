#include "dll/gbe_gc_message_utils.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string hex_string(const std::string &bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : bytes)
        out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}

unsigned char hex_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return static_cast<unsigned char>(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return static_cast<unsigned char>(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F')
        return static_cast<unsigned char>(ch - 'A' + 10);
    return 0;
}

std::string from_hex(const char *hex)
{
    std::string bytes;
    for (size_t index = 0; hex[index] != '\0' && hex[index + 1] != '\0'; index += 2)
        bytes.push_back(static_cast<char>((hex_value(hex[index]) << 4) | hex_value(hex[index + 1])));
    return bytes;
}

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_u32(std::uint32_t actual, std::uint32_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_string(const std::string &actual, const std::string &expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

} // namespace

int main()
{
    using namespace gbe::gc_message;

    bool ok = true;
    ok &= expect_true(has_proto_mask(0x8000001au), "has proto mask");
    ok &= expect_eq_u32(without_proto_mask(0x8000001au), 26u, "without proto mask");
    ok &= expect_eq_u32(with_proto_mask(26u), 0x8000001au, "with proto mask");
    ok &= expect_true(should_trace_dota_proto_boundary(24u), "trace direct 24");
    ok &= expect_true(should_trace_dota_proto_boundary(0x8000119fu), "trace masked 4511");
    ok &= expect_true(!should_trace_dota_proto_boundary(7273u), "ignore chat message");
    ok &= expect_true(is_supported_dota_wrapped_post_login_request(7009u), "supported wrapped 7009");
    ok &= expect_true(is_supported_dota_wrapped_post_login_request(7070u), "supported wrapped custom game ready up");
    ok &= expect_true(is_supported_dota_wrapped_post_login_request(7091u), "supported wrapped 7091");
    ok &= expect_true(is_supported_dota_wrapped_post_login_request(8052u), "supported wrapped custom game started loading");
    ok &= expect_true(is_supported_dota_wrapped_post_login_request(8053u), "supported wrapped custom game finished loading");
    ok &= expect_true(!is_supported_dota_wrapped_post_login_request(7042u), "unsupported wrapped 7042 preserves old gate");
    ok &= expect_true(!is_supported_dota_wrapped_post_login_request(7273u), "unsupported wrapped 7273");

    std::string message;
    ok &= expect_true(build_dota_zero_header_payload(26u, "abc", message), "build zero header");
    ok &= expect_eq_string(hex_string(message), "1a00008000000000616263", "zero header bytes");

    ok &= expect_true(build_dota_job_reply_payload(7055u, 0x0102030405060708ull, "z", message), "build job reply");
    ok &= expect_eq_string(hex_string(message), "8f1b0080090000005908070605040302017a", "job reply bytes");
    const std::string job_reply_message = message;

    ok &= expect_true(build_dota_varint_response_payload(8728u, 1u, 1u, false, 0ull, message), "build varint response");
    ok &= expect_eq_string(hex_string(message), "18220080000000000801", "varint response bytes");

    ok &= expect_true(build_dota_fixed32_response_payload(4524u, 1u, 0x3f800000u, false, 0ull, message), "build fixed32 response");
    ok &= expect_eq_string(hex_string(message), "ac110080000000000d0000803f", "fixed32 response bytes");

    ok &= expect_true(build_dota_bytes_response_payload(7428u, 1u, std::string("\x08\x00", 2), false, 0ull, message), "build bytes response");
    ok &= expect_eq_string(hex_string(message), "041d0080000000000a020800", "bytes response bytes");

    std::string startup_payload;
    ok &= expect_true(build_dota_lobby_additional_startup_account_data_payload(7u, startup_payload), "build startup account payload");
    ok &= expect_eq_string(hex_string(startup_payload), "08071200", "startup account payload bytes");

    std::string object_2015;
    ok &= expect_true(build_dota_server_lobby_object_2015(2u, 7u, object_2015), "build server lobby object 2015");
    ok &= expect_eq_string(hex_string(object_2015), "0a000a00120908a545120408071200", "server lobby object 2015 bytes");

    std::string object_2016;
    build_dota_server_static_lobby_object_2016(7u, 0x0102ull, 1u, std::vector<DotaStaticLobbyMember>{DotaStaticLobbyMember{0x0102ull, 7u, false}}, object_2016, true);
    ok &= expect_eq_string(hex_string(object_2016), "0a2a0902010000000000004800580161000000000000000068008501000000009801009801009801009801001500000000", "server static lobby object 2016 custom bytes");

    ok &= expect_eq_string(hex_string(build_dota_lobby_team_details_payload(true)), "4000880101", "lobby team details bytes");

    std::string object_2014;
    build_dota_static_lobby_object_2014("p", 2u, object_2014);
    ok &= expect_eq_string(hex_string(object_2014), "0a050a017010000a040a001000", "static lobby object 2014 bytes");

    std::string member_2004;
    build_dota_lobby_member_object_2004(DotaLobbyMemberObjectState{0x0102ull, 2u, 3u, 5u, false, 0u}, 2u, 1u, member_2004);
    ok &= expect_eq_string(hex_string(member_2004), "090201000000000000100518023803800101e00100", "lobby member object 2004 disconnected bytes");
    build_dota_lobby_member_object_2004(DotaLobbyMemberObjectState{}, 0u, 0u, member_2004);
    ok &= expect_eq_string(hex_string(member_2004), "00", "lobby member object 2004 empty bytes");

    DotaLobbyObject2004Options object_2004_options{};
    object_2004_options.steam_id = 0x0102ull;
    object_2004_options.lobby_id = 9u;
    object_2004_options.game_mode = 1u;
    object_2004_options.room_name = "r";
    object_2004_options.server_region = 3u;
    object_2004_options.allow_spectating = true;
    object_2004_options.visibility = 1u;
    std::string object_2004;
    build_dota_lobby_object_2004(object_2004_options, std::vector<DotaLobbyMemberObjectState>{DotaLobbyMemberObjectState{0x0102ull, 2u, 3u, 5u, true, 0u}}, object_2004);
    ok &= expect_eq_string(hex_string(object_2004), "08091801200059020100000000000060016800700082010172a80103e00100f80101a00200ba0200d00200d80200e00200f00200f80200800300980300a80300c80300f2030708f54412020800d80401900500c00500e80500f00500f80500880600f00600880700c20712090201000000000000100518023803800101c80700f80700800882f09fcf06", "lobby object 2004 bytes");

    std::size_t custom_game_count = 0;
    ok &= expect_true(build_dota_top_custom_games_list_payload(std::vector<std::uint64_t>{0u, 7u, 8u}, message, custom_game_count), "build top custom games list");
    ok &= expect_true(custom_game_count == 2u, "top custom games count");
    ok &= expect_eq_string(hex_string(message), "581f008000000000080708081007", "top custom games list bytes");

    ok &= expect_true(build_dota_practice_lobby_cache_subscribed_payload_from_objects(9u, "a", "b", "c", "d", message), "build cache subscribed from objects");
    ok &= expect_eq_string(hex_string(message), "1800008000000000220408031009120608d40f120161120608df0f120162120508dd0f1200120608de0f120163120608e00f120164", "cache subscribed from objects bytes");

    ok &= expect_true(build_dota_practice_lobby_details_update_payload_from_objects(9u, "c", "b", "a", "d", true, message), "build details update from objects");
    ok &= expect_eq_string(hex_string(message), "1a00008000000000320408031009120608de0f120163120508dd0f1200120608df0f120162120608d40f120161120608e00f120164", "details update from objects bytes");

    ok &= expect_true(build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(9u, "c", "d", "b", "a", message), "build prelaunch details update from objects");
    ok &= expect_eq_string(hex_string(message), "1a00008000000000120608de0f120163120608e00f120164120608df0f120162120508dd0f1200120608d40f120161198be767c5f5e76900320408031009", "prelaunch details update from objects bytes");

    DotaPracticeLobbyObjectOptions lobby_objects_options{};
    lobby_objects_options.extra_startup_account_id = 7u;
    lobby_objects_options.steam_id = 0x0102ull;
    lobby_objects_options.game_mode = 1u;
    lobby_objects_options.is_custom_game = true;
    lobby_objects_options.player_name = "p";
    lobby_objects_options.static_lobby_members.push_back(DotaStaticLobbyMember{0x0102ull, 7u, false});
    lobby_objects_options.lobby_members.push_back(DotaLobbyMemberObjectState{0x0102ull, 2u, 3u, 5u, true, 0u});
    lobby_objects_options.object_2004_options = object_2004_options;
    DotaPracticeLobbyObjects lobby_objects;
    ok &= expect_true(build_dota_practice_lobby_objects(lobby_objects_options, lobby_objects), "build practice lobby objects");
    ok &= expect_eq_string(hex_string(lobby_objects.object_2015), "0a00120908a545120408071200", "practice lobby object 2015 bytes");
    ok &= expect_eq_string(hex_string(lobby_objects.object_2016), "0a2a0902010000000000004800580161000000000000000068008501000000009801009801009801009801001500000000", "practice lobby object 2016 bytes");
    ok &= expect_eq_string(hex_string(lobby_objects.object_2004), hex_string(object_2004), "practice lobby object 2004 bytes");
    ok &= expect_eq_string(hex_string(lobby_objects.object_2014), "0a050a01701000", "practice lobby object 2014 bytes");

    std::string custom_modes_body;
    ok &= expect_true(build_dota_joinable_custom_game_modes_body(std::vector<DotaJoinableCustomGameMode>{DotaJoinableCustomGameMode{0u, 0u}, DotaJoinableCustomGameMode{7u, 0u}, DotaJoinableCustomGameMode{7u, 3u}, DotaJoinableCustomGameMode{8u, 2u}}, custom_modes_body) == 2u, "joinable custom game modes count");
    ok &= expect_eq_string(hex_string(custom_modes_body), "0a060807100118010a06080810011802", "joinable custom game modes body bytes");

    DotaJoinableCustomLobby joinable_lobby{};
    joinable_lobby.lobby_id = 0x0102ull;
    joinable_lobby.custom_game_id = 7u;
    joinable_lobby.display_name = "d";
    joinable_lobby.member_count = 2u;
    joinable_lobby.owner_account_id = 3u;
    joinable_lobby.owner_name = "o";
    joinable_lobby.map_name = "m";
    joinable_lobby.max_players = 4u;
    joinable_lobby.server_region = 5u;
    joinable_lobby.has_pass_key = true;
    joinable_lobby.lan_host_ping_location = "p";
    joinable_lobby.created_time = 6u;
    joinable_lobby.custom_game_timestamp = 8u;
    joinable_lobby.custom_game_crc = 0x0102030405060708ull;
    joinable_lobby.min_players = 1u;
    joinable_lobby.penalties = true;
    std::string custom_lobbies_body;
    ok &= expect_true(build_dota_joinable_custom_lobbies_body(std::vector<DotaJoinableCustomLobby>{joinable_lobby, joinable_lobby}, 7u, custom_lobbies_body) == 1u, "joinable custom lobbies count");
    ok &= expect_eq_string(hex_string(custom_lobbies_body), "0a3409020100000000000010071a01642002280332016f3a016d40044805580162017068067008790807060504030201800101880101", "joinable custom lobbies body bytes");

    std::string source_tv_body;
    build_dota_find_top_source_tv_games_body(nullptr, source_tv_body);
    ok &= expect_eq_string(hex_string(source_tv_body), "", "find top SourceTV empty body bytes");
    build_dota_find_top_source_tv_games_empty_body(source_tv_body);
    ok &= expect_eq_string(hex_string(source_tv_body), "2800", "find top SourceTV no games body bytes");

    DotaSourceTVGame source_tv_game{};
    source_tv_game.start_time = 1u;
    source_tv_game.server_id = 0x0102ull;
    source_tv_game.lobby_id = 0x0304ull;
    source_tv_game.game_time = 5u;
    source_tv_game.game_mode = 6u;
    source_tv_game.match_id = 0x0708ull;
    source_tv_game.players.push_back(DotaSourceTVPlayer{0u, 99u, 1u, 2u});
    source_tv_game.players.push_back(DotaSourceTVPlayer{7u, 8u, 9u, 10u});
    build_dota_find_top_source_tv_games_body(&source_tv_game, source_tv_body);
    ok &= expect_eq_string(hex_string(source_tv_body), "3a2008011882022084063001380540004800500660880eb20108080710081809200a2801", "find top SourceTV body bytes");

    std::string spectate_body;
    build_dota_spectate_friend_game_response_body(0x0102ull, spectate_body);
    ok &= expect_eq_string(hex_string(spectate_body), "210201000000000000", "spectate friend game body bytes");

    std::string watch_body;
    build_dota_watch_game_pending_response_body(watch_body);
    ok &= expect_eq_string(hex_string(watch_body), "0800", "watch game pending body bytes");
    build_dota_watch_game_ready_response_body(0x01020304u, 27020u, 0x0102ull, 0x0304ull, watch_body);
    ok &= expect_eq_string(hex_string(watch_body), "080110848688081884868808208cd301290201000000000000310201000000000000390403000000000000", "watch game ready body bytes");
    build_dota_watch_game_ready_response_body(0u, 27020u, 0x0102ull, 0x0304ull, watch_body);
    ok &= expect_eq_string(hex_string(watch_body), "0801208cd301290201000000000000310201000000000000390403000000000000", "watch game ready no addr body bytes");

    std::string claim_body;
    build_dota_claim_event_action_response_body(7u, claim_body);
    ok &= expect_eq_string(hex_string(claim_body), "0800120808001000180028071807", "claim event action response body bytes");
    build_dota_claim_event_action_using_item_response_body(7u, claim_body);
    ok &= expect_eq_string(hex_string(claim_body), "0a0e0800120808001000180028071807", "claim event action using item response body bytes");

    ok &= expect_true(build_dota_practice_lobby_response_payload(0x0102030405060708ull, true, message), "build practice lobby response job");
    ok &= expect_eq_string(hex_string(message), "8f1b0080090000005908070605040302010801", "practice lobby response job bytes");

    ok &= expect_true(build_dota_practice_lobby_response_payload(0ull, false, message), "build practice lobby response no job");
    ok &= expect_eq_string(hex_string(message), "8f1b0080000000000801", "practice lobby response no job bytes");

    ok &= expect_true(build_dota_practice_lobby_join_response_payload(false, 0ull, 2u, message), "build practice lobby join response");
    ok &= expect_eq_string(hex_string(message), "c91b0080000000000802", "practice lobby join response bytes");

    ok &= expect_true(build_dota_invitation_created_payload(0x0102ull, 0x0102030405060708ull, true, message), "build invitation created");
    ok &= expect_eq_string(hex_string(message), "96110080000000000882021108070605040302011801", "invitation created bytes");

    ok &= expect_true(build_dota_other_joined_channel_payload(0x0102030405060708ull, "nm", 0x1112131415161718ull, message), "build other joined channel");
    ok &= expect_eq_string(hex_string(message), "651b00800000000009080706050403020112026e6d1918171615141312112800", "other joined channel bytes");

    ok &= expect_true(build_dota_other_left_channel_payload(0x0102ull, 0x0102030405060708ull, message), "build other left channel");
    ok &= expect_eq_string(hex_string(message), "661b008000000000090201000000000000110807060504030201", "other left channel bytes");

    ok &= expect_eq_string(hex_string(build_dota_7034_leaver_state_payload(3u, 4u)), "080310041800200028003000", "7034 leaver state bytes");

    ok &= expect_true(build_dota_join_chat_channel_response_payload(0x0506ull, "chan", std::vector<DotaChatMember>{{0x0102ull, "p"}, {0ull, "skip"}, {0x0304ull, ""}}, 7u, message), "build join chat channel response");
    ok &= expect_eq_string(hex_string(message), "621b008000000000080012046368616e19060500000000000020c8012a0e09020100000000000012017018002a10090403000000000000120337373218003007380048005800", "join chat channel response bytes");

    Dota7034ExtraState extra_7034{};
    extra_7034.has_first_blood_happened = true;
    extra_7034.first_blood_happened = 1u;
    extra_7034.has_send_reason = true;
    extra_7034.send_reason = 9u;
    extra_7034.has_radiant_kills = true;
    extra_7034.radiant_kills = 10u;
    extra_7034.has_dire_kills = true;
    extra_7034.dire_kills = 11u;
    extra_7034.has_radiant_lead = true;
    extra_7034.radiant_lead = 12u;
    extra_7034.has_building_state = true;
    extra_7034.building_state = 13u;
    ok &= expect_true(build_dota_7034_connected_players_response_payload(std::vector<Dota7034Player>{{0x0102ull, 5u, 2u, 0u, 3u, 4u, true}}, std::vector<Dota7034Player>{{0x0304ull, 0u, 0u, 0u, 3u, 4u, false}}, {}, 4u, extra_7034, false, 0ull, message), "build 7034 connected players response");
    ok &= expect_eq_string(hex_string(message), "7a1b0080000000000a1b09020100000000000010051a0c080310041800200028003000200082010d090201000000000000100218003a190904030000000000001a0c0803100418002000280030002000100430014009580a600b700c780d", "7034 connected players response bytes");

    ok &= expect_true(build_dota_post_game_join_chat_channel_response_payload(0x0102ull, 0x0304ull, "chan", "p", message), "build post game join chat channel response");
    ok &= expect_eq_string(hex_string(message), "621b008000000000080012046368616e19040300000000000020c8012a0e090201000000000000120170180030123800400148005800", "post game join chat channel response bytes");

    const std::string chat_request = from_hex("080910051a017822026869");
    ok &= expect_true(build_dota_chat_message_payload(chat_request, 0x0102ull, 7u, "p", message), "build chat message");
    ok &= expect_eq_string(hex_string(message), "691c00800000000008071082021a017022026869", "chat message bytes");

    ok &= expect_true(build_dota_so_owner_cache_unsubscribed_payload(3u, 0x0102ull, message), "build so owner cache unsubscribed");
    ok &= expect_eq_string(hex_string(message), "190000800000000012050803108202", "so owner cache unsubscribed bytes");

    ok &= expect_true(build_dota_lobby_cache_unsubscribed_payload(0x0102ull, message), "build lobby cache unsubscribed");
    ok &= expect_eq_string(hex_string(message), "190000800000000012050803108202", "lobby cache unsubscribed bytes");

    ok &= expect_true(build_dota_lobby_cache_subscribed_up_to_date_payload(0x0102ull, message), "build lobby cache up to date");
    ok &= expect_eq_string(hex_string(message), "1d0000800000000012050803108202", "lobby cache up to date bytes");

    ok &= expect_true(build_dota_lobby_cache_subscribed_up_to_date_payload(0x0102ull, true, 0x0102030405060708ull, true, 9u, std::vector<std::uint32_t>{3u, 4u}, true, 0x1112131415161718ull, message), "build extended lobby cache up to date");
    ok &= expect_eq_string(hex_string(message), "1d0000800000000009080706050403020112050803108202180920032004291817161514131211", "extended lobby cache up to date bytes");

    ok &= expect_true(build_dota_remove_lobby_invite_payload(0x0102ull, 8ull, message), "build remove lobby invite");
    ok &= expect_eq_string(hex_string(message), "1a000080000000001955722b00000000002a0808db0f1203088202320408041008", "remove lobby invite bytes");

    ok &= expect_true(build_dota_practice_lobby_kicked_popup_payload(message), "build kicked popup");
    ok &= expect_eq_string(hex_string(message), "be1b0080000000000801", "kicked popup bytes");

    ok &= expect_true(build_dota_destroy_lobby_response_payload(0x0102030405060708ull, message), "build destroy lobby response");
    ok &= expect_eq_string(hex_string(message), "37200080090000005908070605040302010800", "destroy lobby response bytes");

    ok &= expect_true(build_dota_ready_up_status_payload(false, 0ull, 0x0102ull, 3u, 4u, message), "build ready up status");
    ok &= expect_eq_string(hex_string(message), "021c00800000000009020100000000000020033004", "ready up status bytes");

    ok &= expect_true(build_dota_7428_response_payload(false, 0ull, message), "build 7428 response");
    ok &= expect_eq_string(hex_string(message), "041d0080000000000a020800", "7428 response bytes");

    ok &= expect_true(build_dota_4524_response_payload(false, 0ull, message), "build 4524 response");
    ok &= expect_eq_string(hex_string(message), "ac110080000000000d0000803f", "4524 response bytes");

    ok &= expect_true(build_dota_7388_minimal_response_payload(5u, 7u, false, 0ull, message), "build 7388 minimal response");
    ok &= expect_eq_string(hex_string(message), "dc1c00800000000008e8071000180520e807280038074001", "7388 minimal response bytes");

    ok &= expect_true(build_dota_2582_lookup_account_name_response_payload(7u, "ab", true, 0x0102030405060708ull, message), "build 2582 lookup account name response");
    ok &= expect_eq_string(hex_string(message), "160a008009000000590807060504030201080712026162", "2582 lookup account name response bytes");

    std::string unlock_style_body;
    build_dota_unlock_item_style_response_body(0x0102ull, 3u, unlock_style_body);
    ok &= expect_eq_string(hex_string(unlock_style_body), "08001082021803", "unlock item style response body bytes");
    build_dota_unlock_item_style_response_body(0ull, 255u, unlock_style_body);
    ok &= expect_eq_string(hex_string(unlock_style_body), "0800", "unlock item style minimal response body bytes");

    std::string set_style_body;
    build_dota_set_item_style_response_body(set_style_body);
    ok &= expect_eq_string(hex_string(set_style_body), "0800", "set item style response body bytes");

    ok &= expect_true(build_dota_7504_response_payload(7u, false, 0ull, message), "build 7504 response");
    ok &= expect_eq_string(hex_string(message), "501d0080000000000a020807", "7504 response bytes");

    ok &= expect_true(build_dota_8096_response_payload(7u, false, 0ull, message), "build 8096 response");
    ok &= expect_eq_string(hex_string(message), "a01f00800000000008078801e05d9001e05da80100", "8096 response bytes");

    ok &= expect_true(build_dota_game_match_sign_out_response_payload(0x0102ull, 30u, 0x11223344u, false, 0ull, message), "build game match sign out response");
    ok &= expect_eq_string(hex_string(message), "611b008000000000088202154433221128003d000000004202081e4a02080052020800720208007a020800", "game match sign out response bytes");

    ok &= expect_true(build_dota_8880_response_payload(true, true, true, false, 0ull, message), "build 8880 response");
    ok &= expect_eq_string(hex_string(message), "b02200800000000008001000180020002800", "8880 response bytes");

    ok &= expect_true(build_dota_give_tip_response_payload(false, 0ull, message), "build give tip response");
    ok &= expect_eq_string(hex_string(message), "1b200080000000000800", "give tip response bytes");

    ok &= expect_true(build_dota_rank_request_response_payload(false, 0ull, message), "build rank request response");
    ok &= expect_eq_string(hex_string(message), "b022008000000000080010904e18904e20002800", "rank request response bytes");

    const std::string report_request = from_hex("080710031004");
    ok &= expect_true(build_dota_submit_player_report_response_v2_payload(reinterpret_cast<const std::uint8_t *>(report_request.data()), report_request.size(), false, 0ull, message), "build submit player report response v2");
    ok &= expect_eq_string(hex_string(message), "ab1b0080000000000807100310042801", "submit player report response v2 bytes");

    std::string store_purchase_body;
    build_dota_store_purchase_init_response_body(0x0102ull, store_purchase_body);
    ok &= expect_eq_string(hex_string(store_purchase_body), "0801108202", "store purchase init response body bytes");

    std::string crate_items_body;
    build_dota_crate_items_response_body(std::vector<std::uint32_t>{0u, 7u, 8u}, crate_items_body);
    ok &= expect_eq_string(hex_string(crate_items_body), "080010071008", "crate items response body bytes");

    std::string use_item_body;
    build_dota_use_item_response_body(true, use_item_body);
    ok &= expect_eq_string(hex_string(use_item_body), "0804", "use item response body bytes");
    build_dota_use_item_response_body(false, use_item_body);
    ok &= expect_eq_string(hex_string(use_item_body), "0800", "use item minimal response body bytes");

    std::string unlock_crate_body;
    build_dota_unlock_crate_response_body(std::vector<std::uint32_t>{0u, 7u, 8u}, unlock_crate_body);
    ok &= expect_eq_string(hex_string(unlock_crate_body), "08001202100712021008", "unlock crate response body bytes");

    std::string unpack_bundle_body;
    build_dota_unpack_bundle_response_body(std::vector<std::uint32_t>{0u, 7u, 8u}, unpack_bundle_body);
    ok &= expect_eq_string(hex_string(unpack_bundle_body), "100018071808", "unpack bundle response body bytes");

    std::string add_socket_body;
    build_dota_add_socket_response_body(0u, 0x0102ull, 3u, add_socket_body);
    ok &= expect_eq_string(hex_string(add_socket_body), "08001082021803", "add socket response body bytes");
    build_dota_add_socket_response_body(1u, 0ull, 0u, add_socket_body);
    ok &= expect_eq_string(hex_string(add_socket_body), "0801", "add socket minimal response body bytes");

    ok &= expect_true(build_dota_7451_batch_player_resources_response_payload(std::vector<std::uint32_t>{0u, 7u}, false, 0ull, message), "build 7451 batch player resources response");
    ok &= expect_eq_string(hex_string(message), "1b1d008000000000320e080730004805500570e05d78e05d", "7451 batch player resources response bytes");

    const std::string practice_entry = build_dota_practice_lobby_list_entry_body(0x0102ull, 7u, "p", "", 2u, 3u, true, 0u, 0u, "lan");
    ok &= expect_eq_string(hex_string(practice_entry), "0882022a0508071201703001380752054c6f62627960026801700180010a880103a201036c616ea80101b00100", "practice lobby list entry bytes");

    ok &= expect_true(build_dota_lobby_list_response_payload(std::vector<std::string>{practice_entry}, message), "build lobby list response");
    ok &= expect_eq_string(hex_string(message), "4c1f00800000000059ffffffffffffffff0a2d0882022a0508071201703001380752054c6f62627960026801700180010a880103a201036c616ea80101b00100", "lobby list response bytes");

    ok &= expect_true(build_dota_friend_practice_lobby_list_response_payload(practice_entry, true, message), "build friend lobby list response");
    ok &= expect_eq_string(hex_string(message), "c81b0080000000000a2d0882022a0508071201703001380752054c6f62627960026801700180010a880103a201036c616ea80101b00100", "friend lobby list response bytes");

    ok &= expect_true(build_dota_custom_game_info_response_payload(0x0102ull, false, 0ull, message), "build custom game info response");
    ok &= expect_eq_string(hex_string(message), "551f008000000000088202", "custom game info response bytes");

    const std::string custom_entry = build_dota_custom_lobby_list_entry_body(0x0102ull, 7u, "", false, "lan");
    ok &= expect_eq_string(hex_string(custom_entry), "0902010000000000002a0e0807120a4c6f62627920486f737430003807880100a201036c616e", "custom lobby list entry bytes");

    ok &= expect_true(build_dota_custom_lobby_list_response_payload(0x0102030405060708ull, std::vector<std::string>{custom_entry}, message), "build custom lobby list response");
    ok &= expect_eq_string(hex_string(message), "831b00800000000059080706050403020112260902010000000000002a0e0807120a4c6f62627920486f737430003807880100a201036c616e", "custom lobby list response bytes");

    const std::string session_field_raw(1, static_cast<char>(0x07));
    ok &= expect_true(build_wrapped_dota_replay_message(5453u, 570u, job_reply_message, session_field_raw, 0x0102030405060708ull, message), "build wrapped replay");
    ok &= expect_eq_string(hex_string(message), "4d1500800b000000090807060504030201100708ba04108fb78080081a128f1b0080090000005908070605040302017a", "wrapped replay bytes");

    if (!ok)
        return 1;

    std::cout << "gc_message_utils_test passed" << std::endl;
    return 0;
}

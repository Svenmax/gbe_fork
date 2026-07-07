#include "gc_replay_summary.h"

#include "../../dll/gbe_gc_message_utils.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace gbe::gc_replay {

namespace {

std::string trim(const std::string &value)
{
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end)
        return {};
    return std::string(begin, end);
}

bool parse_u32(const std::string &text, uint32_t &value)
{
    try {
        size_t consumed = 0;
        const unsigned long parsed = std::stoul(text, &consumed, 0);
        if (consumed != text.size() || parsed > UINT32_MAX)
            return false;
        value = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool is_hex_digit(char ch)
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

uint8_t hex_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return static_cast<uint8_t>(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return static_cast<uint8_t>(ch - 'a' + 10);
    return static_cast<uint8_t>(ch - 'A' + 10);
}

bool decode_hex(std::string hex, std::vector<uint8_t> &out)
{
    if (hex.rfind("hex=", 0) == 0)
        hex.erase(0, 4);
    if (hex.rfind("0x", 0) == 0 || hex.rfind("0X", 0) == 0)
        hex.erase(0, 2);

    std::string digits;
    digits.reserve(hex.size());
    for (char ch : hex) {
        if (is_hex_digit(ch)) {
            digits.push_back(ch);
        } else if (ch == '_' || ch == '-' || ch == ':') {
            continue;
        } else {
            return false;
        }
    }

    if ((digits.size() % 2) != 0)
        return false;

    out.clear();
    out.reserve(digits.size() / 2);
    for (size_t index = 0; index < digits.size(); index += 2) {
        out.push_back(static_cast<uint8_t>((hex_value(digits[index]) << 4) | hex_value(digits[index + 1])));
    }
    return true;
}

std::string format_hash(uint64_t hash)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

std::string format_field_value(const ProtoField &field, const std::vector<uint8_t> &bytes)
{
    std::ostringstream out;
    switch (field.wire_type) {
        case 0: {
            size_t offset = field.value_offset;
            uint64_t value = 0;
            if (gbe::proto_wire::read_varuint(bytes, offset, value))
                out << value;
            else
                out << "invalid";
            break;
        }
        case 1:
            out << "0x" << std::hex << std::setfill('0') << std::setw(16)
                << gbe::proto_wire::read_little_endian(bytes, field.value_offset, field.value_size);
            break;
        case 2:
            out << gbe::proto_wire::hex_prefix(bytes, field.value_offset, field.value_size, 8);
            break;
        case 5:
            out << "0x" << std::hex << std::setfill('0') << std::setw(8)
                << static_cast<uint32_t>(gbe::proto_wire::read_little_endian(bytes, field.value_offset, field.value_size));
            break;
        default:
            out << "unknown";
            break;
    }
    return out.str();
}

std::string format_fields(const std::vector<ProtoField> &fields, const std::vector<uint8_t> &bytes)
{
    if (fields.empty())
        return "none";

    std::ostringstream out;
    for (size_t index = 0; index < fields.size(); ++index) {
        if (index != 0)
            out << ',';
        out << fields[index].number << ':' << fields[index].wire_type << ':' << fields[index].value_size << '=' << format_field_value(fields[index], bytes);
    }
    return out.str();
}

std::vector<uint8_t> payload_body(const ReplayCase &replay_case)
{
    if (gbe::gc_message::has_proto_mask(replay_case.emsg) && replay_case.payload.size() >= 8u
        && gbe::proto_wire::read_little_endian(replay_case.payload, 0, 4) == replay_case.emsg) {
        return std::vector<uint8_t>(replay_case.payload.begin() + 8, replay_case.payload.end());
    }
    return replay_case.payload;
}

std::string json_escape(const std::string &value)
{
    std::ostringstream out;
    for (char ch : value) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(static_cast<unsigned char>(ch));
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

} // namespace

bool read_file(const std::string &path, std::string &contents)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

std::vector<ProtoField> parse_proto_fields(const std::vector<uint8_t> &bytes)
{
    return gbe::proto_wire::parse_fields(bytes);
}

uint64_t fnv1a64(const std::vector<uint8_t> &bytes)
{
    return gbe::proto_wire::fnv1a64(bytes);
}

bool parse_fixture(const std::string &path, std::vector<ReplayCase> &cases, std::string &error)
{
    std::ifstream file(path);
    if (!file) {
        error = "failed to open fixture: " + path;
        return false;
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream line_stream(line);
        std::string emsg_token;
        std::string hex_token;
        if (!(line_stream >> emsg_token >> hex_token)) {
            error = "invalid fixture line " + std::to_string(line_number) + ": expected <emsg> <hex_payload> [label]";
            return false;
        }

        ReplayCase replay_case{};
        if (!parse_u32(emsg_token, replay_case.emsg)) {
            error = "invalid emsg on line " + std::to_string(line_number) + ": " + emsg_token;
            return false;
        }
        if (!decode_hex(hex_token, replay_case.payload)) {
            error = "invalid hex payload on line " + std::to_string(line_number);
            return false;
        }

        std::string label;
        std::getline(line_stream, label);
        replay_case.label = trim(label);
        cases.push_back(std::move(replay_case));
    }
    return true;
}

std::string build_summary(const std::vector<ReplayCase> &cases)
{
    std::ostringstream out;
    out << "gc_replay_test summary v1\n";
    out << "cases=" << cases.size() << "\n";

    for (size_t index = 0; index < cases.size(); ++index) {
        const ReplayCase &replay_case = cases[index];
        const uint32_t masked_emsg = gbe::gc_message::without_proto_mask(replay_case.emsg);
        const bool proto = gbe::gc_message::has_proto_mask(replay_case.emsg);
        out << "case " << (index + 1) << "\n";
        out << "label=" << replay_case.label << "\n";
        out << "emsg=" << replay_case.emsg << "\n";
        out << "masked_emsg=" << masked_emsg << "\n";
        out << "proto=" << (proto ? 1 : 0) << "\n";
        out << "size=" << replay_case.payload.size() << "\n";
        out << "fnv1a64=" << format_hash(fnv1a64(replay_case.payload)) << "\n";
        const std::vector<uint8_t> body = payload_body(replay_case);
        out << "body_size=" << body.size() << "\n";
        out << "fields=" << format_fields(parse_proto_fields(body), body) << "\n";
    }

    return out.str();
}

std::string build_jsonl_summary(const std::vector<ReplayCase> &cases)
{
    std::ostringstream out;
    for (size_t index = 0; index < cases.size(); ++index) {
        const ReplayCase &replay_case = cases[index];
        const uint32_t masked_emsg = gbe::gc_message::without_proto_mask(replay_case.emsg);
        const bool proto = gbe::gc_message::has_proto_mask(replay_case.emsg);
        out << "{"
            << "\"case\":" << (index + 1)
            << ",\"label\":\"" << json_escape(replay_case.label) << "\""
            << ",\"emsg\":" << replay_case.emsg
            << ",\"masked_emsg\":" << masked_emsg
            << ",\"proto\":" << (proto ? 1 : 0)
            << ",\"size\":" << replay_case.payload.size()
            << ",\"fnv1a64\":\"" << format_hash(fnv1a64(replay_case.payload)) << "\""
            << ",\"body_size\":" << payload_body(replay_case).size()
            << ",\"fields\":\"" << json_escape(format_fields(parse_proto_fields(payload_body(replay_case)), payload_body(replay_case))) << "\""
            << "}\n";
    }
    return out.str();
}

} // namespace gbe::gc_replay

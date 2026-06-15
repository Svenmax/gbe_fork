#include "dll/gbe_proto_wire.h"

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

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool expect_eq_size(std::size_t actual, std::size_t expected, const char *label)
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
    using namespace gbe::proto_wire;

    bool ok = true;
    std::vector<std::uint8_t> varuint{0xac, 0x02};
    std::size_t offset = 0;
    std::uint64_t value = 0;
    std::size_t raw_size = 0;
    ok &= expect_true(read_varuint(varuint, offset, value, &raw_size), "read varuint");
    ok &= expect_eq_u64(value, 300u, "varuint value");
    ok &= expect_eq_size(raw_size, 2u, "varuint raw size");
    ok &= expect_eq_size(offset, 2u, "varuint offset");

    const std::vector<std::uint8_t> mixed{
        0x08, 0x01,
        0x11, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x1a, 0x03, 0x61, 0x62, 0x63,
        0x25, 0x0d, 0x0c, 0x0b, 0x0a,
    };
    const std::vector<Field> fields = parse_fields(mixed);
    ok &= expect_eq_size(fields.size(), 4u, "field count");
    if (fields.size() == 4u) {
        ok &= expect_eq_u64(fields[0].number, 1u, "field 1 number");
        ok &= expect_eq_u64(fields[0].wire_type, 0u, "field 1 wire");
        ok &= expect_eq_u64(fields[1].number, 2u, "field 2 number");
        ok &= expect_eq_u64(fields[1].wire_type, 1u, "field 2 wire");
        ok &= expect_eq_u64(fields[2].number, 3u, "field 3 number");
        ok &= expect_eq_u64(fields[2].wire_type, 2u, "field 3 wire");
        ok &= expect_eq_u64(fields[2].value_size, 3u, "field 3 size");
        ok &= expect_eq_u64(fields[3].number, 4u, "field 4 number");
        ok &= expect_eq_u64(fields[3].wire_type, 5u, "field 4 wire");
    }

    ok &= expect_eq_u64(read_little_endian(mixed, 3u, 8u), 0x0102030405060708ull, "fixed64 value");
    ok &= expect_eq_string(hex_prefix(mixed, 13u, 3u, 8u), "0x616263", "hex prefix full");
    ok &= expect_eq_string(hex_prefix(mixed, 0u, mixed.size(), 4u), "0x08011108...", "hex prefix truncated");

    Field field{};
    ok &= expect_true(find_field(mixed.data(), mixed.size(), 3u, field), "find field 3");
    ok &= expect_eq_u64(field.value_offset, 13u, "found field value offset");
    ok &= expect_eq_u64(field.value_size, 3u, "found field value size");
    std::string field_bytes;
    ok &= expect_true(read_field_bytes(mixed.data(), mixed.size(), field, field_bytes), "read field bytes");
    ok &= expect_eq_string(field_bytes, "abc", "field bytes value");

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 1u, field), "find field 1");
    ok &= expect_true(read_field_uint64(mixed.data(), mixed.size(), field, value), "read field uint64 varint");
    ok &= expect_eq_u64(value, 1u, "field uint64 varint value");

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 2u, field), "find field 2");
    ok &= expect_true(read_field_uint64(mixed.data(), mixed.size(), field, value), "read field uint64 fixed64");
    ok &= expect_eq_u64(value, 0x0102030405060708ull, "field uint64 fixed64 value");

    ok &= expect_true(find_field(mixed.data(), mixed.size(), 4u, field), "find field 4");
    std::uint32_t value32 = 0;
    ok &= expect_true(read_field_uint32(mixed.data(), mixed.size(), field, value32), "read field uint32 fixed32");
    ok &= expect_eq_u64(value32, 0x0a0b0c0du, "field uint32 fixed32 value");

    offset = 0;
    std::size_t field_offset = 0;
    std::size_t field_end = 0;
    ok &= expect_true(read_next_field(mixed.data(), mixed.size(), offset, field, &field_offset, &field_end), "read next field");
    ok &= expect_eq_u64(field_offset, 0u, "next field offset");
    ok &= expect_eq_u64(field_end, 2u, "next field end");

    std::string encoded;
    append_varuint(encoded, 300u);
    ok &= expect_eq_string(hex_string(encoded), "ac02", "append varuint");

    encoded.clear();
    append_varint_field(encoded, 1u, 1u);
    append_fixed64_field(encoded, 2u, 0x0102030405060708ull);
    append_bytes_field(encoded, 3u, "abc");
    append_fixed32_field(encoded, 4u, 0x0a0b0c0du);
    ok &= expect_eq_string(hex_string(encoded), "08011108070605040302011a03616263250d0c0b0a", "append fields");

    if (!ok)
        return 1;

    std::cout << "gbe_proto_wire_test passed" << std::endl;
    return 0;
}

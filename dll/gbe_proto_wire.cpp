#include "gbe_proto_wire.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace gbe::proto_wire {

bool read_varuint(const std::vector<std::uint8_t> &bytes, std::size_t &offset, std::uint64_t &value, std::size_t *raw_size)
{
    value = 0;
    const std::size_t begin = offset;
    for (std::uint32_t shift = 0; shift < 64 && offset < bytes.size(); shift += 7) {
        const std::uint8_t byte = bytes[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            if (raw_size)
                *raw_size = offset - begin;
            return true;
        }
    }
    return false;
}

bool read_varuint(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint64_t &value, std::size_t *raw_begin, std::size_t *raw_end)
{
    if (!data || offset >= size)
        return false;

    const std::size_t begin = offset;
    value = 0;
    for (std::uint32_t shift = 0; shift < 64 && offset < size; shift += 7) {
        const std::uint8_t byte = data[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            if (raw_begin)
                *raw_begin = begin;
            if (raw_end)
                *raw_end = offset;
            return true;
        }
    }
    return false;
}

std::uint64_t read_little_endian(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < size && offset + index < bytes.size() && index < sizeof(value); ++index)
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
    return value;
}

std::vector<Field> parse_fields(const std::vector<std::uint8_t> &bytes)
{
    std::vector<Field> fields;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        Field field{};
        if (!read_next_field(bytes.data(), bytes.size(), offset, field))
            return fields;
        fields.push_back(field);
    }
    return fields;
}

bool read_next_field(const std::uint8_t *data, std::size_t size, std::size_t &offset, Field &field, std::size_t *field_offset, std::size_t *field_end)
{
    if (!data || offset >= size)
        return false;

    const std::size_t start = offset;
    std::uint64_t tag = 0;
    if (!read_varuint(data, size, offset, tag) || tag == 0)
        return false;

    field = {};
    field.number = static_cast<std::uint32_t>(tag >> 3);
    field.wire_type = static_cast<std::uint32_t>(tag & 0x07);
    field.value_offset = offset;

    switch (field.wire_type) {
        case 0: {
            std::size_t raw_begin = offset;
            std::size_t raw_end = offset;
            std::uint64_t ignored = 0;
            if (!read_varuint(data, size, offset, ignored, &raw_begin, &raw_end))
                return false;
            field.value_offset = raw_begin;
            field.value_size = raw_end - raw_begin;
            break;
        }
        case 1:
            if (size - offset < 8)
                return false;
            field.value_size = 8;
            offset += 8;
            break;
        case 2: {
            std::uint64_t length = 0;
            if (!read_varuint(data, size, offset, length))
                return false;
            if (length > size - offset)
                return false;
            field.value_offset = offset;
            field.value_size = static_cast<std::size_t>(length);
            offset += static_cast<std::size_t>(length);
            break;
        }
        case 5:
            if (size - offset < 4)
                return false;
            field.value_size = 4;
            offset += 4;
            break;
        default:
            return false;
    }

    if (field_offset)
        *field_offset = start;
    if (field_end)
        *field_end = offset;
    return true;
}

bool find_field(const std::uint8_t *data, std::size_t size, std::uint32_t wanted_field, Field &field)
{
    std::size_t offset = 0;
    while (offset < size) {
        Field current{};
        if (!read_next_field(data, size, offset, current))
            return false;
        if (current.number == wanted_field) {
            field = current;
            return true;
        }
    }
    return false;
}

bool read_field_uint64(const std::uint8_t *data, std::size_t size, const Field &field, std::uint64_t &value)
{
    if (!data || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    if (field.wire_type == 0) {
        std::size_t offset = field.value_offset;
        return read_varuint(data, size, offset, value);
    }

    if (field.wire_type == 1 && field.value_size == sizeof(value)) {
        std::memcpy(&value, data + field.value_offset, sizeof(value));
        return true;
    }

    return false;
}

bool read_field_uint32(const std::uint8_t *data, std::size_t size, const Field &field, std::uint32_t &value)
{
    if (!data || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    if (field.wire_type == 0) {
        std::uint64_t wide_value = 0;
        if (!read_field_uint64(data, size, field, wide_value))
            return false;
        value = static_cast<std::uint32_t>(wide_value > 0xffffffffull ? 0xffffffffu : wide_value);
        return true;
    }

    if (field.wire_type == 5 && field.value_size == sizeof(value)) {
        std::memcpy(&value, data + field.value_offset, sizeof(value));
        return true;
    }

    return false;
}

bool read_field_bytes(const std::uint8_t *data, std::size_t size, const Field &field, std::string &value)
{
    if (!data || field.wire_type != 2 || field.value_offset > size || field.value_size > size - field.value_offset)
        return false;

    value.assign(reinterpret_cast<const char *>(data + field.value_offset), field.value_size);
    return true;
}

void append_varuint(std::string &buffer, std::uint64_t value)
{
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7f);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buffer.push_back(static_cast<char>(byte));
    } while (value != 0);
}

void append_little_endian32(std::string &buffer, std::uint32_t value)
{
    for (int index = 0; index < 4; ++index)
        buffer.push_back(static_cast<char>((value >> (index * 8)) & 0xffu));
}

void append_little_endian64(std::string &buffer, std::uint64_t value)
{
    for (int index = 0; index < 8; ++index)
        buffer.push_back(static_cast<char>((value >> (index * 8)) & 0xffu));
}

void append_varint_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 0u);
    append_varuint(buffer, value);
}

void append_fixed64_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 1u);
    append_little_endian64(buffer, value);
}

void append_bytes_field(std::string &buffer, std::uint32_t field_number, const std::string &value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 2u);
    append_varuint(buffer, static_cast<std::uint64_t>(value.size()));
    buffer.append(value);
}

void append_fixed32_field(std::string &buffer, std::uint32_t field_number, std::uint32_t value)
{
    append_varuint(buffer, (static_cast<std::uint64_t>(field_number) << 3) | 5u);
    append_little_endian32(buffer, value);
}

std::uint64_t fnv1a64(const std::vector<std::uint8_t> &bytes)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex_prefix(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size, std::size_t max_bytes)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setfill('0');
    const std::size_t end = std::min(bytes.size(), offset + std::min(size, max_bytes));
    for (std::size_t index = offset; index < end; ++index)
        out << std::setw(2) << static_cast<unsigned>(bytes[index]);
    if (size > max_bytes)
        out << "...";
    return out.str();
}

} // namespace gbe::proto_wire

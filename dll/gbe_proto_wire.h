#ifndef GBE_PROTO_WIRE_H
#define GBE_PROTO_WIRE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::proto_wire {

struct Field {
    std::uint32_t number{};
    std::uint32_t wire_type{};
    std::size_t value_offset{};
    std::size_t value_size{};
};

bool read_varuint(const std::vector<std::uint8_t> &bytes, std::size_t &offset, std::uint64_t &value, std::size_t *raw_size = nullptr);
bool read_varuint(const std::uint8_t *data, std::size_t size, std::size_t &offset, std::uint64_t &value, std::size_t *raw_begin = nullptr, std::size_t *raw_end = nullptr);
std::uint64_t read_little_endian(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size);
std::vector<Field> parse_fields(const std::vector<std::uint8_t> &bytes);
bool read_next_field(const std::uint8_t *data, std::size_t size, std::size_t &offset, Field &field, std::size_t *field_offset = nullptr, std::size_t *field_end = nullptr);
bool find_field(const std::uint8_t *data, std::size_t size, std::uint32_t wanted_field, Field &field);
bool read_field_uint64(const std::uint8_t *data, std::size_t size, const Field &field, std::uint64_t &value);
bool read_field_uint32(const std::uint8_t *data, std::size_t size, const Field &field, std::uint32_t &value);
bool read_field_bytes(const std::uint8_t *data, std::size_t size, const Field &field, std::string &value);
void append_varuint(std::string &buffer, std::uint64_t value);
void append_little_endian32(std::string &buffer, std::uint32_t value);
void append_little_endian64(std::string &buffer, std::uint64_t value);
void append_varint_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value);
void append_fixed64_field(std::string &buffer, std::uint32_t field_number, std::uint64_t value);
void append_bytes_field(std::string &buffer, std::uint32_t field_number, const std::string &value);
void append_fixed32_field(std::string &buffer, std::uint32_t field_number, std::uint32_t value);
std::uint64_t fnv1a64(const std::vector<std::uint8_t> &bytes);
std::string hex_prefix(const std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t size, std::size_t max_bytes);

} // namespace gbe::proto_wire

#endif // GBE_PROTO_WIRE_H

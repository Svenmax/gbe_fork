#ifndef GBE_TOOLS_GC_REPLAY_SUMMARY_H
#define GBE_TOOLS_GC_REPLAY_SUMMARY_H

#include <cstdint>
#include <string>
#include <vector>

#include "../../dll/gbe_proto_wire.h"

namespace gbe::gc_replay {

struct ReplayCase {
    uint32_t emsg{};
    std::vector<uint8_t> payload;
    std::string label;
};

using ProtoField = gbe::proto_wire::Field;

bool read_file(const std::string &path, std::string &contents);
bool parse_fixture(const std::string &path, std::vector<ReplayCase> &cases, std::string &error);
std::string build_summary(const std::vector<ReplayCase> &cases);
std::string build_jsonl_summary(const std::vector<ReplayCase> &cases);
std::vector<ProtoField> parse_proto_fields(const std::vector<uint8_t> &bytes);
uint64_t fnv1a64(const std::vector<uint8_t> &bytes);

} // namespace gbe::gc_replay

#endif // GBE_TOOLS_GC_REPLAY_SUMMARY_H

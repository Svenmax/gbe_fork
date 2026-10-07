#include "gc_replay_summary.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char *program)
{
    std::cerr << "usage: " << program << " <fixture.txt> [--format text|jsonl] [--expect <expected-summary.txt>]" << std::endl;
    std::cerr << "fixture line: <emsg> <hex_payload> [label]" << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string fixture_path = argv[1];
    std::string expect_path;
    std::string format = "text";

    for (int index = 2; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--format") {
            if (++index >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            format = argv[index];
            if (format != "text" && format != "jsonl") {
                std::cerr << "unsupported format: " << format << std::endl;
                return 1;
            }
        } else if (arg == "--expect") {
            if (++index >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            expect_path = argv[index];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    std::vector<gbe::gc_replay::ReplayCase> cases;
    std::string error;
    if (!gbe::gc_replay::parse_fixture(fixture_path, cases, error)) {
        std::cerr << error << std::endl;
        return 1;
    }

    const std::string summary = format == "jsonl"
        ? gbe::gc_replay::build_jsonl_summary(cases)
        : gbe::gc_replay::build_summary(cases);
    if (expect_path.empty()) {
        std::cout << summary;
        return 0;
    }

    std::string expected;
    if (!gbe::gc_replay::read_file(expect_path, expected)) {
        std::cerr << "failed to open expected summary: " << expect_path << std::endl;
        return 1;
    }

    if (summary != expected) {
        std::cerr << "summary mismatch" << std::endl;
        std::cout << summary;
        return 2;
    }

    std::cout << "summary matches " << expect_path << std::endl;
    return 0;
}

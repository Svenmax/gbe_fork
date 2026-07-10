#ifndef __INCLUDED_GBE_DOTA_LOBBY_GENERATION_H__
#define __INCLUDED_GBE_DOTA_LOBBY_GENERATION_H__

#include <cstdint>
#include <limits>

namespace gbe::dota_lobby_generation {

struct Generation {
    std::uint64_t value{};

    constexpr bool assigned() const { return value != 0u; }
};

constexpr bool operator==(Generation lhs, Generation rhs)
{
    return lhs.value == rhs.value;
}

constexpr bool operator!=(Generation lhs, Generation rhs)
{
    return !(lhs == rhs);
}

enum class Boundary : std::uint8_t {
    Create,
    Join,
    Leave,
    Reset,
    Recover,
};

struct AdvanceResult {
    Boundary boundary{Boundary::Create};
    Generation previous;
    Generation current;
    bool advanced{};
};

class Counter {
public:
    constexpr Counter() = default;
    explicit constexpr Counter(Generation initial) : current_(initial) {}

    constexpr Generation current() const { return current_; }

    constexpr AdvanceResult advance(Boundary boundary)
    {
        AdvanceResult result{boundary, current_, current_, false};
        if (current_.value == std::numeric_limits<std::uint64_t>::max())
            return result;

        ++current_.value;
        result.current = current_;
        result.advanced = true;
        return result;
    }

private:
    Generation current_;
};

constexpr bool is_newer(Generation candidate, Generation reference)
{
    return candidate.value > reference.value;
}

} // namespace gbe::dota_lobby_generation

#endif // __INCLUDED_GBE_DOTA_LOBBY_GENERATION_H__

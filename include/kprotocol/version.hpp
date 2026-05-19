#pragma once

#include <cstdint>

namespace kprotocol {

enum class ProtocolVersion : std::int32_t {
    v1_8 = 47,
    v1_12_2 = 340,
    v1_16_5 = 754,
    v1_20_4 = 765,
    v1_21_1 = 767
};

constexpr std::int32_t protocol_number(const ProtocolVersion version) noexcept {
    return static_cast<std::int32_t>(version);
}

} // namespace kprotocol

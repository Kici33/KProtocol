#pragma once

#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#include <optional>
#include <string>

namespace kprotocol {

// Lookup default block state ids by block name (from embedded minecraft-data).
class BlockRegistry {
public:
    [[nodiscard]] static std::optional<std::int32_t> default_state_id(
        ProtocolVersion version,
        std::string_view block_name);

    [[nodiscard]] static BlockState default_state(
        ProtocolVersion version,
        std::string_view block_name);

    [[nodiscard]] static BlockState translate(
        const BlockState& state,
        ProtocolVersion from,
        ProtocolVersion to);
};

} // namespace kprotocol

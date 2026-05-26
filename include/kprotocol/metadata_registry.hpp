#pragma once

#include "kprotocol/version.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace kprotocol {

// Per-entity metadata key -> wire index (embedded from minecraft-data 1.21.1).
class MetadataRegistry {
public:
    [[nodiscard]] static std::optional<std::uint8_t> index_for(
        std::string_view entity_name,
        std::string_view metadata_key);
};

} // namespace kprotocol

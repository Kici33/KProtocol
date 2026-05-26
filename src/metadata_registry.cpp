#include "kprotocol/metadata_registry.hpp"

#include <string>
#include <unordered_map>

namespace kprotocol::detail {

const std::unordered_map<std::string, std::unordered_map<std::string, std::uint8_t>>&
embedded_entity_metadata_indices();

} // namespace kprotocol::detail

namespace kprotocol {

std::optional<std::uint8_t> MetadataRegistry::index_for(
    const std::string_view entity_name,
    const std::string_view metadata_key) {

    const auto& all = detail::embedded_entity_metadata_indices();
    const auto entity_it = all.find(std::string(entity_name));
    if (entity_it == all.end()) {
        return std::nullopt;
    }
    const auto key_it = entity_it->second.find(std::string(metadata_key));
    if (key_it == entity_it->second.end()) {
        return std::nullopt;
    }
    return key_it->second;
}

} // namespace kprotocol

#include "kprotocol/block_registry.hpp"

#include "kprotocol/translation_registry.hpp"

#include <string>
#include <unordered_map>

namespace kprotocol::detail {

const std::unordered_map<std::string, std::unordered_map<std::string, std::int32_t>>&
embedded_block_default_states();

} // namespace kprotocol::detail

namespace kprotocol {

namespace {

std::string version_key(ProtocolVersion version) {
    return name_of(version);
}

const std::unordered_map<std::string, std::int32_t>* states_for(ProtocolVersion version) {
    const auto& all = detail::embedded_block_default_states();
    const auto it = all.find(version_key(version));
    if (it == all.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace

std::optional<std::int32_t> BlockRegistry::default_state_id(
    const ProtocolVersion version,
    const std::string_view block_name) {

    const auto* states = states_for(version);
    if (states == nullptr) {
        return std::nullopt;
    }
    const auto it = states->find(std::string(block_name));
    if (it == states->end()) {
        return std::nullopt;
    }
    return it->second;
}

BlockState BlockRegistry::default_state(const ProtocolVersion version, const std::string_view block_name) {
    return BlockState{default_state_id(version, block_name).value_or(0)};
}

BlockState BlockRegistry::translate(
    const BlockState& state,
    const ProtocolVersion from,
    const ProtocolVersion to) {

    return BlockState{TranslationRegistry::map_block_id(from, to, state.id)};
}

} // namespace kprotocol

#include "kprotocol/translation_registry.hpp"
#include "kprotocol/version.hpp"

#include <unordered_map>

namespace kprotocol {

// Initialize translation registry with all version mappings
void TranslationRegistry::initialize_all(PacketTranslator& translator) {
    // This function will register packet translation rules
    // For now, translation happens at the codec level
}

// Block ID mapping helper
std::int32_t TranslationRegistry::map_block_id(
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {
    
    if (from == to) {
        return source_id;
    }
    
    // For now, perform identity mapping (no translation)
    // TODO: Implement actual ID mapping using extracted mappings
    return source_id;
}

// Item ID mapping helper
std::int32_t TranslationRegistry::map_item_id(
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {
    
    return source_id;
}

// Entity type ID mapping helper
std::int32_t TranslationRegistry::map_entity_id(
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {
    
    return source_id;
}

// Particle ID mapping helper
std::int32_t TranslationRegistry::map_particle_id(
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {
    
    return source_id;
}

std::string TranslationRegistry::mapping_key(ProtocolVersion from, ProtocolVersion to) {
    return std::to_string(static_cast<int>(from)) + "_to_" + std::to_string(static_cast<int>(to));
}

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> 
    TranslationRegistry::block_mappings_;
    
std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> 
    TranslationRegistry::item_mappings_;
    
std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> 
    TranslationRegistry::entity_mappings_;
    
std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> 
    TranslationRegistry::particle_mappings_;

bool TranslationRegistry::initialized_ = false;

} // namespace kprotocol

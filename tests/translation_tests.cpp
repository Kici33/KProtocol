#include "kprotocol/translation.hpp"
#include "kprotocol/translation_registry.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

#include <cassert>
#include <iostream>

/**
 * Test suite for version translation functionality.
 * Verifies that packets can be translated between different Minecraft versions.
 */

int main() {
    std::cout << "Testing version translation infrastructure...\n";
    
    // Test 1: PacketTranslator basic registration and retrieval
    {
        std::cout << "Test 1: Packet translator registration...\n";
        
        kprotocol::PacketTranslator translator;
        
        // Register a simple test translation
        translator.register_translation(
            "TestPacket",
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            [](const kprotocol::PacketFields& fields) -> kprotocol::PacketFields {
                kprotocol::PacketFields result = fields;
                // Map block ID from 1.20.4 to 1.16.5
                if (const auto* block_id = std::get_if<std::int32_t>(&fields.at("block_id"))) {
                    result["block_id"] = *block_id + 100; // Simple test transformation
                }
                return result;
            }
        );
        
        // Create test packet
        kprotocol::PacketFields fields;
        fields["block_id"] = static_cast<std::int32_t>(42);
        
        kprotocol::Packet packet{
            "TestPacket",
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound,
            fields
        };
        
        // Translate packet
        const auto translated = translator.translate(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5
        );
        
        assert(translated.key == "TestPacket");
        assert(std::get<std::int32_t>(translated.fields.at("block_id")) == 142);
        
        std::cout << "  ✓ Packet translation works correctly\n";
    }
    
    // Test 2: Identity translation (same version)
    {
        std::cout << "Test 2: Identity translation (same version)...\n";
        
        kprotocol::PacketTranslator translator;
        
        kprotocol::PacketFields fields;
        fields["block_id"] = static_cast<std::int32_t>(42);
        
        kprotocol::Packet packet{
            "TestPacket",
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound,
            fields
        };
        
        // Translate to same version (should be identity)
        const auto translated = translator.translate(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_20_4
        );
        
        assert(translated.key == "TestPacket");
        assert(std::get<std::int32_t>(translated.fields.at("block_id")) == 42);
        
        std::cout << "  ✓ Identity translation works\n";
    }
    
    // Test 3: No translation registered (fallback)
    {
        std::cout << "Test 3: Fallback for unregistered translation...\n";
        
        kprotocol::PacketTranslator translator;
        
        kprotocol::PacketFields fields;
        fields["block_id"] = static_cast<std::int32_t>(42);
        
        kprotocol::Packet packet{
            "UnregisteredPacket",
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound,
            fields
        };
        
        // Try to translate unregistered packet (should return unchanged)
        const auto translated = translator.translate(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5
        );
        
        assert(translated.key == "UnregisteredPacket");
        assert(std::get<std::int32_t>(translated.fields.at("block_id")) == 42);
        
        std::cout << "  ✓ Fallback for unregistered packet works\n";
    }
    
    // Test 4: TranslationRegistry ID mapping functions
    {
        std::cout << "Test 4: TranslationRegistry ID mapping functions...\n";
        
        // Test block ID mapping (identity for now, will be populated later)
        const auto mapped = kprotocol::TranslationRegistry::map_block_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            42
        );
        assert(mapped == 42); // Currently identity mapping
        
        // Test item ID mapping
        const auto item_mapped = kprotocol::TranslationRegistry::map_item_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            10
        );
        assert(item_mapped == 10); // Currently identity mapping
        
        // Test entity ID mapping
        const auto entity_mapped = kprotocol::TranslationRegistry::map_entity_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            5
        );
        assert(entity_mapped == 5); // Currently identity mapping
        
        // Test particle ID mapping
        const auto particle_mapped = kprotocol::TranslationRegistry::map_particle_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            3
        );
        assert(particle_mapped == 3); // Currently identity mapping
        
        std::cout << "  ✓ TranslationRegistry ID mapping functions work\n";
    }
    
    // Test 5: Initialize translation registry
    {
        std::cout << "Test 5: Initialize translation registry...\n";
        
        kprotocol::PacketTranslator translator;
        
        // Initialize should not throw
        kprotocol::TranslationRegistry::initialize_all(translator);
        
        // Calling again should be idempotent
        kprotocol::TranslationRegistry::initialize_all(translator);
        
        std::cout << "  ✓ TranslationRegistry initialization works\n";
    }
    
    std::cout << "\n✅ All translation tests passed!\n";
    return 0;
}

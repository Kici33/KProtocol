#include "kprotocol/translation.hpp"
#include "kprotocol/translation_registry.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

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
        
        kprotocol::Packet packet;
        packet.key = "TestPacket";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields = fields;
        
        // Translate packet
        const auto checked = translator.translate_checked(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5
        );
        const auto translated = checked.packet;
        
        assert(checked.status == kprotocol::PacketTranslator::Status::applied);
        assert(translated.key == "TestPacket");
        assert(std::get<std::int32_t>(translated.fields.at("block_id")) == 142);
        assert(translator.has_translation(
            "TestPacket",
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5));
        
        std::cout << "  ✓ Packet translation works correctly\n";
    }
    
    // Test 2: Identity translation (same version)
    {
        std::cout << "Test 2: Identity translation (same version)...\n";
        
        kprotocol::PacketTranslator translator;
        
        kprotocol::PacketFields fields;
        fields["block_id"] = static_cast<std::int32_t>(42);
        
        kprotocol::Packet packet;
        packet.key = "TestPacket";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields = fields;
        
        // Translate to same version (should be identity)
        const auto checked = translator.translate_checked(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_20_4
        );
        const auto translated = checked.packet;
        
        assert(checked.status == kprotocol::PacketTranslator::Status::identity);
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
        
        kprotocol::Packet packet;
        packet.key = "UnregisteredPacket";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields = fields;
        
        // Try to translate unregistered packet (should return unchanged, but
        // report that a rule was missing).
        const auto checked = translator.translate_checked(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5
        );
        const auto translated = checked.packet;
        
        assert(checked.status == kprotocol::PacketTranslator::Status::missing_packet_key);
        assert(translated.key == "UnregisteredPacket");
        assert(std::get<std::int32_t>(translated.fields.at("block_id")) == 42);
        
        std::cout << "  ✓ Fallback for unregistered packet works\n";
    }
    
    // Test 4: TranslationRegistry ID mapping functions
    {
        std::cout << "Test 4: TranslationRegistry ID mapping functions...\n";

        kprotocol::PacketTranslator bootstrap;
        kprotocol::TranslationRegistry::initialize_all(bootstrap);

        const auto block_mapped = kprotocol::TranslationRegistry::map_block_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5,
            21
        );
        assert(block_mapped == 20); // dark_oak_planks default state

        const auto cross_era = kprotocol::TranslationRegistry::map_block_id(
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_21_1,
            16
        );
        assert(cross_era == 1); // stone default state on 1.8 -> 1.21.1

        std::cout << "  ✓ TranslationRegistry ID mapping functions work\n";
    }
    
    // Test 5: Initialize translation registry
    {
        std::cout << "Test 5: Initialize translation registry...\n";
        
        kprotocol::PacketTranslator translator;
        
        kprotocol::TranslationRegistry::initialize_all(translator);
        const auto count_after_first = translator.registered_count();
        kprotocol::TranslationRegistry::initialize_all(translator);
        assert(translator.registered_count() == count_after_first);
        
        std::cout << "  ✓ TranslationRegistry initialization works\n";
    }

    // Test 6: Cross-version block_change roundtrip via registered rule
    {
        std::cout << "Test 6: block_change cross-version translation...\n";

        kprotocol::PacketTranslator translator;
        kprotocol::TranslationRegistry::initialize_all(translator);

        kprotocol::PacketFields fields;
        fields["location"] = kprotocol::Position{1, 64, 2};
        fields["type"] = static_cast<std::int32_t>(21);

        kprotocol::Packet packet;
        packet.key = "play.clientbound.block_change";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields = fields;

        const auto translated = translator.translate(
            packet,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5
        );

        assert(std::get<std::int32_t>(translated.fields.at("type")) == 20);
        assert(std::get<kprotocol::Position>(translated.fields.at("location")).x == 1);

        const auto roundtrip = translator.translate(
            translated,
            kprotocol::ProtocolVersion::v1_16_5,
            kprotocol::ProtocolVersion::v1_20_4
        );
        assert(std::get<std::int32_t>(roundtrip.fields.at("type")) == 21);

        std::cout << "  ✓ block_change cross-version roundtrip works\n";
    }

    // Test 7: initialize_all must populate every translator, not only the
    // first translator created during process lifetime.
    {
        std::cout << "Test 7: TranslationRegistry initializes independent translators...\n";

        kprotocol::PacketTranslator first;
        kprotocol::PacketTranslator second;
        kprotocol::TranslationRegistry::initialize_all(first);
        kprotocol::TranslationRegistry::initialize_all(second);

        assert(first.registered_count() > 0);
        assert(second.registered_count() == first.registered_count());
        assert(second.has_translation(
            "play.clientbound.block_change",
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_16_5));

        std::cout << "  ✓ independent translator initialization works\n";
    }

    // Test 8: strict mode makes missing semantic translation visible.
    {
        std::cout << "Test 8: strict missing-translation diagnostics...\n";

        kprotocol::PacketTranslator translator;
        translator.set_require_explicit_translation(true);

        kprotocol::PacketFields fields;
        fields["value"] = static_cast<std::int32_t>(7);

        kprotocol::Packet packet;
        packet.key = "play.clientbound.untranslated";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields = fields;

        bool threw = false;
        try {
            (void)translator.translate_checked(
                packet,
                kprotocol::ProtocolVersion::v1_20_4,
                kprotocol::ProtocolVersion::v1_16_5);
        } catch (const kprotocol::MissingTranslationError&) {
            threw = true;
        }
        assert(threw);

        std::cout << "  ✓ strict diagnostics work\n";
    }
    
    std::cout << "\n✅ All translation tests passed!\n";
    return 0;
}

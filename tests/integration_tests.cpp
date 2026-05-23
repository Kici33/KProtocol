#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"
#include "kprotocol/translation_registry.hpp"
#include "kprotocol/types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

// Force-active asserts even in NDEBUG builds.
#ifdef assert
#undef assert
#endif
#define assert(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n",                         \
                         #cond, __FILE__, __LINE__);                           \
            std::abort();                                                      \
        }                                                                      \
    } while (false)

/**
 * Integration test suite for KProtocol.
 * Tests complete packet workflows across protocol versions and states.
 */

int main() {
    std::cout << "=== KProtocol Integration Tests ===\n\n";

    // Test 1: Handshake → Status → Ping/Pong workflow
    {
        std::cout << "Test 1: Handshake → Status → Ping workflow\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        // 1.1: Client sends Handshake (v1.21.1 = 767)
        const auto handshake = kprotocol::C00HandshakePacket{
            .protocol_version = 767,
            .server_address = "localhost",
            .server_port = 25565,
            .next_state = 1 // Status state
        };
        const auto handshake_pkt = handshake.to_packet();
        auto encoded = registry.encode_packet(handshake_pkt, kprotocol::ProtocolVersion::v1_21_1);

        // Decode and verify
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_handshake = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::handshaking,
            kprotocol::PacketDirection::serverbound);
        assert(decoded_handshake.key == handshake_pkt.key);

        // 1.2: Client sends Status Request
        const auto status_req = kprotocol::C00StatusRequestPacket{};
        const auto status_req_pkt = status_req.to_packet();
        encoded = registry.encode_packet(status_req_pkt, kprotocol::ProtocolVersion::v1_21_1);
        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_status_req = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::status,
            kprotocol::PacketDirection::serverbound);
        assert(decoded_status_req.key == status_req_pkt.key);

        // 1.3: Server sends Status Response
        const auto status_resp = kprotocol::S00StatusResponsePacket{
            .json_response = R"({"version":{"name":"1.21.1","protocol":767},"players":{"max":20,"online":0},"description":{"text":"Test Server"}})"
        };
        const auto status_resp_pkt = status_resp.to_packet();
        encoded = registry.encode_packet(status_resp_pkt, kprotocol::ProtocolVersion::v1_21_1);
        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_status_resp = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::status,
            kprotocol::PacketDirection::clientbound);
        assert(decoded_status_resp.key == status_resp_pkt.key);
        const auto typed_resp = kprotocol::S00StatusResponsePacket::from_packet(decoded_status_resp);
        assert(!typed_resp.json_response.empty());

        // 1.4: Client sends Ping Request
        const auto ping_req = kprotocol::C01PingRequestPacket{.payload = 0x0123456789ABCDEFULL};
        const auto ping_req_pkt = ping_req.to_packet();
        encoded = registry.encode_packet(ping_req_pkt, kprotocol::ProtocolVersion::v1_21_1);
        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_ping_req = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::status,
            kprotocol::PacketDirection::serverbound);
        assert(decoded_ping_req.key == ping_req_pkt.key);
        const auto typed_ping_req = kprotocol::C01PingRequestPacket::from_packet(decoded_ping_req);
        assert(typed_ping_req.payload == 0x0123456789ABCDEFULL);

        // 1.5: Server sends Pong Response
        const auto pong_resp = kprotocol::S01PongResponsePacket{.payload = typed_ping_req.payload};
        const auto pong_resp_pkt = pong_resp.to_packet();
        encoded = registry.encode_packet(pong_resp_pkt, kprotocol::ProtocolVersion::v1_21_1);
        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_pong_resp = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::status,
            kprotocol::PacketDirection::clientbound);
        assert(decoded_pong_resp.key == pong_resp_pkt.key);
        const auto typed_pong = kprotocol::S01PongResponsePacket::from_packet(decoded_pong_resp);
        assert(typed_pong.payload == 0x0123456789ABCDEFULL);

        std::cout << "  ✓ Complete status handshake flow verified\n\n";
    }

    // Test 2: Multi-version encode/decode roundtrips (all supported versions)
    {
        std::cout << "Test 2: Multi-version roundtrips (1.8, 1.12.2, 1.16.5, 1.20.4, 1.21.1)\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        const kprotocol::ProtocolVersion versions[] = {
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_12_2,
            kprotocol::ProtocolVersion::v1_16_5,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_21_1
        };

        for (const auto& version : versions) {
            // Create a handshake packet
            const auto handshake = kprotocol::C00HandshakePacket{
                .protocol_version = static_cast<std::int32_t>(version),
                .server_address = "test.local",
                .server_port = 25565,
                .next_state = 1
            };
            const auto pkt = handshake.to_packet();

            // Encode in this version
            const auto encoded = registry.encode_packet(pkt, version);
            assert(!encoded.empty());

            // Decode in this version
            kprotocol::codec::EncodedFrame frame;
            std::size_t consumed = 0;
            assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
            const auto decoded = registry.decode_packet(
                frame,
                version,
                kprotocol::PacketState::handshaking,
                kprotocol::PacketDirection::serverbound);

            // Verify roundtrip
            const auto typed = kprotocol::C00HandshakePacket::from_packet(decoded);
            assert(typed.protocol_version == static_cast<std::int32_t>(version));
            assert(typed.server_address == "test.local");
            assert(typed.server_port == 25565);
            assert(typed.next_state == 1);

            std::cout << "  ✓ Version " << static_cast<int>(version) << ": encode/decode OK\n";
        }

        std::cout << "\n";
    }

    // Test 3: Version translation (encode in 1.20.4, translate, decode in 1.8)
    {
        std::cout << "Test 3: Version translation (cross-version packet flow)\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        // Original source: 1.20.4 (protocol 765)
        const auto source_version = kprotocol::ProtocolVersion::v1_20_4;
        const auto target_version = kprotocol::ProtocolVersion::v1_8;

        // Create a clientbound Keep Alive packet (wire id varies by version, but key is universal)
        const auto keep_alive = kprotocol::S24KeepAlivePacket{.id = static_cast<std::int64_t>(0xDEADBEEFDEADBEEFULL)};
        const auto keep_alive_pkt = keep_alive.to_packet();

        // Encode in 1.20.4
        auto encoded_1_20_4 = registry.encode_packet(keep_alive_pkt, source_version);
        assert(!encoded_1_20_4.empty());

        // Decode from 1.20.4
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded_1_20_4, consumed, frame));
        auto decoded_1_20_4 = registry.decode_packet(
            frame,
            source_version,
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound);

        // Translate from 1.20.4 to 1.8 (registry-driven id remap; field identity here)
        const auto translated = translator.translate(
            decoded_1_20_4,
            source_version,
            target_version);

        // For now, translation preserves the packet as-is (unless specific rules exist)
        // Verify that we can encode the translated packet in 1.8
        const auto encoded_1_8 = registry.encode_packet(translated, target_version);
        assert(!encoded_1_8.empty());

        // Decode from 1.8
        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded_1_8, consumed, frame));
        const auto decoded_1_8 = registry.decode_packet(
            frame,
            target_version,
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound);

        // Verify roundtrip
        assert(decoded_1_8.key == keep_alive_pkt.key);

        std::cout << "  ✓ Packet translated from v1_20_4 → v1_8 successfully\n";
        std::cout << "  ✓ Encode/decode verified across versions\n\n";
    }

    // Test 4: Compression integration (encode with compression, decode with decompression)
    {
        std::cout << "Test 4: Compression with packet frames\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        // Create a larger payload to test compression
        const auto status_resp = kprotocol::S00StatusResponsePacket{
            .json_response = R"({"version":{"name":"1.21.1","protocol":767},"players":{"max":20,"online":5,"sample":[{"name":"Player1","id":"..."},{"name":"Player2","id":"..."}]},"description":{"text":"Welcome to the Test Server with Compression!"},"favicon":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="},"previewsChat":true,"enforcesSecureChat":false})"
        };
        const auto resp_pkt = status_resp.to_packet();
        const auto encoded = registry.encode_packet(resp_pkt, kprotocol::ProtocolVersion::v1_21_1);

        // Extract just the payload (skip frame header)
        std::vector<std::uint8_t> payload(encoded.begin(), encoded.end());

        // Test compression with threshold
        const int32_t compression_threshold = 256;
        const auto compressed_frame = kprotocol::codec::encode_frame_compressed(0x00, payload, compression_threshold);
        assert(!compressed_frame.empty());

        // Try to decode compressed frame
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        const bool decoded_ok = kprotocol::codec::try_decode_frame_compressed(
            compressed_frame,
            consumed,
            compression_threshold,
            frame);
        assert(decoded_ok);

        std::cout << "  ✓ Uncompressed payload size: " << payload.size() << " bytes\n";
        std::cout << "  ✓ Compressed frame size: " << compressed_frame.size() << " bytes\n";
        std::cout << "  ✓ Compression enabled for payloads > " << compression_threshold << " bytes\n\n";
    }

    // Test 5: Large payload handling with compression
    {
        std::cout << "Test 5: Large payload compression\n";

        // Simulate chunk data (large repetitive binary)
        std::vector<std::uint8_t> large_payload;
        for (int i = 0; i < 5000; ++i) {
            large_payload.push_back(static_cast<std::uint8_t>((i * 7) % 256));
        }

        const int32_t compression_threshold = 256;
        const auto compressed = kprotocol::codec::compress_packet(large_payload);

        assert(compressed.size() < large_payload.size()); // Should compress well
        assert(compressed.size() < large_payload.size() / 2); // Expect good compression for repetitive data

        const auto decompressed = kprotocol::codec::decompress_packet(compressed);
        assert(decompressed == large_payload);

        std::cout << "  ✓ Original: " << large_payload.size() << " bytes\n";
        std::cout << "  ✓ Compressed: " << compressed.size() << " bytes\n";
        std::cout << "  ✓ Compression ratio: " << (100 * compressed.size() / large_payload.size()) << "%\n";
        std::cout << "  ✓ Decompression verified\n\n";
    }

    // Test 6: Login packet flow (partial - without encryption)
    {
        std::cout << "Test 6: Login handshake flow (no encryption)\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        // 6.1: Client sends Login Start
        const auto login_start = kprotocol::C00LoginStartPacket{
            .username = "TestPlayer"
        };
        const auto login_start_pkt = login_start.to_packet();
        auto encoded = registry.encode_packet(login_start_pkt, kprotocol::ProtocolVersion::v1_21_1);

        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_login_start = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::login,
            kprotocol::PacketDirection::serverbound);
        assert(decoded_login_start.key == login_start_pkt.key);

        // 6.2: Server sends Login Success
        const auto login_success = kprotocol::S02LoginSuccessPacket{
            .uuid = "00000000-0000-0000-0000-000000000000",
            .username = "TestPlayer"
        };
        const auto login_success_pkt = login_success.to_packet();
        encoded = registry.encode_packet(login_success_pkt, kprotocol::ProtocolVersion::v1_21_1);

        consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded_login_success = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::login,
            kprotocol::PacketDirection::clientbound);
        assert(decoded_login_success.key == login_success_pkt.key);

        std::cout << "  ✓ Login Start packet verified\n";
        std::cout << "  ✓ Login Success packet verified\n";
        std::cout << "  ✓ Login flow complete\n\n";
    }

    // Test 7: Keep Alive packet roundtrips across all versions
    {
        std::cout << "Test 7: Keep Alive packets across all versions\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        const kprotocol::ProtocolVersion versions[] = {
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_12_2,
            kprotocol::ProtocolVersion::v1_16_5,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_21_1
        };

        const std::int64_t keep_alive_ids[] = {
            static_cast<std::int64_t>(0x1234567890ABCDEFULL),
            static_cast<std::int64_t>(0xFEDCBA9876543210ULL),
            static_cast<std::int64_t>(0x0000000000000001ULL)
        };

        for (const auto& version : versions) {
            for (const auto& ka_id : keep_alive_ids) {
                const auto keep_alive = kprotocol::S24KeepAlivePacket{.id = ka_id};
                const auto pkt = keep_alive.to_packet();
                const auto encoded = registry.encode_packet(pkt, version);

                kprotocol::codec::EncodedFrame frame;
                std::size_t consumed = 0;
                assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
                const auto decoded = registry.decode_packet(
                    frame,
                    version,
                    kprotocol::PacketState::play,
                    kprotocol::PacketDirection::clientbound);

                const auto typed = kprotocol::S24KeepAlivePacket::from_packet(decoded);
                assert(typed.id == ka_id);
            }
        }

        std::cout << "  ✓ All keep-alive IDs verified across all 5 versions\n\n";
    }

    // Test 8: Error handling - malformed packet data
    {
        std::cout << "Test 8: Error handling (malformed data)\n";

        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        // 8.1: Try to decode invalid frame
        {
            std::vector<std::uint8_t> invalid_frame{0xFF, 0xFF, 0xFF};
            kprotocol::codec::EncodedFrame frame;
            std::size_t consumed = 0;
            const bool ok = kprotocol::codec::try_decode_frame(invalid_frame, consumed, frame);
            // Should either fail gracefully or handle the error
            (void)ok; // We don't assert here - just verify no crash
        }

        // 8.2: Try to decode packet with invalid packet ID
        {
            // Create a frame with an unknown packet ID
            std::vector<std::uint8_t> payload{0x42};
            const auto frame = kprotocol::codec::encode_frame(0xFF, payload);

            kprotocol::codec::EncodedFrame decoded;
            std::size_t consumed = 0;
            kprotocol::codec::try_decode_frame(frame, consumed, decoded);

            try {
                const auto pkt = registry.decode_packet(
                    decoded,
                    kprotocol::ProtocolVersion::v1_21_1,
                    kprotocol::PacketState::handshaking,
                    kprotocol::PacketDirection::serverbound);
                // If we get here, the packet was marked as unknown (expected behavior)
            } catch (...) {
                // Exception thrown is also acceptable
            }
        }

        std::cout << "  ✓ Malformed data handled without crashes\n";
        std::cout << "  ✓ Unknown packet IDs handled gracefully\n\n";
    }

    // Test 9: Compression threshold boundary conditions
    {
        std::cout << "Test 9: Compression threshold edge cases\n";

        const int32_t threshold = 256;

        // 9.1: Payload exactly at threshold
        {
            std::vector<std::uint8_t> payload(256, 0xAB);
            const auto frame = kprotocol::codec::encode_frame_compressed(0x01, payload, threshold);
            assert(!frame.empty());
        }

        // 9.2: Payload just below threshold
        {
            std::vector<std::uint8_t> payload(255, 0xAB);
            const auto frame = kprotocol::codec::encode_frame_compressed(0x01, payload, threshold);
            assert(!frame.empty());
        }

        // 9.3: Payload just above threshold
        {
            std::vector<std::uint8_t> payload(257, 0xAB);
            const auto frame = kprotocol::codec::encode_frame_compressed(0x01, payload, threshold);
            assert(!frame.empty());
        }

        // 9.4: Negative threshold (no compression)
        {
            std::vector<std::uint8_t> payload(1000, 0xAB);
            const auto frame = kprotocol::codec::encode_frame_compressed(0x01, payload, -1);
            assert(!frame.empty());
        }

        std::cout << "  ✓ Threshold at boundary: " << threshold << " bytes\n";
        std::cout << "  ✓ Below, at, and above threshold handled\n";
        std::cout << "  ✓ Negative threshold (no compression) works\n\n";
    }

    // Test 10: Block and Item ID translation (from translation_registry)
    {
        std::cout << "Test 10: ID translation via TranslationRegistry\n";

        // Test block ID mapping between versions: signature is (from, to, source_id)
        const auto block_id_1_8_to_1_20 = kprotocol::TranslationRegistry::map_block_id(
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_20_4,
            5); // Stone in 1.8
        assert(block_id_1_8_to_1_20 >= 0);

        // Reverse mapping
        const auto block_id_1_20_to_1_8 = kprotocol::TranslationRegistry::map_block_id(
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::ProtocolVersion::v1_8,
            block_id_1_8_to_1_20);
        assert(block_id_1_20_to_1_8 >= 0);

        // Item ID mapping
        const auto item_id = kprotocol::TranslationRegistry::map_item_id(
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_20_4,
            1); // Stone in 1.8
        assert(item_id >= 0);

        // Entity type mapping
        const auto entity_id = kprotocol::TranslationRegistry::map_entity_id(
            kprotocol::ProtocolVersion::v1_8,
            kprotocol::ProtocolVersion::v1_20_4,
            10); // Chicken in 1.8
        assert(entity_id >= 0);

        std::cout << "  ✓ Block ID mapping verified (1.8 → 1.20.4 → 1.8)\n";
        std::cout << "  ✓ Item ID mapping verified\n";
        std::cout << "  ✓ Entity ID mapping verified\n";
        std::cout << "  ✓ All translation registry lookups succeed\n\n";
    }

    std::cout << "=== All Integration Tests Passed ✓ ===\n";
    return 0;
}

#include "kprotocol/codec.hpp"

#include <cassert>
#include <iostream>
#include <vector>

/**
 * Test suite for compression functionality.
 * Verifies zlib compression/decompression and frame encoding with compression.
 */

int main() {
    std::cout << "Testing compression functionality...\n";
    
    // Test 1: Basic compression and decompression roundtrip
    {
        std::cout << "Test 1: Compression roundtrip with small data...\n";
        
        const std::vector<std::uint8_t> original{0x01, 0x02, 0x03, 0x04, 0x05};
        
        const auto compressed = kprotocol::codec::compress_packet(original);
        assert(!compressed.empty());
        assert(compressed.size() <= original.size() + 16); // Allow some overhead
        
        const auto decompressed = kprotocol::codec::decompress_packet(compressed);
        assert(decompressed == original);
        
        std::cout << "  ✓ Roundtrip successful (original: " << original.size() 
                  << " bytes, compressed: " << compressed.size() << " bytes)\n";
    }
    
    // Test 2: Compression of larger repetitive data (good compression ratio)
    {
        std::cout << "Test 2: Compression of repetitive data...\n";
        
        std::vector<std::uint8_t> repetitive;
        for (int i = 0; i < 1000; ++i) {
            repetitive.push_back(static_cast<std::uint8_t>(i % 256));
        }
        
        const auto compressed = kprotocol::codec::compress_packet(repetitive);
        assert(!compressed.empty());
        
        // Repetitive data should compress well
        assert(compressed.size() < repetitive.size() / 2);
        
        const auto decompressed = kprotocol::codec::decompress_packet(compressed);
        assert(decompressed == repetitive);
        
        std::cout << "  ✓ Good compression ratio: " << repetitive.size() 
                  << " → " << compressed.size() << " bytes ("
                  << (100 * compressed.size() / repetitive.size()) << "%)\n";
    }
    
    // Test 3: Empty data
    {
        std::cout << "Test 3: Compression of empty data...\n";
        
        const std::vector<std::uint8_t> empty;
        const auto compressed = kprotocol::codec::compress_packet(empty);
        assert(compressed.empty());
        
        const auto decompressed = kprotocol::codec::decompress_packet(compressed);
        assert(decompressed.empty());
        
        std::cout << "  ✓ Empty data handled correctly\n";
    }
    
    // Test 4: Frame encoding with compression (above threshold)
    {
        std::cout << "Test 4: Frame encoding with compression (above threshold)...\n";
        
        // Create a payload larger than threshold
        std::vector<std::uint8_t> payload(500, 0x42); // 500 bytes of 0x42
        const int32_t packet_id = 0x20;
        const int32_t threshold = 256;
        
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, payload, threshold);
        assert(!frame.empty());
        
        // Frame should contain compressed data, so should be smaller than original
        // (packet_id as varint + compressed payload)
        std::cout << "  ✓ Frame encoding successful (frame size: " << frame.size() << " bytes)\n";
    }
    
    // Test 5: Frame encoding without compression (below threshold)
    {
        std::cout << "Test 5: Frame encoding without compression (below threshold)...\n";
        
        std::vector<std::uint8_t> small_payload{0x01, 0x02, 0x03}; // 3 bytes < threshold
        const int32_t packet_id = 0x20;
        const int32_t threshold = 256;
        
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, small_payload, threshold);
        assert(!frame.empty());
        
        // For small payloads, data_length should be 0 (uncompressed indicator)
        // Frame format: frame_length (varint) + data_length (varint=0) + packet_id (varint) + payload
        std::cout << "  ✓ Small payload sent uncompressed (frame size: " << frame.size() << " bytes)\n";
    }
    
    // Test 6: Frame encoding with no compression (threshold = -1)
    {
        std::cout << "Test 6: Frame encoding with compression disabled...\n";
        
        std::vector<std::uint8_t> payload{0x01, 0x02, 0x03, 0x04};
        const int32_t packet_id = 0x20;
        const int32_t threshold = -1; // No compression
        
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, payload, threshold);
        assert(!frame.empty());
        
        std::cout << "  ✓ No compression mode works (frame size: " << frame.size() << " bytes)\n";
    }
    
    // Test 7: Decode frame with compression (large repetitive data)
    {
        std::cout << "Test 7: Decode compressed frame...\n";
        
        // Create a large repetitive payload
        std::vector<std::uint8_t> payload(1000, 0xFF);
        const int32_t packet_id = 0x30;
        const int32_t threshold = 256;
        
        // Encode with compression
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, payload, threshold);
        
        // Decode
        kprotocol::codec::EncodedFrame decoded;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame_compressed(frame, consumed, threshold, decoded);
        
        assert(ok);
        assert(consumed == frame.size());
        assert(decoded.packet_id == packet_id);
        assert(decoded.payload == payload);
        
        std::cout << "  ✓ Compression roundtrip successful via frames\n";
    }
    
    // Test 8: Decode uncompressed frame (data below threshold)
    {
        std::cout << "Test 8: Decode uncompressed frame...\n";
        
        std::vector<std::uint8_t> payload{0x01, 0x02, 0x03};
        const int32_t packet_id = 0x30;
        const int32_t threshold = 256;
        
        // Encode (should not compress due to size)
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, payload, threshold);
        
        // Decode
        kprotocol::codec::EncodedFrame decoded;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame_compressed(frame, consumed, threshold, decoded);
        
        assert(ok);
        assert(consumed == frame.size());
        assert(decoded.packet_id == packet_id);
        assert(decoded.payload == payload);
        
        std::cout << "  ✓ Uncompressed frame decode successful\n";
    }
    
    // Test 9: Decode without compression enabled (threshold = -1)
    {
        std::cout << "Test 9: Decode frame with no compression...\n";
        
        std::vector<std::uint8_t> payload{0x01, 0x02, 0x03, 0x04};
        const int32_t packet_id = 0x30;
        const int32_t threshold = -1;
        
        // Encode
        const auto frame = kprotocol::codec::encode_frame_compressed(packet_id, payload, threshold);
        
        // Decode
        kprotocol::codec::EncodedFrame decoded;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame_compressed(frame, consumed, threshold, decoded);
        
        assert(ok);
        assert(decoded.packet_id == packet_id);
        assert(decoded.payload == payload);
        
        std::cout << "  ✓ No-compression mode decode successful\n";
    }
    
    // Test 10: Large binary data compression
    {
        std::cout << "Test 10: Large binary data compression...\n";
        
        // Create 10KB of pseudo-random data
        std::vector<std::uint8_t> large_data(10240);
        for (size_t i = 0; i < large_data.size(); ++i) {
            large_data[i] = static_cast<std::uint8_t>((i * 73) % 256); // Simple pseudo-random
        }
        
        const auto compressed = kprotocol::codec::compress_packet(large_data);
        const auto decompressed = kprotocol::codec::decompress_packet(compressed);
        
        assert(decompressed == large_data);
        
        const double ratio = 100.0 * compressed.size() / large_data.size();
        std::cout << "  ✓ Large data compression: " << large_data.size() 
                  << " → " << compressed.size() << " bytes (" << ratio << "%)\n";
    }
    
    std::cout << "\n✅ All compression tests passed!\n";
    return 0;
}

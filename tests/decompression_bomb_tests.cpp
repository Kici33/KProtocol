// DoS-resistance tests for the compression path.
//
// Goal: a malicious peer cannot make us allocate more than
// codec::limits::max_decompressed_length bytes from a tiny zlib stream.
//
// The classic attack: 1 KiB of compressed zeros expands to >1 GiB of zeros.
// decompress_packet() must abort with codec::DecodeError when the bound is
// exceeded, NOT recurse into an OS-level OOM.

#include "kprotocol/codec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_zero_payload(std::size_t size) {
    return std::vector<std::uint8_t>(size, 0);
}

void test_legitimate_payload_roundtrips() {
    std::cout << "  Legitimate roundtrip below limit succeeds... ";
    const auto payload = make_zero_payload(1024 * 64); // 64 KiB
    const auto compressed = kprotocol::codec::compress_packet(payload);
    const auto round_tripped = kprotocol::codec::decompress_packet(compressed);
    assert(round_tripped == payload);
    std::cout << "ok\n";
}

void test_bomb_blocked_by_default_limit() {
    std::cout << "  Decompression bomb blocked by default cap... ";
    // Build a 2 MiB payload of zeros; compress to <1 KiB.
    // Then ask decompress to expand into a 1 MiB ceiling: must throw.
    const auto payload = make_zero_payload(2 * 1024 * 1024);
    const auto compressed = kprotocol::codec::compress_packet(payload);
    assert(compressed.size() < payload.size() / 100); // well under 1% ratio

    bool threw = false;
    try {
        (void)kprotocol::codec::decompress_packet(compressed, 1024 * 1024 /* 1 MiB ceiling */);
    } catch (const kprotocol::codec::DecodeError&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

void test_explicit_high_ceiling_allows_expansion() {
    std::cout << "  Caller-supplied higher ceiling allows expansion... ";
    const auto payload = make_zero_payload(3 * 1024 * 1024); // 3 MiB
    const auto compressed = kprotocol::codec::compress_packet(payload);
    // Ceiling well above the actual expanded size.
    const auto round_tripped = kprotocol::codec::decompress_packet(compressed, 8 * 1024 * 1024);
    assert(round_tripped == payload);
    std::cout << "ok\n";
}

void test_corrupt_stream_throws_decode_error() {
    std::cout << "  Corrupt compressed stream throws DecodeError... ";
    std::vector<std::uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool threw = false;
    try {
        (void)kprotocol::codec::decompress_packet(garbage);
    } catch (const kprotocol::codec::DecodeError&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

void test_compressed_frame_with_oversized_declared_length() {
    std::cout << "  Compressed frame with data_length > max_decompressed_length -> false... ";
    // Hand-craft a frame whose body is `varint(huge_data_length) || compressed_data`.
    // The decoder should reject it BEFORE attempting decompression.
    std::vector<std::uint8_t> body;
    kprotocol::codec::write_var_int(body, 64 * 1024 * 1024); // way above the 8 MiB cap
    body.push_back(0x78); // pseudo zlib header
    body.push_back(0x9C);

    std::vector<std::uint8_t> frame;
    kprotocol::codec::write_var_int(frame, static_cast<std::int32_t>(body.size()));
    frame.insert(frame.end(), body.begin(), body.end());

    kprotocol::codec::EncodedFrame out;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame_compressed(frame, consumed, 256, out);
    assert(!ok);
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "decompression_bomb_tests:\n";
    test_legitimate_payload_roundtrips();
    test_bomb_blocked_by_default_limit();
    test_explicit_high_ceiling_allows_expansion();
    test_corrupt_stream_throws_decode_error();
    test_compressed_frame_with_oversized_declared_length();
    std::cout << "All decompression-bomb tests passed.\n";
    return 0;
}

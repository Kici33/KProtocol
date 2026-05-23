// Tests that try_decode_frame and try_decode_frame_compressed gracefully
// reject malformed input WITHOUT throwing - the contract relied upon by
// network code that cannot use exception-driven control flow.

#include "kprotocol/codec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void test_truncated_length_returns_false() {
    std::cout << "  Truncated length VarInt -> false (wait for more)... ";
    // A single continuation byte: incomplete VarInt for the outer length.
    const std::vector<std::uint8_t> input = {0x80};
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame(input, consumed, frame);
    assert(!ok);
    assert(consumed == 0);
    std::cout << "ok\n";
}

void test_overlong_length_returns_false() {
    std::cout << "  Overlong length VarInt -> false (not throw)... ";
    // Six 0x80 bytes: VarInt that never terminates within the legal 5-byte
    // budget. Pre-Wave1 this could enter an infinite-loop / throw path.
    const std::vector<std::uint8_t> input = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame(input, consumed, frame);
    assert(!ok);
    std::cout << "ok\n";
}

void test_oversized_length_returns_false() {
    std::cout << "  Frame length above max_packet_length -> false... ";
    // Encode VarInt = 2 GiB which is well above the 2 MiB hard cap.
    std::vector<std::uint8_t> input;
    kprotocol::codec::write_var_int(input, 2 * 1024 * 1024 + 1);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame(input, consumed, frame);
    assert(!ok);
    std::cout << "ok\n";
}

void test_partial_body_returns_false() {
    std::cout << "  Length=10 but only 3 bytes of body -> false (need more)... ";
    std::vector<std::uint8_t> input;
    kprotocol::codec::write_var_int(input, 10);
    input.push_back(0x01); // partial body
    input.push_back(0x02);
    input.push_back(0x03);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame(input, consumed, frame);
    assert(!ok);
    assert(consumed == 0); // didn't claim bytes
    std::cout << "ok\n";
}

void test_round_trip_after_partial_buffer() {
    std::cout << "  Streaming: partial -> complete frame -> success... ";
    const std::vector<std::uint8_t> payload = {0xAA, 0xBB, 0xCC};
    const auto full_frame = kprotocol::codec::encode_frame(0x12, payload);

    // Feed all but the last byte; expect false.
    {
        std::vector<std::uint8_t> partial(full_frame.begin(), full_frame.end() - 1);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame(partial, consumed, frame);
        assert(!ok);
        assert(consumed == 0);
    }
    // Now feed the full frame.
    {
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame(full_frame, consumed, frame);
        assert(ok);
        assert(consumed == full_frame.size());
        assert(frame.packet_id == 0x12);
        assert(frame.payload == payload);
    }
    std::cout << "ok\n";
}

void test_compressed_negative_data_length_returns_false() {
    std::cout << "  Compressed frame with negative data_length -> false... ";
    // Build a frame whose body starts with VarInt = -1, threshold > 0.
    std::vector<std::uint8_t> body;
    kprotocol::codec::write_var_int(body, -1); // negative data_length
    body.push_back(0x00);                      // garbage

    std::vector<std::uint8_t> frame;
    kprotocol::codec::write_var_int(frame, static_cast<std::int32_t>(body.size()));
    frame.insert(frame.end(), body.begin(), body.end());

    kprotocol::codec::EncodedFrame out;
    std::size_t consumed = 0;
    const bool ok = kprotocol::codec::try_decode_frame_compressed(frame, consumed, 256, out);
    assert(!ok);
    std::cout << "ok\n";
}

void test_consumed_unchanged_on_failure() {
    std::cout << "  `consumed` is zero on every failure path... ";
    kprotocol::codec::EncodedFrame frame;

    std::size_t consumed = 999;
    (void)kprotocol::codec::try_decode_frame({}, consumed, frame);
    assert(consumed == 0);

    consumed = 999;
    const std::vector<std::uint8_t> incomplete = {0x80};
    (void)kprotocol::codec::try_decode_frame(incomplete, consumed, frame);
    assert(consumed == 0);

    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "frame_malformed_tests:\n";
    test_truncated_length_returns_false();
    test_overlong_length_returns_false();
    test_oversized_length_returns_false();
    test_partial_body_returns_false();
    test_round_trip_after_partial_buffer();
    test_compressed_negative_data_length_returns_false();
    test_consumed_unchanged_on_failure();
    std::cout << "All malformed-frame tests passed.\n";
    return 0;
}

// VarInt / VarLong property tests.
//
// Covers the wire-format edge cases that historically caused bugs:
//   - signed/unsigned conversion at INT32_MIN / INT64_MIN
//   - canonical encoding lengths (var_int_size closed form vs. write-and-count)
//   - overlong VarInts (6-byte continuation) must be rejected
//   - truncated VarInts mid-stream must produce a recoverable Reader error
//   - round-trip identity on a representative integer corpus

#include "kprotocol/codec.hpp"
#include "kprotocol/codec/reader.hpp"
#include "kprotocol/codec/writer.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {

constexpr std::array<std::int32_t, 13> int32_corpus = {
    0, 1, -1, 2, -2, 127, 128, -127, -128, 16384, -16384,
    std::numeric_limits<std::int32_t>::min(),
    std::numeric_limits<std::int32_t>::max(),
};

constexpr std::array<std::int64_t, 11> int64_corpus = {
    0, 1, -1, 127, 128, -127, -128, 4294967296LL, -4294967296LL,
    std::numeric_limits<std::int64_t>::min(),
    std::numeric_limits<std::int64_t>::max(),
};

void test_var_int_round_trip() {
    std::cout << "  VarInt round-trip... ";
    for (auto v : int32_corpus) {
        std::vector<std::uint8_t> buf;
        kprotocol::codec::write_var_int(buf, v);

        kprotocol::codec::Reader r(buf);
        const auto out = r.read_var_int();
        assert(r.ok());
        assert(r.cursor() == buf.size());
        assert(out == v);
    }
    std::cout << "ok\n";
}

void test_var_long_round_trip() {
    std::cout << "  VarLong round-trip... ";
    for (auto v : int64_corpus) {
        std::vector<std::uint8_t> buf;
        kprotocol::codec::write_var_long(buf, v);

        kprotocol::codec::Reader r(buf);
        const auto out = r.read_var_long();
        assert(r.ok());
        assert(r.cursor() == buf.size());
        assert(out == v);
    }
    std::cout << "ok\n";
}

void test_var_int_size_matches_write() {
    std::cout << "  var_int_size matches write_var_int byte count... ";
    for (auto v : int32_corpus) {
        std::vector<std::uint8_t> buf;
        kprotocol::codec::write_var_int(buf, v);
        const auto closed_form = kprotocol::codec::var_int_size(v);
        assert(static_cast<std::size_t>(closed_form) == buf.size());
    }
    std::cout << "ok\n";
}

void test_overlong_var_int_rejected() {
    std::cout << "  Reject 6-byte VarInt continuation... ";
    // Six bytes all with the continuation bit set: an overlong VarInt the
    // Mojang spec explicitly forbids.
    const std::vector<std::uint8_t> overlong = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    kprotocol::codec::Reader r(overlong);
    (void)r.read_var_int();
    assert(!r.ok());
    assert(r.error() == kprotocol::codec::ReadError::overlong_varint);
    std::cout << "ok\n";
}

void test_truncated_var_int_recoverable() {
    std::cout << "  Truncated VarInt reports `truncated` error... ";
    // Continuation bit set but no follow-on byte.
    const std::vector<std::uint8_t> truncated = {0x80};
    kprotocol::codec::Reader r(truncated);
    (void)r.read_var_int();
    assert(!r.ok());
    assert(r.error() == kprotocol::codec::ReadError::truncated);
    std::cout << "ok\n";
}

void test_legacy_throwing_decoder_still_works() {
    std::cout << "  Legacy throwing read_var_int still raises on truncation... ";
    const std::vector<std::uint8_t> truncated = {0x80};
    std::size_t offset = 0;
    bool threw = false;
    try {
        (void)kprotocol::codec::read_var_int(truncated, offset);
    } catch (const kprotocol::codec::DecodeError&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

void test_negative_int_uses_full_five_bytes() {
    std::cout << "  Negative VarInt encodes as 5 bytes (sign-extended)... ";
    std::vector<std::uint8_t> buf;
    kprotocol::codec::write_var_int(buf, -1);
    assert(buf.size() == 5);
    assert(kprotocol::codec::var_int_size(-1) == 5);
    std::cout << "ok\n";
}

void test_zero_encodes_as_single_byte() {
    std::cout << "  Zero VarInt = single 0x00 byte... ";
    std::vector<std::uint8_t> buf;
    kprotocol::codec::write_var_int(buf, 0);
    assert(buf.size() == 1);
    assert(buf[0] == 0x00);
    assert(kprotocol::codec::var_int_size(0) == 1);
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "varint_property_tests:\n";
    test_var_int_round_trip();
    test_var_long_round_trip();
    test_var_int_size_matches_write();
    test_overlong_var_int_rejected();
    test_truncated_var_int_recoverable();
    test_legacy_throwing_decoder_still_works();
    test_negative_int_uses_full_five_bytes();
    test_zero_encodes_as_single_byte();
    std::cout << "All VarInt property tests passed.\n";
    return 0;
}

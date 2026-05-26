// SPDX-License-Identifier: MIT
//
// Wire codec for the Minecraft Java protocol.
//
// Design goals:
//   - No undefined behavior on adversarial input. Bounds-checked.
//   - try_* decoders never throw; on malformed/truncated input they return
//     false (so a coroutine-driven Wave 3 server doesn't have to wrap awaits
//     in try/catch).
//   - All length-prefixed reads are capped at codec::limits::* unless the
//     caller passes a larger explicit bound.
//   - Decompression is bounded - an attacker cannot force unbounded memory
//     allocation by sending a small "zip-bomb" stream.
//   - The legacy free-function API (read_*(span, size_t&) -> T) is preserved
//     verbatim and reimplemented on top of codec::Reader so a single source
//     of truth governs bounds checks.

#include "kprotocol/codec.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <zlib.h>

namespace kprotocol::codec {

// ===== Reader implementation =================================================

std::int32_t Reader::read_var_int() noexcept {
    if (!ok()) return 0;
    std::uint32_t result = 0;
    std::size_t bytes_read = 0;
    while (bytes_read < limits::max_var_int_bytes) {
        if (cursor_ + bytes_read >= data_.size()) {
            poison(ReadError::truncated);
            return 0;
        }
        const auto current = data_[cursor_ + bytes_read];
        result |= static_cast<std::uint32_t>(current & 0x7FU) << (7U * bytes_read);
        ++bytes_read;
        if ((current & 0x80U) == 0U) {
            cursor_ += bytes_read;
            return static_cast<std::int32_t>(result);
        }
    }
    poison(ReadError::overlong_varint);
    return 0;
}

std::int64_t Reader::read_var_long() noexcept {
    if (!ok()) return 0;
    std::uint64_t result = 0;
    std::size_t bytes_read = 0;
    while (bytes_read < limits::max_var_long_bytes) {
        if (cursor_ + bytes_read >= data_.size()) {
            poison(ReadError::truncated);
            return 0;
        }
        const auto current = data_[cursor_ + bytes_read];
        result |= static_cast<std::uint64_t>(current & 0x7FU) << (7U * bytes_read);
        ++bytes_read;
        if ((current & 0x80U) == 0U) {
            cursor_ += bytes_read;
            return static_cast<std::int64_t>(result);
        }
    }
    poison(ReadError::overlong_varint);
    return 0;
}

std::uint8_t Reader::read_u8() noexcept {
    if (!ok()) return 0;
    if (!has_remaining(1)) { poison(ReadError::truncated); return 0; }
    return data_[cursor_++];
}

std::int8_t Reader::read_i8() noexcept {
    return static_cast<std::int8_t>(read_u8());
}

std::uint16_t Reader::read_u16_be() noexcept {
    if (!ok()) return 0;
    if (!has_remaining(2)) { poison(ReadError::truncated); return 0; }
    const auto hi = static_cast<std::uint16_t>(data_[cursor_]);
    const auto lo = static_cast<std::uint16_t>(data_[cursor_ + 1]);
    cursor_ += 2;
    return static_cast<std::uint16_t>((hi << 8U) | lo);
}

std::int16_t Reader::read_i16_be() noexcept {
    return static_cast<std::int16_t>(read_u16_be());
}

std::uint32_t Reader::read_u32_be() noexcept {
    if (!ok()) return 0;
    if (!has_remaining(4)) { poison(ReadError::truncated); return 0; }
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v = (v << 8U) | static_cast<std::uint32_t>(data_[cursor_ + static_cast<std::size_t>(i)]);
    }
    cursor_ += 4;
    return v;
}

std::int32_t Reader::read_i32_be() noexcept {
    return static_cast<std::int32_t>(read_u32_be());
}

std::uint64_t Reader::read_u64_be() noexcept {
    if (!ok()) return 0;
    if (!has_remaining(8)) { poison(ReadError::truncated); return 0; }
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8U) | static_cast<std::uint64_t>(data_[cursor_ + static_cast<std::size_t>(i)]);
    }
    cursor_ += 8;
    return v;
}

std::int64_t Reader::read_i64_be() noexcept {
    return static_cast<std::int64_t>(read_u64_be());
}

float Reader::read_f32_be() noexcept {
    const auto bits = read_u32_be();
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(float));
    return v;
}

double Reader::read_f64_be() noexcept {
    const auto bits = read_u64_be();
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof(double));
    return v;
}

bool Reader::read_bool() noexcept {
    return read_u8() != 0U;
}

std::string Reader::read_string(std::int32_t max_len) noexcept {
    if (!ok()) return {};
    const auto length = read_var_int();
    if (!ok()) return {};
    if (length < 0) { poison(ReadError::negative_length); return {}; }
    if (length > max_len) { poison(ReadError::length_exceeds_limit); return {}; }
    const auto n = static_cast<std::size_t>(length);
    if (!has_remaining(n)) { poison(ReadError::truncated); return {}; }
    // Direct slice construction - no per-byte push_back.
    const auto* begin = reinterpret_cast<const char*>(data_.data() + cursor_);
    std::string out(begin, n);
    cursor_ += n;
    return out;
}

std::span<const std::uint8_t> Reader::read_bytes(std::int32_t max_len) noexcept {
    if (!ok()) return {};
    const auto length = read_var_int();
    if (!ok()) return {};
    if (length < 0) { poison(ReadError::negative_length); return {}; }
    if (length > max_len) { poison(ReadError::length_exceeds_limit); return {}; }
    const auto n = static_cast<std::size_t>(length);
    if (!has_remaining(n)) { poison(ReadError::truncated); return {}; }
    const auto view = data_.subspan(cursor_, n);
    cursor_ += n;
    return view;
}

// ===== Writer implementation =================================================

void Writer::write_var_int(std::int32_t value) noexcept {
    if (!ok()) return;
    auto current = static_cast<std::uint32_t>(value);
    do {
        auto temp = static_cast<std::uint8_t>(current & 0x7FU);
        current >>= 7U;
        if (current != 0U) temp |= 0x80U;
        out_.push_back(temp);
        ++written_;
    } while (current != 0U);
}

void Writer::write_var_long(std::int64_t value) noexcept {
    if (!ok()) return;
    auto current = static_cast<std::uint64_t>(value);
    do {
        auto temp = static_cast<std::uint8_t>(current & 0x7FU);
        current >>= 7U;
        if (current != 0U) temp |= 0x80U;
        out_.push_back(temp);
        ++written_;
    } while (current != 0U);
}

void Writer::write_u8(std::uint8_t value) noexcept {
    if (!ok()) return;
    out_.push_back(value);
    ++written_;
}

void Writer::write_i8(std::int8_t value) noexcept {
    write_u8(static_cast<std::uint8_t>(value));
}

void Writer::write_u16_be(std::uint16_t value) noexcept {
    if (!ok()) return;
    out_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out_.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    written_ += 2;
}

void Writer::write_i16_be(std::int16_t value) noexcept {
    write_u16_be(static_cast<std::uint16_t>(value));
}

void Writer::write_u32_be(std::uint32_t value) noexcept {
    if (!ok()) return;
    out_.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out_.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out_.push_back(static_cast<std::uint8_t>((value >> 8U)  & 0xFFU));
    out_.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    written_ += 4;
}

void Writer::write_i32_be(std::int32_t value) noexcept {
    write_u32_be(static_cast<std::uint32_t>(value));
}

void Writer::write_u64_be(std::uint64_t value) noexcept {
    if (!ok()) return;
    for (int i = 7; i >= 0; --i) {
        out_.push_back(static_cast<std::uint8_t>((value >> (static_cast<std::uint32_t>(i) * 8U)) & 0xFFU));
    }
    written_ += 8;
}

void Writer::write_i64_be(std::int64_t value) noexcept {
    write_u64_be(static_cast<std::uint64_t>(value));
}

void Writer::write_f32_be(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));
    write_u32_be(bits);
}

void Writer::write_f64_be(double value) noexcept {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(double));
    write_u64_be(bits);
}

void Writer::write_bool(bool value) noexcept {
    write_u8(value ? 0x01U : 0x00U);
}

void Writer::write_string(const std::string& value, std::int32_t max_len) noexcept {
    if (!ok()) return;
    if (value.size() > static_cast<std::size_t>(max_len)) {
        poison(WriteError::length_exceeds_limit);
        return;
    }
    write_var_int(static_cast<std::int32_t>(value.size()));
    if (!ok()) return;
    out_.insert(out_.end(), value.begin(), value.end());
    written_ += value.size();
}

void Writer::write_bytes(std::span<const std::uint8_t> value, std::int32_t max_len) noexcept {
    if (!ok()) return;
    if (value.size() > static_cast<std::size_t>(max_len)) {
        poison(WriteError::length_exceeds_limit);
        return;
    }
    write_var_int(static_cast<std::int32_t>(value.size()));
    if (!ok()) return;
    out_.insert(out_.end(), value.begin(), value.end());
    written_ += value.size();
}

void Writer::write_raw(std::span<const std::uint8_t> value) noexcept {
    if (!ok()) return;
    out_.insert(out_.end(), value.begin(), value.end());
    written_ += value.size();
}

// ===== Legacy free-function API =============================================
//
// Every public read_*/write_* free function from previous releases is preserved
// with its original signature. They now delegate to Reader/Writer so that bounds
// checking is centralized. The read_* functions still throw codec::DecodeError
// when the legacy contract demanded it (e.g. they return a value, not a status),
// but the THROWN exception type is now codec::DecodeError specifically (still
// derived from std::runtime_error so old catch blocks remain effective).

namespace {

[[noreturn]] void throw_read_failure(ReadError e) {
    switch (e) {
    case ReadError::truncated:            throw DecodeError("codec: unexpected end of input");
    case ReadError::overlong_varint:      throw DecodeError("codec: VarInt/VarLong overflow");
    case ReadError::length_exceeds_limit: throw DecodeError("codec: length-prefixed value exceeds limit");
    case ReadError::negative_length:      throw DecodeError("codec: negative length-prefix");
    case ReadError::ok:                   throw DecodeError("codec: unexpected reader state");
    }
    throw DecodeError("codec: unspecified decode error");
}

template <typename F>
auto with_reader(std::span<const std::uint8_t> input, std::size_t& offset, F&& fn) {
    Reader r(input.subspan(offset));
    auto value = fn(r);
    if (!r.ok()) {
        throw_read_failure(r.error());
    }
    offset += r.cursor();
    return value;
}

} // namespace

// --- Encoders (delegate to Writer; preserve original void signatures) -------

void write_var_int(std::vector<std::uint8_t>& out, std::int32_t value) {
    Writer w(out); w.write_var_int(value);
}

void write_var_long(std::vector<std::uint8_t>& out, std::int64_t value) {
    Writer w(out); w.write_var_long(value);
}

void write_byte(std::vector<std::uint8_t>& out, std::int8_t value)   { Writer w(out); w.write_i8(value); }
void write_ubyte(std::vector<std::uint8_t>& out, std::uint8_t value) { Writer w(out); w.write_u8(value); }
void write_short(std::vector<std::uint8_t>& out, std::int16_t value)   { Writer w(out); w.write_i16_be(value); }
void write_ushort(std::vector<std::uint8_t>& out, std::uint16_t value) { Writer w(out); w.write_u16_be(value); }
void write_int(std::vector<std::uint8_t>& out, std::int32_t value)   { Writer w(out); w.write_i32_be(value); }
void write_uint(std::vector<std::uint8_t>& out, std::uint32_t value) { Writer w(out); w.write_u32_be(value); }
void write_long(std::vector<std::uint8_t>& out, std::int64_t value)   { Writer w(out); w.write_i64_be(value); }
void write_ulong(std::vector<std::uint8_t>& out, std::uint64_t value) { Writer w(out); w.write_u64_be(value); }

void write_float(std::vector<std::uint8_t>& out, float value)   { Writer w(out); w.write_f32_be(value); }
void write_double(std::vector<std::uint8_t>& out, double value) { Writer w(out); w.write_f64_be(value); }

void write_bool(std::vector<std::uint8_t>& out, bool value) { Writer w(out); w.write_bool(value); }
void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value) { Writer w(out); w.write_u16_be(value); }

void write_string(std::vector<std::uint8_t>& out, const std::string& value) {
    Writer w(out);
    w.write_string(value);
    if (!w.ok()) {
        throw EncodeError("codec: string length exceeds protocol limit");
    }
}

void write_bytes(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value) {
    Writer w(out);
    w.write_bytes(value);
    if (!w.ok()) {
        throw EncodeError("codec: byte-array length exceeds protocol limit");
    }
}

void write_byte_array(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value) {
    write_bytes(out, value);
}

void write_angle(std::vector<std::uint8_t>& out, float yaw) {
    // Map any real value to [0, 1) before scaling - handles negative and out-of-range yaws.
    float frac = yaw - static_cast<float>(static_cast<long long>(yaw));
    if (frac < 0.0f) frac += 1.0f;
    const auto scaled = static_cast<std::uint8_t>(static_cast<int>(frac * 256.0f) & 0xFF);
    Writer w(out); w.write_u8(scaled);
}

void write_rotation(std::vector<std::uint8_t>& out, float yaw, float pitch) {
    write_angle(out, yaw);
    write_angle(out, pitch);
}

void write_var_int_array(std::vector<std::uint8_t>& out, std::span<const std::int32_t> values) {
    Writer w(out);
    w.write_var_int(static_cast<std::int32_t>(values.size()));
    for (auto v : values) w.write_var_int(v);
}

void write_var_long_array(std::vector<std::uint8_t>& out, std::span<const std::int64_t> values) {
    Writer w(out);
    w.write_var_int(static_cast<std::int32_t>(values.size()));
    for (auto v : values) w.write_var_long(v);
}

// --- Decoders (legacy span+offset; throw DecodeError on failure) ------------

std::int32_t read_var_int(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) { return r.read_var_int(); });
}

std::int64_t read_var_long(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) { return r.read_var_long(); });
}

bool try_read_var_int(std::span<const std::uint8_t> input, std::size_t& offset, std::int32_t& out) noexcept {
    Reader reader(input.subspan(offset));
    out = reader.read_var_int();
    if (!reader.ok()) {
        return false;
    }
    offset += reader.cursor();
    return true;
}

bool try_read_var_long(std::span<const std::uint8_t> input, std::size_t& offset, std::int64_t& out) noexcept {
    Reader reader(input.subspan(offset));
    out = reader.read_var_long();
    if (!reader.ok()) {
        return false;
    }
    offset += reader.cursor();
    return true;
}

bool try_read_string(std::span<const std::uint8_t> input,
                     std::size_t& offset,
                     std::string& out,
                     const std::int32_t max_len) noexcept {
    Reader reader(input.subspan(offset));
    out = reader.read_string(max_len);
    if (!reader.ok()) {
        return false;
    }
    offset += reader.cursor();
    return true;
}

std::int8_t  read_byte(std::span<const std::uint8_t> input, std::size_t& offset)  { return with_reader(input, offset, [](Reader& r) { return r.read_i8(); }); }
std::uint8_t read_ubyte(std::span<const std::uint8_t> input, std::size_t& offset) { return with_reader(input, offset, [](Reader& r) { return r.read_u8(); }); }
std::int16_t  read_short(std::span<const std::uint8_t> input, std::size_t& offset)  { return with_reader(input, offset, [](Reader& r) { return r.read_i16_be(); }); }
std::uint16_t read_ushort(std::span<const std::uint8_t> input, std::size_t& offset) { return with_reader(input, offset, [](Reader& r) { return r.read_u16_be(); }); }
std::int32_t  read_int(std::span<const std::uint8_t> input, std::size_t& offset)  { return with_reader(input, offset, [](Reader& r) { return r.read_i32_be(); }); }
std::uint32_t read_uint(std::span<const std::uint8_t> input, std::size_t& offset) { return with_reader(input, offset, [](Reader& r) { return r.read_u32_be(); }); }
std::int64_t  read_long(std::span<const std::uint8_t> input, std::size_t& offset)  { return with_reader(input, offset, [](Reader& r) { return r.read_i64_be(); }); }
std::uint64_t read_ulong(std::span<const std::uint8_t> input, std::size_t& offset) { return with_reader(input, offset, [](Reader& r) { return r.read_u64_be(); }); }

float  read_float(std::span<const std::uint8_t> input, std::size_t& offset)  { return with_reader(input, offset, [](Reader& r) { return r.read_f32_be(); }); }
double read_double(std::span<const std::uint8_t> input, std::size_t& offset) { return with_reader(input, offset, [](Reader& r) { return r.read_f64_be(); }); }

bool read_bool(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) { return r.read_bool(); });
}

std::uint16_t read_u16(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) { return r.read_u16_be(); });
}

std::string read_string(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) { return r.read_string(); });
}

std::vector<std::uint8_t> read_bytes(std::span<const std::uint8_t> input, std::size_t& offset) {
    return with_reader(input, offset, [](Reader& r) {
        const auto view = r.read_bytes();
        return std::vector<std::uint8_t>(view.begin(), view.end());
    });
}

std::vector<std::uint8_t> read_byte_array(std::span<const std::uint8_t> input, std::size_t& offset) {
    return read_bytes(input, offset);
}

float read_angle(std::span<const std::uint8_t> input, std::size_t& offset) {
    return static_cast<float>(read_ubyte(input, offset)) / 256.0f;
}

std::pair<float, float> read_rotation(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto yaw = read_angle(input, offset);
    const auto pitch = read_angle(input, offset);
    return {yaw, pitch};
}

std::vector<std::int32_t> read_var_int_array(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto count = read_var_int(input, offset);
    if (count < 0) {
        throw DecodeError("codec: negative VarInt-array length");
    }
    std::vector<std::int32_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        out.push_back(read_var_int(input, offset));
    }
    return out;
}

std::vector<std::int64_t> read_var_long_array(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto count = read_var_int(input, offset);
    if (count < 0) {
        throw DecodeError("codec: negative VarLong-array length");
    }
    std::vector<std::int64_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        out.push_back(read_var_long(input, offset));
    }
    return out;
}

// ===== Frame encoding / decoding ============================================

std::vector<std::uint8_t> encode_frame(std::int32_t packet_id, std::span<const std::uint8_t> payload) {
    // Compute exact byte budget up front; one allocation.
    const auto id_size = var_int_size(packet_id);
    const auto body_len = id_size + static_cast<std::int32_t>(payload.size());
    const auto len_size = var_int_size(body_len);

    std::vector<std::uint8_t> frame;
    frame.reserve(static_cast<std::size_t>(len_size + body_len));

    Writer w(frame);
    w.write_var_int(body_len);
    w.write_var_int(packet_id);
    w.write_raw(payload);
    return frame;
}

bool try_decode_frame(std::span<const std::uint8_t> input,
                      std::size_t& consumed,
                      EncodedFrame& frame) {
    consumed = 0;

    // Step 1: peek the outer length VarInt without committing to the buffer.
    Reader length_reader(input);
    const auto length = length_reader.read_var_int();
    if (length_reader.error() == ReadError::truncated) {
        // Need more data; not malformed.
        return false;
    }
    if (!length_reader.ok()) {
        // Overlong VarInt or other structural error - not recoverable
        // by reading more bytes, but we don't throw here.
        return false;
    }
    if (length < 0 || length > limits::max_packet_length) {
        return false;
    }

    const auto length_bytes = length_reader.cursor();
    const auto payload_size = static_cast<std::size_t>(length);

    if (input.size() < length_bytes + payload_size) {
        return false; // wait for more data
    }

    // Step 2: parse inner body (packet_id varint + payload bytes).
    Reader body_reader(input.subspan(length_bytes, payload_size));
    const auto packet_id = body_reader.read_var_int();
    if (!body_reader.ok()) {
        return false;
    }

    frame.packet_id = packet_id;
    const auto body_remaining = body_reader.remaining_bytes();
    const auto* begin = input.data() + length_bytes + body_reader.cursor();
    frame.payload.assign(begin, begin + body_remaining);
    consumed = length_bytes + payload_size;
    return true;
}

// ===== Compression (zlib) ===================================================

std::vector<std::uint8_t> compress_packet(std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return {};
    }

    // compressBound is an upper bound for the deflate output size.
    const auto bound = ::compressBound(static_cast<uLong>(data.size()));
    std::vector<std::uint8_t> out(static_cast<std::size_t>(bound));
    uLongf out_size = bound;

    const auto rc = ::compress2(
        out.data(),
        &out_size,
        data.data(),
        static_cast<uLong>(data.size()),
        Z_DEFAULT_COMPRESSION);

    if (rc != Z_OK) {
        throw EncodeError("codec: zlib compress2 failed (rc=" + std::to_string(rc) + ")");
    }
    out.resize(static_cast<std::size_t>(out_size));
    return out;
}

std::vector<std::uint8_t> decompress_packet(std::span<const std::uint8_t> compressed_data,
                                            std::int32_t max_output_length) {
    if (compressed_data.empty()) {
        return {};
    }
    if (max_output_length <= 0) {
        throw DecodeError("codec: decompress_packet called with non-positive max_output_length");
    }

    // Use inflate with growth + ceiling so we can abort on a zip-bomb rather
    // than letting uncompress() try to size a multi-gigabyte buffer.
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed_data.data());
    stream.avail_in = static_cast<uInt>(compressed_data.size());

    if (::inflateInit(&stream) != Z_OK) {
        throw DecodeError("codec: zlib inflateInit failed");
    }

    std::vector<std::uint8_t> out;
    // Seed with a reasonable initial capacity bounded by max_output_length.
    const auto seed = std::min(static_cast<std::size_t>(max_output_length),
                               compressed_data.size() * 4U + 64U);
    out.resize(seed);

    std::size_t written = 0;
    int rc = Z_OK;
    while (rc != Z_STREAM_END) {
        if (written == out.size()) {
            if (out.size() >= static_cast<std::size_t>(max_output_length)) {
                ::inflateEnd(&stream);
                throw DecodeError("codec: decompressed payload exceeds limit");
            }
            const auto next = std::min(out.size() * 2U,
                                       static_cast<std::size_t>(max_output_length));
            out.resize(next);
        }
        stream.next_out = out.data() + written;
        stream.avail_out = static_cast<uInt>(out.size() - written);

        const auto before = stream.avail_out;
        rc = ::inflate(&stream, Z_NO_FLUSH);
        written += (before - stream.avail_out);

        if (rc == Z_STREAM_END) break;
        if (rc == Z_OK || rc == Z_BUF_ERROR) continue;

        ::inflateEnd(&stream);
        throw DecodeError("codec: zlib inflate failed (rc=" + std::to_string(rc) + ")");
    }
    ::inflateEnd(&stream);
    out.resize(written);
    return out;
}

std::vector<std::uint8_t> encode_frame_compressed(std::int32_t packet_id,
                                                  std::span<const std::uint8_t> payload,
                                                  std::int32_t compression_threshold) {
    if (compression_threshold < 0) {
        return encode_frame(packet_id, payload);
    }

    // The "uncompressed body" is (varint packet_id || payload bytes).
    const auto id_size = var_int_size(packet_id);
    const auto body_len = id_size + static_cast<std::int32_t>(payload.size());

    if (body_len >= compression_threshold) {
        std::vector<std::uint8_t> body;
        body.reserve(static_cast<std::size_t>(body_len));
        Writer body_w(body);
        body_w.write_var_int(packet_id);
        body_w.write_raw(payload);

        const auto compressed = compress_packet(body);
        const auto data_len_field_size = var_int_size(body_len);
        const auto outer_len = data_len_field_size + static_cast<std::int32_t>(compressed.size());

        std::vector<std::uint8_t> frame;
        frame.reserve(static_cast<std::size_t>(var_int_size(outer_len) + outer_len));
        Writer w(frame);
        w.write_var_int(outer_len);
        w.write_var_int(body_len);
        w.write_raw(compressed);
        return frame;
    }

    // Below threshold: data_length = 0 indicates "this body is not compressed".
    const auto data_len_field_size = var_int_size(0);
    const auto outer_len = data_len_field_size + body_len;

    std::vector<std::uint8_t> frame;
    frame.reserve(static_cast<std::size_t>(var_int_size(outer_len) + outer_len));
    Writer w(frame);
    w.write_var_int(outer_len);
    w.write_var_int(0); // data_length = 0
    w.write_var_int(packet_id);
    w.write_raw(payload);
    return frame;
}

bool try_decode_frame_compressed(std::span<const std::uint8_t> input,
                                 std::size_t& consumed,
                                 std::int32_t compression_threshold,
                                 EncodedFrame& frame) {
    if (compression_threshold < 0) {
        return try_decode_frame(input, consumed, frame);
    }
    consumed = 0;

    // Outer frame_length VarInt.
    Reader length_reader(input);
    const auto frame_length = length_reader.read_var_int();
    if (length_reader.error() == ReadError::truncated) return false;
    if (!length_reader.ok()) return false;
    if (frame_length < 0 || frame_length > limits::max_packet_length) {
        return false;
    }
    const auto length_bytes = length_reader.cursor();
    if (input.size() < length_bytes + static_cast<std::size_t>(frame_length)) {
        return false;
    }
    const auto frame_data = input.subspan(length_bytes, static_cast<std::size_t>(frame_length));

    Reader body_reader(frame_data);
    const auto data_length = body_reader.read_var_int();
    if (!body_reader.ok()) {
        return false;
    }

    if (data_length == 0) {
        // Uncompressed body: (varint packet_id || payload bytes).
        const auto packet_id = body_reader.read_var_int();
        if (!body_reader.ok()) return false;
        frame.packet_id = packet_id;
        const auto rem = body_reader.remaining_bytes();
        const auto* begin = frame_data.data() + body_reader.cursor();
        frame.payload.assign(begin, begin + rem);
        consumed = length_bytes + static_cast<std::size_t>(frame_length);
        return true;
    }

    if (data_length < 0 || data_length > limits::max_decompressed_length) {
        return false;
    }

    // Compressed body. Decompress with the declared data_length as the ceiling.
    const auto compressed = frame_data.subspan(body_reader.cursor());
    std::vector<std::uint8_t> decompressed;
    try {
        decompressed = decompress_packet(compressed, data_length);
    } catch (const DecodeError&) {
        return false;
    }
    // Spec says data_length must equal the actual decompressed length.
    if (static_cast<std::int32_t>(decompressed.size()) != data_length) {
        return false;
    }

    Reader inner(decompressed);
    const auto packet_id = inner.read_var_int();
    if (!inner.ok()) return false;
    frame.packet_id = packet_id;
    const auto rem = inner.remaining_bytes();
    const auto* begin = decompressed.data() + inner.cursor();
    frame.payload.assign(begin, begin + rem);
    consumed = length_bytes + static_cast<std::size_t>(frame_length);
    return true;
}

} // namespace kprotocol::codec

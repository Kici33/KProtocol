#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/version.hpp"

#include <cstdint>
#include <span>

namespace kprotocol {

// Minecraft enables zlib framing only after login.clientbound.compress.
enum class CompressionPhase : std::uint8_t {
    disabled,   // threshold < 0, or compression not negotiated yet
    enabled     // threshold >= 0; use compressed frame codec
};

// Per-connection frame decoder: plain pre-login, compressed after Set Compression.
class FrameDecoder {
public:
    FrameDecoder() = default;
    explicit FrameDecoder(std::int32_t compression_threshold) noexcept
        : threshold_(compression_threshold) {
        phase_ = compression_threshold >= 0 ? CompressionPhase::enabled
                                          : CompressionPhase::disabled;
    }

    [[nodiscard]] CompressionPhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::int32_t threshold() const noexcept { return threshold_; }

    void set_threshold(std::int32_t compression_threshold) noexcept {
        threshold_ = compression_threshold;
        phase_ = compression_threshold >= 0 ? CompressionPhase::enabled
                                            : CompressionPhase::disabled;
    }

    [[nodiscard]] bool try_decode(std::span<const std::uint8_t> input,
                                  std::size_t& consumed,
                                  codec::EncodedFrame& frame) const noexcept {
        if (phase_ == CompressionPhase::enabled) {
            return codec::try_decode_frame_compressed(input, consumed, threshold_, frame);
        }
        return codec::try_decode_frame(input, consumed, frame);
    }

private:
    CompressionPhase phase_{CompressionPhase::disabled};
    std::int32_t threshold_{-1};
};

} // namespace kprotocol

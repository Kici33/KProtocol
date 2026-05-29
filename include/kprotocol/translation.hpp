#pragma once

#include "kprotocol/internal/packet_key.hpp"
#include "kprotocol/packet.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace kprotocol {

class PacketTranslator {
public:
    using TranslationFn = std::function<PacketFields(const PacketFields&)>;

    enum class Status : std::uint8_t {
        identity,
        applied,
        missing_packet_key,
        missing_rule
    };

    struct Result {
        Packet packet;
        Status status{Status::identity};

        [[nodiscard]] bool translated() const noexcept { return status == Status::applied; }
        [[nodiscard]] bool missing() const noexcept {
            return status == Status::missing_packet_key || status == Status::missing_rule;
        }
    };

    void register_translation(std::string_view key, ProtocolVersion from, ProtocolVersion to, TranslationFn fn);
    [[nodiscard]] bool has_translation(std::string_view key, ProtocolVersion from, ProtocolVersion to) const;
    [[nodiscard]] std::size_t registered_count() const noexcept { return translations_.size(); }
    void set_require_explicit_translation(bool enabled) noexcept { require_explicit_translation_ = enabled; }
    [[nodiscard]] bool require_explicit_translation() const noexcept { return require_explicit_translation_; }
    [[nodiscard]] Result translate_checked(const Packet& packet, ProtocolVersion from, ProtocolVersion to) const;
    Packet translate(const Packet& packet, ProtocolVersion from, ProtocolVersion to) const;

private:
    struct TranslationKey {
        internal::PacketKeyHandle packet_key{};
        ProtocolVersion from{};
        ProtocolVersion to{};

        bool operator==(const TranslationKey& rhs) const noexcept = default;
    };

    struct TranslationKeyHash {
        std::size_t operator()(const TranslationKey& value) const noexcept;
    };

    std::unordered_map<TranslationKey, TranslationFn, TranslationKeyHash> translations_;
    bool require_explicit_translation_{false};
};

class MissingTranslationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace kprotocol

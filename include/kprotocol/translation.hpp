#pragma once

#include "kprotocol/internal/packet_key.hpp"
#include "kprotocol/packet.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace kprotocol {

class PacketTranslator {
public:
    using TranslationFn = std::function<PacketFields(const PacketFields&)>;

    void register_translation(std::string_view key, ProtocolVersion from, ProtocolVersion to, TranslationFn fn);
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
};

} // namespace kprotocol

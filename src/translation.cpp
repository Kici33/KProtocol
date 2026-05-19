#include "kprotocol/translation.hpp"

#include <functional>

namespace kprotocol {

std::size_t PacketTranslator::TranslationKeyHash::operator()(const TranslationKey& value) const noexcept {
    std::size_t seed = 0;
    auto combine = [&seed](const std::size_t h) {
        seed ^= h + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    };
    combine(std::hash<std::string>{}(value.packet_key));
    combine(std::hash<std::int32_t>{}(protocol_number(value.from)));
    combine(std::hash<std::int32_t>{}(protocol_number(value.to)));
    return seed;
}

void PacketTranslator::register_translation(
    const PacketKey& key,
    const ProtocolVersion from,
    const ProtocolVersion to,
    TranslationFn fn) {
    translations_.insert_or_assign(TranslationKey{key, from, to}, std::move(fn));
}

Packet PacketTranslator::translate(const Packet& packet, const ProtocolVersion from, const ProtocolVersion to) const {
    if (from == to) {
        return packet;
    }
    const TranslationKey key{packet.key, from, to};
    const auto it = translations_.find(key);
    if (it == translations_.end()) {
        return packet;
    }

    Packet translated = packet;
    translated.fields = it->second(packet.fields);
    return translated;
}

} // namespace kprotocol

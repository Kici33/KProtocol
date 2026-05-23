#include "kprotocol/translation.hpp"

#include <functional>
#include <string>

namespace kprotocol {

std::size_t PacketTranslator::TranslationKeyHash::operator()(const TranslationKey& value) const noexcept {
    std::size_t seed = 0;
    auto combine = [&seed](const std::size_t h) {
        seed ^= h + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    };
    combine(std::hash<std::uint32_t>{}(value.packet_key.value));
    combine(std::hash<std::int32_t>{}(protocol_number(value.from)));
    combine(std::hash<std::int32_t>{}(protocol_number(value.to)));
    return seed;
}

void PacketTranslator::register_translation(
    std::string_view key,
    const ProtocolVersion from,
    const ProtocolVersion to,
    TranslationFn fn) {
    const auto handle = internal::PacketKeyInterner::instance().intern(key);
    translations_.insert_or_assign(TranslationKey{handle, from, to}, std::move(fn));
}

Packet PacketTranslator::translate(const Packet& packet, const ProtocolVersion from, const ProtocolVersion to) const {
    if (from == to) {
        return packet;
    }
    // Read-only lookup: if the key has never been interned, we definitely have
    // no translation registered for it.
    const auto handle = internal::PacketKeyInterner::instance().find(packet.key);
    if (!handle.has_value()) {
        return packet;
    }
    const TranslationKey key{*handle, from, to};
    const auto it = translations_.find(key);
    if (it == translations_.end()) {
        return packet;
    }

    Packet translated = packet;
    translated.fields = it->second(packet.fields);
    return translated;
}

} // namespace kprotocol

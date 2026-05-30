#include "kprotocol/translation.hpp"

#include <functional>
#include <string>
#include <utility>

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

bool PacketTranslator::has_translation(
    std::string_view key,
    const ProtocolVersion from,
    const ProtocolVersion to) const {
    if (from == to) {
        return true;
    }
    const auto handle = internal::PacketKeyInterner::instance().find(key);
    if (!handle.has_value()) {
        return false;
    }
    return translations_.contains(TranslationKey{*handle, from, to});
}

PacketTranslator::Result PacketTranslator::translate_checked(
    const Packet& packet,
    const ProtocolVersion from,
    const ProtocolVersion to) const {
    return translate_checked(packet, from, to, require_explicit_translation_);
}

PacketTranslator::Result PacketTranslator::translate_checked(
    const Packet& packet,
    const ProtocolVersion from,
    const ProtocolVersion to,
    const bool require_explicit_translation) const {
    if (from == to) {
        return Result{packet, Status::identity};
    }

    const auto handle = internal::PacketKeyInterner::instance().find(packet.key);
    if (!handle.has_value()) {
        if (require_explicit_translation) {
            throw MissingTranslationError("No translation registered for uninterned packet key: " + packet.key);
        }
        return Result{packet, Status::missing_packet_key};
    }

    const TranslationKey key{*handle, from, to};
    const auto it = translations_.find(key);
    if (it == translations_.end()) {
        if (require_explicit_translation) {
            throw MissingTranslationError("No translation registered for packet key: " + packet.key);
        }
        return Result{packet, Status::missing_rule};
    }

    Packet translated = packet;
    translated.fields = it->second(packet.fields);
    return Result{std::move(translated), Status::applied};
}

Packet PacketTranslator::translate(const Packet& packet, const ProtocolVersion from, const ProtocolVersion to) const {
    return translate_checked(packet, from, to).packet;
}

Packet PacketTranslator::translate(
    const Packet& packet,
    const ProtocolVersion from,
    const ProtocolVersion to,
    const bool require_explicit_translation) const {
    return translate_checked(packet, from, to, require_explicit_translation).packet;
}

} // namespace kprotocol

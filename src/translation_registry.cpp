#include "kprotocol/translation_registry.hpp"

#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

namespace kprotocol::detail {

std::optional<std::int32_t> lookup_block_mapping(
    KnownVersion from,
    KnownVersion to,
    std::int32_t source_id) noexcept;

} // namespace kprotocol::detail

namespace kprotocol {

bool TranslationRegistry::initialized_ = false;

namespace {

PacketFields translate_block_type_field(
    const PacketFields& fields,
    const ProtocolVersion from,
    const ProtocolVersion to) {

    PacketFields out = fields;
    const auto it = out.named.find("type");
    if (it == out.named.end()) {
        return out;
    }
    if (const auto* block_id = std::get_if<std::int32_t>(&it->second); block_id != nullptr) {
        it->second = TranslationRegistry::map_block_id(from, to, *block_id);
    }
    return out;
}

void register_block_change_pair(PacketTranslator& translator, ProtocolVersion from, ProtocolVersion to) {
    if (from == to) {
        return;
    }
    translator.register_translation(
        "play.clientbound.block_change",
        from,
        to,
        [from, to](const PacketFields& fields) {
            return translate_block_type_field(fields, from, to);
        });
}

constexpr ProtocolVersion kCatalogVersions[] = {
    ProtocolVersion::v1_8,
    ProtocolVersion::v1_12_2,
    ProtocolVersion::v1_13,
    ProtocolVersion::v1_14,
    ProtocolVersion::v1_16_5,
    ProtocolVersion::v1_17,
    ProtocolVersion::v1_18,
    ProtocolVersion::v1_19,
    ProtocolVersion::v1_20_2,
    ProtocolVersion::v1_20_4,
    ProtocolVersion::v1_21_1,
    ProtocolVersion::v1_21_4,
    ProtocolVersion::v1_21_5,
    ProtocolVersion::v1_21_6,
    ProtocolVersion::v1_21_7,
    ProtocolVersion::v1_21_9,
    ProtocolVersion::v1_21_11,
};

} // namespace

void TranslationRegistry::initialize_all(PacketTranslator& translator) {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    for (const auto from : kCatalogVersions) {
        for (const auto to : kCatalogVersions) {
            register_block_change_pair(translator, from, to);
        }
    }

    translator.register_translation(
        "play.clientbound.scoreboard_display_objective",
        ProtocolVersion::v1_21_1,
        ProtocolVersion::v1_8,
        [](const PacketFields& fields) -> PacketFields { return fields; });

    translator.register_translation(
        "play.clientbound.scoreboard_display_objective",
        ProtocolVersion::v1_8,
        ProtocolVersion::v1_21_1,
        [](const PacketFields& fields) -> PacketFields { return fields; });
}

std::int32_t TranslationRegistry::map_block_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {

    if (from == to) {
        return source_id;
    }

    if (const auto mapped = detail::lookup_block_mapping(
            to_known_version(from), to_known_version(to), source_id);
        mapped.has_value()) {
        return *mapped;
    }
    return source_id;
}

std::int32_t TranslationRegistry::map_item_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {
    (void)from;
    (void)to;
    return source_id;
}

std::int32_t TranslationRegistry::map_entity_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {
    (void)from;
    (void)to;
    return source_id;
}

std::int32_t TranslationRegistry::map_particle_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {
    (void)from;
    (void)to;
    return source_id;
}

} // namespace kprotocol

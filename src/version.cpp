#include "kprotocol/version.hpp"

#include <array>
#include <string>
#include <unordered_map>

namespace kprotocol {

namespace {

constexpr std::array<std::int32_t, static_cast<std::size_t>(KnownVersion::count)> kWireByKnown = {
#define KPROTOCOL_X(name, wire, display) wire,
    KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
};

const std::unordered_map<std::int32_t, std::string_view>& wire_to_name() {
    static const std::unordered_map<std::int32_t, std::string_view> map = [] {
        std::unordered_map<std::int32_t, std::string_view> m;
#define KPROTOCOL_X(name, wire, display) m.emplace(wire, display);
        KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
        return m;
    }();
    return map;
}

const std::unordered_map<std::int32_t, KnownVersion>& wire_to_known() {
    static const std::unordered_map<std::int32_t, KnownVersion> map = [] {
        std::unordered_map<std::int32_t, KnownVersion> m;
#define KPROTOCOL_X(name, wire, display) \
    m.emplace(wire, KnownVersion::name);
        KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
        return m;
    }();
    return map;
}

} // namespace

constexpr std::int32_t wire_number(const KnownVersion version) noexcept {
    const auto index = static_cast<std::size_t>(version);
    if (index >= kWireByKnown.size()) {
        return 0;
    }
    return kWireByKnown[index];
}

std::optional<KnownVersion> try_from_wire(const WireProtocol wire) noexcept {
    const auto& m = wire_to_known();
    if (const auto it = m.find(wire.value); it != m.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<ProtocolVersion> try_from_wire(const std::int32_t wire) noexcept {
    if (const auto known = try_from_wire(WireProtocol{wire}); known.has_value()) {
        return static_cast<ProtocolVersion>(wire_number(*known));
    }
    return std::nullopt;
}

bool is_known_protocol(const WireProtocol wire) noexcept {
    return try_from_wire(wire).has_value();
}

bool is_known_protocol(const std::int32_t wire) noexcept {
    return is_known_protocol(WireProtocol{wire});
}

std::string name_of(const std::int32_t wire) {
    const auto& m = wire_to_name();
    if (const auto it = m.find(wire); it != m.end()) {
        return std::string(it->second);
    }
    return "protocol_" + std::to_string(wire);
}

std::string name_of(const WireProtocol wire) {
    return name_of(wire.value);
}

std::string name_of(const ProtocolVersion version) {
    return name_of(protocol_number(version));
}

std::string name_of(const KnownVersion version) {
    return name_of(wire_number(version));
}

ProtocolVersion catalog_anchor_for(const std::int32_t wire) {
    return catalog_anchor_for(WireProtocol{wire});
}

ProtocolVersion catalog_anchor_for(const WireProtocol wire) noexcept {
    std::int32_t anchor_wire = wire_number(KnownVersion::v1_8);
    if (wire.value < anchor_wire) {
        return protocol_version_from_wire(wire.value);
    }
#define KPROTOCOL_X(name, known_wire, display) \
    if (known_wire <= wire.value && known_wire >= anchor_wire) { \
        anchor_wire = known_wire; \
    }
    KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
    return protocol_version_from_wire(anchor_wire);
}

KnownVersion catalog_anchor_known_for(const WireProtocol wire) noexcept {
    const auto anchor = catalog_anchor_for(wire);
    if (const auto known = try_from_wire(WireProtocol{protocol_number(anchor)}); known.has_value()) {
        return *known;
    }
    return KnownVersion::v1_21_11;
}

} // namespace kprotocol

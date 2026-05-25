#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kprotocol {

// X-macro for known Minecraft Java Edition protocol versions.
// Schema: X(enumerator, wire_number, display_name)
//
// Each wire number appears at most once. Patch releases that did NOT bump the
// protocol number (e.g. 1.21 and 1.21.1 both = 767) collapse onto a single
// canonical name. To add a new version, append a single X() line below and
// recompile - the enum, lookup table, and human-readable name list all update.
#define KPROTOCOL_FOR_EACH_KNOWN_VERSION(X) \
    X(v1_8,     47,  "1.8")                 \
    X(v1_9,     107, "1.9")                 \
    X(v1_9_2,   109, "1.9.2")               \
    X(v1_9_4,   110, "1.9.4")               \
    X(v1_10,    210, "1.10")                \
    X(v1_11,    315, "1.11")                \
    X(v1_11_2,  316, "1.11.2")              \
    X(v1_12,    335, "1.12")                \
    X(v1_12_1,  338, "1.12.1")              \
    X(v1_12_2,  340, "1.12.2")              \
    X(v1_13,    393, "1.13")                \
    X(v1_13_2,  404, "1.13.2")              \
    X(v1_14,    477, "1.14")                \
    X(v1_14_4,  498, "1.14.4")              \
    X(v1_15,    573, "1.15")                \
    X(v1_15_2,  578, "1.15.2")              \
    X(v1_16,    735, "1.16")                \
    X(v1_16_2,  751, "1.16.2")              \
    X(v1_16_5,  754, "1.16.5")              \
    X(v1_17,    755, "1.17")                \
    X(v1_17_1,  756, "1.17.1")              \
    X(v1_18,    757, "1.18")                \
    X(v1_18_2,  758, "1.18.2")              \
    X(v1_19,    759, "1.19")                \
    X(v1_19_2,  760, "1.19.2")              \
    X(v1_19_3,  761, "1.19.3")              \
    X(v1_19_4,  762, "1.19.4")              \
    X(v1_20,    763, "1.20")                \
    X(v1_20_2,  764, "1.20.2")              \
    X(v1_20_4,  765, "1.20.4")              \
    X(v1_20_5,  766, "1.20.5")              \
    X(v1_21_1,  767, "1.21.1")              \
    X(v1_21_3,  768, "1.21.3")              \
    X(v1_21_4,  769, "1.21.4")              \
    X(v1_21_5,  770, "1.21.5")              \
    X(v1_21_6,  771, "1.21.6")              \
    X(v1_21_7,  772, "1.21.7")              \
    X(v1_21_9,  773, "1.21.9")              \
    X(v1_21_11, 774, "1.21.11")

enum class ProtocolVersion : std::int32_t {
#define KPROTOCOL_X(name, wire, display) name = wire,
    KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
};

constexpr std::int32_t protocol_number(const ProtocolVersion version) noexcept {
    return static_cast<std::int32_t>(version);
}

// Try to map a raw wire number to a known enumerator. Returns std::nullopt
// for protocol numbers the library has not been built with explicit support for.
[[nodiscard]] std::optional<ProtocolVersion> try_from_wire(std::int32_t wire) noexcept;

// True iff the wire number corresponds to a known enumerator.
[[nodiscard]] bool is_known_protocol(std::int32_t wire) noexcept;

// Human-readable name. For known versions this is the canonical display
// string (e.g. "1.20.4"); for unknown versions this returns "protocol_<wire>"
// so logs remain useful.
[[nodiscard]] std::string name_of(ProtocolVersion version);
[[nodiscard]] std::string name_of(std::int32_t wire);

// Highest wire number compiled into the generated packet catalog.
[[nodiscard]] constexpr ProtocolVersion latest_catalog_version() noexcept {
    return ProtocolVersion::v1_21_11;
}

// Map a client wire number to the nearest catalog anchor (largest known wire <= wire).
[[nodiscard]] ProtocolVersion catalog_anchor_for(std::int32_t wire) noexcept;

} // namespace kprotocol

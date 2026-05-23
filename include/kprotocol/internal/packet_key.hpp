// SPDX-License-Identifier: MIT
//
// Internal-only: strong typedef + process-wide string interner for PacketKey.
//
// Public API exposes packet keys as std::string (kprotocol::PacketKey), which
// is ergonomic for callers but makes every hot-path lookup hash a string AND
// pay an equality comparison. The registry, translator, and any future scope
// map can re-key their internals on PacketKeyHandle: a thin 32-bit handle into
// a process-wide table maintained by PacketKeyInterner.
//
// Stability guarantees:
//   - Once interned, a string -> handle mapping never changes for the lifetime
//     of the process.
//   - Handle 0 is reserved as the "invalid" sentinel; valid handles start at 1.
//   - The interner is thread-safe (mutex). Lookups and inserts can happen
//     from any thread, but a hot path that only ever does intern-on-startup
//     and lookups thereafter pays no synchronisation cost beyond a shared lock.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace kprotocol::internal {

struct PacketKeyHandle {
    std::uint32_t value{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    friend constexpr bool operator==(PacketKeyHandle a, PacketKeyHandle b) noexcept {
        return a.value == b.value;
    }
    friend constexpr bool operator!=(PacketKeyHandle a, PacketKeyHandle b) noexcept {
        return a.value != b.value;
    }
};

class PacketKeyInterner {
public:
    // Process-wide singleton. Cheap to retrieve - thread-safe Meyers singleton.
    static PacketKeyInterner& instance();

    // Idempotently registers the string and returns its handle.
    // Subsequent calls with the same string always yield the same handle.
    PacketKeyHandle intern(std::string_view key);

    // Read-only lookup. Returns nullopt if the string has never been interned.
    // Cheap for the steady state where every packet key has been registered
    // at startup and lookups are pure reads.
    [[nodiscard]] std::optional<PacketKeyHandle> find(std::string_view key) const;

    // Reverse map: handle -> canonical interned string. Returns an empty
    // string_view for invalid handles. The string lives for the process
    // lifetime - safe to store the view.
    [[nodiscard]] std::string_view lookup(PacketKeyHandle handle) const;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    PacketKeyInterner() = default;
    PacketKeyInterner(const PacketKeyInterner&) = delete;
    PacketKeyInterner& operator=(const PacketKeyInterner&) = delete;

    struct Impl;
    static Impl& impl();
};

} // namespace kprotocol::internal

namespace std {
template <>
struct hash<::kprotocol::internal::PacketKeyHandle> {
    std::size_t operator()(::kprotocol::internal::PacketKeyHandle h) const noexcept {
        return std::hash<std::uint32_t>{}(h.value);
    }
};
} // namespace std

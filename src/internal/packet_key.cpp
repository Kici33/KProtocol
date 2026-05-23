#include "kprotocol/internal/packet_key.hpp"

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace kprotocol::internal {

// Transparent hash/eq so we can look up by string_view without allocating a
// std::string. Requires C++20 unordered_map heterogeneous-lookup support.
namespace {

struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view v) const noexcept {
        return std::hash<std::string_view>{}(v);
    }
    std::size_t operator()(const std::string& v) const noexcept {
        return std::hash<std::string_view>{}(v);
    }
};

struct StringEqual {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
    bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
    bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
    bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
};

} // namespace

struct PacketKeyInterner::Impl {
    mutable std::shared_mutex mutex;

    // string -> handle. Stable across pointer movement: the key strings live
    // here, and reverse_table indexes into the by_name keys.
    std::unordered_map<std::string, PacketKeyHandle, StringHash, StringEqual> by_name;

    // handle.value -> stable string_view into by_name's keys. Indexed by
    // (handle.value - 1) since handle 0 is reserved.
    std::deque<std::string_view> reverse_table;
};

PacketKeyInterner::Impl& PacketKeyInterner::impl() {
    static Impl singleton;
    return singleton;
}

PacketKeyInterner& PacketKeyInterner::instance() {
    static PacketKeyInterner singleton;
    return singleton;
}

PacketKeyHandle PacketKeyInterner::intern(std::string_view key) {
    auto& s = impl();
    {
        std::shared_lock read_lock(s.mutex);
        if (const auto it = s.by_name.find(key); it != s.by_name.end()) {
            return it->second;
        }
    }
    std::unique_lock write_lock(s.mutex);
    // Recheck after upgrading - another thread may have inserted.
    if (const auto it = s.by_name.find(key); it != s.by_name.end()) {
        return it->second;
    }
    const auto handle_value = static_cast<std::uint32_t>(s.reverse_table.size() + 1U);
    PacketKeyHandle handle{handle_value};
    auto [emplaced, _] = s.by_name.emplace(std::string(key), handle);
    s.reverse_table.push_back(std::string_view{emplaced->first});
    return handle;
}

std::optional<PacketKeyHandle> PacketKeyInterner::find(std::string_view key) const {
    const auto& s = impl();
    std::shared_lock read_lock(s.mutex);
    if (const auto it = s.by_name.find(key); it != s.by_name.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string_view PacketKeyInterner::lookup(PacketKeyHandle handle) const {
    if (!handle.valid()) {
        return {};
    }
    const auto& s = impl();
    std::shared_lock read_lock(s.mutex);
    const auto idx = static_cast<std::size_t>(handle.value - 1U);
    if (idx >= s.reverse_table.size()) {
        return {};
    }
    return s.reverse_table[idx];
}

std::size_t PacketKeyInterner::size() const noexcept {
    const auto& s = impl();
    std::shared_lock read_lock(s.mutex);
    return s.reverse_table.size();
}

} // namespace kprotocol::internal

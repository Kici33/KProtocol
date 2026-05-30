#include "kprotocol/version.hpp"

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace kprotocol::detail {
namespace {

constexpr std::uint32_t kMagic = 0x4d42504b; // 'KPBM'
constexpr std::uint32_t kFormatVersion = 1;

struct BlockMappingEntry {
    std::int32_t from;
    std::int32_t to;
};

struct BlockMappingTable {
    std::uint16_t from_index;
    std::uint16_t to_index;
    std::uint32_t entry_offset;
    std::uint32_t entry_count;
};

std::vector<std::uint8_t> g_blob;
bool g_loaded = false;
std::optional<std::filesystem::path> g_configured_path;
std::mutex g_mutex;

std::uint16_t known_index(KnownVersion version) noexcept {
    return static_cast<std::uint16_t>(version);
}

bool load_blob_from_gzip(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::vector<std::uint8_t> compressed(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    if (compressed.empty()) {
        return false;
    }

    z_stream strm{};
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        return false;
    }

    strm.next_in = compressed.data();
    strm.avail_in = static_cast<uInt>(compressed.size());

    std::vector<std::uint8_t> out(compressed.size() * 8);
    int rc = Z_OK;
    while (out.size() < 256U * 1024U * 1024U) {
        strm.next_out = out.data() + strm.total_out;
        strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);
        rc = inflate(&strm, Z_FINISH);
        if (rc == Z_STREAM_END) {
            break;
        }
        if (rc == Z_BUF_ERROR && strm.avail_out == 0) {
            out.resize(out.size() * 2);
            continue;
        }
        inflateEnd(&strm);
        return false;
    }
    inflateEnd(&strm);
    if (rc != Z_STREAM_END) {
        return false;
    }
    out.resize(strm.total_out);
    g_blob = std::move(out);
    return true;
}

std::optional<std::filesystem::path> executable_path() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD size = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (size == 0) {
            return std::nullopt;
        }
        if (size < buffer.size()) {
            buffer.resize(size);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    std::uint32_t size = static_cast<std::uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return std::nullopt;
        }
    }
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(buffer.data(), ec);
    if (ec) {
        return std::filesystem::path(buffer.data());
    }
    return canonical;
#else
    std::vector<char> buffer(1024);
    for (;;) {
        const ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (size < 0) {
            return std::nullopt;
        }
        if (static_cast<std::size_t>(size) < buffer.size()) {
            return std::filesystem::path(
                std::string(buffer.data(), static_cast<std::size_t>(size)));
        }
        buffer.resize(buffer.size() * 2);
    }
#endif
}

std::vector<std::filesystem::path> mapping_candidates() {
    constexpr auto filename = "translation_mappings.bin.gz";
    std::vector<std::filesystem::path> candidates;

    if (g_configured_path.has_value()) {
        candidates.push_back(*g_configured_path);
    }

    if (const char* env = std::getenv("KPROTOCOL_TRANSLATION_MAPPINGS");
        env != nullptr && env[0] != '\0') {
        candidates.emplace_back(env);
    }

#ifdef KPROTOCOL_BLOCK_MAPPINGS_GZ
    candidates.emplace_back(KPROTOCOL_BLOCK_MAPPINGS_GZ);
#endif

    if (const auto exe = executable_path(); exe.has_value()) {
        const auto exe_dir = exe->parent_path();
        candidates.emplace_back(exe_dir / ".." / "share" / "kprotocol" / filename);
        candidates.emplace_back(exe_dir / "share" / "kprotocol" / filename);
    }

    candidates.emplace_back(std::filesystem::path("share") / "kprotocol" / filename);
    candidates.emplace_back(std::filesystem::path("data") / filename);

    return candidates;
}

bool ensure_loaded() {
    if (g_loaded) {
        return !g_blob.empty();
    }
    g_loaded = true;

    try {
        for (const auto& candidate : mapping_candidates()) {
            std::error_code ec;
            const auto normalized = std::filesystem::weakly_canonical(candidate, ec);
            const auto& path = ec ? candidate : normalized;
            if (load_blob_from_gzip(path)) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

const std::uint8_t* blob_data() noexcept {
    return g_blob.data();
}

bool parse_header(std::uint32_t& pair_count) noexcept {
    if (g_blob.size() < 16) {
        return false;
    }
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::memcpy(&magic, blob_data(), 4);
    std::memcpy(&version, blob_data() + 4, 4);
    std::memcpy(&pair_count, blob_data() + 8, 4);
    return magic == kMagic && version == kFormatVersion;
}

const BlockMappingTable* find_pair_table(std::uint16_t from_idx, std::uint16_t to_idx) noexcept {
    std::uint32_t pair_count = 0;
    if (!parse_header(pair_count)) {
        return nullptr;
    }
    const auto* dir = reinterpret_cast<const BlockMappingTable*>(blob_data() + 16);
    std::size_t lo = 0;
    std::size_t hi = pair_count;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const auto& row = dir[mid];
        if (row.from_index < from_idx || (row.from_index == from_idx && row.to_index < to_idx)) {
            lo = mid + 1;
        } else if (row.from_index > from_idx || (row.from_index == from_idx && row.to_index > to_idx)) {
            hi = mid;
        } else {
            return &row;
        }
    }
    return nullptr;
}

const BlockMappingEntry* entries_slice(const BlockMappingTable& table) noexcept {
    std::uint32_t pair_count = 0;
    if (!parse_header(pair_count)) {
        return nullptr;
    }
    const auto entries_start = static_cast<std::size_t>(16 + pair_count * sizeof(BlockMappingTable));
    const auto offset = entries_start + table.entry_offset;
    const auto bytes = static_cast<std::size_t>(table.entry_count) * sizeof(BlockMappingEntry);
    if (offset > g_blob.size() || bytes > g_blob.size() - offset) {
        return nullptr;
    }
    return reinterpret_cast<const BlockMappingEntry*>(blob_data() + offset);
}

} // namespace

void set_block_mapping_path(std::string path) {
    std::lock_guard lock(g_mutex);
    g_configured_path = std::filesystem::path(std::move(path));
    g_blob.clear();
    g_loaded = false;
}

void clear_block_mapping_path() {
    std::lock_guard lock(g_mutex);
    g_configured_path.reset();
    g_blob.clear();
    g_loaded = false;
}

bool block_mappings_available() noexcept {
    try {
        std::lock_guard lock(g_mutex);
        return ensure_loaded();
    } catch (...) {
        return false;
    }
}

std::optional<std::int32_t> lookup_block_mapping(
    KnownVersion from,
    KnownVersion to,
    std::int32_t source_id) noexcept {

    std::unique_lock lock(g_mutex, std::defer_lock);
    try {
        lock.lock();
    } catch (...) {
        return std::nullopt;
    }
    if (!ensure_loaded()) {
        return std::nullopt;
    }

    const auto* table = find_pair_table(known_index(from), known_index(to));
    if (table == nullptr || table->entry_count == 0) {
        return std::nullopt;
    }

    const auto* slice = entries_slice(*table);
    if (slice == nullptr) {
        return std::nullopt;
    }

    std::size_t lo = 0;
    std::size_t hi = table->entry_count;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (slice[mid].from < source_id) {
            lo = mid + 1;
        } else if (slice[mid].from > source_id) {
            hi = mid;
        } else {
            return slice[mid].to;
        }
    }
    return std::nullopt;
}

} // namespace kprotocol::detail

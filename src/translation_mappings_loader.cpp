#include "kprotocol/version.hpp"

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <vector>

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

std::uint16_t known_index(KnownVersion version) noexcept {
    return static_cast<std::uint16_t>(version);
}

bool load_blob_from_gzip(const char* path) {
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

bool ensure_loaded() {
    if (g_loaded) {
        return !g_blob.empty();
    }
    g_loaded = true;
#ifdef KPROTOCOL_BLOCK_MAPPINGS_GZ
    if (load_blob_from_gzip(KPROTOCOL_BLOCK_MAPPINGS_GZ)) {
        return true;
    }
#endif
    return false;
}

const std::uint8_t* blob_data() noexcept {
    return g_blob.data();
}

std::size_t blob_size() noexcept {
    return g_blob.size();
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

const BlockMappingEntry* entries_base() noexcept {
    std::uint32_t pair_count = 0;
    if (!parse_header(pair_count)) {
        return nullptr;
    }
    return reinterpret_cast<const BlockMappingEntry*>(blob_data() + 16 + pair_count * sizeof(BlockMappingTable));
}

} // namespace

std::optional<std::int32_t> lookup_block_mapping(
    KnownVersion from,
    KnownVersion to,
    std::int32_t source_id) noexcept {

    if (!ensure_loaded()) {
        return std::nullopt;
    }

    const auto* table = find_pair_table(known_index(from), known_index(to));
    if (table == nullptr || table->entry_count == 0) {
        return std::nullopt;
    }

    const auto* entries = entries_base();
    if (entries == nullptr) {
        return std::nullopt;
    }
    const auto* slice = entries + table->entry_offset;

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

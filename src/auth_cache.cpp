#include "auth_cache.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace kprotocol::detail {
std::string read_auth_cache(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read Microsoft auth cache");
    std::string value{std::istreambuf_iterator<char>(in), {}};
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(value.size()), reinterpret_cast<BYTE*>(value.data())}, output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        throw std::runtime_error("Cannot decrypt auth cache for this Windows user; remove it to sign in again");
    value.assign(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
#else
    struct stat status{};
    if (stat(path.c_str(), &status) != 0 || status.st_uid != getuid() || (status.st_mode & 0077))
        throw std::runtime_error("Auth cache must be owned by this user and have mode 0600");
#endif
    return value;
}
void save_auth_cache(const std::filesystem::path& path, std::string value) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(value.size()), reinterpret_cast<BYTE*>(value.data())}, output{};
    if (!CryptProtectData(&input, L"KProtocol Microsoft refresh token", nullptr, nullptr, nullptr,
            CRYPTPROTECT_UI_FORBIDDEN, &output)) throw std::runtime_error("Cannot encrypt auth cache");
    value.assign(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(value.data(), static_cast<std::streamsize>(value.size()));
        out.close();
        if (!out) throw std::runtime_error("Cannot write auth cache");
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace auth cache");
#else
    auto temporary = path.string() + ".tmp.XXXXXX";
    const int fd = mkstemp(temporary.data()); // Atomic creation with mode 0600.
    if (fd < 0) throw std::runtime_error("Cannot create auth cache temporary file");
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto count = write(fd, value.data() + offset, value.size() - offset);
        if (count <= 0) { close(fd); unlink(temporary.c_str()); throw std::runtime_error("Cannot write auth cache"); }
        offset += static_cast<std::size_t>(count);
    }
    close(fd);
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        throw std::runtime_error("Cannot replace auth cache");
    }
#endif
}

}

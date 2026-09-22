#pragma once
#include <filesystem>
#include <string>
namespace kprotocol::detail {
std::string read_auth_cache(const std::filesystem::path& path);
void save_auth_cache(const std::filesystem::path& path, std::string value);
}

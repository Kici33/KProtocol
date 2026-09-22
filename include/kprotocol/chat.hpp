#pragma once

#include <nlohmann/json.hpp>
#include <cstddef>
#include <string>

namespace kprotocol {

// Strip legacy formatting/control characters, collapse whitespace and remove
// malformed UTF-8. Application-specific chat routing belongs to the consumer.
std::string clean_text(const std::string& input);
// Returns a prefix of valid UTF-8 text without splitting a code point.
std::string take_utf8(const std::string& value, std::size_t byte_limit);
// Flatten JSON text/extra components and vanilla chat.type.text. Does not load
// a translation catalogue; other translation keys contribute their arguments.
std::string component_text(const nlohmann::json& component);

} // namespace kprotocol

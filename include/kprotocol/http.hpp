#pragma once

#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <map>
#include <string>

namespace kprotocol::http {

using Headers = std::map<std::string, std::string>;

struct Response {
    long status{};
    std::string body;
    nlohmann::json json() const;
};

// HTTPS-only, verified certificates, no redirects, bounded response and timeouts.
Response request(const std::string& method, const std::string& url,
    const std::string& body = "", const Headers& headers = {},
    const std::atomic_bool* stop = nullptr);
using Transport = std::function<Response(const std::string&, const std::string&,
    const std::string&, const Headers&, const std::atomic_bool*)>;

std::string form(const std::map<std::string, std::string>& fields);
nlohmann::json require_json(const Response& response, const std::string& operation);

} // namespace kprotocol::http

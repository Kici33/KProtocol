#include "kprotocol/http.hpp"
#include <curl/curl.h>
#include <memory>
#include <stdexcept>
namespace kprotocol::http {
static void init() {
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("Cannot initialize HTTP client");
}
nlohmann::json Response::json() const {
    try { return nlohmann::json::parse(body); }
    catch (const nlohmann::json::exception&) { throw std::runtime_error("Invalid JSON in HTTPS response"); }
}
Response request(const std::string& method, const std::string& url, const std::string& body,
    const std::map<std::string, std::string>& headers, const std::atomic_bool* stop) {
    init();
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("Cannot allocate HTTP client");
    curl_slist* list = nullptr;
    for (const auto& [key, value] : headers) list = curl_slist_append(list, (key + ": " + value).c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> guard(list, curl_slist_free_all);
    Response result;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "KProtocol/0.3 (+https://github.com/Kici33/KProtocol)");
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    // Never follow a redirect with credentials; all production endpoints are HTTPS.
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
#endif
#ifdef _WIN32
    curl_easy_setopt(curl.get(), CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
    if (method != "GET") {
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, +[](char* data, std::size_t size, std::size_t count, void* context) -> std::size_t {
        auto& out = *static_cast<std::string*>(context);
        const auto bytes = size * count;
        if (out.size() + bytes > 2 * 1024 * 1024) return 0;
        out.append(data, bytes);
        return bytes;
    });
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &result.body);
    if (stop) {
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION,
            +[](void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                return static_cast<const std::atomic_bool*>(context)->load() ? 1 : 0;
            });
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, stop);
    }
    const auto status = curl_easy_perform(curl.get());
    if (status != CURLE_OK) throw std::runtime_error(std::string("HTTPS request failed: ") + curl_easy_strerror(status));
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.status);
    return result;
}
std::string form(const std::map<std::string, std::string>& fields) {
    init();
    std::string result;
    for (const auto& [key, value] : fields) {
        std::unique_ptr<char, decltype(&curl_free)> k(curl_easy_escape(nullptr, key.c_str(), static_cast<int>(key.size())), curl_free);
        std::unique_ptr<char, decltype(&curl_free)> v(curl_easy_escape(nullptr, value.c_str(), static_cast<int>(value.size())), curl_free);
        if (!k || !v) throw std::runtime_error("Cannot encode form");
        if (!result.empty()) result += '&';
        result += std::string(k.get()) + "=" + v.get();
    }
    return result;
}
nlohmann::json require_json(const Response& r, const std::string& operation) {
    if (r.status < 200 || r.status >= 300)
        throw std::runtime_error(operation + " failed (HTTP " + std::to_string(r.status) + ")");
    return r.json();
}
}

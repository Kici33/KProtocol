#include "kprotocol/microsoft_auth.hpp"
#include "kprotocol/cancellation.hpp"
#include "auth_cache.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace kprotocol {
namespace {
using nlohmann::json;
const std::string oauth = "https://login.microsoftonline.com/consumers/oauth2/v2.0/";
const std::string scope = "XboxLive.signin offline_access";

void check_stop(const std::atomic_bool& stop) {
    if (stop) throw std::runtime_error("Microsoft authentication cancelled");
}
} // namespace

MicrosoftAuth::MicrosoftAuth(MicrosoftAuthOptions options, http::Transport transport)
    : options_(std::move(options)), transport_(std::move(transport)) {
    if (options_.client_id.empty()) throw std::invalid_argument("Microsoft client_id is required");
    if (!transport_) throw std::invalid_argument("HTTP transport is required");
}

void MicrosoftAuth::log(const std::string& message) const {
    if (options_.on_log) options_.on_log(message);
}

void MicrosoftAuth::invalidate() noexcept { expires_ = {}; }

MinecraftIdentity MicrosoftAuth::login(const std::atomic_bool& stop) {
    check_stop(stop);
    if (std::chrono::steady_clock::now() < expires_) return identity_;
    auto post_form = [&](const std::string& endpoint, const std::map<std::string, std::string>& data) {
        check_stop(stop);
        return transport_("POST", oauth + endpoint, http::form(data),
            {{"Content-Type", "application/x-www-form-urlencoded"}}, &stop);
    };
    auto post = [&](const std::string& url, const json& data, const std::string& label) {
        check_stop(stop);
        return http::require_json(transport_("POST", url, data.dump(),
            {{"Content-Type", "application/json"}}, &stop), label);
    };
    if (!cache_loaded_) {
        if (!options_.cache_path.empty()) {
            const auto cached = detail::read_auth_cache(options_.cache_path);
            if (!cached.empty()) {
                try {
                    const auto cache = json::parse(cached);
                    if (cache.value("client_id", "") == options_.client_id)
                        refresh_token_ = cache.at("refresh_token").get<std::string>();
                } catch (const json::exception&) {
                    throw std::runtime_error("Invalid auth cache; remove it to sign in again");
                }
            }
        }
        cache_loaded_ = true;
    }
    try {
        json tokens;
        if (!refresh_token_.empty()) {
            const auto response = post_form("token", {{"client_id", options_.client_id}, {"scope", scope},
                {"grant_type", "refresh_token"}, {"refresh_token", refresh_token_}});
            if (response.status == 200) tokens = response.json();
            else if (response.status == 400 && response.json().value("error", "") == "invalid_grant") {
                refresh_token_.clear();
                log("Microsoft sign-in expired. Starting device sign-in again.");
            } else http::require_json(response, "Microsoft token refresh");
        }
        if (tokens.is_null()) {
            if (!options_.on_device_code)
                throw std::runtime_error("Interactive Microsoft login requires an on_device_code callback");
            const auto device = http::require_json(post_form("devicecode",
                {{"client_id", options_.client_id}, {"scope", scope}}), "Microsoft device sign-in");
            int interval = device.value("interval", 5);
            const int expires_in = device.at("expires_in").get<int>();
            if (interval < 1 || interval > 300 || expires_in < 1 || expires_in > 3600)
                throw std::runtime_error("Invalid Microsoft device-code timing");
            options_.on_device_code({device.at("verification_uri").get<std::string>(),
                device.at("user_code").get<std::string>(), std::chrono::seconds(expires_in)});
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in);
            while (!stop && std::chrono::steady_clock::now() < deadline) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                if (!pause(stop, static_cast<int>(std::min<std::int64_t>(remaining, interval * 1000LL)))) break;
                if (std::chrono::steady_clock::now() >= deadline) break;
                const auto response = post_form("token", {{"client_id", options_.client_id},
                    {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
                    {"device_code", device.at("device_code").get<std::string>()}});
                if (response.status == 200) { tokens = response.json(); break; }
                if (response.status != 400) http::require_json(response, "Microsoft device-code polling");
                const auto error = response.json().value("error", "");
                if (error == "slow_down") interval = std::min(interval + 5, 300);
                else if (error != "authorization_pending")
                    throw std::runtime_error("Microsoft device sign-in was rejected or expired");
            }
            if (tokens.is_null()) throw std::runtime_error("Microsoft sign-in cancelled or timed out");
        }
        check_stop(stop);
        if (tokens.contains("refresh_token")) refresh_token_ = tokens["refresh_token"].get<std::string>();
        if (!options_.cache_path.empty() && !refresh_token_.empty()) {
            detail::save_auth_cache(options_.cache_path,
                json{{"client_id", options_.client_id}, {"refresh_token", refresh_token_}}.dump());
        }
        const auto xbox = post("https://user.auth.xboxlive.com/user/authenticate", {
            {"Properties", {{"AuthMethod", "RPS"}, {"SiteName", "user.auth.xboxlive.com"},
                {"RpsTicket", "d=" + tokens.at("access_token").get<std::string>()}}},
            {"RelyingParty", "http://auth.xboxlive.com"}, {"TokenType", "JWT"}}, "Xbox Live authentication");
        const auto xsts = post("https://xsts.auth.xboxlive.com/xsts/authorize", {
            {"Properties", {{"SandboxId", "RETAIL"}, {"UserTokens", json::array({xbox.at("Token")})}}},
            {"RelyingParty", "rp://api.minecraftservices.com/"}, {"TokenType", "JWT"}}, "Xbox XSTS authentication");
        const auto minecraft = post("https://api.minecraftservices.com/authentication/login_with_xbox", {
            {"identityToken", "XBL3.0 x=" + xsts.at("DisplayClaims").at("xui").at(0).at("uhs").get<std::string>() +
                ";" + xsts.at("Token").get<std::string>()}}, "Minecraft authentication (application must have Minecraft API access)");
        MinecraftIdentity identity;
        identity.access_token = minecraft.at("access_token").get<std::string>();
        check_stop(stop);
        const auto profile = http::require_json(transport_("GET", "https://api.minecraftservices.com/minecraft/profile", "",
            {{"Authorization", "Bearer " + identity.access_token}}, &stop), "Minecraft Java profile (account must own Java Edition)");
        identity.username = profile.at("name").get<std::string>();
        identity.uuid = profile.at("id").get<std::string>();
        if (identity.access_token.empty() || identity.username.empty() || identity.uuid.empty())
            throw std::runtime_error("Incomplete Minecraft identity returned by authentication");
        identity_ = std::move(identity);
        expires_ = std::chrono::steady_clock::now() + std::chrono::seconds(std::max(0, minecraft.value("expires_in", 3600) - 120));
        log("Minecraft authenticated as " + identity_.username);
        return identity_;
    } catch (const json::exception&) {
        throw std::runtime_error("Invalid Microsoft/Xbox/Minecraft authentication response");
    }
}

void MicrosoftAuth::join(const MinecraftIdentity& identity, const std::string& hash, const std::atomic_bool& stop) {
    check_stop(stop);
    const auto response = transport_("POST", "https://sessionserver.mojang.com/session/minecraft/join",
        json{{"accessToken", identity.access_token}, {"selectedProfile", identity.uuid}, {"serverId", hash}}.dump(),
        {{"Content-Type", "application/json"}}, &stop);
    if (response.status != 204) {
        invalidate();
        throw std::runtime_error("Minecraft session join failed (HTTP " + std::to_string(response.status) + ")");
    }
}

} // namespace kprotocol

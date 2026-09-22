#include <kprotocol/microsoft_auth.hpp>
#include <kprotocol/crypto.hpp>
#include <kprotocol/cancellation.hpp>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <vector>
using namespace kprotocol;
using namespace kprotocol::crypto;
using nlohmann::json;
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void crypto_tests() {
    // Published Minecraft signed SHA-1 examples, including negative and leading-zero results.
    check(server_hash("Notch", {}, {}) == "4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48", "Positive server hash");
    check(server_hash("jeb_", {}, {}) == "-7c9d5b0044c130109a5d7b5fb5c317c02b4e28c1", "Negative server hash");
    check(server_hash("simon", {}, {}) == "88e16a1019277b15d58faf0541e11910eb756f6", "Leading zero hash");
    const auto secret = random_secret();
    Cipher enc(secret, true), dec(secret, false);
    const Bytes input{1, 2, 3, 4, 5, 6, 7};
    const auto cipher = enc.update(input);
    auto a = dec.update(std::span(cipher.data(), 3));
    auto b = dec.update(std::span(cipher.data() + 3, cipher.size() - 3));
    a.insert(a.end(), b.begin(), b.end());
    check(a == input, "AES-CFB8 stream survives fragmented reads");
    const auto second = enc.update(input);
    check(dec.update(second) == input && second != cipher, "AES state continues across packets");
}

struct FakeMicrosoft {
    int calls = 0, devices = 0, polls = 0, refreshes = 0, joins = 0;
    bool revoked = false, join_failed = false;
    kprotocol::http::Response operator()(const std::string& method, const std::string& url,
        const std::string& body, const kprotocol::http::Headers& headers, const std::atomic_bool* stop) {
        ++calls;
        check(stop && !*stop, "Pass cancellation through all HTTP requests");
        auto reply = [](const json& data) { return kprotocol::http::Response{200, data.dump()}; };
        if (url.ends_with("/devicecode")) {
            ++devices;
            check(method == "POST" && body.find("scope=XboxLive.signin%20offline_access") != std::string::npos,
                "Device authorization scopes");
            return reply({{"verification_uri", "https://microsoft.com/devicelogin"}, {"user_code", "TEST-CODE"},
                {"device_code", "private-device-code"}, {"interval", 1}, {"expires_in", 30}});
        }
        if (url.ends_with("/token")) {
            if (body.find("grant_type=refresh_token") != std::string::npos) {
                ++refreshes;
                check(body.find("refresh_token=refresh-") != std::string::npos, "Use saved refresh token");
                if (revoked) { revoked = false; return {400, R"({"error":"invalid_grant"})"}; }
            } else {
                ++polls;
                check(body.find("private-device-code") != std::string::npos, "Poll the correct device code");
                if (polls == 1) return {400, R"({"error":"authorization_pending"})"};
            }
            return reply({{"access_token", "msa-secret"}, {"refresh_token", "refresh-" + std::to_string(refreshes)}});
        }
        if (url.ends_with("/user/authenticate")) {
            check(json::parse(body)["Properties"]["RpsTicket"] == "d=msa-secret", "Xbox RPS token");
            return reply({{"Token", "xbox-secret"}});
        }
        if (url.ends_with("/xsts/authorize")) {
            const auto value = json::parse(body);
            check(value["Properties"]["UserTokens"][0] == "xbox-secret" &&
                  value["RelyingParty"] == "rp://api.minecraftservices.com/", "XSTS exchange");
            return reply({{"Token", "xsts-secret"}, {"DisplayClaims", {{"xui", json::array({{{"uhs", "user-hash"}}})}}}});
        }
        if (url.ends_with("/authentication/login_with_xbox")) {
            check(json::parse(body)["identityToken"] == "XBL3.0 x=user-hash;xsts-secret", "Minecraft identity token");
            return reply({{"access_token", "minecraft-secret"}, {"expires_in", 3600}});
        }
        if (url.ends_with("/minecraft/profile")) {
            check(method == "GET" && headers.at("Authorization") == "Bearer minecraft-secret", "Profile authorization");
            return reply({{"name", "LibraryUser"}, {"id", "00000000000000000000000000000001"}});
        }
        if (url.ends_with("/session/minecraft/join")) {
            ++joins;
            const auto value = json::parse(body);
            check(value["accessToken"] == "minecraft-secret" && value["serverId"] == "test-hash" &&
                value["selectedProfile"] == "00000000000000000000000000000001", "Mojang session join payload");
            return {join_failed ? 403 : 204, ""};
        }
        throw std::runtime_error("Unexpected HTTP endpoint in auth test");
    }
};

static void auth_tests() {
    const auto path = std::filesystem::temp_directory_path() /
        ("kprotocol-auth-" + server_hash("test", random_secret(), {}) + ".cache");
    struct RemoveCache { std::filesystem::path path; ~RemoveCache() { std::error_code ec; std::filesystem::remove(path, ec); } } cleanup{path};
    std::atomic_bool stop{false};
    FakeMicrosoft server;
    auto transport = [&](const auto& method, const auto& url, const auto& body, const auto& headers, auto* cancelled) {
        return server(method, url, body, headers, cancelled);
    };
    int prompts = 0;
    std::vector<std::string> logs;
    MicrosoftAuthOptions options{
        .client_id = "test-client", .cache_path = path,
        .on_device_code = [&](const MicrosoftDeviceCode& code) {
            ++prompts;
            check(code.user_code == "TEST-CODE" && code.expires_in.count() == 30, "Structured device-code callback");
        },
        .on_log = [&](const std::string& text) { logs.push_back(text); },
    };
    MicrosoftAuth auth(options, transport);
    const auto identity = auth.login(stop);
    check(identity.username == "LibraryUser" && identity.access_token == "minecraft-secret", "Authenticated identity");
    check(prompts == 1 && server.devices == 1 && server.polls == 2, "Device polling handles authorization_pending");
    const int requests = server.calls;
    check(auth.login(stop).username == identity.username && server.calls == requests, "Access token reuse");
    check(std::filesystem::exists(path), "Refresh cache created");
#ifndef _WIN32
    const auto perms = std::filesystem::status(path).permissions();
    check((perms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) == std::filesystem::perms::none,
        "Owner-only auth cache");
#else
    std::ifstream in(path, std::ios::binary);
    std::string cache{std::istreambuf_iterator<char>(in), {}};
    check(cache.find("refresh-") == std::string::npos, "Windows cache encrypted with DPAPI");
#endif
    auth.join(identity, "test-hash", stop);
    check(server.joins == 1, "Session join");
    auth.invalidate();
    (void)auth.login(stop);
    check(server.refreshes == 1 && prompts == 1, "Refresh without prompting");
    // A new consumer instance reads the cache, with no interactive UI installed.
    MicrosoftAuth cached({.client_id = "test-client", .cache_path = path}, transport);
    (void)cached.login(stop);
    check(server.refreshes == 2, "Persistent cache can authenticate a new instance");
    server.revoked = true;
    auth.invalidate();
    (void)auth.login(stop);
    check(prompts == 2 && server.devices == 2, "Revoked refresh token falls back to device authentication");
    server.join_failed = true;
    bool rejected = false;
    try { auth.join(identity, "test-hash", stop); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "Failed session join reports an error");
    const int previous_refreshes = server.refreshes;
    (void)auth.login(stop);
    check(server.refreshes == previous_refreshes + 1, "Failed join invalidates access token");
    const int before_cancel = server.calls;
    stop = true;
    rejected = false;
    try { (void)auth.login(stop); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected && server.calls == before_cancel, "Cancellation even with a cached access token");
    for (const auto& message : logs)
        check(message.find("secret") == std::string::npos && message.find("refresh-") == std::string::npos, "Logs do not reveal tokens");
    stop = false;
    MicrosoftAuth no_ui({.client_id = "test-client"}, transport);
    rejected = false;
    try { (void)no_ui.login(stop); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected && server.calls == before_cancel, "Library does not print or silently discard device prompts");
    MicrosoftAuth cancelled({.client_id = "test-client", .on_device_code = [&](const MicrosoftDeviceCode&) { stop = true; }}, transport);
    rejected = false;
    try { (void)cancelled.login(stop); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "Device-code cancellation");
}

int main() {
    try {
        crypto_tests();
        auth_tests();
        std::cout << "PASS authentication, cache, cancellation, session and crypto tests\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}

#include <kprotocol/microsoft_auth.hpp>
#include <kprotocol/crypto.hpp>
#include <stdexcept>

namespace {
// Executed by the installed-consumer smoke test. No credentials or network.
const bool checked = [] {
    kprotocol::MicrosoftAuth auth({.client_id = "consumer-test"});
    std::atomic_bool stop{true};
    try { (void)auth.login(stop); }
    catch (const std::runtime_error&) {
        return kprotocol::crypto::server_hash("Notch", {}, {}) ==
            "4ed1f46bbe04bc756bcb17c0c7ce3e4632f06a48";
    }
    throw std::runtime_error("Cancelled authentication did not stop");
}();
}

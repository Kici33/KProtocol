#include <kprotocol/minecraft_client.hpp>
#include <kprotocol/chat.hpp>
#include <stdexcept>

namespace {
const bool checked = [] {
    kprotocol::MinecraftClient client({.host = "localhost"});
    if (client.ready() || client.send_chat("test") || kprotocol::clean_text("a\nb") != "a b")
        throw std::runtime_error("Installed client API failed");
    return true;
}();
}

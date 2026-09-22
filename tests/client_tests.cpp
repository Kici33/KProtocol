#include <kprotocol/crypto.hpp>
#include <kprotocol/minecraft_client.hpp>
#include <kprotocol/chat.hpp>
#include <kprotocol/codec.hpp>
#include <asio.hpp>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace kprotocol;
using namespace kprotocol::crypto;
using namespace std::chrono_literals;
namespace codec = kprotocol::codec;
using nlohmann::json;
static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Peer {
public:
    explicit Peer(asio::ip::tcp::socket& socket) : socket_(socket) { socket_.non_blocking(true); }
    int compression = -1;
    std::unique_ptr<Cipher> encrypt, decrypt;
    void send(int id, const Bytes& payload, bool fragmented = false) {
        auto bytes = codec::encode_frame_compressed(id, payload, compression);
        if (encrypt) bytes = encrypt->update(bytes);
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            asio::error_code e;
            offset += socket_.write_some(asio::buffer(bytes.data() + offset, fragmented ? 1 : bytes.size() - offset), e);
            check(!e || e == asio::error::would_block || e == asio::error::try_again, "Fake server write error");
            check(std::chrono::steady_clock::now() < deadline, "Fake server write timeout");
            if (fragmented) std::this_thread::sleep_for(1ms);
        }
    }
    codec::EncodedFrame read() {
        Bytes frame;
        for (int i = 0; i < 3; ++i) {
            const auto byte = byte_read(); frame.push_back(byte);
            if (!(byte & 0x80)) break;
        }
        std::size_t offset = 0;
        const int size = codec::read_var_int(frame, offset);
        check(size > 0 && size < 1024 * 1024, "Fake server frame bound");
        for (int i = 0; i < size; ++i) frame.push_back(byte_read());
        codec::EncodedFrame out;
        std::size_t consumed = 0;
        check(codec::try_decode_frame_compressed(frame, consumed, compression, out), "Decode client frame");
        return out;
    }
private:
    std::uint8_t byte_read() {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        for (;;) {
            std::uint8_t b{};
            asio::error_code e;
            const auto count = socket_.read_some(asio::buffer(&b, 1), e);
            if (count) return decrypt ? decrypt->update(std::span(&b, 1))[0] : b;
            check(!e || e == asio::error::would_block || e == asio::error::try_again, "Fake server read error");
            check(std::chrono::steady_clock::now() < deadline, "Fake server read timeout");
            std::this_thread::sleep_for(1ms);
        }
    }
    asio::ip::tcp::socket& socket_;
};
static Bytes private_decrypt(EVP_PKEY* key, const Bytes& input) {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(EVP_PKEY_CTX_new(key, nullptr), EVP_PKEY_CTX_free);
    check(ctx && EVP_PKEY_decrypt_init(ctx.get()) > 0 && EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) > 0, "RSA decryption init");
    std::size_t size = 0;
    check(EVP_PKEY_decrypt(ctx.get(), nullptr, &size, input.data(), input.size()) > 0, "RSA size");
    Bytes out(size);
    check(EVP_PKEY_decrypt(ctx.get(), out.data(), &size, input.data(), input.size()) > 0, "RSA decrypt");
    out.resize(size); return out;
}
static void minecraft_test() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, {asio::ip::address_v4::loopback(), 0});
    MinecraftClientOptions config; config.host = "127.0.0.1"; config.port = acceptor.local_endpoint().port(); config.send_interval = 100ms;
    std::atomic_bool stop{false};
    std::mutex mutex;
    std::vector<std::string> received;
    std::string joined_hash, client_error;
    MinecraftClient client(config, {.on_chat = [&](const MinecraftChatMessage& message) {
        std::lock_guard lock(mutex); received.push_back(message.text);
    }});
    std::jthread thread([&] {
        try { client.connect({"BridgeBot", "00000000000000000000000000000001", "test-token"}, stop,
            [&](const MinecraftIdentity& identity, const std::string& hash, const std::atomic_bool&) {
                check(identity.username == "BridgeBot", "Join identity"); joined_hash = hash;
            }); }
        catch (const std::exception& e) { client_error = e.what(); }
    });
    struct Stop { std::atomic_bool& value; ~Stop() { value = true; } } stop_guard{stop};
    asio::ip::tcp::socket socket(io);
    acceptor.non_blocking(true);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    for (;;) {
        asio::error_code e; acceptor.accept(socket, e);
        if (!e) break;
        check(e == asio::error::would_block || e == asio::error::try_again, "Accept error");
        check(std::chrono::steady_clock::now() < deadline, "Accept timeout");
        std::this_thread::sleep_for(10ms);
    }
    Peer peer(socket);
    auto frame = peer.read(); std::size_t pos = 0;
    check(frame.packet_id == 0 && codec::read_var_int(frame.payload, pos) == 47, "Handshake protocol");
    check(codec::read_string(frame.payload, pos) == "127.0.0.1", "Handshake host");
    check(codec::read_ushort(frame.payload, pos) == config.port && codec::read_var_int(frame.payload, pos) == 2, "Handshake login state");
    frame = peer.read(); pos = 0;
    check(frame.packet_id == 0 && codec::read_string(frame.payload, pos) == "BridgeBot", "Login start");
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> gen(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    check(gen && EVP_PKEY_keygen_init(gen.get()) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(gen.get(), 1024) > 0, "RSA generate init");
    EVP_PKEY* raw_key = nullptr;
    check(EVP_PKEY_keygen(gen.get(), &raw_key) > 0, "RSA keygen");
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw_key, EVP_PKEY_free);
    Bytes public_key(static_cast<std::size_t>(i2d_PUBKEY(key.get(), nullptr)));
    auto* key_cursor = public_key.data(); i2d_PUBKEY(key.get(), &key_cursor);
    const Bytes verify_token{1, 2, 3, 4};
    Bytes data; codec::write_string(data, ""); codec::write_byte_array(data, public_key); codec::write_byte_array(data, verify_token);
    peer.send(1, data, true);
    frame = peer.read(); pos = 0;
    check(frame.packet_id == 1, "Encryption response");
    const auto secret = private_decrypt(key.get(), codec::read_byte_array(frame.payload, pos));
    check(secret.size() == 16 && private_decrypt(key.get(), codec::read_byte_array(frame.payload, pos)) == verify_token, "Encrypted challenge response");
    peer.encrypt = std::make_unique<Cipher>(secret, true); peer.decrypt = std::make_unique<Cipher>(secret, false);
    data.clear(); codec::write_var_int(data, 32); peer.send(3, data, true); peer.compression = 32;
    data.clear(); codec::write_string(data, "00000000-0000-0000-0000-000000000001"); codec::write_string(data, "BridgeBot"); peer.send(2, data, true);
    // Join Game packet, protocol 47.
    data.clear(); codec::write_int(data, 1); codec::write_ubyte(data, 0); codec::write_byte(data, 0);
    codec::write_ubyte(data, 1); codec::write_ubyte(data, 20); codec::write_string(data, "default"); codec::write_bool(data, false);
    peer.send(1, data);
    frame = peer.read(); check(frame.packet_id == 0x15, "Client settings");
    frame = peer.read(); pos = 0; check(frame.packet_id == 0x17 && codec::read_string(frame.payload, pos) == "MC|Brand", "Client brand");
    auto position = [&](double x, double y, double z, std::uint8_t flags) {
        data.clear(); codec::write_double(data, x); codec::write_double(data, y); codec::write_double(data, z);
        codec::write_float(data, 10); codec::write_float(data, 20); codec::write_ubyte(data, flags); peer.send(8, data, true);
        do { frame = peer.read(); } while (frame.packet_id == 3);
        check(frame.packet_id == 6, "Position acknowledgement");
    };
    position(100, 65, 10, 0); pos = 0;
    check(codec::read_double(frame.payload, pos) == 100 && codec::read_double(frame.payload, pos) == 65, "Absolute position");
    position(2, 1, -3, 7); pos = 0;
    check(codec::read_double(frame.payload, pos) == 102 && codec::read_double(frame.payload, pos) == 66 && codec::read_double(frame.payload, pos) == 7, "Relative position flags");
    data.clear(); codec::write_var_int(data, 123456); peer.send(0, data, true);
    do { frame = peer.read(); } while (frame.packet_id == 3);
    pos = 0; check(frame.packet_id == 0 && codec::read_var_int(frame.payload, pos) == 123456, "Keepalive echo");
    auto chat = [&](const std::string& text) {
        data.clear(); codec::write_string(data, json{{"text", text}}.dump()); codec::write_ubyte(data, 0); peer.send(2, data, true);
    };
    chat("Hello from the game");
    chat("<BridgeBot> own message remains visible to the consumer");
    chat("A server system message");
    check(!client.send_chat("invalid\nmessage"), "Reject control characters");
    check(!client.send_chat(std::string(101, 'x')), "Reject oversized chat");
    check(client.send_chat("hello from client"), "Queue generic chat");
    do { frame = peer.read(); } while (frame.packet_id == 3);
    pos = 0; check(frame.packet_id == 1 && codec::read_string(frame.payload, pos) == "hello from client", "Generic outgoing chat without injected commands");
    data.clear(); codec::write_string(data, json{{"text", "Test complete"}}.dump()); peer.send(0x40, data);
    thread.join();
    check(!client.ready(), "Disconnected client not ready");
    check(joined_hash == server_hash("", secret, public_key), "Session join server hash");
    check(client_error.find("Test complete") != std::string::npos, "Disconnect reason");
    check(received == std::vector<std::string>{"Hello from the game", "<BridgeBot> own message remains visible to the consumer", "A server system message"}, "Library does not apply application routing");
    check(!client.send_chat("offline"), "Reject offline chat");
}
static void malformed_frame_test() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, {asio::ip::address_v4::loopback(), 0});
    MinecraftClientOptions config; config.host = "127.0.0.1"; config.port = acceptor.local_endpoint().port();
    std::atomic_bool stop{false};
    MessageQueue queue;
    std::string error;
    MinecraftClient client(config);
    std::jthread thread([&] {
        try { client.connect({"BridgeBot", "", ""}, stop); }
        catch (const std::exception& e) { error = e.what(); }
    });
    struct Stop { std::atomic_bool& value; ~Stop() { value = true; } } stop_guard{stop};
    asio::ip::tcp::socket socket(io);
    acceptor.accept(socket);
    Peer peer(socket);
    (void)peer.read(); (void)peer.read();
    // A fourth length byte must be rejected before allocating a frame.
    const std::array<std::uint8_t, 3> invalid{0x80, 0x80, 0x80};
    asio::write(socket, asio::buffer(invalid));
    thread.join();
    check(error.find("frame length prefix") != std::string::npos, "Reject oversized VarInt frame prefix");
    check(!client.ready(), "Malformed stream disconnects");
}
int main() {
    try {
        minecraft_test();
        malformed_frame_test();
        MessageQueue queue(1, 10ms);
        check(queue.push({"old"}), "Queue push");
        std::this_thread::sleep_for(20ms);
        check(queue.push({"new"}) && queue.pop() == "new", "Expired entries release queue capacity");
        check(clean_text("  hi\nthere ") == "hi there", "Chat cleanup");
        check(component_text(json{{"text", "hello"}, {"extra", json::array({" world"})}}) == "hello world", "Component parsing");
        std::cout << "PASS generic client, encrypted login, compression, framing and queue tests\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}

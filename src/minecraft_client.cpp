#include "kprotocol/minecraft_client.hpp"
#include "kprotocol/crypto.hpp"
#include "kprotocol/cancellation.hpp"
#include "kprotocol/chat.hpp"
#include <kprotocol/codec.hpp>
#include <asio.hpp>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>
namespace kprotocol {
using namespace crypto;
namespace codec = kprotocol::codec;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace {
// Fixed protocol 47 (Java 1.8.x). All field and compression encoding uses KProtocol.
constexpr int protocol = 47;
constexpr int max_frame = codec::limits::max_packet_length;
bool would_block(const asio::error_code& e) { return e == asio::error::would_block || e == asio::error::try_again; }
void check_stop(const std::atomic_bool& stop) { if (stop) throw std::runtime_error("Minecraft stopped"); }
void write(asio::ip::tcp::socket& socket, const Bytes& data, const std::atomic_bool& stop) {
    auto deadline = Clock::now() + 15s;
    std::size_t offset = 0;
    while (offset < data.size()) {
        check_stop(stop);
        asio::error_code error;
        offset += socket.write_some(asio::buffer(data.data() + offset, data.size() - offset), error);
        if (error && !would_block(error)) throw asio::system_error(error);
        if (Clock::now() > deadline) throw std::runtime_error("Minecraft write timeout");
        if (would_block(error)) std::this_thread::sleep_for(5ms);
    }
}
// Read an exact frame: this prevents bytes after Encryption Request from being consumed
// under the wrong cipher state. Lengths are checked before any allocation.
std::optional<codec::EncodedFrame> read(asio::ip::tcp::socket& socket, Bytes& buffer,
    Cipher* cipher, int compression) {
    auto receive = [&](std::size_t count) {
        std::array<std::uint8_t, 8192> data{};
        asio::error_code error;
        const auto n = socket.read_some(asio::buffer(data.data(), std::min(count, data.size())), error);
        if (error && !would_block(error)) throw asio::system_error(error);
        if (n) {
            const auto bytes = cipher ? cipher->update(std::span(data.data(), n)) : Bytes(data.begin(), data.begin() + n);
            buffer.insert(buffer.end(), bytes.begin(), bytes.end());
        }
        return n;
    };
    for (;;) {
        bool prefix_complete = false;
        for (std::size_t i = 0; i < std::min<std::size_t>(3, buffer.size()); ++i)
            if (!(buffer[i] & 0x80)) { prefix_complete = true; break; }
        if (prefix_complete) break;
        if (buffer.size() >= 3) throw std::runtime_error("Invalid Minecraft frame length prefix");
        if (!receive(1)) return std::nullopt;
    }
    std::size_t offset = 0;
    const auto length = codec::read_var_int(buffer, offset);
    if (length <= 0 || length > max_frame) throw std::runtime_error("Minecraft frame exceeds size limit");
    const auto total = offset + static_cast<std::size_t>(length);
    if (buffer.size() < total && !receive(total - buffer.size())) return std::nullopt;
    if (buffer.size() < total) return std::nullopt;
    codec::EncodedFrame frame;
    std::size_t consumed = 0;
    if (!codec::try_decode_frame_compressed(buffer, consumed, compression, frame) || consumed != total)
        throw std::runtime_error("Invalid Minecraft compressed frame");
    buffer.clear();
    return frame;
}
}
void MinecraftClient::connect(const MinecraftIdentity& identity, const std::atomic_bool& stop, Join join) {
    if (connecting_.test_and_set()) throw std::logic_error("MinecraftClient is already connecting/running");
    struct Reset {
        MinecraftClient& client;
        ~Reset() { client.set_ready(false); client.connecting_.clear(); }
    } reset{*this};
    set_ready(false);
    try {
    if (identity.username.empty() || identity.username.size() > 16)
        throw std::invalid_argument("Minecraft username must contain 1-16 characters");
    check_stop(stop);
    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    asio::ip::tcp::socket socket(io);
    // Resolve and connect asynchronously so shutdown and the connection deadline remain bounded.
    bool complete = false;
    asio::error_code connection_error;
    resolver.async_resolve(options_.host, std::to_string(options_.port),
        [&](const asio::error_code& error, const asio::ip::tcp::resolver::results_type& endpoints) {
            if (error) { connection_error = error; complete = true; return; }
            asio::async_connect(socket, endpoints, [&](const asio::error_code& e, const asio::ip::tcp::endpoint&) {
                connection_error = e; complete = true;
            });
        });
    const auto deadline = Clock::now() + 20s;
    while (!complete && !stop && Clock::now() < deadline) { io.poll(); io.restart(); std::this_thread::sleep_for(10ms); }
    if (!complete) { resolver.cancel(); socket.close(); throw std::runtime_error("Minecraft connection timed out or stopped"); }
    if (connection_error) throw asio::system_error(connection_error);
    socket.non_blocking(true);
    socket.set_option(asio::ip::tcp::no_delay(true));
    int compression = -1;
    bool play = false, encrypted = false, joined = false;
    std::unique_ptr<Cipher> encrypt, decrypt;
    auto send = [&](int id, const Bytes& payload) {
        auto data = codec::encode_frame_compressed(id, payload, compression);
        if (encrypt) data = encrypt->update(data);
        write(socket, data, stop);
    };
    Bytes packet;
    codec::write_var_int(packet, protocol);
    codec::write_string(packet, options_.host);
    codec::write_ushort(packet, options_.port);
    codec::write_var_int(packet, 2);
    send(0x00, packet);
    packet.clear(); codec::write_string(packet, identity.username); send(0x00, packet);
    Bytes buffer;
    double x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    auto last_packet = Clock::now(), last_send = Clock::now(), last_ground = Clock::now();
    const auto login_deadline = Clock::now() + 45s;
    while (!stop) {
        const auto frame = read(socket, buffer, decrypt.get(), compression);
        if (frame) {
            last_packet = Clock::now();
            std::size_t pos = 0;
            const auto& data = frame->payload;
            if (!play) {
                switch (frame->packet_id) {
                case 0x00: throw std::runtime_error("Minecraft login rejected: " + clean_text(component_text(nlohmann::json::parse(codec::read_string(data, pos)))));
                case 0x01: {
                    if (encrypted) throw std::runtime_error("Duplicate encryption request");
                    const auto id = codec::read_string(data, pos);
                    const auto key = codec::read_byte_array(data, pos);
                    const auto token = codec::read_byte_array(data, pos);
                    if (key.size() > 8192 || token.size() > 1024) throw std::runtime_error("Invalid login encryption challenge");
                    const auto secret = random_secret();
                    if (!join) throw std::runtime_error("Online-mode login requires an authentication provider or join callback");
                    join(identity, server_hash(id, secret, key), stop);
                    packet.clear();
                    codec::write_byte_array(packet, rsa_encrypt(key, secret));
                    codec::write_byte_array(packet, rsa_encrypt(key, token));
                    send(0x01, packet);
                    encrypt = std::make_unique<Cipher>(secret, true);
                    decrypt = std::make_unique<Cipher>(secret, false);
                    encrypted = true;
                    break;
                }
                case 0x02:
                    (void)codec::read_string(data, pos); // UUID
                    if (codec::read_string(data, pos) != identity.username) throw std::runtime_error("Minecraft login profile mismatch");
                    play = true;
                    break;
                case 0x03:
                    compression = codec::read_var_int(data, pos);
                    if (compression < 0 || compression > max_frame) throw std::runtime_error("Invalid compression threshold");
                    break;
                default: throw std::runtime_error("Unexpected Minecraft login packet (expected protocol 47)");
                }
            } else {
                switch (frame->packet_id) {
                case 0x00: // Keep Alive: same VarInt payload in both directions.
                    (void)codec::read_var_int(data, pos); send(0x00, data); break;
                case 0x01: { // Join Game
                    joined = true;
                    packet.clear();
                    codec::write_string(packet, "en_US");
                    codec::write_byte(packet, 2); // view distance
                    codec::write_byte(packet, 0); // chat enabled
                    codec::write_bool(packet, true);
                    codec::write_ubyte(packet, 0x7f);
                    send(0x15, packet);
                    packet.clear(); codec::write_string(packet, "MC|Brand"); codec::write_string(packet, "vanilla"); send(0x17, packet);
                    break;
                }
                case 0x02: { // Chat Message
                    const auto message = codec::read_string(data, pos);
                    const auto position = codec::read_ubyte(data, pos);
                    if (callbacks_.on_chat)
                        callbacks_.on_chat({message, component_text(nlohmann::json::parse(message)), position});
                    break;
                }
                case 0x07: set_ready(false); break; // Respawn / server transfer: wait for position.
                case 0x08: { // Position and look; acknowledge relative flags using absolute coordinates.
                    const double nx = codec::read_double(data, pos), ny = codec::read_double(data, pos), nz = codec::read_double(data, pos);
                    const float nyaw = codec::read_float(data, pos), npitch = codec::read_float(data, pos);
                    const auto flags = codec::read_ubyte(data, pos);
                    x = nx + ((flags & 1) ? x : 0); y = ny + ((flags & 2) ? y : 0); z = nz + ((flags & 4) ? z : 0);
                    yaw = nyaw + ((flags & 8) ? yaw : 0); pitch = npitch + ((flags & 16) ? pitch : 0);
                    packet.clear(); codec::write_double(packet, x); codec::write_double(packet, y); codec::write_double(packet, z);
                    codec::write_float(packet, yaw); codec::write_float(packet, pitch); codec::write_bool(packet, false);
                    send(0x06, packet);
                    if (joined && !ready_) {
                        set_ready(true);
                        if (callbacks_.on_ready) callbacks_.on_ready(identity.username);
                    }
                    break;
                }
                case 0x40: throw std::runtime_error("Minecraft disconnected: " + clean_text(component_text(nlohmann::json::parse(codec::read_string(data, pos)))));
                default: break; // World rendering, entities and inventory are not needed for a chat client.
                }
            }
        }
        const auto now = Clock::now();
        if (!play && now > login_deadline) throw std::runtime_error("Minecraft login timeout");
        if (now - last_packet > 90s) throw std::runtime_error("Minecraft idle timeout");
        if (ready_ && now - last_ground >= 1s) {
            // Ground state only; this client does not implement world physics.
            packet.clear(); codec::write_bool(packet, true); send(0x03, packet); last_ground = now;
        }
        if (ready_ && now - last_send >= options_.send_interval) {
            if (auto message = outbound_.pop()) { packet.clear(); codec::write_string(packet, *message); send(0x01, packet); last_send = now; }
        }
        if (!frame) std::this_thread::sleep_for(10ms);
    }
    } catch (const std::exception& error) {
        set_ready(false);
        if (!stop && callbacks_.on_disconnect) callbacks_.on_disconnect(error.what());
        throw;
    }
}

MinecraftClient::MinecraftClient(MinecraftClientOptions options, MinecraftClientCallbacks callbacks)
    : options_(std::move(options)), callbacks_(std::move(callbacks)),
      outbound_(options_.queue_capacity, options_.message_ttl) {
    if (options_.host.empty() || options_.port == 0 || options_.send_interval.count() < 0 ||
        options_.reconnect_initial.count() <= 0 || options_.reconnect_max < options_.reconnect_initial ||
        options_.reconnect_max.count() > 3600000)
        throw std::invalid_argument("Invalid Minecraft client options");
}
void MinecraftClient::log(const std::string& message) const {
    if (callbacks_.on_log) callbacks_.on_log(message);
}
void MinecraftClient::set_ready(bool ready) {
    std::lock_guard lock(state_mutex_);
    ready_ = ready;
    if (!ready) outbound_.clear();
}
bool MinecraftClient::send_chat(const std::string& message) {
    return send_chat(std::vector<std::string>{message});
}
bool MinecraftClient::send_chat(const std::vector<std::string>& messages) {
    for (const auto& message : messages) {
        if (message.empty() || message.size() > 100 || message.find("\xc2\xa7") != std::string::npos)
            return false;
        for (unsigned char c : message) if (c < 32 || c == 127) return false;
        try { (void)nlohmann::json(message).dump(); }
        catch (const nlohmann::json::exception&) { return false; }
    }
    std::lock_guard lock(state_mutex_);
    return ready_ && outbound_.push(messages);
}
void MinecraftClient::run(MinecraftAuth& auth, const std::atomic_bool& stop) {
    auto backoff = options_.reconnect_initial;
    while (!stop) {
        const auto started = Clock::now();
        try {
            const auto identity = auth.login(stop);
            connect(identity, stop, [&](const MinecraftIdentity& current, const std::string& hash,
                                       const std::atomic_bool& cancelled) { auth.join(current, hash, cancelled); });
        } catch (const std::exception& error) {
            if (!stop) log(error.what());
        }
        if (!stop) {
            if (Clock::now() - started > std::chrono::minutes(2)) backoff = options_.reconnect_initial;
            log("Minecraft reconnecting in " + std::to_string(backoff.count()) + " ms.");
            pause(stop, static_cast<int>(backoff.count()));
            backoff = std::min(backoff * 2, options_.reconnect_max);
        }
    }
}
} // namespace kprotocol

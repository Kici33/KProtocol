#include "kprotocol/text_component.hpp"

#include "kprotocol/codec.hpp"

namespace kprotocol {

std::string json_text(const std::string& plain) {
    std::string out = R"({"text":")";
    for (const char c : plain) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out += R"("})";
    return out;
}

NBTBlob text_component_nbt(const std::string& plain) {
    std::vector<std::uint8_t> out;
    out.push_back(0x0A); // TAG_Compound
    out.push_back(0x08); // TAG_String child
    codec::write_string(out, "text");
    codec::write_string(out, plain);
    out.push_back(0x00); // TAG_End
    return NBTBlob{.data = std::move(out)};
}

bool uses_nbt_text(const ProtocolVersion version) noexcept {
    return protocol_number(version) >= protocol_number(ProtocolVersion::v1_20_4);
}

bool uses_split_title_packets(const ProtocolVersion version) noexcept {
    return protocol_number(version) >= protocol_number(ProtocolVersion::v1_17);
}

bool uses_action_bar_packet(const ProtocolVersion version) noexcept {
    return protocol_number(version) >= protocol_number(ProtocolVersion::v1_17);
}

} // namespace kprotocol

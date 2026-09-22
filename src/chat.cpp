#include "kprotocol/chat.hpp"

namespace kprotocol {
std::string clean_text(const std::string& input) {
    std::string out;
    bool space = false;
    for (std::size_t i = 0; i < input.size();) {
        const auto ch = static_cast<unsigned char>(input[i]);
        if (ch == 0xc2 && i + 1 < input.size() && static_cast<unsigned char>(input[i + 1]) == 0xa7) {
            i += 2;
            if (i < input.size()) ++i;
            continue;
        }
        if (ch <= 32 || ch == 127) {
            space = !out.empty();
            ++i;
            continue;
        }
        std::size_t size = ch < 128 ? 1 : (ch >= 0xc2 && ch <= 0xdf ? 2 :
            (ch >= 0xe0 && ch <= 0xef ? 3 : (ch >= 0xf0 && ch <= 0xf4 ? 4 : 0)));
        if (!size || i + size > input.size()) { ++i; continue; }
        bool valid = true;
        for (std::size_t j = 1; j < size; ++j)
            if ((static_cast<unsigned char>(input[i + j]) & 0xc0) != 0x80) valid = false;
        if (size >= 3) {
            const auto next = static_cast<unsigned char>(input[i + 1]);
            if ((ch == 0xe0 && next < 0xa0) || (ch == 0xed && next >= 0xa0) ||
                (ch == 0xf0 && next < 0x90) || (ch == 0xf4 && next >= 0x90)) valid = false;
        }
        if (!valid) { ++i; continue; }
        if (space) out += ' ';
        space = false;
        out.append(input, i, size);
        i += size;
    }
    return out;
}
std::string take_utf8(const std::string& value, std::size_t limit) {
    if (value.size() <= limit) return value;
    while (limit && (static_cast<unsigned char>(value[limit]) & 0xc0) == 0x80) --limit;
    return value.substr(0, limit);
}
std::string component_text(const nlohmann::json& c) {
    if (c.is_string()) return c.get<std::string>();
    std::string text;
    if (c.is_array()) { for (const auto& part : c) text += component_text(part); return text; }
    if (!c.is_object()) return text;
    text = c.value("text", "");
    if (c.contains("translate")) {
        // Vanilla player chat. Literal server messages use text/extra.
        const auto args = c.value("with", nlohmann::json::array());
        if (c["translate"] == "chat.type.text" && args.size() == 2)
            text += "<" + component_text(args[0]) + "> " + component_text(args[1]);
        else for (const auto& arg : args) text += component_text(arg);
    }
    if (c.contains("extra")) text += component_text(c["extra"]);
    return text;
}

} // namespace kprotocol

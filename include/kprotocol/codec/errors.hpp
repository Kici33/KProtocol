#pragma once

#include <stdexcept>
#include <string>

namespace kprotocol::codec {

// Thrown by encode-side helpers when input violates a structural invariant
// (e.g. string length exceeds protocol limit, VarInt out of range).
// Derived from std::runtime_error so existing catch (std::exception&) sites still work.
class EncodeError : public std::runtime_error {
public:
    explicit EncodeError(const std::string& what) : std::runtime_error(what) {}
};

// Thrown by decode-side helpers for unrecoverable malformed input
// (e.g. corrupt compression stream, decompressed payload exceeds limit).
// The non-throwing try_* variants prefer returning false; this exception
// is reserved for paths where the caller asked for a value, not a status.
class DecodeError : public std::runtime_error {
public:
    explicit DecodeError(const std::string& what) : std::runtime_error(what) {}
};

} // namespace kprotocol::codec

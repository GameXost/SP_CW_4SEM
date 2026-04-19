#include "execution/serializer.h"
#include <stdexcept>
#include <cstring>

namespace Serializer {

static void pushU32(uint32_t v, std::vector<uint8_t>& buf) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8)  & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 24) & 0xFF);
}

static uint32_t readU32(const std::vector<uint8_t>& buf, size_t pos) {
    if (pos + 4 > buf.size()) throw std::runtime_error("Serializer: buffer too short for u32");
    return  static_cast<uint32_t>(buf[pos]) | (static_cast<uint32_t>(buf[pos+1]) << 8) | (static_cast<uint32_t>(buf[pos+2]) << 16) | (static_cast<uint32_t>(buf[pos+3]) << 24);
}

void encodeValue(const Value& v, std::vector<uint8_t>& buf) {
    if (std::holds_alternative<std::monostate>(v)) {
        buf.push_back(0x00);     // null
    } else if (std::holds_alternative<int>(v)) {
        buf.push_back(0x01);    // int
        int32_t n = static_cast<int32_t>(std::get<int>(v));
        uint32_t raw;
        std::memcpy(&raw, &n, 4);
        pushU32(raw, buf);
    } else {
        const auto& s = std::get<std::string>(v);
        buf.push_back(0x02);    // string
        pushU32(static_cast<uint32_t>(s.size()), buf);
        buf.insert(buf.end(), s.begin(), s.end());
    }
}

std::vector<uint8_t> encodeRow(const std::vector<Value>& row) {
    std::vector<uint8_t> buf;
    for (const auto& v : row) encodeValue(v, buf);
    return buf;
}

Value decodeValue(const std::vector<uint8_t>& buf, size_t& pos) {
    if (pos >= buf.size()) throw std::runtime_error("Serializer: unexpected end of buffer");
    uint8_t tag = buf[pos++];

    if (tag == 0x00) {
        return std::monostate{};
    }
    if (tag == 0x01) {
        uint32_t raw = readU32(buf, pos); pos += 4;
        int32_t  n;
        std::memcpy(&n, &raw, 4);
        return static_cast<int>(n);
    }
    if (tag == 0x02) {
        uint32_t len = readU32(buf, pos); pos += 4;
        if (pos + len > buf.size()) throw std::runtime_error("Serializer: string out of bounds");
        std::string s(buf.begin() + pos, buf.begin() + pos + len);
        pos += len;
        return s;
    }
    throw std::runtime_error("Serializer: unknown tag " + std::to_string(tag));
}

std::vector<Value> decodeRow(const std::vector<uint8_t>& buf) {
    std::vector<Value> row;
    size_t pos = 0;
    while (pos < buf.size()) row.push_back(decodeValue(buf, pos));
    return row;
}

}
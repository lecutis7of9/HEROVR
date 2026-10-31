// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. 0.0.74 MP INTERNET: STUN Binding and the relay frame (see mp_inet.hpp).
#include "spidy/mp_inet.hpp"

#include <cstring>

namespace spidy::mp {
namespace {
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(uint8_t(x >> 8)), v.push_back(uint8_t(x)); }
void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8)
        v.push_back(uint8_t(x >> s));
}
constexpr uint16_t bindingRequest = 0x0001, bindingSuccess = 0x0101;
constexpr uint16_t attrMapped = 0x0001, attrXorMapped = 0x0020;
} // namespace

std::vector<uint8_t> stunRequest(const StunTxn& txn) {
    std::vector<uint8_t> v;
    put16(v, bindingRequest);
    put16(v, 0);
    put32(v, stunCookie);
    v.insert(v.end(), txn.begin(), txn.end());
    return v;
}

bool isStun(const uint8_t* data, size_t size) {
    if (!data || size < stunHeaderBytes || (data[0] & 0xc0) != 0 || be32(data + 4) != stunCookie)
        return false;
    const size_t len = be16(data + 2);
    return len % 4 == 0 && stunHeaderBytes + len <= size;
}

bool stunMapped(const uint8_t* data, size_t size, const StunTxn& txn, Peer& out) {
    if (!isStun(data, size) || be16(data) != bindingSuccess || std::memcmp(data + 8, txn.data(), txn.size()) != 0)
        return false;
    const size_t end = stunHeaderBytes + be16(data + 2);
    bool got = false;
    Peer plain{}, xored{};
    bool havePlain = false;
    for (size_t at = stunHeaderBytes; at + 4 <= end;) {
        const uint16_t type = be16(data + at), len = be16(data + at + 2);
        const size_t v = at + 4;
        if (v + len > end)
            return false;
        if ((type == attrXorMapped || type == attrMapped) && len >= 8 && data[v + 1] == 0x01) { // IPv4
            Peer p{be32(data + v + 4), be16(data + v + 2)};
            if (type == attrXorMapped) {
                p.port = static_cast<uint16_t>(p.port ^ (stunCookie >> 16));
                p.addr ^= stunCookie;
                xored = p, got = true;
            } else {
                plain = p, havePlain = true;
            }
        }
        at = v + ((len + 3u) & ~3u);
    }
    if (!got && !havePlain)
        return false;
    out = got ? xored : plain;
    return out.port != 0 && out.addr != 0;
}

std::vector<uint8_t> stunAnswer(const uint8_t* data, size_t size, Peer seen) {
    if (!isStun(data, size) || be16(data) != bindingRequest)
        return {};
    std::vector<uint8_t> v;
    put16(v, bindingSuccess);
    put16(v, 12);
    put32(v, stunCookie);
    v.insert(v.end(), data + 8, data + 20);
    put16(v, attrXorMapped);
    put16(v, 8);
    v.push_back(0), v.push_back(0x01);
    put16(v, static_cast<uint16_t>(seen.port ^ (stunCookie >> 16)));
    put32(v, seen.addr ^ stunCookie);
    return v;
}

std::vector<uint8_t> relayWrap(const RelayToken& token, const uint8_t* data, size_t size) {
    std::vector<uint8_t> v{'H', 'V', 'R', 'Y'};
    v.insert(v.end(), token.begin(), token.end());
    if (data && size)
        v.insert(v.end(), data, data + size);
    return v;
}

bool relayFrame(const uint8_t* data, size_t size, const RelayToken& token) {
    return data && size >= relayHeaderBytes && std::memcmp(data, "HVRY", 4) == 0 &&
           std::memcmp(data + 4, token.data(), token.size()) == 0;
}

bool parseToken(const std::string& hex, RelayToken& out) {
    if (hex.size() != 16)
        return false;
    RelayToken t{};
    for (size_t i = 0; i < 16; ++i) {
        const char c = hex[i];
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0)
            return false;
        t[i / 2] = static_cast<uint8_t>(t[i / 2] << 4 | d);
    }
    out = t;
    return true;
}

std::string tokenText(const RelayToken& t) {
    static const char* hexd = "0123456789abcdef";
    std::string s;
    for (uint8_t b : t)
        s += hexd[b >> 4], s += hexd[b & 15];
    return s;
}

bool tokenSet(const RelayToken& t) {
    for (uint8_t b : t)
        if (b)
            return true;
    return false;
}
} // namespace spidy::mp

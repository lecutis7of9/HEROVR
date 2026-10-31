// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. 0.0.74 MP LOBBY: the LAN host beacon, its targets, the heard list and MP-LOBBY.txt.
#include "spidy/mp_lobby.hpp"

#include "spidy/mp_net.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace spidy::mp {
namespace {
void put16(std::vector<uint8_t>& o, size_t at, uint16_t v) { o[at] = v & 255, o[at + 1] = v >> 8; }
void put32(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        o[at + i] = (v >> (8 * i)) & 255;
}
uint32_t get32(const uint8_t* d) { return d[0] | (d[1] << 8) | (d[2] << 16) | (static_cast<uint32_t>(d[3]) << 24); }
void putText(std::vector<uint8_t>& o, size_t at, const std::string& s) {
    const std::string c = cleanName(s.c_str());
    std::memcpy(o.data() + at, c.data(), std::min<size_t>(c.size(), 16));
}
bool getText(const uint8_t* d, std::string& out) {
    char b[17]{};
    std::memcpy(b, d, 16);
    for (int i = 0; i < 16 && b[i]; ++i)
        if (static_cast<unsigned char>(b[i]) < 32 || static_cast<unsigned char>(b[i]) > 126)
            return false;
    out = b;
    return true;
}
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), z = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string{} : s.substr(a, z - a + 1);
}
} // namespace

std::vector<uint8_t> encodeBeacon(const Beacon& b) {
    std::vector<uint8_t> o(beaconBytes, 0);
    put32(o, 0, beaconMagic);
    o[4] = lobbyVersion;
    o[5] = b.protocol;
    put16(o, 6, b.gamePort);
    o[8] = b.players;
    o[9] = b.maxPlayers;
    o[10] = b.hero;
    o[11] = 0;
    put32(o, 12, b.hostId);
    putText(o, 16, b.name);
    putText(o, 32, b.heroName);
    put32(o, 48, crc32(o.data(), 48));
    return o;
}

bool decodeBeacon(const uint8_t* d, size_t size, Beacon& out) {
    if (!d || size != beaconBytes || get32(d) != beaconMagic || d[4] != lobbyVersion || get32(d + 48) != crc32(d, 48))
        return false;
    Beacon b;
    b.protocol = d[5];
    b.gamePort = static_cast<uint16_t>(d[6] | (d[7] << 8));
    b.players = d[8];
    b.maxPlayers = d[9];
    b.hero = d[10];
    b.hostId = get32(d + 12);
    if (!b.gamePort || !b.maxPlayers || b.players > 16 || b.maxPlayers > 16)
        return false;
    if (!getText(d + 16, b.name) || !getText(d + 32, b.heroName))
        return false;
    out = b;
    return true;
}

std::vector<uint32_t> broadcastTargets(const std::vector<uint32_t>& localAddrs) {
    std::vector<uint32_t> t{0xffffffffu};
    for (uint32_t a : localAddrs) {
        if (!a || (a >> 24) == 127 || (a >> 16) == 0xa9feu) // 0.0.0.0, loopback, link-local (no DHCP)
            continue;
        const uint32_t bc = a | 0xffu;
        if (std::find(t.begin(), t.end(), bc) == t.end())
            t.push_back(bc);
    }
    return t;
}

void HostList::heard(uint32_t fromAddr, const Beacon& b, double now) {
    for (auto& h : hosts_)
        if (h.beacon.hostId == b.hostId && h.beacon.gamePort == b.gamePort) {
            // The same host heard on two networks: keep the address it was first heard on (it answers there).
            if (now - h.lastSeen > forget)
                h.addr = fromAddr;
            h.beacon = b;
            h.lastSeen = now;
            return;
        }
    if (hosts_.size() >= 32)
        return;
    hosts_.push_back({fromAddr, b, now});
}

std::vector<HostSeen> HostList::list(double now) const {
    std::vector<HostSeen> out;
    for (const auto& h : hosts_)
        if (now - h.lastSeen <= forget)
            out.push_back(h);
    std::sort(out.begin(), out.end(), [](const HostSeen& a, const HostSeen& b) {
        return a.beacon.name != b.beacon.name ? a.beacon.name < b.beacon.name : a.addr < b.addr;
    });
    return out;
}

LobbySetting parseLobbySetting(const std::string& text) {
    LobbySetting s;
    LobbySetting::Mode mode = LobbySetting::none;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (line.size() >= 3 && static_cast<uint8_t>(line[0]) == 0xef) // a UTF-8 mark (Notepad, .NET)
            line = line.substr(3);
        if (const size_t hash = line.find('#'); hash != std::string::npos)
            line.resize(hash);
        line = trim(line);
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        for (char& c : key)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "mode") {
            std::string v = value;
            for (char& c : v)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            mode = v == "host" ? LobbySetting::host : v == "join" ? LobbySetting::join : LobbySetting::none;
        } else if (key == "join") {
            const PeerSetting p = parsePeerSetting(value);
            if (p.ok)
                s.joinHost = p.host, s.joinPort = p.port;
        } else if (key == "name") {
            s.name = cleanName(value.c_str());
        } else if (key == "host") {
            s.hostName = cleanName(value.c_str());
        }
        if (end == text.size())
            break;
    }
    s.mode = mode == LobbySetting::join && s.joinHost.empty() ? LobbySetting::none : mode;
    return s;
}

std::string lobbyText(const LobbySetting& s) {
    std::string t = "# HERO VR multiplayer (written by the launcher's Multiplayer sheet; LEAVE deletes it)\n";
    t += s.mode == LobbySetting::host ? "mode=host\n" : s.mode == LobbySetting::join ? "mode=join\n" : "mode=none\n";
    if (s.mode == LobbySetting::join)
        t += "join=" + s.joinHost + ":" + std::to_string(s.joinPort) + "\n";
    if (!s.name.empty())
        t += "name=" + s.name + "\n";
    if (!s.hostName.empty())
        t += "host=" + s.hostName + "\n";
    return t;
}
} // namespace spidy::mp

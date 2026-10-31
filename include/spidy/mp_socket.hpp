#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: one non-blocking UDP socket (Winsock / POSIX).
// Every call reports failure by its result (and lastError()), never by throwing or blocking.
#include "spidy/mp_net.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::mp {
inline constexpr uint32_t loopbackAddr = 0x7f000001u; // 127.0.0.1 (host order)
inline constexpr uint32_t anyAddr = 0;
std::string addrText(Peer p);
bool parseAddr(const std::string& text, uint32_t& addr); // dotted IPv4, host order
// 0.0.74 MP JOIN: a friend's address - dotted IPv4 or a host name (the system's resolver, IPv4 only; it may
// take a moment: the network thread calls it, never the render thread). False: `why` says.
bool resolvePeer(const std::string& host, uint16_t port, Peer& out, std::string& why);
// 0.0.74 MP LOBBY: this PC's IPv4 addresses (the host name's, from the system resolver; loopback left out).
std::vector<uint32_t> localAddresses();

class UdpSocket {
  public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    // Binds `addr`:`port` (port 0: any free one). False: see lastError().
    bool open(uint32_t addr, uint16_t port);
    // 0.0.74 MP LOBBY: bind with SO_REUSEADDR (the launcher and a game may both listen on the lobby port) and
    // allow broadcasts out. False: see lastError() (the socket stays usable for plain sends).
    bool openShared(uint32_t addr, uint16_t port);
    bool allowBroadcast();
    void close();
    bool isOpen() const;
    // A datagram's size (and its sender), 0 when none is waiting, -1 on an error.
    int receive(uint8_t* buf, size_t capacity, Peer& from);
    bool send(const uint8_t* data, size_t size, Peer to);
    uint16_t localPort() const { return port_; }
    const std::string& lastError() const { return error_; }

  private:
    bool openImpl(uint32_t addr, uint16_t port, bool shared);
    uintptr_t handle_ = ~uintptr_t{0};
    uint16_t port_{};
    std::string error_;
    bool started_{};
};
} // namespace spidy::mp

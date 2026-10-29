#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: one non-blocking UDP socket (Winsock / POSIX).
// Every call reports failure by its result (and lastError()), never by throwing or blocking.
#include "spidy/mp_net.hpp"

#include <cstdint>
#include <string>

namespace spidy::mp {
inline constexpr uint32_t loopbackAddr = 0x7f000001u; // 127.0.0.1 (host order)
inline constexpr uint32_t anyAddr = 0;
std::string addrText(Peer p);
bool parseAddr(const std::string& text, uint32_t& addr); // dotted IPv4, host order

class UdpSocket {
  public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    // Binds `addr`:`port` (port 0: any free one). False: see lastError().
    bool open(uint32_t addr, uint16_t port);
    void close();
    bool isOpen() const;
    // A datagram's size (and its sender), 0 when none is waiting, -1 on an error.
    int receive(uint8_t* buf, size_t capacity, Peer& from);
    bool send(const uint8_t* data, size_t size, Peer to);
    uint16_t localPort() const { return port_; }
    const std::string& lastError() const { return error_; }

  private:
    uintptr_t handle_ = ~uintptr_t{0};
    uint16_t port_{};
    std::string error_;
    bool started_{};
};
} // namespace spidy::mp

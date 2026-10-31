// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. Version 0.0.73 MP SOLO TEST: the UDP socket (mp_socket.hpp).
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "spidy/mp_socket.hpp"

#include <cstdio>

namespace spidy::mp {
namespace {
constexpr uintptr_t none = ~uintptr_t{0};
#ifdef _WIN32
using Native = SOCKET;
int lastNative() { return WSAGetLastError(); }
bool wouldBlock(int e) { return e == WSAEWOULDBLOCK || e == WSAECONNRESET || e == WSAEMSGSIZE; }
#else
using Native = int;
int lastNative() { return errno; }
bool wouldBlock(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == ECONNREFUSED || e == EINTR; }
#endif
Native native(uintptr_t h) { return static_cast<Native>(h); }
std::string describe(const char* what) {
    char b[96];
    std::snprintf(b, sizeof b, "%s failed (error %d)", what, lastNative());
    return b;
}
} // namespace

std::string addrText(Peer p) {
    char b[32];
    std::snprintf(b, sizeof b, "%u.%u.%u.%u:%u", (p.addr >> 24) & 255, (p.addr >> 16) & 255, (p.addr >> 8) & 255,
                  p.addr & 255, p.port);
    return b;
}
bool parseAddr(const std::string& text, uint32_t& addr) {
    unsigned a, b, c, d;
    char tail;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        return false;
    addr = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
}

bool resolvePeer(const std::string& host, uint16_t port, Peer& out, std::string& why) {
    uint32_t a = 0;
    if (!port) {
        why = "no port";
        return false;
    }
    if (parseAddr(host, a)) {
        if (!a) {
            why = "0.0.0.0 is nobody";
            return false;
        }
        out = {a, port};
        return true;
    }
    if (host.empty() || host.size() > 253) {
        why = "no address";
        return false;
    }
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        why = "WSAStartup failed";
        return false;
    }
#endif
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* got = nullptr;
    const int e = ::getaddrinfo(host.c_str(), nullptr, &hints, &got);
    bool ok = false;
    if (e == 0) {
        for (addrinfo* i = got; i && !ok; i = i->ai_next)
            if (i->ai_family == AF_INET && i->ai_addr) {
                const auto* sa = reinterpret_cast<const sockaddr_in*>(i->ai_addr);
                out = {ntohl(sa->sin_addr.s_addr), port};
                ok = out.addr != 0;
            }
        ::freeaddrinfo(got);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    if (!ok) {
        char b[96];
        std::snprintf(b, sizeof b, "'%s' not found (error %d)", host.substr(0, 40).c_str(), e);
        why = b;
    }
    return ok;
}

UdpSocket::~UdpSocket() { close(); }
bool UdpSocket::isOpen() const { return handle_ != none; }

bool UdpSocket::open(uint32_t addr, uint16_t port) {
    close();
    error_.clear();
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error_ = "WSAStartup failed";
        return false;
    }
    started_ = true;
#endif
    const Native s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
#else
    if (s < 0) {
#endif
        error_ = describe("socket");
        close();
        return false;
    }
    handle_ = static_cast<uintptr_t>(s);
#ifdef _WIN32
    u_long nb = 1;
    if (ioctlsocket(s, FIONBIO, &nb) != 0) {
        error_ = describe("non-blocking");
        close();
        return false;
    }
    // A send to a closed port makes Windows fail the NEXT receive with WSAECONNRESET: switch that off.
    BOOL off = FALSE;
    DWORD got = 0;
    WSAIoctl(s, _WSAIOW(IOC_VENDOR, 12) /* SIO_UDP_CONNRESET */, &off, sizeof off, nullptr, 0, &got, nullptr, nullptr);
#else
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) != 0) {
        error_ = describe("non-blocking");
        close();
        return false;
    }
#endif
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(addr);
    if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0) {
        error_ = describe("bind");
        close();
        return false;
    }
    sockaddr_in got2{};
#ifdef _WIN32
    int len = sizeof got2;
#else
    socklen_t len = sizeof got2;
#endif
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&got2), &len) == 0)
        port_ = ntohs(got2.sin_port);
    return true;
}

void UdpSocket::close() {
    if (handle_ != none) {
#ifdef _WIN32
        ::closesocket(native(handle_));
#else
        ::close(native(handle_));
#endif
        handle_ = none;
    }
#ifdef _WIN32
    if (started_)
        WSACleanup();
#endif
    started_ = false;
    port_ = 0;
}

int UdpSocket::receive(uint8_t* buf, size_t capacity, Peer& from) {
    if (handle_ == none || !buf)
        return -1;
    sockaddr_in a{};
#ifdef _WIN32
    int len = sizeof a;
    const int n = ::recvfrom(native(handle_), reinterpret_cast<char*>(buf), static_cast<int>(capacity), 0,
                             reinterpret_cast<sockaddr*>(&a), &len);
#else
    socklen_t len = sizeof a;
    const int n = static_cast<int>(::recvfrom(native(handle_), buf, capacity, 0, reinterpret_cast<sockaddr*>(&a), &len));
#endif
    if (n < 0) {
        const int e = lastNative();
        if (wouldBlock(e))
            return 0;
        error_ = describe("receive");
        return -1;
    }
    from = {ntohl(a.sin_addr.s_addr), ntohs(a.sin_port)};
    // (An empty datagram reads as 0, "nothing waiting": there is nothing in it to use anyway.)
    return n;
}

bool UdpSocket::send(const uint8_t* data, size_t size, Peer to) {
    if (handle_ == none || !data || !size)
        return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(to.port);
    a.sin_addr.s_addr = htonl(to.addr);
#ifdef _WIN32
    const int n = ::sendto(native(handle_), reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                           reinterpret_cast<const sockaddr*>(&a), sizeof a);
#else
    const int n = static_cast<int>(::sendto(native(handle_), data, size, 0, reinterpret_cast<const sockaddr*>(&a), sizeof a));
#endif
    if (n != static_cast<int>(size)) {
        error_ = describe("send");
        return false;
    }
    return true;
}
} // namespace spidy::mp

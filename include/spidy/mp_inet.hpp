#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 MP INTERNET: the two small wire formats a game needs to meet a friend over the internet, pure.
// - STUN (RFC 5389) Binding: the game asks a public STUN server (stun.cloudflare.com:3478) from its own game
//   socket "which address and port do you see?" - that is the address a friend's game can reach after both
//   have sent to each other (UDP hole punching). Only the Binding request/success, IPv4, no auth.
// - The relay frame, for the routers punching cannot get through (both behind strict or shared NATs): the
//   packet goes to the HERO VR relay (server/relay) as "HVRY" + an 8-byte token the lobby gave both players +
//   the game packet. The relay passes it to the other player of the same token, header kept; the game strips it.
// No game data, no game addresses (CONFIRMED by construction).
#include "spidy/mp_net.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spidy::mp {
inline constexpr uint32_t stunCookie = 0x2112A442u;
inline constexpr size_t stunHeaderBytes = 20;
inline constexpr uint16_t stunDefaultPort = 3478;
using StunTxn = std::array<uint8_t, 12>;
// A Binding request (20 bytes, no attributes).
std::vector<uint8_t> stunRequest(const StunTxn& txn);
// Looks like STUN (top two bits 0, the magic cookie, length a multiple of 4 that fits). Game packets never do.
bool isStun(const uint8_t* data, size_t size);
// A Binding success for `txn`: the mapped IPv4 address (XOR-MAPPED-ADDRESS, else MAPPED-ADDRESS). Anything
// else (another transaction, an error, IPv6, damaged): false.
bool stunMapped(const uint8_t* data, size_t size, const StunTxn& txn, Peer& out);
// The server side, for the tests and the local test server: the success answer to a Binding request telling
// `seen`; empty when `data` is not a Binding request.
std::vector<uint8_t> stunAnswer(const uint8_t* data, size_t size, Peer seen);

inline constexpr size_t relayHeaderBytes = 12;   // "HVRY" + token (8, as sent)
using RelayToken = std::array<uint8_t, 8>;
std::vector<uint8_t> relayWrap(const RelayToken& token, const uint8_t* data, size_t size);
// `data` is a relay frame for `token`: the game packet starts at relayHeaderBytes (may be empty: a keepalive).
bool relayFrame(const uint8_t* data, size_t size, const RelayToken& token);
// 16 hex digits (what the lobby hands out). False: not a token.
bool parseToken(const std::string& hex, RelayToken& out);
std::string tokenText(const RelayToken& t);
bool tokenSet(const RelayToken& t); // not all zero
} // namespace spidy::mp

# HERO VR 0.0.73 MP SOLO TEST

A private test of the multiplayer plumbing with one player. Nothing here talks to the game's own
network, accounts or servers, and no game memory address is read or written by it.

## What it does

* **Wrist setting `MP SOLO TEST`** (SURVIVAL AND REPUTATION page), **ON by default**, live (no restart).
* While ON, every frame the mod packs your pose - hero, suit, head, both hands, body root and yaw,
  action flags (flying, swinging, grapple, firing) and a sequence number - into a small versioned UDP
  packet (`include/spidy/mp_net.hpp`, protocol version 1, CRC32 trailer) and sends it at 30 Hz.
* **Loopback ghost:** the packets go to an echo on `127.0.0.1` that holds them ~0.4 s and sends them back
  marked as another player; with the 0.1 s interpolation you see yourself about half a second late as
  `PLAYER 2 (YOU, LOOPBACK)  PING n MS`, ~3 m ahead, facing you, mirrored like a dance partner, in your
  current hero and suit.
* **HERO VR MP Test Buddy** (`tools/mp_test_buddy.py`, runs on the zip's bundled Python): connects with
  HELLO / WELCOME on UDP 39470; while connected its puppet (waves, walks a circle, switches suits on a key)
  replaces the loopback as `PLAYER 2 (TEST BUDDY)`. When it quits (BYE) or goes silent 3 s, the loopback
  comes back.
* Drawn only with the mod's own models: the remote suit's `suit_wear` rig + `mesh_model` mesh from
  `Models\Suits` when installed, else a soft stick figure in the hero's colour.
* Failure rule: a socket that cannot open, a malformed / short / wrong-version / bad-checksum packet, or a
  draw failure hides the remote player and logs one console line (`MP SOLO TEST: ...`). Never a crash,
  never a fall back to flatscreen.

## Files

| File | Role |
|---|---|
| `include/spidy/mp_net.hpp`, `src/mp_net.cpp` | packet encode/decode, validation, interpolation, echo delay, partner pose, buddy handshake (pure) |
| `include/spidy/mp_socket.hpp`, `src/mp_socket.cpp` | non-blocking UDP (Winsock / BSD) |
| `include/spidy/mp_session.hpp`, `src/mp_session.cpp` | the network thread: loopback echo, buddy link, ping |
| `include/spidy/mp_ghost.hpp`, `src/mp_ghost.cpp` | placement, stick figure, nameplate (pure) |
| `src/game_xr.cpp` | wiring: publish the pose, draw the remote player (after the suit, before the mirror) |
| `tools/mp_test_buddy.py` | the Test Buddy (stdlib only), `--selftest` matches `spidy_mp_probe golden` byte for byte |
| `tests/mp_tests.cpp`, `tests/mp_buddy_tests.py` | round trip, versioning, malformed packets, interpolation, loopback delay, buddy handshake, live buddy |

## CONFIRMED / CANDIDATE

| Item | Status |
|---|---|
| Pose source: the mod's own tracked head / grips / anchor / turn (`rendered.*`, `travel`) | CONFIRMED (mod-internal) |
| Hero (`heroNow()`), worn suit (`wornSuitSlot`), action flags (`flyNow`, `swingState`, `capeNow`, `palmGlow`, `beamFade`) | CONFIRMED (mod-internal) |
| Remote body drawn from `suit_wear` / `mesh_model` only | CONFIRMED (mod-internal) |
| Loopback + Test Buddy on localhost | CONFIRMED (Linux tests and CI; in-headset: to be confirmed by the tester) |
| Buddy on another PC over LAN (opt-in: empty file `%APPDATA%\Spidy\MP-LAN-ON.txt`, UDP 39470 through Windows Firewall) | CANDIDATE |
| Remote player's placement in the world (drawn relative to you, ~3 m ahead; not at its real world position) | CANDIDATE design for the solo test |
| Game's own suit id at `+0x8c` | NOT USED |
| Any game body / actor address for a remote player | NOT USED (none invented) |
| Steam P2P / NAT traversal / more than 2 players | FUTURE |

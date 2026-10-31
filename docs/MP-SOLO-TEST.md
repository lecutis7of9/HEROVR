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
* **0.0.74 FLIGHT POSE:** the pose also carries the body's flight lean (its leaned up, 12 bytes more: a
  124-byte POSE), so Player 2 leans forward / back in flight as you do. A 0.0.73 112-byte POSE (game or Test Buddy)
  is still read (upright); an older build ignores the extra bytes. A broken lean draws upright, never hidden.
* **HERO VR MP Test Buddy** (`tools/mp_test_buddy.py`, runs on the zip's bundled Python): connects with
  HELLO / WELCOME on UDP 39470; while connected its puppet (waves, walks a circle, switches suits on a key)
  replaces the loopback as `PLAYER 2 (TEST BUDDY)`. When it quits (BYE) or goes silent 3 s, the loopback
  comes back.
* Drawn only with the mod's own models: the remote suit's `suit_wear` rig + `mesh_model` mesh from
  `Models\Suits` when installed, else (0.0.73 MPSUIT, PLAYER 2 BODY = SUIT LOOK, the default) a shaded hero
  body (`mp_body`): Spider-Man in a game suit is drawn web-patterned in that suit's colours as VR MIRROR
  learned them (sent in the pose's LOOK tail), the other heroes in their colour with a visor and badge.
  PLAYER 2 BODY = FIGURE (`mp_player2_body = figure`, export `SpidyMpPlayer2Body`, 5477), or a body that
  cannot be drawn, keeps test 6's soft stick figure.
* Failure rule: a socket that cannot open, a malformed / short / wrong-version / bad-checksum packet, or a
  draw failure hides the remote player and logs one console line (`MP SOLO TEST: ...`). Never a crash,
  never a fall back to flatscreen.

## Playing with a friend (0.0.74 MP JOIN)

Two real games, one on each PC, see each other: the friend is PLAYER 2, standing in front of you
(his moves, his hero and suit, his name and ping on the nameplate). Both players need this build.

1. Each player finds the address the other can reach:
   - same house / LAN: the PC's LAN IPv4 (`ipconfig`, e.g. `192.168.1.23`);
   - over the internet: a VPN such as Tailscale, ZeroTier or Radmin VPN (use its `100.x` / `10.x` /
     `26.x` address), or forward **UDP 39470** on the router to that PC and use the public IP.
2. Each player creates `%APPDATA%\Spidy\MP-PEER.txt` (Notepad is fine) holding the *other* player's
   address, and optionally a name:

   ```
   # my friend
   192.168.1.23
   name=Peter
   ```

   `host:port` works too (the port defaults to 39470); so do `peer=`, `friend=` or `address=`, and a
   host name. Only one side really needs the file (the other just answers), but both having it helps
   through firewalls and NATs (each side's call opens its own firewall for the other's).
3. Allow the game through Windows Firewall (private networks) when Windows asks, or allow UDP 39470.
4. Start the game. MP turns on by itself while the file has an address (the MP SOLO TEST setting is
   not needed; no loopback echo). The console shows:
   - `MP JOIN: on - calling 192.168.1.23:39470 as 'PETER', listening on UDP 39470 (every interface)`
   - `MP: calling ...`, then `MP: still calling ...` every 15 s with what to check;
   - `MP: player 'MJ' joined from 192.168.1.23:39470 (answered our call)` (or `(called us)`).
   If he quits, `MP: player 'MJ' left - calling again`.
5. Editing or deleting MP-PEER.txt takes effect within 2 s (the network restarts; deleted = back to
   the MP SOLO TEST setting).

Solo check of the same path without a friend: `spidy_mp_probe run 39480 60 "" Bob` on any PC, and an
MP-PEER.txt holding `127.0.0.1:39480` (or that PC's address).

## Seeing what your friend holds (0.0.74 MP PROPS)

Each pose packet now carries a small 'P' tail (at most 342 bytes, so a packet stays under 512): what the
player holds in each hand, what they wear (holstered pistols, the batarang on the belt, the shield on the
back), Thor's hammer (in hand, on the back, or flying, with its spin) and the batons. The ghost draws them
with the same models your own hero uses; the first time a friend's prop needs a model your game has not
loaded yet, it is loaded right then and the console says `MP PROPS: PLAYER 2's <folder>: <status>`.
Thor's hammer is drawn only with its model (never the built-in bars). In the loopback mirror the hands swap,
as in a mirror. Old 0.0.73/0.0.74 builds ignore the tail (they see the friend without props).

Solo test with the buddy: `python tools/mp_test_buddy.py --props all` (the default) shows a shield in
the left hand, the hammer in the right, a batarang and a pistol worn. `--props batarang|hammer|batons|shield|guns|claws|none`
picks one show, P in the buddy window cycles them, `--props-every 5` cycles by itself.

## Files

| File | Role |
|---|---|
| `include/spidy/mp_net.hpp`, `src/mp_net.cpp` | packet encode/decode, validation, interpolation, echo delay, partner pose, buddy handshake (pure) |
| `include/spidy/mp_socket.hpp`, `src/mp_socket.cpp` | non-blocking UDP (Winsock / BSD) |
| `include/spidy/mp_session.hpp`, `src/mp_session.cpp` | the network thread: loopback echo, buddy link, ping |
| `include/spidy/mp_ghost.hpp`, `src/mp_ghost.cpp` | placement, stick figure, nameplate (pure) |
| `include/spidy/mp_body.hpp`, `src/mp_body.cpp` | 0.0.73 MPSUIT PLAYER 2 BODY: the shaded hero body for a suit with no mod model, and its LOOK (pure) |
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

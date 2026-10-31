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

## Host and join from the launcher (0.0.74 MP LOBBY, same network)

No addresses to type. Both players need this build and the same home network (Wi-Fi or cable).

1. Host: in the launcher, open **Multiplayer** (header button, or the "Multiplayer" link on the new design's home),
   type a name, press **Host**, then PLAY VR. The game announces itself on the network once a second
   (UDP broadcast on port 39469: name, hero, 1/2 or 2/2 players).
2. Friend: open **Multiplayer** in his launcher. "Games on this network" lists the host within a second or two
   (name, hero, address, players). Press **Join** next to it, then PLAY VR. FULL games and games from another
   mod version cannot be joined.
3. Firewall: one line in the sheet says it - *The first time, Windows asks to let Spider-Man 2 and the launcher
   through the firewall: tick Private networks and press Allow (look at the desktop if you are in the headset).*
   Or press **Allow through firewall**: it adds one inbound rule "HERO VR multiplayer" for UDP 39469-39474 on
   Private networks (Windows asks for admin). If someone clicked "Block" before, that choice wins until the rule
   is added or the block is removed in Windows Defender Firewall.
4. **Leave** deletes the setting; a running game picks up Host / Join / Leave within 2 s (`MP LOBBY: changed in
   the launcher - the network restarts`). The console shows `MP LOBBY: hosting as 'MARY' ...` or
   `MP LOBBY: joining 'MARY' at 192.168.1.20:39470 ...`.

The launcher writes `%APPDATA%\Spidy\MP-LOBBY.txt` (`mode=host` or `mode=join`, `join=IP:port`, `name=`,
`host=`). While it exists it wins over MP-PEER.txt; MP-PEER.txt (below) stays the advanced fallback, for VPNs and
the internet. A host keeps its first friend (a 2-player game): another caller is ignored until he leaves.
The list is the launcher only for now (no in-headset list yet). Broadcast reaches the same subnet only
(255.255.255.255 and each network card's /24); on networks that block broadcast (some guest Wi-Fi, office
networks), use MP-PEER.txt.

Check without a second PC: `spidy_mp_probe lobby 30` lists hosts on the network; `spidy_mp_probe run 39480 60
host Mary` hosts.

## Internet server list (0.0.74 MP INTERNET)

- The launcher opens on **Multiplayer - servers**. It lists the games on this network (LAN) and on the internet
  together, refreshes every 15 s (or with **Refresh**), and shows **Join**, **Full**, **Update** (another mod version) or **You**.
- **HOST**: the game asks public STUN for its internet address and writes `%APPDATA%\Spidy\MP-PUBLIC.txt`.
  The launcher lists it on the server (`InternetLobby.DefaultUrl`, see docs/LOBBY-OWNER-SETUP.md) and keeps it
  listed while the game runs.
- **JOIN** on an internet row: the launcher writes `MP-LOBBY.txt` with `join=`, `relay=` and `token=`. Both games
  then call each other at the same time ("hole punching"). If they are not in after 15 s, they go through the relay.
- **First start**: a pop-up asks "No" / "Yes, Autosetup" for the one Windows Firewall rule (UDP 39469-39474,
  Private + Public networks, one admin prompt). "No" is remembered; **Allow through firewall** offers it again.
- No server set (`DefaultUrl` empty): it's LAN only, exactly as before. No internet connection: the LAN still works.
- Test without the real server: `RELAY_SECRET=x RELAY_ADDR=127.0.0.1:3479 node server/vps/herovr-server.mjs --local`
  and `SM2VR_LOBBY_URL=http://127.0.0.1:8080`. The tests are in tests/hero074_mp_internet_tests.py.
- CONFIRMED by construction (all mod-internal): the files, the ports and the protocol. Untested so far on real
  Windows PCs behind real home routers.

## Playing with a friend by address (0.0.74 MP JOIN, advanced)

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

Thrown things (0.0.74 MP PROPS phase 2) go by state in every packet (30 Hz): a thrown batarang, Cap's
spinning shield, Thor's hammer in flight and Batman's smoke pellets are where your friend's game has them,
moved smoothly between packets (spin the short way round); a catch or a new throw snaps instead of sliding.
`--props throw` makes the buddy throw the hammer, the shield and a batarang out 8 m and back, and lob pellets.

Beams and hit effects (0.0.74 MP PROPS phase 3). Things that last (heat vision and Superman's freeze breath,
Iron Man's chest beam, palm glows and repulsor bolts, web balls in flight, Green Lantern's rings, beam, bubble,
chain and fist, Thor's lightning, grapple lines, Daredevil's cable, the Punisher's hook and Batman's rope) go by
state like the thrown things, at most 5 lines a packet (the most telling first; extra lightning bolts are cut).
One-off effects (muzzle flashes, bullet hits, smoke puffs, heat scorch marks, repulsor and web splashes, shock
rings) go through an effects ring: each is sent with a number in the packets for 0.3 s (about 9 packets, so a
lost packet loses nothing), your game takes it once, puts it where it happened and plays it out on its own
clock. At most 4 a packet: in a very busy fight the oldest may be skipped. `--props effects` shows them all on
the buddy (a shot every half second, a shock ring every 3 s); `--props all` adds a lightning bolt and the shots.

## Files

| File | Role |
|---|---|
| `include/spidy/mp_net.hpp`, `src/mp_net.cpp` | packet encode/decode, validation, interpolation, echo delay, partner pose, buddy handshake (pure) |
| `include/spidy/mp_socket.hpp`, `src/mp_socket.cpp` | non-blocking UDP (Winsock / BSD) |
| `include/spidy/mp_lobby.hpp`, `src/mp_lobby.cpp` | 0.0.74 MP LOBBY: the 52-byte host beacon, the host list, MP-LOBBY.txt (pure) |
| `tools/launcher/src/Lobby.cs`, `MainForm.Multiplayer.cs` | the launcher's Multiplayer sheet: Host, the list, Join, firewall rule |
| `include/spidy/mp_session.hpp`, `src/mp_session.cpp` | the network thread: loopback echo, buddy link, ping |
| `include/spidy/mp_ghost.hpp`, `src/mp_ghost.cpp` | placement, stick figure, nameplate (pure) |
| `include/spidy/mp_body.hpp`, `src/mp_body.cpp` | 0.0.73 MPSUIT PLAYER 2 BODY: the shaded hero body for a suit with no mod model, and its LOOK (pure) |
| `src/game_xr.cpp` | wiring: publish the pose, draw the remote player (after the suit, before the mirror) |
| `tools/mp_test_buddy.py` | the Test Buddy (stdlib only), `--selftest` matches `spidy_mp_probe golden` byte for byte |
| `tests/mp_tests.cpp`, `tests/mp_buddy_tests.py`, `tests/hero074_mp_lobby_tests.py` | round trip, versioning, malformed packets, interpolation, loopback delay, buddy handshake, live buddy |

## CONFIRMED / CANDIDATE

| Item | Status |
|---|---|
| Pose source: the mod's own tracked head / grips / anchor / turn (`rendered.*`, `travel`) | CONFIRMED (mod-internal) |
| Hero (`heroNow()`), worn suit (`wornSuitSlot`), action flags (`flyNow`, `swingState`, `capeNow`, `palmGlow`, `beamFade`) | CONFIRMED (mod-internal) |
| Remote body drawn from `suit_wear` / `mesh_model` only | CONFIRMED (mod-internal) |
| Loopback + Test Buddy on localhost | CONFIRMED (Linux tests and CI; in-headset: to be confirmed by the tester) |
| Buddy on another PC over LAN (opt-in: empty file `%APPDATA%\Spidy\MP-LAN-ON.txt`, UDP 39470 through Windows Firewall) | CANDIDATE |
| Remote player's placement in the world (drawn relative to you, ~3 m ahead; not at its real world position) | CANDIDATE design for the solo test |
| LAN lobby beacon / list / MP-LOBBY.txt (no game data, mod-internal) | CONFIRMED by construction (Linux tests); across two real Windows PCs and Windows Firewall: CANDIDATE, to be confirmed by the tester |
| Game's own suit id at `+0x8c` | NOT USED |
| Any game body / actor address for a remote player | NOT USED (none invented) |
| Steam P2P / NAT traversal / more than 2 players | FUTURE |

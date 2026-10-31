# HERO VR 0.0.74 (EXPERIMENTAL release, tag v0.0.74, launcher build 15)

Released as tag v0.0.74 (Version 0.0.74, build Update 39.7, launcher 0.0.74.1, build 15), from test build 10's code
(1d50e07) on the branch hero-0.0.74-release.
EXPERIMENTAL: it may crash or have issues; 0.0.63 is the most recent STABLE build (version.json `stable_version`).
Revert offers 0.0.73 first, then 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
The release zip carries `Models/` (with the Batmobiles, `Models/Vehicle/Batmobile`, and the hideouts,
`Models/Hideout`), `Sounds/` and `Music/`.
Made by Agent Mulder. Only the model artists are credited (THIRD-PARTY-MODELS.md).

## What the release changes from test build 10
- **No test labels**: HERO_TEST_BUILD OFF, so no test notice; the wrist reads "HERO VR 0.0.74 - MADE BY AGENT MULDER"
  and the console "HERO VR 0.0.74 - Made by Agent Mulder: starting OpenXR". CAR PAINT's third choice reads CLASSIC
  (it was TEST 1; live.txt `car_paint = test_1` still works) and the mirror's auto-off note no longer names a test build.
- **Multiplayer stays in** (the user's choice): MP SOLO TEST (OFF by default), LAN hosting and joining, and the internet
  server list (InternetLobby.DefaultUrl, HERO VR's free Cloudflare lobby). The client holds no server credentials.
- SM1STYLE (eye AA) and the WINDOW EDGES lens cover stay on by default, as in test build 10.

## Known issues for 0.0.75
- Aliasing is better but still needs work (SM1STYLE).
- Window edges are much less noticeable but not fully fixed.

What's new for players: RELEASE-NOTES-v0.0.74.md and CHANGELOG.md.

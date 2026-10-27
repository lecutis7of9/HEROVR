# HERO VR 0.0.72 (PRIVATE TEST BUILD, branch hero-0.0.72)

Not a release: no tag, no GitHub release, no Nexus upload. CI runs with `upload=false`. The launcher's
VERSION stays 0.0.70. Install it over the player's 0.0.70 folder (tools/release072/READ-ME-FIRST.txt).
Made by Agent Mulder. Only the model artists are credited (THIRD-PARTY-MODELS.md).

Game addresses and hooks are marked CONFIRMED (the 0.0.7 paths: sm2_damage, the bridge raycasts, the swing
asks) or CANDIDATE (not yet confirmed in the game). A CANDIDATE feature sits behind a toggle that turns
itself off on failure. A missing model shows MODEL MISSING and is never replaced by a stand-in.

## Preview notes

- **REAL MODELS:** the artists' real geometry, UVs and textures go into a texture array, and a lite copy is
  kept for the untextured fallback. Gate: `tools/models072/model_check.py`. It renders every model side by side
  with its source and fails below 30 % of the source's triangles, or on a missing texture or UV set.
- **TEST BUILD NOTICE:** the user's exact text shows once a session, after the calibration closes. It is
  dismissed with a poke or an aimed trigger and never takes game input (option HERO_TEST_BUILD).
- **THE PLAYER'S SOUNDS:** these play as sent, with sha256 pinned in tools/release072/required_assets.py:
  `user-shotgun.mp3`, `user-pistol.mp3`, `user-pistol-reload.mp3`, `user-lightning-1.mp3` and
  `user-lightning-2.mp3`. Thor's two lightnings alternate at random, and one of them is the strike. The
  playback count follows the shots.
- **GRAPPLE (BATMAN, DAREDEVIL, PUNISHER):**
  - takeoff works from the ground, with a sustained pull and a line drawn for every hero;
  - the gun points along the aim;
  - simulated frames are in the test folder's `grapple-frames/` (simulations, not game captures).
  - FLAG: BATMAN's grapple fires on the trigger, which breaks the rule that triggers fire only basic
    projectiles. It is left as in 0.0.7, pending the player's call.
- **THOR'S HAMMER:** `Models\Hammer` shows MODEL MISSING until the player picks a model; the candidate
  sheets are in `hammer-candidates/`.
- **PUNISHER VAN (EXPERIMENTAL, OFF):**
  - the logic is in `van072.hpp`: a safe street spawn, the driver door handle in and out, a grip-latched
    wheel, RT/LT/both-brake, a wall stop, and NaN safety;
  - the summon is a tap of the left **X**. X is free for the Punisher (he has no gadget wheel) and is held
    back from the game only while the setting is on and the hero is the Punisher;
  - `Models\Vehicle\PunisherVan` is MODEL MISSING until the pick (`van-candidates/`), so nothing spawns yet;
  - the body carry (moving the player with the van) is NOT WIRED. The bridge has no velocity input, so that
    command is CANDIDATE, and the ground and wall probes are not yet joined to the async ray batch.
- **JOKER GANG (Arkham mix):** the sources are noted in `tools/factions/factions.py` (`JOKER_SOURCES`), and
  the gang stays vanilla until a model ships:
  - gtav_KWABZ's pack downloads from gta5-mods without a login, but it is in GTA V `.ydd`/`.ytd` format,
    which needs CodeWalker on the PC;
  - XNALaraFanatic's Arkham City sets 1 and 2 need a DeviantArt login (not fetched);
  - Kabalstein's Harley thugs are a MediaFire `.rar` in XNALara format.
  - All of them are ports of the game's own Arkham meshes: fine for a private test, not for a public build.
- **SETTINGS SUBTEXT:** every COMBAT setting's note is at most 80 characters, has exactly one '.', and says
  whether a restart is needed (tests/update38_tests.py).
- **MSVC fix:** `mesh_model.cpp` no longer names a variable `small`, which is a macro in `rpcndr.h`.

## Coming soon / pending

- PERCH TAKEDOWN: its hooks are CANDIDATE.
- Alternate suits that are not fetched yet are labelled COMING SOON.
- The enemy faction swap (THE HAND, HYDRA, JOKER GANG) is CANDIDATE.
- The hammer and van models are awaiting the pick.
- Multiplayer is skipped.

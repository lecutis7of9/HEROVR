# HERO VR 0.0.72 (EXPERIMENTAL release, tag v0.0.72, launcher build 13)

Released as tag v0.0.72 (Version 0.0.72, build Update 39.5, launcher 0.0.72.1, build 13), from test build 12's code.
EXPERIMENTAL: it may crash or have issues; 0.0.63 is the most recent STABLE build (Revert > USE 0.0.63).
The release zip carries `Models/` (every model and suit of test build 12) and `Sounds/`.
Made by Agent Mulder. Only the model artists are credited (THIRD-PARTY-MODELS.md).
The notes below were written during the test builds (1-12).

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
  - BATMAN's grapple stays on the trigger: the player's call (a gun that fires a line counts as a projectile).
- **THOR HAMMER (setting, COMBAT):** four real Mjolnir models (CC BY 4.0, in the user's order): #1 BOOOOP
  (Boooooop, the default), #5 DEVILSEYE (TheDevilsEye), #6 LOVE THUNDER (Jesus Osco) and #3 FCCOOPER
  (fccooper), in `Models\Hammer\<name>`.
  - It starts SHEATHED on the back (right shoulder); reach back and grip to draw it. No magic spawn.
  - The throw, recall and lightning are unchanged; the two user lightning sounds alternate at random.
  - A missing pick shows MODEL MISSING (no stand-in). Real geometry, UVs and textures (model-check PASS).
- **PUNISHER VAN (EXPERIMENTAL, OFF):**
  - the logic is in `van072.hpp`: a safe street spawn, the driver door handle in and out, a grip-latched
    wheel, RT/LT/both-brake, a wall stop, and NaN safety;
  - the summon is a tap of the left **X**. X is free for the Punisher (he has no gadget wheel) and is held
    back from the game only while the setting is on and the hero is the Punisher;
  - the model is the GMC Vandura 1976 by snafuj (CC BY 4.0; the owner's pick #8), painted black, 135k of its
    431k triangles with its textures, the glass left out. Its cab is an empty shell in the download, so the
    cab (seats, dash, gauges, door card) and a separate steering wheel are BUILT (840 + 1848 triangles,
    one generated texture). The wheel turns with the hands on its rim;
  - SUMMON: the X tap asks the async ray batch, one world ray a frame (8 per spot, up to 7 spots: four
    down for the ground, three across the van's box, one from the chest), about 0.1-0.6 s (CANDIDATE);
  - ENGINE PREVIEW: getting in brings the DRAWN van round the player (the seat at the eyes) and getting
    out puts it back. RT/LT/both work on the engine and the triggers are held back from combat and the
    game while seated, but the body carry (moving the player with the van) is NOT WIRED (the bridge has
    no velocity input: CANDIDATE), so the van does not drive through the city yet.
- **JOKER GANG (Arkham mix):** the sources are noted in `tools/factions/factions.py` (`JOKER_SOURCES`), and
  the gang stays vanilla until a model ships:
  - gtav_KWABZ's pack downloads from gta5-mods without a login, but it is in GTA V `.ydd`/`.ytd` format,
    which needs CodeWalker on the PC;
  - XNALaraFanatic's Arkham City sets 1 and 2 need a DeviantArt login (not fetched);
  - Kabalstein's Harley thugs are a MediaFire `.rar` in XNALara format.
  - All of them are ports of the game's own Arkham meshes, so none of them ships.
- **SETTINGS SUBTEXT:** every COMBAT setting's note is at most 80 characters, has exactly one '.', and says
  whether a restart is needed (tests/update38_tests.py).
- **MSVC fix:** `mesh_model.cpp` no longer names a variable `small`, which is a macro in `rpcndr.h`.

- **0.0.71 VIDEOS FIX ROUND** (simulated previews in the test folder's `previews-0.0.72b/`, drawn by
  `spidy_hero072b_preview`; simulations, not game captures):
  - BATMAN: the line flies out of the muzzle to the hook before the pull; the grapple gun hangs on the
    belt (0.60 m under the eyes, was 0.78 m, by the foot) and is held with the barrel along the aim; the
    batarang is pinched in the fingers, square to the palm. The game body (Iron Spider / Spider-Man) is
    hidden under BATMAN, which also ends the arm stretch and the vanishing hand looking down. PARTIAL: the
    game's hands are hidden with it, and there is no BATMAN body model yet.
  - SUPERMAN: two thin beams (under 1 cm) from the eyes, re-aimed from the rendered head each frame (no
    lag), hit sparks, a scorch that fades, and one seamless 1 s loop (heat_vision.wav) held while firing.
  - PUNISHER: drop-leg thigh holsters, the barrel along the aim, a muzzle flash, hit puffs, a stagger
    (pistol 140, buckshot 160 plus a 380 shove); the ammo readout stays. The reaction kind is CONFIRMED
    (sm2_damage); how big it looks in game is CANDIDATE.
  - CAPTAIN AMERICA: a thrown shield always flies home (it turns back after 4 s at the latest), slows for
    the catch and eases onto the arm. The "huge blue reticle" was the right hand's target marker, sized by
    the enemy; under CAP it is now 0.15 m and half opacity, with no wide lock-on ring. The slam ring is
    subtle too.
  - WOLVERINE: a preview clip of the claws.
  - VAN: the real model with its built cab and wheel (`previews-0.0.72c/`, SIMULATED). The velocity
    carry is CANDIDATE, so the world does not move and nothing can crash.

## Coming soon / pending

- PERCH TAKEDOWN: its hooks are CANDIDATE.
- Alternate suits that are not fetched yet are labelled COMING SOON.
- The enemy faction swap (THE HAND, HYDRA, JOKER GANG) is CANDIDATE.
- PUNISHER VAN driving: the body carry is CANDIDATE (engine preview only). VRAM: about 137k van triangles.
- Multiplayer is skipped.

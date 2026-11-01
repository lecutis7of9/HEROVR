# HERO VR 0.0.76 SM1: hero suits in Spider-Man 1 (Install / Remove / Game info)

Made by Agent Mulder. Spider-Man 1 is iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
https://github.com/IlyaMez/SpidyVR); none of his files are read or written by this. The install mechanism is the one
Overstrike suit mods use, and `tools/sm1_suits/vendor/dat1lib` is ALERT's (both by Tkachov, GPL-3.0).
Target: Marvel's Spider-Man Remastered v4.630.0.0 (Steam 1817070).

**Status: built and tested on a synthetic game folder only. Not yet seen in the real game (CANDIDATE).**

## What the launcher does (Spider-Man 1 page, "Hero suits in Spider-Man 1")

| Button | What it does | When it takes effect |
| --- | --- | --- |
| Install suits | Runs `python\python.exe tools\sm1_suits\sm1suits.py install`: reads the game's classic suit (its vanity config, model, body material and textures) as a template, fits each of the ten heroes in `Models\SM1Suits\*.hsm1` to that suit's skeleton, writes one archive `asset_archive\HeroVR\herovr_suits`, and adds the new suits to the toc, the tech-web (suit list), the loadout list, the vanity master and launch lists and the localization (all as new copies in our archive; the game's own archives are not written). The untouched toc is first copied to `asset_archive\toc.herovr-sm1.bak` (sha256 checked). A second Install removes the first one before writing. | Next game start |
| Remove suits | `sm1suits.py revert`: puts the saved toc back byte for byte (sha256 checked) and deletes `asset_archive\HeroVR`. If the toc was replaced since (a game update or Steam's Verify), only our folder is deleted and the game's toc is left alone; if that new toc still points at our archive (another mod tool), nothing is touched and Steam's Verify is the fix. | Next game start |
| Game info | `sm1suits.py dump`: reads only; writes `%APPDATA%\HERO VR SM1\sm1-dump\` (see below). | Changes nothing |

All three are refused while Spider-Man.exe runs. The log is `%APPDATA%\HERO VR SM1\sm1suits.log`.

Fail safe: every hero is built on its own; one that fails is logged and skipped while the rest install. If writing
fails part way, the tool logs "install failed part way" and runs Remove itself. A toc that does not match what our
install wrote (someone else changed it) is never overwritten.

In the game: open the Suits menu and pick the hero (Iron Man, Daredevil, Hulk, Batman, Wolverine, Superman, Punisher,
Captain America, Thor, Green Lantern). The wrist menu's hero pick does **not** switch the model yet: there is no
CONFIRMED address for equipping a suit, and none is guessed.

## CONFIRMED / CANDIDATE

| Part | Status | Source |
| --- | --- | --- |
| toc format (DAT1 sections, archives, spans), asset id = crc64 of the normalized path | CONFIRMED | ALERT / Overstrike source |
| the classic suit's vanity config path, the tech-web / loadout / vanity / localization asset ids | CONFIRMED | the ids Overstrike's MSMR installer edits |
| adding an archive and assets to the toc the way Overstrike does | CONFIRMED mechanism, CANDIDATE for our files | Overstrike |
| model rebuild: our vertices and weights in the classic suit's model (grown buffers, uv scale, appended material slots) | CANDIDATE | ALERT's model reader; untested in the game |
| retarget / fitter (our skeleton to the suit's joints, mirror and up-axis checks) | CANDIDATE | until a dump from the real game is read |
| BC1 textures in an SD-only texture asset, flat normal map | CANDIDATE | |
| packs with more texture groups than the model's mesh slots: small groups folded into a big texture | CANDIDATE look | |
| heads: kept as in the classic suit (as ALERT suits do) | CANDIDATE | |

## What one owner run of "Game info" provides

`%APPDATA%\HERO VR SM1\sm1-dump\herovr_sm1_dump.json`, plus raw `.bin` copies of what it read:

- the toc's sha256 and size, the archive list, the asset count, the spans, and whether our suits are installed;
- for the classic suit's vanity config, the tech-web (progression), loadout list, vanity master and launch configs
  and the localization: every copy (span, size, sha256, magic), saved as `<name>_span<N>.bin`;
- the suit names in the tech web;
- the classic suit's model: name, size, its mesh / joint / material info and the fitter's report (frame, axes, mirror);
- the body material: its path, its textures and the diffuse and normal texture headers (`*_header.bin`);
- a dry run of all ten heroes (each hero's report and every asset it would write, with sizes), nothing written;
- the tool's log.

That is enough to turn the CANDIDATE rows above (model layout, fitter frame, texture headers) into CONFIRMED or fix
them without guessing. It takes about two minutes and changes nothing in the game folder.

## Files

- `tools/sm1_suits/sm1suits.py` - the command line (dump, install, revert, status, selftest); last line `RESULT {json}`.
- `tools/sm1_suits/{assets,inject,retarget,bcn,toc_ops,hsm1,heroes}.py` - the parts; `vendor/` - ALERT's dat1lib.
- `tools/sm1_suits/prepack.py` - build box only: `Models/Suits` -> `Models/SM1Suits/*.hsm1` + `license.txt`.
- `tools/launcher/src/Sm1Suits.cs`, `MainForm.Sm1.cs` - the buttons.
- `tests/hero076_sm1_suits_tests.py` - round trips on a synthetic game with thinned packs (`tests/data/sm1suits`).

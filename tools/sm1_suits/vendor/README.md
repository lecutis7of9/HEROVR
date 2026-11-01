# Vendored: dat1lib (from ALERT)

`dat1lib/` is copied from **ALERT** by **Tkachov** (https://github.com/Tkachov/ALERT, commit `bc90ed9b`),
licensed **GPL-3.0** (`LICENSE-ALERT.txt`). It reads and writes Spider-Man Remastered's toc, configs, models,
materials and textures. The toc/archive install follows the way **Overstrike** (also by Tkachov, GPL-3.0,
https://github.com/Tkachov/Overstrike) installs suit mods; no Overstrike code is copied here.

HERO VR changes to the copy (GPL-3.0 requires us to say so):

- `dat1lib/types/sections/__init__.py`: `TypedEmptyList` - an empty serialized array remembers its element type,
  so a config that holds an empty array (the game's own configs do) saves again instead of failing.
  `type(v) == list` checks became `isinstance(v, list)`.

Nothing else in the copy is changed. HERO VR builds on iilyn's SpidyVR (https://github.com/IlyaMez/SpidyVR, GPL-3.0).

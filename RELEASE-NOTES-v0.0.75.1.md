# HERO VR 0.0.75 hotfix 1 (v0.0.75.1)

**EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.75,
USE 0.0.74 or USE 0.0.63 goes back, settings kept).

Version 0.0.75 hotfix 1 (build Update 39.8.1), launcher build 17 (0.0.75.2). It still shows as **Version 0.0.75**: the
VERSION file stays 0.0.75 (a four-part 0.0.75.1 would read as older than 0.0.75 in every existing launcher), and the
newer launcher build is what offers it to 0.0.75 players. HERO_TEST_BUILD OFF.

- **Fixed: the freeze right after loading a save in 0.0.75** (the see-through depth lock). The see-through depth
  barrier no longer holds its lock while waiting for the command list lock, so the game no longer hangs on load.
- Everything else is the same as 0.0.75.
- **Spider-Man 1:** the heroes and the wrist menu are not working in Spider-Man 1 yet; that comes in the next hotfix.
- Launcher: Revert offers 0.0.75 first, then 0.0.74, 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

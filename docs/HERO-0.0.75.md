# HERO VR 0.0.75 (EXPERIMENTAL release, tag v0.0.75, launcher build 16)

Released as tag v0.0.75 (Version 0.0.75, build Update 39.8, launcher 0.0.75.1, build 16), from the hero-0.0.75 work
(a823158) on the branch hero-0.0.75-release.
EXPERIMENTAL: it may crash or have issues; 0.0.63 is the most recent STABLE build (version.json `stable_version`).
Revert offers 0.0.74 first, then 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
The release zip carries `Models/` (now with the motorcycle, `Models/Vehicle/Motorcycle` and its `Handlebars`; the
files are not in git, tools/release072/required_assets.py RELEASE075_MODEL_FILES lists them), `Sounds/` and `Music/`.
Made by Agent Mulder. The model artists are credited in THIRD-PARTY-MODELS.md.

## Release build
- HERO_TEST_BUILD OFF: no test notice; the wrist reads "HERO VR 0.0.75 - MADE BY AGENT MULDER".
- Mulder's Heroes add-on for Spider-Man 1 (herovr_sm1_addon.dll) is optional: when it is not in the mod files,
  Spider-Man 1 starts iilyn's mod alone (tools/package.ps1 warns, the launcher says so).

## Known issues
- Aliasing and window edges are still being improved.
- Damage to the green symbiote enemies is not confirmed yet.
- The '89 Batmobile can look grey.
- The motorcycle and the Spider-Man 1 option are untested in a headset.
- No passenger seats.

What's new for players: RELEASE-NOTES-v0.0.75.md and CHANGELOG.md.

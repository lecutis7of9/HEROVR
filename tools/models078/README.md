# HERO VR 0.0.78 PUNISHER ARSENAL models

The game loads the four guns from `Models\Punisher078\` next to the mod (mesh_model.hpp `slotArsenal078First`):

| Folder | What |
|---|---|
| `Pump`, `Pump/Forend`, `Pump/Shell` | pump shotgun body, the sliding forend, one loose shell |
| `Pistol`, `Pistol/Slide`, `Pistol/Mag` | suppressed pistol body (can included), the slide, the magazine |
| `Sniper`, `Sniper/Bolt`, `Sniper/Mag` | bolt rifle body with its scope, the bolt handle, the magazine |
| `Rifle`, `Rifle/Handle`, `Rifle/Mag` | carbine body with its red dot, the charging handle, the magazine |

Frame (every file): metres, real scale; the grip point at the origin; -z along the barrel, +y the top, +x right.
A magazine or shell has its top (the end that goes in) at the origin. The moving part (forend, slide, bolt,
handle) is modelled in its forward (home) place; the game moves it back along the barrel by the rack's travel.
Where the parts must sit is `punisher078::pointsOf()` (muzzle, port, magazine well, forend, rack, lens).

No lens glass in the models: the eye pass draws the scope picture / red dot in the lens circle
(`punisher078::opticConstants`, native_eye_gpu `optic()`); leave the lens opening empty or a separate,
un-exported object.

Caps (mesh_model): at most 60000 triangles per gun and 20000 per part; never decimated by the game. PBR
(base colour, normal, metallic-roughness, occlusion), 2K-4K textures. A missing file is MODEL MISSING in
the game; there is never a stand-in gun.

No holster model: the holstered pistol is drawn in the PUNISHER model's own right-leg holster
(`punisher078::legHolsterPose`; previews/punisher078_right_leg_holster_lit.png).

## Credits

No gun model ships in this branch yet. Every model added here must be CC0 or CC-BY and gets a line below:
name, author, source URL, license, and what was changed (scale, origin, split into parts).

## Candidates (not downloaded: Sketchfab downloads need the owner's signed-in account)

All Sketchfab, real-scale photoreal PBR, inside the caps. License as listed on the model page at search time
(CANDIDATE until the downloaded file's license is read).

| Gun | Model | Author | License | Triangles | Sketchfab uid |
|---|---|---|---|---|---|
| Pump | Shotgun (Remington 870 style) | see page | CC BY | 41,510 | 2ed20e23... (search "shotgun 870") |
| Pistol | BERETTA M9 W/ SUPRESSOR | drcrazzie | CC BY | 22,919 | 93cda4e59523467ea20559ec4ca5b5cd |
| Pistol (alt) | Colt 1911 (Silencer + Flashlight) | MKoegler3D | CC BY | 18,142 | de3da384c0d541e6ae7869667c451ad5 |
| Pistol (alt, no can) | Beretta 92G Brigadier Elite II | eNse7en | CC BY | 36,004 | 474cb73931114a669780783c44b5bb88 |
| Sniper | Special Sniper Rifle VSS Vintorez | Arms Museum | CC0 | 35,960 | 4d5d8c1b7b79429abfa4816f18330089 |
| Rifle | AR-15 MOD | Rendy K | CC BY | 26,794 | e9629073d65b48979542dafd43275b63 |
| Rifle optic | Eotech 553 | yarogor | CC BY | 12,456 | 8d1191779e1c4a66a98e122f7551c305 |
| Rifle optic (alt) | Crimson Trace CTS-1550 Red Dot Sight | Orshia 3D | CC BY | 6,416 | a29f7326e13c454a9de1a027c74dd8ae |

The VSS is CC0 and in budget but is an integrally suppressed marksman rifle with a PSO-style scope, not a
classic bolt rifle; the Sniper's bolt part is cut from its charging handle if it is used.

## Previews

`tools/models078/render_holster_078.py <model.glb> previews` renders the PUNISHER model lit (key, fill, rim;
Cycles) from the front and close on the right-leg holster.

`previews/gun_blood_*_offline.png`: hero007::appendImpact's own overlay triangles (a shot along -z hitting at
4 m, ages .04-2 s), dumped by a small program linked with spidy_core and rasterized unlit offline; the grey box
is a stand-in for the enemy's size only. Not an in-game capture.

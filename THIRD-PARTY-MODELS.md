# Third-party 3D models (HERO VR 0.0.72)

HERO VR's on-screen credit is "Made by Agent Mulder". The models below belong to their authors:
Digital Goblin, iqbalmulyawan, Yanez Designs (Anthony Yanez), TORI106, LiliumLetifer, Eira, Ludus101,
baton268 and Falxx (0.0.7); Boooooop, TheDevilsEye, Jesus Osco, fccooper and snafuj (0.0.72); the 23 worn
suits' artists and the ROSTER pictures' artists listed below. They are credited here, in CREDITS.md, on the launcher's Credits page and in the release notes,
as their licences require. HERO VR is free: it is never sold, paywalled or ad-supported.

The release zip's `Models/` folder carries them (and `CapSuit/` the prepared suit for the optional,
manual suit swap in docs/CAP-SUIT-PIPELINE.md). The folder is made by `tools/models007/convert_models.py` from the Sketchfab downloads, which
are kept read-only. Every converted model is in metres, x right, y up, -z forward, with the grip at
the origin. Its colours are baked per triangle from the model's own base-colour texture, because the
overlay cannot sample textures. No texture ships for the converted models.

| Slot (`Models/...`) | Model | Author | Licence | Changes |
|---|---|---|---|---|
| `Batarang/EIRA` | [Batarang](https://sketchfab.com/3d-models/batarang-fe47172e6e924e58860c39e101ec0cda) (Sketchfab ID fe47172e6e924e58860c39e101ec0cda) | [Eira](https://sketchfab.com/eirav) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 13570 to 700 triangles, colours baked and textures dropped (4 × 2048²), rescaled and re-centred. |
| `Batarang/CLOONEY` | [Batarang Clooney](https://sketchfab.com/3d-models/batarang-clooney-8770284b5e9240bd8c4f16aa02a90f21) (ID 8770284b5e9240bd8c4f16aa02a90f21) | [Ludus101](https://sketchfab.com/Ludus101) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 1428 to 700 triangles. The download has no textures and a black material named "Enamel Glossy Dark Grey", so it is drawn dark grey. |
| `Batarang/JOKER` | [Batarang Joker](https://sketchfab.com/3d-models/batarang-joker-498fe473081942a9a10b1026ef01c57b) (ID 498fe473081942a9a10b1026ef01c57b) | [baton268](https://sketchfab.com/baton2) | [CC BY-NC-ND 4.0](http://creativecommons.org/licenses/by-nc-nd/4.0/) | **None.** The files are copied byte for byte (scene.gltf, scene.bin and its 3 × 4096² textures). The mod places and scales it when drawing and never writes it back. |
| `Pistol` (both of PUNISHER's pistols) | [9mm Pistol](https://sketchfab.com/3d-models/9mm-pistol-43bc09f5aace4346a8a2b6e1580fc03f) (ID 43bc09f5aace4346a8a2b6e1580fc03f) | [TORI106](https://sketchfab.com/TORI106) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 4707 to 699 triangles, colours baked and textures dropped (6 × 2048²), scaled to 19 cm. |
| `SawedOff/Frame`, `SawedOff/Barrels` | [Sawed-off Shotgun](https://sketchfab.com/3d-models/sawed-off-shotgun-6e8892e053d247eab1cad53ba456397e) (ID 6e8892e053d247eab1cad53ba456397e) | [LiliumLetifer](https://sketchfab.com/LiliumLetifer) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Split at the hinge into a frame and barrels (with the hinge as a pivot) so it breaks open. Decimated from 38340 to 700 triangles (420 + 280), colours baked and textures dropped (9 × 2048²), scaled to 58 cm. |
| `GrappleGun` (BATMAN's grapple gun) | [Batman's Grapple Gun](https://sketchfab.com/3d-models/batmans-grapple-gun-d961bc05652f41b1b90f9358f69eb23a) (ID d961bc05652f41b1b90f9358f69eb23a) | [Falxx](https://sketchfab.com/Falxx) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 10272 to 699 triangles, colours baked and textures dropped, scaled to about 22 cm, grip at the origin and the muzzle stored as its pivot. |
| `Baton/Left`, `Baton/Right` | [Daredevil's baton](https://sketchfab.com/3d-models/daredevils-baton-e6addd57e2004ecaa44d3114fa90d78a) (ID e6addd57e2004ecaa44d3114fa90d78a) | [Yanez Designs (Anthony Yanez)](https://sketchfab.com/paulyanez) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | **Rope removed, split into two batons.** The joining cord (624 triangles) is deleted, and the two batons are separated into LEFT and RIGHT, each with its grip at the origin as its pivot. Decimated (2486 → 399 and 2546 → 400), colours baked and textures dropped, 57 cm each. |
| `Shield` (CAPTAIN AMERICA, SHIELD LOOK MODEL) | [Captain America Shield](https://sketchfab.com/3d-models/captain-america-shield-cbcf4fa7d4464b65a5bf051340f626ab) (ID cbcf4fa7d4464b65a5bf051340f626ab) | [Digital Goblin](https://sketchfab.com/DigitalGoblin) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | **Disc remeshed, straps decimated.** The disc is rebuilt from the source's own heights and its texture's ring edges and star (530 triangles) so the one-colour-a-triangle overlay keeps the rings sharp; the straps, buckles and pads are decimated (21388 → 168). 22028 → 698 triangles, colours baked (paint wear kept, visual only), textures dropped, 0.61 m across, the hand strap at the origin and the elbow strap as the pivot. |
| `CapSuit/` (the PC suit swap, not loaded by the VR mod) | [Captain America](https://sketchfab.com/3d-models/captain-america-d4898ea3aac3474fa0661de3ade9b7c6) (ID d4898ea3aac3474fa0661de3ade9b7c6) | [iqbalmulyawan](https://sketchfab.com/Iqbal.Mulyawan.Putera) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Re-oriented (Y-up, facing +Z), rescaled to a 1.80 m start, welded, its material slots merged into two (`cap_suit_body`, `cap_suit_gear`), 17186 triangles kept, the emissive copy dropped (`tools/cap_suit/prep_suit.py`). It has no rig: it is fitted and weighted to the game's rig on the player's PC (docs/CAP-SUIT-PIPELINE.md). |

## Credit lines (as the authors ask)

- This work is based on "Batarang" (https://sketchfab.com/3d-models/batarang-fe47172e6e924e58860c39e101ec0cda) by Eira (https://sketchfab.com/eirav) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated and recoloured for VR.
- This work is based on "Batarang Clooney" (https://sketchfab.com/3d-models/batarang-clooney-8770284b5e9240bd8c4f16aa02a90f21) by Ludus101 (https://sketchfab.com/Ludus101) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated for VR.
- This work is based on "Batarang Joker" (https://sketchfab.com/3d-models/batarang-joker-498fe473081942a9a10b1026ef01c57b) by baton268 (https://sketchfab.com/baton2) licensed under CC-BY-NC-ND-4.0 (http://creativecommons.org/licenses/by-nc-nd/4.0/). Unmodified.
- This work is based on "9mm Pistol" (https://sketchfab.com/3d-models/9mm-pistol-43bc09f5aace4346a8a2b6e1580fc03f) by TORI106 (https://sketchfab.com/TORI106) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated and recoloured for VR.
- This work is based on "Sawed-off Shotgun" (https://sketchfab.com/3d-models/sawed-off-shotgun-6e8892e053d247eab1cad53ba456397e) by LiliumLetifer (https://sketchfab.com/LiliumLetifer) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Split at the hinge, decimated and recoloured for VR.
- This work is based on "Batman's Grapple Gun" (https://sketchfab.com/3d-models/batmans-grapple-gun-d961bc05652f41b1b90f9358f69eb23a) by Falxx (https://sketchfab.com/Falxx) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated and recoloured for VR.
- This work is based on "Daredevil's baton" (https://sketchfab.com/3d-models/daredevils-baton-e6addd57e2004ecaa44d3114fa90d78a) by Yanez Designs (Anthony Yanez) (https://sketchfab.com/paulyanez) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Rope removed, split into two batons, decimated and recoloured for VR.
- This work is based on "Captain America Shield" (https://sketchfab.com/3d-models/captain-america-shield-cbcf4fa7d4464b65a5bf051340f626ab) by Digital Goblin (https://sketchfab.com/DigitalGoblin) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Disc remeshed, straps decimated and recoloured for VR.
- This work is based on "Captain America" (https://sketchfab.com/3d-models/captain-america-d4898ea3aac3474fa0661de3ade9b7c6) by iqbalmulyawan (https://sketchfab.com/Iqbal.Mulyawan.Putera) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Re-oriented, rescaled and welded; rigged on the player's PC.
- The Batcave hideout (Models/Hideout/Batcave) is the Batcave stage from Injustice: Gods Among Us (mobile) by NetherRealm Studios / Warner Bros. Games, ripped by Ecelon and shared on The Models Resource (https://models.spriters-resource.com/mobile/injusticegodsamongus/asset/302872/). Rescaled and re-exported for VR.

## Version 0.0.72: the shipped models are the real ones

0.0.72 replaces the 0.0.7 conversions above (decimated to ~700 triangles, colours baked, textures dropped) with
the artists' real geometry, UVs, normals and base-colour textures (`tools/models072/convert_models_072.py`).
The side-by-side gate (`tools/models072/model_check.py`) fails a
model under 30 % of its source's triangles or with a texture or UV missing. Same authors and licences as above.

| Slot | Triangles shipped / source | Textures | Changes in 0.0.72 |
|---|---|---|---|
| `Batarang/EIRA` | 6998 / 13570 | 1 | Decimated to 52 % for VR; texture kept. |
| `Batarang/CLOONEY` | 1428 / 1428 | none in the download | Unchanged geometry (the source is untextured). |
| `Batarang/JOKER` | 396 / 396 | 3 | None: copied byte for byte (CC BY-NC-ND). |
| `Pistol` | 4707 / 4707 | 2 | Full geometry and textures; scaled to 19 cm. |
| `SawedOff/Frame`, `SawedOff/Barrels` | 9000 / 14180, 12000 / 24160 | 2 | Split at the hinge; decimated to 63 % and 50 %. |
| `Baton/Left`, `Baton/Right` | 2486 / 2486, 2546 / 2546 | 3 | Cord removed, split in two; full geometry. |
| `Shield` | 22028 / 22028 | 1 | Shipped whole with its texture layer. |
| `GrappleGun` | 10272 / 10272 | 1 | Full geometry and texture. |

| `Hammer/BOOOOP` (THOR HAMMER #1, default) | 50000 / 76028 | 5 | Quality-decimated to 66 %, textures atlased; handle end at the grip, 50 cm. |
| `Hammer/DEVILSEYE` (THOR HAMMER #5) | 1550 / 1550 | 1 | Full geometry and texture; grip at the origin, 50 cm. |
| `Hammer/LOVEANDTHUNDER` (THOR HAMMER #6) | 50000 / 161728 | 3 | Quality-decimated to 31 %, textures kept; grip at the origin, 50 cm. |
| `Hammer/FCCOOPER` (THOR HAMMER #3) | 21236 / 21236 | 3 | Full geometry and textures; grip at the origin, 50 cm. |
| `Vehicle/PunisherVan` (PUNISHER VAN, EXPERIMENTAL) | 135000 / 431452 | 4 | Quality-decimated to 31 %, painted black (the body texture's paint recoloured; chrome, rubber, lights kept), the window glass (3200 triangles) left out, real size 1.97 × 1.94 × 4.83 m. |
| `Vehicle/PunisherVan/Interior`, `/SteeringWheel` | 840, 1848 (BUILT) | 1 (generated) | Not from the source (its cab is an empty shell): built by Agent Mulder for HERO VR; the wheel is its own part so it turns. |

## Version 0.0.72: THOR HAMMER and the PUNISHER VAN

| Slot | Model | Author | Licence |
|---|---|---|---|
| `Hammer/BOOOOP` | [Marvel Thor's Hammer (Mjölnir)](https://sketchfab.com/3d-models/marvel-thors-hammer-mjolnir-847befafabc04998a93e4326c2eb5d94) | [Boooooop](https://sketchfab.com/boooop) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) |
| `Hammer/DEVILSEYE` | [Mjolnir (Thor's hammer)](https://sketchfab.com/3d-models/mjolnir-thors-hammer-ecca1232710f4721b14d64a90bba6557) | [TheDevilsEye](https://sketchfab.com/TheDevilsEye) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) |
| `Hammer/LOVEANDTHUNDER` | [Mjolnir - Thor love and thunder](https://sketchfab.com/3d-models/mjolnir-thor-love-and-thunder-9689a953a9724987bb962bc00104c5fd) | [Jesus Osco](https://sketchfab.com/gsus.vfx) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) |
| `Hammer/FCCOOPER` | [Thor's Hammer: Mjolnir](https://sketchfab.com/3d-models/thors-hammer-mjolnir-1929e65c877a48bba3b16cfa51f7a33c) | [fccooper](https://sketchfab.com/fccooper) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) |
| `Vehicle/PunisherVan` | [GMC Vandura1976 Van](https://sketchfab.com/3d-models/gmc-vandura1976-van-40cf97bc0f7d4bdb8135c4e9d2264923) | [snafuj](https://sketchfab.com/snafuj) | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) |

- This work is based on "Marvel Thor's Hammer (Mjölnir)" (https://sketchfab.com/3d-models/marvel-thors-hammer-mjolnir-847befafabc04998a93e4326c2eb5d94) by Boooooop (https://sketchfab.com/boooop) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated for VR, textures atlased.
- This work is based on "Mjolnir (Thor's hammer)" (https://sketchfab.com/3d-models/mjolnir-thors-hammer-ecca1232710f4721b14d64a90bba6557) by TheDevilsEye (https://sketchfab.com/TheDevilsEye) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Re-centred and scaled.
- This work is based on "Mjolnir - Thor love and thunder" (https://sketchfab.com/3d-models/mjolnir-thor-love-and-thunder-9689a953a9724987bb962bc00104c5fd) by Jesus Osco (https://sketchfab.com/gsus.vfx) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated for VR.
- This work is based on "Thor's Hammer: Mjolnir" (https://sketchfab.com/3d-models/thors-hammer-mjolnir-1929e65c877a48bba3b16cfa51f7a33c) by fccooper (https://sketchfab.com/fccooper) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Re-centred and scaled.
- This work is based on "GMC Vandura1976 Van" (https://sketchfab.com/3d-models/gmc-vandura1976-van-40cf97bc0f7d4bdb8135c4e9d2264923) by snafuj (https://sketchfab.com/snafuj) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/). Decimated for VR, painted black, glass left out; a cab and steering wheel added (built for HERO VR).

## The JOKER batarang: CC BY-NC-ND and "NoAI"

- **Non-commercial.** Because the JOKER batarang is CC BY-NC-ND, any build that carries it must stay
  non-commercial: no sale, paywall, paid early access or ad-supported download. HERO VR is free, and
  this model is shipped in it unmodified, with its own licence file (`Models/Batarang/JOKER/license.txt`)
  and the credit above. Licence: CC BY-NC-ND 4.0, http://creativecommons.org/licenses/by-nc-nd/4.0/.
- **No derivatives.** It is never converted, decimated, recoloured or downscaled. It is shown as
  downloaded, or the BATARANG STYLE falls back to DEFAULT. At 396 triangles it fits the 700-triangle
  budget unchanged. Its three 4096² textures ship unmodified and go unused, because the overlay draws
  flat colour.
- **"NoAI".** The model's Sketchfab page carries the creator's "NoAI" tag. This means the author
  does not permit the model to be used in AI datasets or for generative AI. The mod uses it only as
  a drop-in game asset. Nothing was trained on it or generated from it, and no other use should be
  made of it.

## 0.0.74 test 1: HIDEOUTS (Models/Hideout/<Room>/scene.glb + room.txt)

| File | Source | Author | Licence | Changes |
|---|---|---|---|---|
| `Models/Hideout/Batcave/` | Injustice: Gods Among Us (mobile) Batcave stage, from [The Models Resource](https://models.spriters-resource.com/mobile/injusticegodsamongus/asset/302872/) | Injustice: Gods Among Us by NetherRealm Studios / Warner Bros. Games; ripped by Ecelon (2017) | As credited by The Models Resource (credit the ripper) | Scaled to metres (x 0.6536), floor at 0, re-exported from Blender (KHR_lights_punctual stripped, blended materials drawn as cut-out), walk grid and points generated (`tools/hideout074/make_hideout.py`). |
| `Models/Hideout/Safehouse/` | Built for HERO VR (a Blender PLACEHOLDER: plain walls, floor, a table, a case board) | Agent Mulder | Made for HERO VR | `tools/hideout074/make_safehouse_blender.py`. To be replaced by a real room. |

The van's dash HOME page offers GO TO SAFEHOUSE; the Batmobile's dash offers GO TO BATCAVE, and the
Batcave's "exit to Batmobile" point takes the player back to the Batmobile.

## 0.0.74 test 1: BATMOBILE (Models/Vehicle/Batmobile/<Car>/model.glb)

BATMAN's four drivable Batmobiles (BATMOBILE CHOICE, or the picker on the Batmobile's dash). Each is
converted by `tools/batmobile074/convert.py`: rescaled to about 6 m long, x right, y up, -z forward,
the ground under its axles, its colours baked per triangle, decimated to the van's 150000-triangle cap
or under, never flattened. Its windshield, glass and canopy are kept exactly as the model came
(no glass split out, nothing made see-through, no faces cut).

| Folder | Model | Author | Licence | Changes |
|---|---|---|---|---|
| `Batmobile/1989` | [Bat-Mobile (1989)](https://dreamloft3d.itch.io/bat-mobile-1989) (itch.io) | Dreamloft3D | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 166302 to 107806 triangles; glass as the model came; colours baked, textures dropped; rescaled to 6.40 m. |
| `Batmobile/ODAMI` | [Batmobile](https://sketchfab.com/3d-models/batmobile-04f571c0fece4c37a01d37c70721d5a6) | Odami_3D | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | 3704 triangles kept; glass as the model came; colours baked, textures dropped; rescaled to 6.08 m. |
| `Batmobile/FOREVER` | [Batmobile](https://sketchfab.com/3d-models/batmobile-4a7dad5866564ab3b497cb1f427cd602) | SorroW | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 506976 to 107922 triangles; canopy as the model came; the model's own seats and steering wheel left out for the shared cockpit; colours baked, textures dropped; rescaled to 6.51 m. |
| `Batmobile/FOREVER2` | [Batmobile - Batman Forever](https://sketchfab.com/3d-models/batmobile-batman-forever-77795a4cef7f4ae29b649eb527c49b05) (the author notes it is AI-generated with Meshy) | S.York-Designs | [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/) | Decimated from 406780 to 107804 triangles; canopy as the model came; colours baked, textures dropped; rescaled to 6.38 m. |
| `Batmobile/Cockpit`, `Batmobile/SteeringWheel` | The shared cockpit (seat, dash, the holo screen's bezel) and wheel, sat inside each car | Agent Mulder | Made for HERO VR | `tools/batmobile074/build_cockpit_game.py`. |

Credit lines: "Bat-Mobile (1989)" by Dreamloft3D, "Batmobile" by Odami_3D, "Batmobile" by SorroW and
"Batmobile - Batman Forever" by S.York-Designs, each licensed under CC-BY-4.0
(http://creativecommons.org/licenses/by/4.0/), converted for VR. The Batmobile's dash shows the
driven car's credit on its HOME page.

### 0.0.74 test 2 changes (Models/Vehicle/Batmobile/<Car>/model.glb and <Car>/Roof/model.glb)

- **CABIN CLEAR** (the owner's t1171u: no shell, glass or canopy face inside the cockpit): the converter
  deletes every body face inside the cockpit's box (1989: 3890 faces, ODAMI: 243, FOREVER: 11225,
  FOREVER 2: 11576) and moves the roof, canopy and cabin glass over the cockpit into a separate
  `Roof/model.glb` (1989: 2563 triangles, ODAMI: 111, FOREVER: 1033, FOREVER 2: 4337). The roof is drawn
  only while the eye is outside the car, so from outside each car looks as it came. Nothing is made
  see-through.
- **CAR PAINT**: the 1989's body is written as satin black (base colour 0.03, metallic 0.40, roughness
  0.42; the trim 0.06 / 0.35 / 0.45, the tyres 0.012 / 0.20 / 0.55). The source has no textures. The
  other three cars keep their own colours.
- Body triangles after the cut: 1989 101353, ODAMI 3350, FOREVER 95664, FOREVER 2 91891.

## Reference only (no files used)

The Sketchfab "Daredevil batons" page the player linked was a style reference only. Nothing was
downloaded from it. Following it, the mod shows no permanent cord: a cable is drawn only while a
baton is thrown or bouncing.

## Version 0.0.72: ROSTER page pictures (Models/Roster/scene.glb)

Each picture is a cropped, scaled Sketchfab thumbnail (a render of the artist's model). The suits that ship are the 23 in `Models/Suits/` (below); a suit not shipped shows MODEL MISSING. Spider-Man's picture is drawn for HERO VR.

Licences: CC BY 4.0, CC BY-NC 4.0, CC BY-NC-SA 4.0; "Free Standard" = the Sketchfab Standard licence.

```
 0 portrait_spiderman: drawn for HERO VR (the game's own suit; no artist's model)
 1 portrait_ironman: "Iron-Man Mark 85 | Rigged" by Nihar-9Afilms (CC Attribution) https://sketchfab.com/3d-models/iron-man-mark-85-rigged-dde1085c464d4f8da259fe6669ae4dd2
 2 portrait_daredevil: "Daredevil - DAREDEVIL: BORN AGAIN SEASON 2" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/daredevil-daredevil-born-again-season-2-dbed252cbc624fd7ae43ec0c49f23a3a
 3 portrait_hulk: "Hulk Maestro" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/hulk-maestro-8a22bab80de94ef18372f338ec51a691
 4 portrait_batman: "Batman 1989" by hl_models (CC Attribution) https://sketchfab.com/3d-models/batman-1989-28717167aead4010b774b67d56aedbe0
 5 portrait_wolverine: "Wolverine - Jacket - Jeans (Rigged and Textured)" by Light.k (CC Attribution) https://sketchfab.com/3d-models/wolverine-jacket-jeans-rigged-and-textured-57c7193d711b4c5eb0c172a238d20dd3
 6 portrait_superman: "Superman" by alen60303 (CC Attribution) https://sketchfab.com/3d-models/superman-fb13b58823cb4c908d2a1cc8327b1f01
 7 portrait_punisher: "Punisher Warzone" by spybg (CC Attribution) https://sketchfab.com/3d-models/punisher-warzone-20b5eba6b24c463cb3a2666cd1ab4ba6
 8 portrait_captainamerica: "Captain America" by alexseagle2004 (CC Attribution) https://sketchfab.com/3d-models/captain-america-5bf43af9198f46318d96e23587338913
 9 portrait_thor: "Thor - Reborn from Ragnarok" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/thor-reborn-from-ragnarok-3d8d4c561c644142bd2af29ae395f94d
10 portrait_greenlantern: "Green Lantern (Ryan Reynolds)" by DavidA3D (CC Attribution-NonCommercial) https://sketchfab.com/3d-models/green-lantern-ryan-reynolds-b5753ab691134b3898df8bd2d501c759
11 CapSuit: "Captain America" by Iqbal.Mulyawan.Putera (CC Attribution) https://sketchfab.com/3d-models/captain-america-d4898ea3aac3474fa0661de3ade9b7c6
12 Suit_batman_1: "Batman Beyond - Textured and Rigged" by Light.k (CC Attribution) https://sketchfab.com/3d-models/batman-beyond-textured-and-rigged-d44a1aae7c444d18a5fe23c5081e6152
13 Suit_batman_2: "Batman 1989" by hl_models (CC Attribution) https://sketchfab.com/3d-models/batman-1989-28717167aead4010b774b67d56aedbe0
14 Suit_batman_3: "Bruce Wayne Batman Year One" by allanromanreyes (Free Standard) https://sketchfab.com/3d-models/bruce-wayne-batman-year-one-7d29d9da765043e0b36256de31c8c027
15 Suit_batman_5: "Batman Origins Suit - Textured and Rigged" by Light.k (CC Attribution) https://sketchfab.com/3d-models/batman-origins-suit-textured-and-rigged-ce83e407d259482c9ba284bced828933
16 Suit_ironMan_1: "Iron-Man Mark 85 | Rigged" by Nihar-9Afilms (CC Attribution) https://sketchfab.com/3d-models/iron-man-mark-85-rigged-dde1085c464d4f8da259fe6669ae4dd2
17 Suit_ironMan_2: "Iron Man - Mark 1" by Nathang30 (CC Attribution-NonCommercial-ShareAlike) https://sketchfab.com/3d-models/iron-man-mark-1-57b18282c1a84c5899fcc7f67762a386
18 Suit_ironMan_3: "Iron Man Mark-7(Subsurf)" by benjamin.borromeo (CC Attribution) https://sketchfab.com/3d-models/iron-man-mark-7subsurf-a13c067e70814dc7b2ebc3f0ace10961
19 Suit_ironMan_4: "Marvel™ Iron Man Mark 6 Blender Model" by 3774428638 (CC Attribution) https://sketchfab.com/3d-models/marveltm-iron-man-mark-6-blender-model-9865dc82c37542ef8d5bd8769e58cb59
20 Suit_captainAmerica_1: "Captain America  (The Winter Soldier)" by Zeztz777 (CC Attribution) https://sketchfab.com/3d-models/captain-america-the-winter-soldier-fdc76dc10f354f5a860d41f8ce1f40bd
21 Suit_captainAmerica_2: "Captain_america__ww2" by Manuel.41 (Free Standard) https://sketchfab.com/3d-models/captain-america--ww2-a66cbde864f147d89c6c4b319f64a79e
22 Suit_captainAmerica_4: "Captain America" by alexseagle2004 (CC Attribution) https://sketchfab.com/3d-models/captain-america-5bf43af9198f46318d96e23587338913
23 Suit_daredevil_1: "Daredevil - DAREDEVIL: BORN AGAIN SEASON 2" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/daredevil-daredevil-born-again-season-2-dbed252cbc624fd7ae43ec0c49f23a3a
24 Suit_daredevil_2: "Daredevil Netflix Marvel Rivals" by VLTXD (CC Attribution) https://sketchfab.com/3d-models/daredevil-netflix-marvel-rivals-d2872ea0ac4647a38281c85d1c4fa347
25 Suit_daredevil_3: "DareDevil Yellow and Red Suit" by officialsupremefilms (CC Attribution) https://sketchfab.com/3d-models/daredevil-yellow-and-red-suit-efa459b17014459db4bb95c6bd4882a0
26 Suit_hulk_1: "Hulk(Grey)" by 3dworldz (CC Attribution) https://sketchfab.com/3d-models/hulkgrey-d2ce3d6d15cf4a39a9898cfcc9716314
27 Suit_hulk_2: "Professor Hulk Endgame" by Shivam.Yadav (CC Attribution) https://sketchfab.com/3d-models/professor-hulk-endgame-8708949dd0204d3b9084da311743e8a3
28 Suit_hulk_4: "Hulk Maestro" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/hulk-maestro-8a22bab80de94ef18372f338ec51a691
29 Suit_wolverine_1: "Wolverine Rigged" by stokissout (CC Attribution) https://sketchfab.com/3d-models/wolverine-rigged-af65afa536854dfebf93e8f342501c5c
30 Suit_wolverine_2: "Legendary Wolverine (XMO Wolverine Game)" by LombaxLover2K (CC Attribution) https://sketchfab.com/3d-models/legendary-wolverine-xmo-wolverine-game-5bc6a65ef349441bb2d3118f7ed2a32d
31 Suit_wolverine_3: "Wolverine - Jacket - Jeans (Rigged and Textured)" by Light.k (CC Attribution) https://sketchfab.com/3d-models/wolverine-jacket-jeans-rigged-and-textured-57c7193d711b4c5eb0c172a238d20dd3
32 Suit_wolverine_4: "Weapon X Wolverine" by assetgoat (CC Attribution) https://sketchfab.com/3d-models/weapon-x-wolverine-6788d2a1b330480a97cc13be606eb7e9
33 Suit_wolverine_5: "BlendAtlas Wolverine (D&W suit)" by BlendAtlas (image from the artist's page) https://blendatlas.com (not on Sketchfab)
34 Suit_punisher_1: "The Punisher (Vest-Version)" by floh (CC Attribution-NonCommercial) https://sketchfab.com/3d-models/the-punisher-vest-version-2d7deb3b811e42419f71263871dd7139
35 Suit_punisher_2: "The Punisher" by floh (CC Attribution-NonCommercial) https://sketchfab.com/3d-models/the-punisher-0ec6a71b0c1e4e5fa07e567b06c8b99b
36 Suit_thor_1: "Thor(Textured)(Rigged)" by 3dworldz (CC Attribution) https://sketchfab.com/3d-models/thortexturedrigged-7fc395b9b26c4d40bafa0d5221b26e9c
37 Suit_thor_2: "Thor (Textured) (No Rig)" by 3dworldz (CC Attribution) https://sketchfab.com/3d-models/thor-textured-no-rig-266488af3874414a8dfa9f488e738aed
38 Suit_thor_3: "Thor - Reborn from Ragnarok" by YEEZY_YE (CC Attribution) https://sketchfab.com/3d-models/thor-reborn-from-ragnarok-3d8d4c561c644142bd2af29ae395f94d
39 Suit_greenLantern_1: "Green Lantern (Ryan Reynolds)" by DavidA3D (CC Attribution-NonCommercial) https://sketchfab.com/3d-models/green-lantern-ryan-reynolds-b5753ab691134b3898df8bd2d501c759
40 Suit_greenLantern_2: "Injustice 2 - Green Lantern Hal Jordan" by Neut2000 (CC Attribution) https://sketchfab.com/3d-models/injustice-2-green-lantern-hal-jordan-ed81b5682e7e455d9527023eaa36d209
41 Suit_greenLantern_3: "Green Lantern (Renegade)" by 3dworldz (CC Attribution) https://sketchfab.com/3d-models/green-lantern-renegade-8aa2691803f04468846f51c6e265bb92
42 Suit_greenLantern_4: "Green Lantern(Textured)(Rigged)" by 3dworldz (CC Attribution) https://sketchfab.com/3d-models/green-lanterntexturedrigged-89362a83a6834d068d05ee5c7589cf33
43 Suit_superman_1: "SUPERMAN SSKTJL" by 20062020year (CC Attribution) https://sketchfab.com/3d-models/superman-ssktjl-cc59403f1a9347e89cfbe662e0acdc5a
44 Suit_superman_2: "Superman" by alen60303 (CC Attribution) https://sketchfab.com/3d-models/superman-fb13b58823cb4c908d2a1cc8327b1f01
45 Suit_superman_3: "Superman (Henry Cavill) - rigged" by DavidA3D (CC Attribution-NonCommercial) https://sketchfab.com/3d-models/superman-henry-cavill-rigged-05b390aab06b42c1b9c656063812107e
46 Suit_superman_4: "Superman - DC" by Chechorams16 (CC Attribution) https://sketchfab.com/3d-models/superman-dc-ccad133a5a684003a059a01c89216780
47 PunisherWarzone: "Punisher Warzone" by spybg (CC Attribution) https://sketchfab.com/3d-models/punisher-warzone-20b5eba6b24c463cb3a2666cd1ab4ba6
```

## Version 0.0.72: the worn SUITS (Models/Suits/<suit>/scene.glb)

The 23 suits the user sent (no others), converted by `tools/suits/convert_suit.py`: re-oriented to Y-up, scaled
to 1.80 m, feet at the origin, their textures packed into atlases, skinned to the game's 16-bone body (the
artist's rig when it has one, else joints from the body's shape), and when over the VR cap of 24000 triangles
reduced by Blender's COLLAPSE decimation only (UVs and shape kept; checked side by side against each download,
`tools/models072/model_check.py`). Parts hidden inside a suit (teeth, eyeballs, an unmasked head) and props not
worn (Captain America's shields, Daredevil's fire) are left out. Each suit's own `license.txt` ships beside it.
Non-commercial (NC) licences: the mod is free and never sold.

| File | Model | Author | Licence | Changes |
|---|---|---|---|---|
| `Models/Suits/Suit_batman_1/` | [Batman Beyond - Textured and Rigged](https://sketchfab.com/3d-models/batman-beyond-textured-and-rigged-d44a1aae7c444d18a5fe23c5081e6152) | [Light.k](https://sketchfab.com/Light.k) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 149855 → 23999 triangles; the artist's rig. |
| `Models/Suits/Suit_batman_2/` | [Batman 1989](https://sketchfab.com/3d-models/batman-1989-28717167aead4010b774b67d56aedbe0) | [H.E.C.U Models](https://sketchfab.com/H.E.C.U) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 69965 → 24000 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_batman_3/` | [Bruce Wayne Batman Year One](https://sketchfab.com/3d-models/bruce-wayne-batman-year-one-7d29d9da765043e0b36256de31c8c027) | [Tigerar1](https://sketchfab.com/allanromanreyes) | [SKETCHFAB Standard](https://sketchfab.com/licenses) | 6010 triangles kept; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_batman_5/` | [Batman Origins Suit - Textured and Rigged](https://sketchfab.com/3d-models/batman-origins-suit-textured-and-rigged-ce83e407d259482c9ba284bced828933) | [Light.k](https://sketchfab.com/Light.k) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 123922 → 24000 triangles; the artist's rig. |
| `Models/Suits/Suit_ironMan_1/` | [Iron-Man Mark 85 \| Rigged](https://sketchfab.com/3d-models/iron-man-mark-85-rigged-dde1085c464d4f8da259fe6669ae4dd2) | [9A Films / Nihar Arora](https://sketchfab.com/Nihar-9Afilms) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 639853 → 24000 triangles; the artist's rig. |
| `Models/Suits/Suit_ironMan_2/` | [Iron Man - Mark 1](https://sketchfab.com/3d-models/iron-man-mark-1-57b18282c1a84c5899fcc7f67762a386) | [Nathang30](https://sketchfab.com/Nathang30) | [CC-BY-NC-SA-4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/) | collapse-decimated 69071 → 23999 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_ironMan_3/` | [Iron Man Mark-7(Subsurf)](https://sketchfab.com/3d-models/iron-man-mark-7subsurf-a13c067e70814dc7b2ebc3f0ace10961) | [benjamin.borromeo](https://sketchfab.com/benjamin.borromeo) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 143228 → 23999 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_ironMan_4/` | [Marvel™ Iron Man Mark 6 Blender Model](https://sketchfab.com/3d-models/marveltm-iron-man-mark-6-blender-model-9865dc82c37542ef8d5bd8769e58cb59) | [Mittergen](https://sketchfab.com/3774428638) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 148592 → 24000 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_captainAmerica_1/` | [Captain America  (The Winter Soldier)](https://sketchfab.com/3d-models/captain-america-the-winter-soldier-fdc76dc10f354f5a860d41f8ce1f40bd) | [mikomagallona](https://sketchfab.com/mikomagallona) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 11859 of 12577 triangles kept (no decimation: hidden parts, props or doubled faces left out); the artist's rig. |
| `Models/Suits/Suit_captainAmerica_2/` | [Captain_america__ww2](https://sketchfab.com/3d-models/captain-america--ww2-a66cbde864f147d89c6c4b319f64a79e) | [Marco.41](https://sketchfab.com/Manuel.41) | [SKETCHFAB Standard](https://sketchfab.com/licenses) | 11996 of 13000 triangles kept (no decimation: the shield left out, a prop); the artist's rig; turned 90 degrees to face forward. |
| `Models/Suits/Suit_daredevil_1/` | [Daredevil - DAREDEVIL: BORN AGAIN SEASON 2](https://sketchfab.com/3d-models/daredevil-daredevil-born-again-season-2-dbed252cbc624fd7ae43ec0c49f23a3a) | [YE YE](https://sketchfab.com/YEEZY_YE) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 97334 → 24000 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_daredevil_3/` | [DareDevil Yellow and Red Suit](https://sketchfab.com/3d-models/daredevil-yellow-and-red-suit-efa459b17014459db4bb95c6bd4882a0) | [officialsupremefilms](https://sketchfab.com/officialsupremefilms) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 15608 triangles kept; the artist's rig. |
| `Models/Suits/Suit_hulk_2/` | [Professor Hulk Endgame](https://sketchfab.com/3d-models/professor-hulk-endgame-8708949dd0204d3b9084da311743e8a3) | [Shivam.Yadav](https://sketchfab.com/Shivam.Yadav) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 7782 triangles kept; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_wolverine_1/` | [Wolverine Rigged](https://sketchfab.com/3d-models/wolverine-rigged-af65afa536854dfebf93e8f342501c5c) | [Stokissout](https://sketchfab.com/Stokissout) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 211045 → 24000 triangles; the artist's rig. |
| `Models/Suits/Suit_wolverine_3/` | [Wolverine - Jacket - Jeans (Rigged and Textured)](https://sketchfab.com/3d-models/wolverine-jacket-jeans-rigged-and-textured-57c7193d711b4c5eb0c172a238d20dd3) | [Light.k](https://sketchfab.com/Light.k) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 23089 of 23091 triangles kept (no decimation: doubled faces left out); the artist's rig; turned 90 degrees to face forward; Version 0.0.73: its claws moved to `Models/Claws/` (retractable, no double claws). |
| `Models/Claws/Left/`, `Models/Claws/Right/` | [Wolverine - Jacket - Jeans (Rigged and Textured)](https://sketchfab.com/3d-models/wolverine-jacket-jeans-rigged-and-textured-57c7193d711b4c5eb0c172a238d20dd3) | [Light.k](https://sketchfab.com/Light.k) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | Version 0.0.73: the suit's own claws (144 triangles a hand, geometry, normals, UVs and texture as the artist made them), only moved and turned to the knuckles (`tools/claws073/extract_claws.py`); every Wolverine suit's retractable claws. |
| `Models/Suits/Suit_wolverine_4/` | [Weapon X Wolverine](https://sketchfab.com/3d-models/weapon-x-wolverine-6788d2a1b330480a97cc13be606eb7e9) | [AssetGoat](https://sketchfab.com/assetgoat) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | collapse-decimated 93612 → 24000 triangles; rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_punisher_1/` | [The Punisher (Vest-Version)](https://sketchfab.com/3d-models/the-punisher-vest-version-2d7deb3b811e42419f71263871dd7139) | [floh](https://sketchfab.com/floh) | [CC-BY-NC-4.0](http://creativecommons.org/licenses/by-nc/4.0/) | 23747 of 25236 triangles kept (no decimation: hidden parts, props or doubled faces left out); rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_punisher_2/` | [The Punisher](https://sketchfab.com/3d-models/the-punisher-0ec6a71b0c1e4e5fa07e567b06c8b99b) | [floh](https://sketchfab.com/floh) | [CC-BY-NC-4.0](http://creativecommons.org/licenses/by-nc/4.0/) | 17343 of 20322 triangles kept (no decimation: hidden parts, props or doubled faces left out); rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_thor_1/` | [Thor(Textured)(Rigged)](https://sketchfab.com/3d-models/thortexturedrigged-7fc395b9b26c4d40bafa0d5221b26e9c) | [CAPTAAINR](https://sketchfab.com/CAPTAAINR) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 11841 of 12229 triangles kept (no decimation: the hammer left out, a prop); the artist's rig; turned 90 degrees to face forward. |
| `Models/Suits/Suit_thor_2/` | [Thor (Textured) (No Rig)](https://sketchfab.com/3d-models/thor-textured-no-rig-266488af3874414a8dfa9f488e738aed) | [CAPTAAINR](https://sketchfab.com/CAPTAAINR) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 19580 of 20324 triangles kept (no decimation: the hammer left out, a prop); rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_greenLantern_2/` | [Injustice 2 - Green Lantern Hal Jordan](https://sketchfab.com/3d-models/injustice-2-green-lantern-hal-jordan-ed81b5682e7e455d9527023eaa36d209) | [Neut2000](https://sketchfab.com/Neut2000) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 13447 of 14985 triangles kept (no decimation: hidden parts, props or doubled faces left out); rigged from its shape (no rig in the download). |
| `Models/Suits/Suit_greenLantern_3/` | [Green Lantern (Renegade)](https://sketchfab.com/3d-models/green-lantern-renegade-8aa2691803f04468846f51c6e265bb92) | [CAPTAAINR](https://sketchfab.com/CAPTAAINR) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 8552 triangles kept; the artist's rig; turned 30 degrees to face forward. |
| `Models/Suits/Suit_superman_4/` | [Superman - DC](https://sketchfab.com/3d-models/superman-dc-ccad133a5a684003a059a01c89216780) | [Chechorams16](https://sketchfab.com/Chechorams16) | [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/) | 19926 triangles kept; the artist's rig. |

# Third-party 3D models (HERO VR 0.0.7)

HERO VR's on-screen credit is "Made by Agent Mulder". The models below belong to their authors:
Digital Goblin, iqbalmulyawan, Yanez Designs (Anthony Yanez), TORI106, LiliumLetifer, Eira, Ludus101,
baton268 and Falxx. They are credited here, in CREDITS.md, on the launcher's Credits page and in the release notes,
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

## Version 0.0.72 (TEST): new models (none in git; prepared on the PC from the Sketchfab downloads)

The 0.0.72 test build adds these artists: floh, spybg (Valentin Yovchev), Bugawuga, RITIKA_87, klava88
and jeandiz (SpatialNeglect), with the kitbash part authors ErhanMatur, Bzovius, SrGeneroso, shedmon,
Albin and Mayess. No Marvel or DC credit: only the artists. The model files are not in this test zip
(the Sketchfab downloads need the user's login); the kits that prepare them are in `tools/`.

| Use | Model | Author | Licence | Changes |
|---|---|---|---|---|
| PUNISHER suit VEST | [The Punisher (Vest-Version)](https://sketchfab.com/3d-models/the-punisher-vest-version-2d7deb3b811e42419f71263871dd7139) | floh | [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) (HERO VR is free, never monetized) | Re-oriented, rescaled, welded (`tools/punisher_suit/prep_punisher.py`); rigged on the PC. |
| PUNISHER suit WARZONE | [Punisher Warzone](https://sketchfab.com/3d-models/punisher-warzone-20b5eba6b24c463cb3a2666cd1ab4ba6) | spybg (Valentin Yovchev) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | As above. |
| THE HAND enemy 1 | [Ninja - Walking](https://sketchfab.com/3d-models/ninja-walking-0268036dde274e6ba9ded5bdf8a082c1) | Bugawuga | CC BY 4.0 | Outfit texture recoloured crimson (skin, eyes, metal kept). |
| THE HAND enemy 2 | [Ninja - low poly - real life based](https://sketchfab.com/3d-models/ninja-low-poly-real-life-based-inspired-22ccb0bf2b6d4f8286e0dd4edb5bce63) | RITIKA_87 | CC BY 4.0 | Decimated ~100k to ~45k, rigged (weights from enemy 1's rig), outfit recoloured crimson. |
| THE HAND enemy 3 | [Saboteur Ninja Animated](https://sketchfab.com/3d-models/saboteur-ninja-animated-3cbc15c178f943a1b4511b973f27a033) | klava88 | CC BY 4.0 | Outfit recoloured crimson. |
| HYDRA enemy 1 | [S.W.A.T. Operator](https://sketchfab.com/3d-models/swat-operator-9e82fabf26194896b5ad4a364d864eab) | jeandiz (SpatialNeglect); kitbash parts by ErhanMatur, Bzovius, SrGeneroso, shedmon, Albin, Mayess | CC BY 4.0 | Decimated ~107k to ~45k, weapons removed, our own skull-and-tentacles emblem (drawn by `tools/factions/factions.py`) on chest and shoulder. |
| HYDRA enemy 2 | [FSB Operator](https://sketchfab.com/3d-models/fsb-operator-43a561e941704eefb1ab0614be4f0049) | jeandiz (SpatialNeglect); same kitbash authors | CC BY 4.0 | Decimated ~114k to ~45k, weapons removed, our own emblem. |

- This work is based on "The Punisher (Vest-Version)" by floh, licensed under CC-BY-NC-4.0. Re-oriented, rescaled and welded; rigged on the player's PC. Free, never monetized.
- This work is based on "Punisher Warzone" by spybg (Valentin Yovchev), licensed under CC-BY-4.0. Re-oriented, rescaled and welded; rigged on the player's PC.
- This work is based on "Ninja - Walking" by Bugawuga, licensed under CC-BY-4.0. Outfit recoloured.
- This work is based on "Ninja - low poly - real life based" by RITIKA_87, licensed under CC-BY-4.0. Decimated, rigged and recoloured.
- This work is based on "Saboteur Ninja Animated" by klava88, licensed under CC-BY-4.0. Outfit recoloured.
- This work is based on "S.W.A.T. Operator" and "FSB Operator" by jeandiz (SpatialNeglect), kitbashed from parts by ErhanMatur, Bzovius, SrGeneroso, shedmon, Albin and Mayess, licensed under CC-BY-4.0. Decimated, weapons removed, our own emblem added.

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

## Reference only (no files used)

The Sketchfab "Daredevil batons" page the player linked was a style reference only. Nothing was
downloaded from it. Following it, the mod shows no permanent cord: a cable is drawn only while a
baton is thrown or bouncing.

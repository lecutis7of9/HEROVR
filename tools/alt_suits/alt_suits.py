"""Version 0.0.72 (TEST) ALTERNATE SUITS: the user's picks (8:59), one place for the C++ table, the
credits and the prep targets. Picks are numbered as on the sheets (/workspace/hero-0.0.72/alt-suits).

Rules (the user): each hero's FINAL suit unlocks at that hero's max reputation; every other pick is
unlocked from the start; UNLOCK ALL SUITS (TEST UNLOCKS) opens the finals too. A model that is not
fetched, rigged and decimated yet is listed COMING SOON (disabled, never loaded, never a crash).
Anything over DECIMATE_OVER triangles is decimated to DECIMATE_TO before it ships.

Sketchfab gave 401 to every download from the build box (no account token), so every pick below is
COMING SOON in this build; `fetched` turns true once a model is converted (see prep notes).
CLI: python alt_suits.py cpp | credits | report
"""
import sys

DECIMATE_OVER = 150_000
DECIMATE_TO = 150_000
HEROES = ['spiderMan', 'ironMan', 'daredevil', 'hulk', 'batman', 'wolverine', 'superman', 'punisher',
          'captainAmerica', 'thor', 'greenLantern']

# (hero, sheet number, NAME (<= 31), title on Sketchfab, artist, licence, triangles, rigged, uid/url, final)
SUITS = [
    ('batman', 1, 'BEYOND', 'Batman Beyond - Textured and Rigged', 'Light.k', 'CC BY', 149855, True,
     'd44a1aae7c444d18a5fe23c5081e6152', True),
    ('batman', 2, '1989', 'Batman 1989', 'Homeless_Models (hl_models)', 'CC BY', 69969, False,
     '28717167aead4010b774b67d56aedbe0', False),
    ('batman', 3, 'YEAR ONE', 'Bruce Wayne Batman Year One', 'Tigerar1 (allanromanreyes)', 'SF STANDARD', 6010, False,
     '7d29d9da765043e0b36256de31c8c027', False),
    ('batman', 5, 'ORIGINS', 'Batman Origins Suit - Textured and Rigged', 'Light.k', 'CC BY', 123922, True,
     'ce83e407d259482c9ba284bced828933', False),
    ('ironMan', 1, 'MARK 85', 'Iron-Man Mark 85 | Rigged', '9A Films / Nihar Arora', 'CC BY', 639859, True,
     'dde1085c464d4f8da259fe6669ae4dd2', False),
    ('ironMan', 2, 'MARK 1', 'Iron Man - Mark 1', 'Nathang30', 'CC BY-NC-SA', 69071, False,
     '57b18282c1a84c5899fcc7f67762a386', True),
    ('ironMan', 3, 'MARK 7', 'Iron Man Mark-7(Subsurf)', 'benjamin.borromeo', 'CC BY', 143264, False,
     'a13c067e70814dc7b2ebc3f0ace10961', False),
    ('ironMan', 4, 'MARK 6', 'Iron Man Mark 6 Blender Model', 'Mittergen', 'CC BY', 148601, True,
     '9865dc82c37542ef8d5bd8769e58cb59', False),
    ('captainAmerica', 1, 'WINTER SOLDIER', 'Captain America (The Winter Soldier)', 'Zeztz777', 'CC BY', 12579, True,
     'fdc76dc10f354f5a860d41f8ce1f40bd', True),
    ('captainAmerica', 2, 'WW2', 'Captain_america__ww2', 'Marco.41', 'SF STANDARD', 13000, True,
     'a66cbde864f147d89c6c4b319f64a79e', False),
    ('captainAmerica', 4, 'CLASSIC', 'Captain America', 'alexseagle2004', 'CC BY', 12929, True,
     '5bf43af9198f46318d96e23587338913', False),
    ('daredevil', 1, 'BORN AGAIN S2', 'Daredevil - DAREDEVIL: BORN AGAIN SEASON 2', 'YE YE', 'CC BY', 97334, True,
     'dbed252cbc624fd7ae43ec0c49f23a3a', False),
    ('daredevil', 2, 'NETFLIX', 'Daredevil Netflix Marvel Rivals', 'VLTXD', 'CC BY', 94279, True,
     'd2872ea0ac4647a38281c85d1c4fa347', False),
    ('daredevil', 3, 'YELLOW AND RED', 'DareDevil Yellow and Red Suit', 'AtoMichael Editz', 'CC BY', 15608, True,
     'efa459b17014459db4bb95c6bd4882a0', True),
    ('hulk', 1, 'GREY', 'Hulk(Grey)', '3D MODELS', 'CC BY', 7873, False,
     'd2ce3d6d15cf4a39a9898cfcc9716314', False),
    ('hulk', 2, 'PROFESSOR', 'Professor Hulk Endgame', 'Shivam.Yadav', 'CC BY', 7783, False,
     '8708949dd0204d3b9084da311743e8a3', False),
    ('hulk', 4, 'MAESTRO', 'Hulk Maestro', 'YE YE', 'CC BY', 128666, True,
     '8a22bab80de94ef18372f338ec51a691', True),
    ('wolverine', 1, 'CLASSIC', 'Wolverine Rigged', 'stokissout', 'CC BY', 211192, True,
     'af65afa536854dfebf93e8f342501c5c', False),
    ('wolverine', 2, 'LEGENDARY', 'Legendary Wolverine (XMO Wolverine Game)', 'LombaxLover2K', 'CC BY', 22106, True,
     '5bc6a65ef349441bb2d3118f7ed2a32d', False),
    ('wolverine', 3, 'JACKET AND JEANS', 'Wolverine - Jacket - Jeans (Rigged and Textured)', 'Light.k', 'CC BY', 23097,
     True, '57c7193d711b4c5eb0c172a238d20dd3', False),
    ('wolverine', 4, 'WEAPON X', 'Weapon X Wolverine', 'AssetGoat', 'CC BY', 93612, False,
     '6788d2a1b330480a97cc13be606eb7e9', True),
    ('wolverine', 5, 'D AND W', 'BlendAtlas Wolverine (D&W suit)', 'BlendAtlas', 'UNKNOWN', 0, False,
     'not on Sketchfab (BlendAtlas; licence not checked)', False),
    ('punisher', 1, 'VEST', 'The Punisher (Vest-Version)', 'floh', 'CC BY-NC', 25236, False,
     '2d7deb3b811e42419f71263871dd7139', False),
    ('punisher', 2, 'CLASSIC', 'The Punisher', 'floh', 'CC BY-NC', 20322, False,
     '0ec6a71b0c1e4e5fa07e567b06c8b99b', False),
    ('thor', 1, 'RIGGED', 'Thor(Textured)(Rigged)', '3D MODELS', 'CC BY', 12229, True,
     '7fc395b9b26c4d40bafa0d5221b26e9c', False),
    ('thor', 2, 'CLASSIC', 'Thor (Textured) (No Rig)', '3D MODELS', 'CC BY', 20324, False,
     '266488af3874414a8dfa9f488e738aed', True),
    ('thor', 3, 'RAGNAROK', 'Thor - Reborn from Ragnarok', 'YE YE', 'CC BY', 114314, True,
     '3d8d4c561c644142bd2af29ae395f94d', False),
    ('greenLantern', 1, 'FILM', 'Green Lantern (Ryan Reynolds)', 'DavidA3D', 'CC BY-NC', 7314, False,
     'b5753ab691134b3898df8bd2d501c759', False),
    ('greenLantern', 2, 'INJUSTICE 2', 'Injustice 2 - Green Lantern Hal Jordan', 'Neut2000', 'CC BY', 14985, False,
     'ed81b5682e7e455d9527023eaa36d209', False),
    ('greenLantern', 3, 'RENEGADE', 'Green Lantern (Renegade)', '3D MODELS', 'CC BY', 8552, True,
     '8aa2691803f04468846f51c6e265bb92', True),
    ('greenLantern', 4, 'RIGGED', 'Green Lantern(Textured)(Rigged)', '3D MODELS', 'CC BY', 7526, True,
     '89362a83a6834d068d05ee5c7589cf33', False),
    ('superman', 1, 'SSKTJL', 'SUPERMAN SSKTJL', '20062020year', 'CC BY', 114771, True,
     'cc59403f1a9347e89cfbe662e0acdc5a', False),
    ('superman', 2, 'CLASSIC', 'Superman', 'Allen Tseng', 'CC BY', 11820, True,
     'fb13b58823cb4c908d2a1cc8327b1f01', False),
    ('superman', 3, 'CAVILL', 'Superman (Henry Cavill) - rigged', 'DavidA3D', 'CC BY-NC', 4654, True,
     '05b390aab06b42c1b9c656063812107e', False),
    ('superman', 4, 'DC', 'Superman - DC', 'Chechorams16', 'CC BY', 19926, True,
     'ccad133a5a684003a059a01c89216780', True),
]
FETCHED = set()  # sheet keys ('batman', 1) once converted; empty in this build (401)


def url(uid):
    return f'https://sketchfab.com/3d-models/{uid}' if len(uid) == 32 else uid


def target_tris(tris):
    return DECIMATE_TO if tris > DECIMATE_OVER else tris


def folder(hero, number):
    return f'Suit_{hero}_{number}'


def cpp():
    out = []
    for h, n, name, title, artist, lic, tris, rig, uid, final in SUITS:
        soon = 'false' if (h, n) in FETCHED else 'true'
        out.append(f'        add(makeSuit(hero_modes::{h}, "{name}", "{artist}", "{lic}", '
                   f'{"repMost" if final else "0"}, "{folder(h, n)}", {"true" if final else "false"}, {soon}));')
    return '\n'.join(out)


def credits():
    lines = []
    for h, n, name, title, artist, lic, tris, rig, uid, final in SUITS:
        lines.append(f"- {h} suit {n} ({name}): '{title}' by {artist}, {lic}, {url(uid)}")
    return '\n'.join(lines)


def report():
    lines = []
    for h, n, name, title, artist, lic, tris, rig, uid, final in SUITS:
        state = 'READY' if (h, n) in FETCHED else 'COMING SOON (not fetched: Sketchfab 401)'
        dec = f'decimate {tris} -> {DECIMATE_TO}' if tris > DECIMATE_OVER else f'{tris} tris'
        lines.append(f'{h} {n} {name}{" FINAL" if final else ""}: {state}; {dec}; '
                     f'{"rigged" if rig else "NOT rigged"}; {lic}')
    return '\n'.join(lines)


if __name__ == '__main__':
    print({'cpp': cpp, 'credits': credits, 'report': report}[sys.argv[1] if len(sys.argv) > 1 else 'report']())

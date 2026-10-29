"""0.0.73 COMING SOON portraits (the owner: "Let's add the profile pictures but put them in a 'coming soon'
section in the roster page"): the 21 SOON tiles' pictures, made once from the 0.0.74 picks and kept in the
repo (tools/roster/soon/<key>.jpg, 400 px squares, and soon.json, each one's credit) so make_roster_glb.py
needs nothing else:

  python3 tools/roster/soon_portraits.py <soon folder>

<soon folder> holds soon_meta.json (each pick's Sketchfab uid, name, artist, licence, url), thumbs/<uid>.jpg
(the model's own Sketchfab thumbnail, 1024 x 576) and flash.png / hawkman.png (Blender renders of the
kanimator collection's models, transparent, 512 px). A head-and-shoulders square is cut from each thumbnail
(clamped to the image: no black bar). Never a Marvel's Avengers (2020) rip. BATMAN and SUPERMAN are playable
already (their new suits are a line on the SOON tab, not tiles); the two CYBORGs are one tile."""
import json, os, sys

KEYS = ['deadpool', 'nightcrawler', 'moonknight', 'magneto', 'cyclops', 'jeangrey', 'vision', 'nickfury',
        'wintersoldier', 'doctorstrange', 'mrfantastic', 'ghostrider', 'humantorch', 'silversurfer', 'thething',
        'cyborg', 'flash', 'hawkman', 'robin', 'nightwing', 'raven']  # roster_page.hpp soon_* order
# (soon_meta key, crop centre x, centre y, square size) in the 1024 x 576 thumbnail.
CROPS = {
    'deadpool': ('deadpool_4', 512, 95, 220), 'nightcrawler': ('nightcrawler_3', 505, 95, 200),
    'moonknight': ('moonknight_2', 512, 190, 380), 'magneto': ('magneto_2', 512, 160, 320),
    'cyclops': ('cyclops_2', 512, 95, 200), 'jeangrey': ('jeangrey_2', 505, 120, 230),
    'vision': ('vision_1', 512, 95, 190), 'nickfury': ('nickfury_3', 512, 230, 420),
    'wintersoldier': ('wintersoldier_1', 512, 140, 320), 'doctorstrange': ('doctorstrange_2', 515, 100, 200),
    'mrfantastic': ('mrfantastic_1', 512, 90, 210), 'ghostrider': ('ghostrider_2', 382, 160, 170),
    'humantorch': ('humantorch_2', 512, 90, 200), 'silversurfer': ('silversurfer_6', 420, 110, 200),
    'thething': ('thething_2', 512, 140, 320), 'robin': ('robin_2', 539, 160, 130),
    'nightwing': ('nightwing_3', 512, 150, 330), 'raven': ('raven_1', 504, 115, 200),
    'cyborg': ('cyborg_1', 512, 115, 190),
}
KANIMATOR = ('Injustice 2 | DCEU | 3D Model Collection Rigged', 'kanimator', 'CC Attribution',
             'https://sketchfab.com/3d-models/e27284b707ec42aab42416ed39911d28')
RENDERS = {'flash': 'flash.png', 'hawkman': 'hawkman.png'}  # rendered from the models (lineup.py)
SIZE = 400


def crop_box(x, y, n, w, h):
    n = min(n, w, h)
    x0 = min(max(x - n // 2, 0), w - n)
    y0 = min(max(y - n // 2, 0), h - n)
    return (x0, y0, x0 + n, y0 + n)


def main(src, out):
    from PIL import Image
    meta = json.load(open(os.path.join(src, 'soon_meta.json')))
    os.makedirs(out, exist_ok=True)
    credits = {}
    for key in KEYS:
        if key in RENDERS:
            fg = Image.open(os.path.join(src, RENDERS[key])).convert('RGBA')
            im = Image.new('RGBA', fg.size, (18, 22, 32, 255))
            im.alpha_composite(fg)
            im = im.convert('RGB')
            name, user, lic, url = KANIMATOR
            credits[key] = dict(name=name, user=user, lic=lic, url=url, made='rendered from the model (Blender)')
        else:
            mk, x, y, n = CROPS[key]
            m = meta[mk]
            im = Image.open(os.path.join(src, 'thumbs', m['uid'] + '.jpg')).convert('RGB')
            im = im.crop(crop_box(x, y, n, *im.size))
            credits[key] = dict(name=m['name'], user=m['user'], lic=m['lic'], url=m['url'],
                                made='cropped from the Sketchfab thumbnail')
        im.resize((SIZE, SIZE), Image.LANCZOS).save(os.path.join(out, key + '.jpg'), quality=90)
    json.dump(credits, open(os.path.join(out, 'soon.json'), 'w'), indent=1)
    print('soon portraits: %d' % len(KEYS))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'soon'))

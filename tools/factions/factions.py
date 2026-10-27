"""Version 0.0.72 ENEMY FACTIONS kit (box side; the game-side swap is CANDIDATE, see docs/HERO-0.0.72.md).

The models (the user's picks, 8:49). None is in git: they are read from the Sketchfab downloads.
  THE HAND  - a mix of three, the OUTFIT texture recoloured crimson (skin, eyes and metal kept):
      hand1  'Ninja - Walking' by Bugawuga, CC BY 4.0, rigged, ~51k tris (as is)
      hand2  'Ninja - low poly - real life based' by RITIKA_87, CC BY 4.0, NOT rigged, ~100k
             (decimated to ~45k; weighted from hand1's rig by tools/cap_suit/blender/transfer_weights.py)
      hand3  'Saboteur Ninja Animated' by klava88, CC BY 4.0, Mixamo rig, ~42k (as is)
  HYDRA     - a mix of two by jeandiz (SpatialNeglect), CC BY 4.0, rigged, ~110k each, decimated to
             ~45k for crowds, with OUR OWN skull-and-tentacles emblem (emblem() below, drawn here,
             nothing ripped) on the chest and the left shoulder:
      hydra1 'S.W.A.T. Operator'   hydra2 'FSB Operator'
             (their kitbash parts by ErhanMatur, Bzovius, SrGeneroso, shedmon, Albin and Mayess;
             the weapons, M4 by bobeer and AKS-74u by N-GONE, are removed: the game's own are used)
No Marvel credit: only the artists.

  factions.py emblem <out.png>                       our emblem (512 px, transparent)
  factions.py recolor <outfit texture> <out> [--mask-skin]  the crimson outfit
  factions.py credits <out.txt>"""
import colorsys, math, sys

MODELS = {
    'hand1': dict(faction='THE HAND', title='Ninja - Walking', artist='Bugawuga', licence='CC BY 4.0',
                  url='https://sketchfab.com/3d-models/ninja-walking-0268036dde274e6ba9ded5bdf8a082c1', rigged=True,
                  tris=51419, target=51419),
    'hand2': dict(faction='THE HAND', title='Ninja - low poly - real life based', artist='RITIKA_87', licence='CC BY 4.0',
                  url='https://sketchfab.com/3d-models/ninja-low-poly-real-life-based-inspired-22ccb0bf2b6d4f8286e0dd4edb5bce63',
                  rigged=False, tris=100000, target=45000),
    'hand3': dict(faction='THE HAND', title='Saboteur Ninja Animated', artist='klava88', licence='CC BY 4.0',
                  url='https://sketchfab.com/3d-models/saboteur-ninja-animated-3cbc15c178f943a1b4511b973f27a033',
                  rigged=True, tris=42000, target=42000),
    'hydra1': dict(faction='HYDRA', title='S.W.A.T. Operator', artist='jeandiz (SpatialNeglect)', licence='CC BY 4.0',
                   url='https://sketchfab.com/3d-models/swat-operator-9e82fabf26194896b5ad4a364d864eab', rigged=True,
                   tris=106714, target=45000),
    'hydra2': dict(faction='HYDRA', title='FSB Operator', artist='jeandiz (SpatialNeglect)', licence='CC BY 4.0',
                   url='https://sketchfab.com/3d-models/fsb-operator-43a561e941704eefb1ab0614be4f0049', rigged=True,
                   tris=114273, target=45000),
}
KITBASH = 'ErhanMatur, Bzovius, SrGeneroso, shedmon, Albin and Mayess'
CRIMSON_HUE = 350 / 360.0
# The spawn mix: each faction's variants in turn (the game side picks variant = spawn index % count).
MIX = {'THE HAND': ['hand1', 'hand2', 'hand3'], 'HYDRA': ['hydra1', 'hydra2']}


def variant(faction, spawn_index):
    v = MIX[faction]
    return v[spawn_index % len(v)]


def credits():
    lines = ['HERO VR - Made by Agent Mulder. Enemy faction models (not by us):']
    for k, m in MODELS.items():
        lines.append('- "%s" (%s) by %s, licensed under %s.%s' % (
            m['title'], m['url'], m['artist'], m['licence'],
            ' Outfit recoloured crimson.' if m['faction'] == 'THE HAND' else ' Decimated, weapons removed, our own emblem added.'))
    lines.append('- The S.W.A.T. and FSB operators are kitbashed from parts shared by %s (credited on jeandiz\'s pages).' % KITBASH)
    return '\n'.join(lines) + '\n'


def is_skin(r, g, b):
    """Skin-like pixels (kept): warm hue, middling saturation, not dark."""
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return (h <= 50 / 360.0 or h >= 345 / 360.0) and .15 <= s <= .65 and v >= .35 and r > g > b


def is_metal(r, g, b):
    """Bright, near grey pixels (blades, buckles; kept)."""
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return s < .12 and v > .7


def crimson(r, g, b):
    """The outfit pixel crimson: its lightness kept (folds and shading stay), the hue and saturation set."""
    h, l, s = colorsys.rgb_to_hls(r / 255.0, g / 255.0, b / 255.0)
    l2 = min(.55, .12 + l * .9)
    rr, gg, bb = colorsys.hls_to_rgb(CRIMSON_HUE, l2, .78)
    return int(rr * 255 + .5), int(gg * 255 + .5), int(bb * 255 + .5)


def recolor(img, mask_skin=True):
    from PIL import Image
    img = img.convert('RGBA')
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if mask_skin and (is_skin(r, g, b) or is_metal(r, g, b)):
                continue
            px[x, y] = crimson(r, g, b) + (a,)
    return img


def emblem(size=512):
    """Our own emblem: a skull with six curling tentacles (red on transparent). Drawn from shapes."""
    from PIL import Image, ImageDraw
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    red, dark = (200, 20, 20, 255), (0, 0, 0, 0)
    c, s = size / 2, size / 512.0
    for k in range(6):  # tentacles: each a chain of shrinking circles curling outward
        base = math.radians(200 + k * 28)
        for i in range(90):
            t = i / 89.0
            ang = base + math.sin(t * math.pi) * .9 * (1 if k % 2 else -1)
            rad = (120 + t * 120) * s
            x, y = c + math.cos(ang) * rad, c + 40 * s - math.sin(ang) * rad * -1
            w = (22 - t * 16) * s
            d.ellipse([x - w, y - w, x + w, y + w], fill=red)
    d.ellipse([c - 105 * s, c - 130 * s, c + 105 * s, c + 70 * s], fill=red)  # the cranium
    d.rectangle([c - 60 * s, c + 40 * s, c + 60 * s, c + 110 * s], fill=red)  # the jaw
    for ex in (-45, 45):  # the eyes
        d.ellipse([c + (ex - 30) * s, c - 50 * s, c + (ex + 30) * s, c + 10 * s], fill=dark)
    d.polygon([(c, c + 15 * s), (c - 14 * s, c + 40 * s), (c + 14 * s, c + 40 * s)], fill=dark)  # the nose
    for tx in range(-45, 50, 30):  # the teeth gaps
        d.rectangle([c + (tx - 3) * s, c + 70 * s, c + (tx + 3) * s, c + 110 * s], fill=dark)
    return img


def main(argv):
    if len(argv) >= 2 and argv[0] == 'emblem':
        emblem().save(argv[1])
    elif len(argv) >= 3 and argv[0] == 'recolor':
        from PIL import Image
        recolor(Image.open(argv[1]), '--no-mask' not in argv).save(argv[2])
    elif len(argv) >= 2 and argv[0] == 'credits':
        open(argv[1], 'w').write(credits())
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

"""Version 0.0.72 ENEMY FACTIONS kit: credits, the mix, the crimson recolour keeps skin and metal, our emblem."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'factions'))
import factions  # noqa: E402

fails = []
def check(ok, what):
    if not ok:
        fails.append(what)

c = factions.credits()
for who in ('Bugawuga', 'RITIKA_87', 'klava88', 'jeandiz', 'ErhanMatur', 'Mayess', 'Agent Mulder'):
    check(who in c, 'credit missing: ' + who)
check('Marvel' not in c and 'Disney' not in c, 'a Marvel credit')
check([factions.variant('THE HAND', i) for i in range(4)] == ['hand1', 'hand2', 'hand3', 'hand1'], 'THE HAND mix')
check({factions.variant('HYDRA', i) for i in range(4)} == {'hydra1', 'hydra2'}, 'HYDRA mix')
check(all(m['target'] <= 52000 for m in factions.MODELS.values()), 'a crowd model above ~50k')
check(factions.is_skin(224, 172, 140) and not factions.is_skin(30, 30, 35), 'skin test')
r, g, b = factions.crimson(40, 40, 45)
check(r > 2 * g and r > 2 * b, 'not crimson')
try:
    from PIL import Image
    img = Image.new('RGB', (4, 1))
    img.putpixel((0, 0), (30, 30, 35))     # black cloth -> crimson
    img.putpixel((1, 0), (224, 172, 140))  # skin kept
    img.putpixel((2, 0), (220, 220, 225))  # metal kept
    img.putpixel((3, 0), (60, 70, 90))     # blue cloth -> crimson
    out = factions.recolor(img)
    check(out.getpixel((1, 0))[:3] == (224, 172, 140) and out.getpixel((2, 0))[:3] == (220, 220, 225), 'skin or metal recoloured')
    for x in (0, 3):
        p = out.getpixel((x, 0))
        check(p[0] > 2 * p[1], 'cloth not recoloured')
    e = factions.emblem(256)
    alpha = [p[3] for p in e.getdata()]
    filled = sum(1 for a in alpha if a) / float(len(alpha))
    check(.1 < filled < .7 and e.getpixel((0, 0))[3] == 0, 'the emblem is empty or a full square')
except ImportError:
    print('SKIP: no PIL')
if fails:
    print('FAIL', fails)
    sys.exit(1)
print('FACTION TESTS PASS')

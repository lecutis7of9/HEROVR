"""HERO VR 0.0.78 PBR MATERIALS: pbr_preview's .ppm pairs as labelled before / after sheets (OFFLINE)."""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

LIGHTS = {'noon': 'sample noon sun: toSun (.45,.75,.5) sun (1.45,1.38,1.25) sky (.40,.46,.58)/(.16,.15,.13)',
          'low': 'sample low sun: toSun (-.7,.3,.45) sun (1.25,.85,.55) sky (.22,.25,.34)/(.09,.08,.07)'}


def font(size):
    for f in ('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', '/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf'):
        if pathlib.Path(f).exists():
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()


def sheet(out, view, light):
    a = Image.open(out / f'{view}_{light}_before.ppm')
    b = Image.open(out / f'{view}_{light}_after.ppm')
    w, h = a.size
    s = Image.new('RGB', (w * 2 + 12, h + 96), (20, 20, 22))
    s.paste(a, (0, 64))
    s.paste(b, (w + 12, 64))
    d = ImageDraw.Draw(s)
    d.text((10, 8), 'OFFLINE PREVIEW (software render of the shader math, sample lights; not an in-game capture)',
           font=font(15), fill=(255, 210, 90))
    d.text((10, 32), LIGHTS[light], font=font(13), fill=(200, 200, 200))
    d.text((10, 70), 'BEFORE: cut10 plain path (baked cavity, 1024)', font=font(15), fill=(255, 255, 255))
    d.text((w + 22, 70), 'AFTER: 0.0.78 PBR (normal + ORM + GGX + sky, HQ 2048)', font=font(15), fill=(255, 255, 255))
    d.text((10, h + 70), CAPTION, font=font(13), fill=(170, 170, 170))
    s.save(out / f'punisher_{view}_{light}_offline.png')


CAPTION = 'Punisher Suit_punisher_1, 80k triangles'

if __name__ == '__main__':
    o = pathlib.Path(sys.argv[1])
    CAPTION = sys.argv[2] if len(sys.argv) > 2 else CAPTION  # e.g. 'Punisher078/Pistol, 17k triangles'
    for v in ('front', 'three', 'chest'):
        for li in LIGHTS:
            sheet(o, v, li)

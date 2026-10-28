"""HERO VR 0.0.73 test 2 (fix 20): the desktop shortcut's own icon, HERO-VR.ico - a navy disc, a gold ring and
HERO over VR in Barlow (OFL, tools/launcher/assets/hero). Drawn here, no outside art. Usage: make_hero_icon.py <out.ico>"""
import os, sys
from PIL import Image, ImageDraw, ImageFont
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONT = os.path.join(ROOT, 'tools', 'launcher', 'assets', 'hero', 'Barlow-Medium.ttf')
S = 1024
im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
gold, navy, red = (233, 186, 64, 255), (18, 24, 44, 255), (200, 28, 40, 255)
d.ellipse((24, 24, S - 24, S - 24), fill=gold)
d.ellipse((76, 76, S - 76, S - 76), fill=navy)
d.ellipse((112, 112, S - 112, S - 112), outline=(233, 186, 64, 120), width=10)
def centred(text, size, y, fill):
    f = ImageFont.truetype(FONT, size)
    l, t, r, b = d.textbbox((0, 0), text, font=f)
    d.text(((S - (r - l)) / 2 - l, y - (b - t) / 2 - t), text, font=f, fill=fill)
centred('HERO', 250, S * .42, (255, 255, 255, 255))
d.rounded_rectangle((S * .30, S * .58, S * .70, S * .78), radius=40, fill=red)
centred('VR', 190, S * .68, (255, 255, 255, 255))
sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
im.resize((256, 256), Image.LANCZOS).save(sys.argv[1], sizes=sizes)

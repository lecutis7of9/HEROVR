import subprocess, os
from PIL import Image, ImageDraw, ImageFont
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'images')
scenes = {'day-wall': 'day-wall.png', 'day-street': 'day-street.png', 'night-street': 'night-street.png', 'night-city': 'night-city.png'}
levels = [('before', None), ('low', 20), ('medium', 60), ('high', 100)]
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 20)
except OSError:
    font = ImageFont.load_default()
tw, th = 480, 270
sheet = Image.new('RGB', (tw * 4, (th + 30) * len(scenes)), 'white')
paths = []
for si, (name, src) in enumerate(scenes.items()):
    base = Image.open(src).convert('RGB')
    base.save('/tmp/p385/in.ppm')
    for li, (label, k) in enumerate(levels):
        if k is None:
            img = base
        else:
            subprocess.run(['./toon', '/tmp/p385/in.ppm', '/tmp/p385/out.ppm', '100', str(k), '1', '4'], check=True)
            img = Image.open('/tmp/p385/out.ppm').convert('RGB')
        path = f'{out}/color-90s-{name}-{label}.png'
        img.resize((1280, 720), Image.LANCZOS).save(path, optimize=True)
        paths.append(path)
        x, y = li * tw, si * (th + 30)
        sheet.paste(img.resize((tw, th), Image.LANCZOS), (x, y + 30))
        title = f'{name}: ' + ('game' if k is None else f'CARTOON {k}')
        ImageDraw.Draw(sheet).text((x + 6, y + 4), title, fill=(0, 0, 0), font=font)
sheet.save(f'{out}/color-90s-sheet.png', optimize=True)
# BANDS and OUTLINES variants on the day wall (medium)
Image.open('day-wall.png').convert('RGB').save('/tmp/p385/in.ppm')
var = Image.new('RGB', (tw * 3, th + 30), 'white')
for i, (label, o, b) in enumerate([('BANDS 3', 1, 3), ('BANDS 5', 1, 5), ('OUTLINES OFF', 0, 4)]):
    subprocess.run(['./toon', '/tmp/p385/in.ppm', '/tmp/p385/out.ppm', '100', '60', str(o), str(b)], check=True)
    var.paste(Image.open('/tmp/p385/out.ppm').convert('RGB').resize((tw, th), Image.LANCZOS), (i * tw, 30))
    ImageDraw.Draw(var).text((i * tw + 6, 4), label, fill=(0, 0, 0), font=font)
var.save(f'{out}/color-90s-options.png', optimize=True)
print('\n'.join(paths))

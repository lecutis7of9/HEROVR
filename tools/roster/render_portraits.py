"""Version 0.0.72 HERO ROSTER portraits (the user, 8:39): a thumbnail per hero rendered in Blender from
OUR actual hero models (the suits the mod ships / the user installs; never a ripped image).

  blender -b -P render_portraits.py -- <models folder> <out folder> [--size 256]

<models folder>/<hero key>/*.glb (or .gltf / .fbx); hero keys: HEROES below. Each found model gets a
head-and-shoulders shot (camera framed on the top 30 % of its bounds, three-point light, transparent
background) saved as <out>/<key>.png. A hero with no model gets tile() instead: his colour and name
(what this test build shows; no Blender and no hero suit models were on the build box)."""
import os, sys

HEROES = [('spider_man', 'SPIDER-MAN', (200, 30, 40)), ('iron_man', 'IRON MAN', (170, 30, 30)),
          ('daredevil', 'DAREDEVIL', (130, 10, 20)), ('hulk', 'HULK', (60, 140, 50)),
          ('batman', 'BATMAN', (50, 50, 60)), ('wolverine', 'WOLVERINE', (220, 190, 40)),
          ('superman', 'SUPERMAN', (30, 70, 190)), ('punisher', 'PUNISHER', (30, 30, 30)),
          ('captain_america', 'CAPTAIN AMERICA', (40, 70, 160)), ('thor', 'THOR', (120, 130, 150)),
          ('green_lantern', 'GREEN LANTERN', (30, 170, 70))]


def tile(name, rgb, size=256):
    """The fallback tile: the hero's colour, a lighter band and his name."""
    from PIL import Image, ImageDraw
    img = Image.new('RGBA', (size, size), rgb + (255,))
    d = ImageDraw.Draw(img)
    light = tuple(min(255, c + 60) for c in rgb) + (255,)
    d.rectangle([0, int(size * .72), size, size], fill=light)
    w = d.textlength(name)
    d.text(((size - w) / 2, size * .82), name, fill=(255, 255, 255, 255))
    return img


def render(model, out_png, size):
    import bpy
    from mathutils import Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ext = model.lower().rsplit('.', 1)[-1]
    (bpy.ops.import_scene.fbx if ext == 'fbx' else bpy.ops.import_scene.gltf)(filepath=model)
    pts = [o.matrix_world @ Vector(c) for o in bpy.data.objects if o.type == 'MESH' for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    h = hi.z - lo.z
    target = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, hi.z - h * .15))
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    bpy.context.scene.collection.objects.link(cam)
    cam.location = target + Vector((0, -h * .9, h * .02))
    cam.rotation_euler = (1.5708, 0, 0)
    cam.data.lens = 85
    bpy.context.scene.camera = cam
    for loc, e in (((-1, -1.2, 1.2), 600), ((1.2, -1, .6), 250), ((0, 1.5, 1.4), 400)):
        L = bpy.data.objects.new('l', bpy.data.lights.new('l', 'AREA'))
        L.data.energy = e * h
        L.location = target + Vector(loc) * h
        L.rotation_euler = (target - L.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.collection.objects.link(L)
    r = bpy.context.scene.render
    r.engine, r.film_transparent = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, 'EEVEE_NEXT') else 'BLENDER_EEVEE', True
    r.resolution_x = r.resolution_y = size
    r.filepath = out_png
    bpy.ops.render.render(write_still=True)


def main(models, out, size=256):
    os.makedirs(out, exist_ok=True)
    made = []
    for key, name, rgb in HEROES:
        folder = os.path.join(models, key)
        found = None
        if os.path.isdir(folder):
            found = next((os.path.join(folder, f) for f in sorted(os.listdir(folder))
                          if f.lower().endswith(('.glb', '.gltf', '.fbx'))), None)
        png = os.path.join(out, key + '.png')
        if found:
            try:
                render(found, png, size)
                made.append((key, 'rendered'))
                continue
            except ImportError:
                pass
        tile(name, rgb, size).save(png)
        made.append((key, 'tile'))
    return made


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    if len(a) < 2:
        print(__doc__)
        sys.exit(2)
    print(main(a[0], a[1], int(a[a.index('--size') + 1]) if '--size' in a else 256))

#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION - build the UA rooms from YOUR OWN Marvel: Ultimate Alliance (PS2) disc.

HERO VR never ships Ultimate Alliance's maps (bring your own game): this script reads your disc image and
writes the converted room into your HERO VR install. Nothing is sent anywhere; the disc image is only read.

  python tools/ua/convert_ua.py --iso "D:/Games/Marvel - Ultimate Alliance (USA).iso"
                                --blender "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe"
                                --igb-blender "D:/tools/igb-blender"
                                --out "<HERO VR folder>/Models/UA/StarkTower"
                                [--work <scratch dir>] [--previews]

Needs: Python 3.10+ with numpy, pillow, pycdlib and raven-formats (pip install numpy pillow pycdlib raven-formats);
Blender 4.4; Kaiko's
igb-blender add-on (github.com/KaikoClanworth1/igb-blender, the folder holding its __init__.py). --wad instead
of --iso takes an already copied Z/ASSETSFB.WAD.

Writes into --out (exactly what HERO VR reads, Models/UA/StarkTower):
  scene.glb  the hub drawn in VR (baked look, raised skyline, a ceiling from the hub's own textures)
  room.txt   the walk grid, the spawn and the RETURN TO CITY point (tools/hideout074/make_hideout.py)
  room.json  the room's description (kept for reference)
  npcs.txt   where the hub's characters stand (the next update's hooks; read, not drawn yet)
  report.txt what the converter did
and, with --previews, walk_overlay.png (the walk grid over the floor plan) and eye-level renders.
Then start the game with ua_locations on (the default) and open the wrist's ULTIMATE ALLIANCE EXPANSION page.
"""
import argparse, os, shutil, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'hideout074'))

MAP = 'stark1'  # MUA1 Act 1: the Stark Tower hub
IGB = 'stark1/__combined.igb'
ENGB = 'stark1/maps/act1/stark/stark1.engb.xml'


def run(cmd, env=None):
    print('>', ' '.join(str(c) for c in cmd), flush=True)
    r = subprocess.run([str(c) for c in cmd], env=env)
    if r.returncode != 0:
        raise SystemExit(f'failed ({r.returncode}): {cmd[0]}')


def blender_env(addon_dir, scratch):
    """A user-scripts folder holding only the add-on (copied: no symlink rights needed on Windows)."""
    root = Path(scratch) / 'blender_scripts'
    dst = root / 'addons' / 'igb_blender'
    if not dst.exists():
        shutil.copytree(addon_dir, dst, ignore=shutil.ignore_patterns('.git', '__pycache__'))
    env = dict(os.environ)
    env['BLENDER_USER_SCRIPTS'] = str(root)
    return env


def overlay(plan_png, walk_png, out_png):
    import numpy as np
    from PIL import Image
    plan = np.asarray(Image.open(plan_png).convert('RGB')).astype(float)
    walk = np.asarray(Image.open(walk_png).convert('RGB')).astype(float)
    h, w = min(plan.shape[0], walk.shape[0]), min(plan.shape[1], walk.shape[1])
    plan, walk = plan[:h, :w], walk[:h, :w]
    mask = (walk.sum(2) > 0)[..., None]
    out = np.where(mask, plan * .55 + walk * .45, plan * .5)
    Image.fromarray(out.clip(0, 255).astype('uint8')).save(out_png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--iso', help='your MUA1 PS2 disc image (.iso)')
    src.add_argument('--wad', help='or its Z/ASSETSFB.WAD, already copied out')
    ap.add_argument('--blender', required=True, help='blender executable (4.4)')
    ap.add_argument('--igb-blender', required=True, help="the igb-blender add-on's folder (its __init__.py inside)")
    ap.add_argument('--out', required=True, help='<HERO VR>/Models/UA/StarkTower')
    ap.add_argument('--work', help='scratch folder (kept; default: a temp folder, removed after)')
    ap.add_argument('--previews', action='store_true', help='also render walk_overlay.png and eye-level views into --out')
    ap.add_argument('--scale', type=float, default=0.022, help='metres per game unit (default 0.022)')
    a = ap.parse_args()

    if not (Path(a.igb_blender) / '__init__.py').exists():
        raise SystemExit(f'--igb-blender: no __init__.py in {a.igb_blender}')
    work = Path(a.work) if a.work else Path(tempfile.mkdtemp(prefix='hv_ua_'))
    work.mkdir(parents=True, exist_ok=True)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    try:
        import ua_extract
        wad_path = ua_extract.resolve_wad(a.iso or a.wad, work)
        wad = ua_extract.Wad(wad_path)
        hits = [n for n in wad.names if n.lower().endswith('.fb') and Path(n).stem.lower() == MAP]
        if not hits:
            raise SystemExit(f'no {MAP}.fb in {wad_path} - is this Marvel: Ultimate Alliance for PS2?')
        pkg = work / 'pkg'
        for n in hits:
            ua_extract.fb_unpack(wad.read(n), pkg / Path(n).stem)
        ua_extract.decompile_xmlb(pkg / MAP)
        igb, engb = pkg / IGB, pkg / ENGB
        if not igb.exists() or not engb.exists():
            raise SystemExit(f'{MAP}: {igb.name} or {engb.name} missing after unpacking')

        env = blender_env(a.igb_blender, work)
        built = work / 'built'
        run([a.blender, '-b', '--factory-startup', '--python', HERE / 'build_stark_tower.py', '--',
             igb, engb, built, '--scale', a.scale], env)

        import make_hideout
        room = work / 'room'
        make_hideout.build('stark_tower', str(built / 'walk.glb'), str(built / 'room.json'), 'mesh', str(room))
        # the walk grid came from walk.glb (no backdrop, no ceiling, the glass kept as walls); the drawn room is scene.glb
        make_hideout.strip_lights(str(built / 'scene.glb'), str(out / 'scene.glb'))
        for name in ('room.txt', 'room.json'):
            shutil.copyfile(room / name, out / name)
        for name in ('npcs.txt', 'report.txt'):
            shutil.copyfile(built / name, out / name)

        if a.previews:
            grid = next(l for l in (room / 'room.txt').read_text().splitlines() if l.startswith('grid '))
            x0, z0, cell, cols, rows = grid.split()[1:]
            x0, z0, cell, cols, rows = float(x0), float(z0), float(cell), int(cols), int(rows)
            x1, z1 = x0 + cell * cols, z0 + cell * rows
            run([a.blender, '-b', '--factory-startup', '--python', HERE / 'render_room_preview.py', '--',
                 built / 'walk.glb', str(work) + os.sep, f'plan:{x0},{z0},{x1},{z1}:2.4:{4 / cell}'])
            overlay(work / 'plan.png', room / 'walk.png', out / 'walk_overlay.png')
            run([a.blender, '-b', '--factory-startup', '--python', HERE / 'render_room_preview.py', '--',
                 out / 'scene.glb', str(out) + os.sep, 'eye_window:-20,1.7,0:180,4', 'eye_spawn:0,1.7,0:-90,2',
                 'look_up:0,1.7,0:-90,60:14'])
        print(f'\nSTARK TOWER ready in {out}')
        print('Start HERO VR with ua_locations on (the default), then: wrist -> ULTIMATE ALLIANCE EXPANSION -> STARK TOWER.')
    finally:
        if not a.work:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()

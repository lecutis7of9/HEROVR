#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION - build the UA rooms from YOUR OWN Marvel: Ultimate Alliance (PS2) disc.

HERO VR never ships Ultimate Alliance's maps (bring your own game): this script reads your disc image and
writes the converted room into your HERO VR install. Nothing is sent anywhere; the disc image is only read.

  python tools/ua/convert_ua.py --iso "D:/Games/Marvel - Ultimate Alliance (USA).iso"
                                --blender "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe"
                                --igb-blender "D:/tools/igb-blender"
                                --out "<HERO VR folder>/Models/UA/StarkTower"
                                [--work <scratch dir>] [--previews]

--no-cast skips the characters, their lines and voices; --no-body the player's body (Models/UA/Body).

Needs: Python 3.10+ with numpy, pillow, pycdlib and raven-formats (pip install numpy pillow pycdlib raven-formats);
Blender 4.4; Kaiko's
igb-blender add-on (github.com/KaikoClanworth1/igb-blender, the folder holding its __init__.py). --wad instead
of --iso takes an already copied Z/ASSETSFB.WAD.

Writes into --out (exactly what HERO VR reads, Models/UA/StarkTower):
  scene.glb  the hub drawn in VR (baked look, raised skyline, a ceiling from the hub's own textures)
  room.txt   the walk grid, the spawn and the RETURN TO CITY point (tools/hideout074/make_hideout.py)
  room.json  the room's description (kept for reference)
  npcs.txt   every character spawner of the map (room space)
  Cast/scene.glb  the hub's characters (Nick Fury, Black Widow, Hank Pym, the Vision, Wyatt Wingfoot and the
             office workers) in their own skins, each posed through the map's own idle performance (EA_ZONE*,
             a few frames of its loop), one texture atlas
  cast.txt   who stands where, in which look, whom you can talk to (tools/ua/ua_cast.py)
  talk.txt   their conversations from the disc: every line, reply and choice, start conditions, unlocks
  Voice/     the lines' own voice clips (v0000.wav ...) from the hub's sound bank (SOUNDS/T/O/TOWN1_V.ENS;
             with --wad and no --sounds: subtitles only)
  report.txt what the converter did
and beside it, Models/UA/Body/scene.glb: the player's own body in UA places - MUA1's Spider-Man (0901, from
packages/generated/characters/spiderman_0901.fb) as a HERO VR suit (16 bones, tools/ua/ua_body.py), worn when
no roster suit is (include/spidy/ua_body.hpp)
and, with --previews, walk_overlay.png (the walk grid over the floor plan), eye-level renders, cast_hub.png (the
hub with its characters) and cast_closeup.png (one character with a subtitle mock).
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


CAST_CAP = 140000  # triangles in Cast/scene.glb: mesh_model::uaCastTriangles (frames dropped to fit)
HUB_BANK = 'TOWN1_V.ENS'  # Act 1's hub voices (SOUNDS/T/O): every stark1 conversation line


def closeup_subtitle(out):
    """cast_closeup.png: the close-up with the opening line he would say to Spider-Man, as a subtitle mock."""
    import json
    import ua_cast
    info = json.loads((out / 'cast_preview.json').read_text())
    speakers, convs = ua_cast.read_talk_txt(out / 'talk.txt')
    conv = convs.get(info['talk'])
    speaker, text = info['label'], ''
    if conv:
        _start, line = ua_cast.opening(conv)
        if line in conv['lines']:
            tok, _v, _s, text = conv['lines'][line]
            speaker = speakers.get(tok, speaker) or speaker
    ua_cast.subtitle_mock(out / 'cast_raw_closeup.png', out / 'cast_closeup.png', speaker, text or '...')
    (out / 'cast_raw_closeup.png').unlink()


def build_cast(a, pkg, work, out, env):
    """The characters (Blender: build_cast.py), their atlas model, cast.txt, talk.txt and the voice clips."""
    import json
    import ua_cast
    cast = ua_cast.plan_cast(pkg, 'maps/act1/stark/stark1', a.scale)
    looks = ua_cast.looks_of(cast)
    plan = {'scale': a.scale, 'looks': [{'name': l['name'], 'skin_igb': str(pkg / 'actors' / f"{l['skin']}.igb"),
                                         'anim_igb': str(pkg / 'actors' / 'zone_stark1.igb'), 'anim': l['anim'],
                                         'frames': 10 if l['principal'] else 6} for l in looks]}
    (work / 'cast_plan.json').write_text(json.dumps(plan, indent=1))
    built = work / 'cast'
    run([a.blender, '-b', '--factory-startup', '--python', HERE / 'build_cast.py', '--', work / 'cast_plan.json', built], env)
    looks = [ua_cast.load_built(l, built / f"{l['name']}.npz") for l in looks]
    lines, tris, sheets = ua_cast.pack_cast(looks, out / 'Cast', CAST_CAP)
    ua_cast.write_cast(out / 'cast.txt', lines, cast)
    print(f'cast: {len(cast)} characters, {len(looks)} looks, {tris} triangles, {sheets} atlas page(s)')
    talks = ua_cast.read_talks(pkg, sorted({c['talk'] for c in cast if c['talk']}))
    bank = {}
    ens = Path(a.sounds) if a.sounds else None
    if ens is None and a.iso:
        import ua_extract
        ens = ua_extract.iso_extract_file(a.iso, HUB_BANK, work / 'sounds')
    if ens and Path(ens).exists():
        bank = ua_cast.voice_bank(ens, work / 'bank')
    else:
        print('no hub sound bank: the characters talk in subtitles only (give --sounds TOWN1_V.ENS)')
    index, durations = ua_cast.write_voices(out / 'Voice', talks, bank)
    stats = ua_cast.read_npcstat(pkg / 'data' / 'npcstat.engb.xml')
    ua_cast.write_talk(out / 'talk.txt', talks, ua_cast.speaker_names(talks, stats), index, durations)
    n = sum(len(t['lines']) for t in talks.values())
    print(f'talk: {len(talks)} conversations, {n} lines, {len(index)} voice clips ({sum(durations.values()):.0f} s)')
    with open(out / 'report.txt', 'a') as f:
        f.write((built / 'cast_report.txt').read_text() if (built / 'cast_report.txt').exists() else '')
        f.write(f'cast {len(cast)} characters, {tris} triangles, {sheets} atlas pages; talk {len(talks)} '
                f'conversations, {n} lines, {len(index)} voice clips\n')


def build_body(a, wad, work, body_out, env):
    """The player's body in UA places (include/spidy/ua_body.hpp): MUA1's own Spider-Man (0901) from the disc, made
    into a HERO VR suit at Models/UA/Body (Blender: build_body.py, then ua_body.make_suit)."""
    import ua_body
    import ua_extract
    want = ua_body.BODY_PACKAGE.lower()
    hits = [n for n in wad.names if n.replace('\\', '/').lower().endswith(want)]
    if not hits:
        print(f'no {ua_body.BODY_PACKAGE} on this disc: no body in UA places (the gloves instead)')
        return None
    pkg = work / 'pkgbody'
    ua_extract.fb_unpack(wad.read(hits[0]), pkg / 'body')
    actors = pkg / 'body' / 'actors'
    skin, anim = actors / f'{ua_body.BODY_SKIN}.igb', actors / ua_body.BODY_ANIM
    if not skin.exists() or not anim.exists():
        print(f'{ua_body.BODY_PACKAGE}: {skin.name} or {anim.name} missing: no body in UA places')
        return None
    npz = work / 'body.npz'
    run([a.blender, '-b', '--factory-startup', '--python', HERE / 'build_body.py', '--', skin, anim, npz], env)
    notes = ua_body.make_suit(npz, body_out)[0]
    print('body: ' + '; '.join(notes))
    return notes


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
    ap.add_argument('--sounds', help="the disc's hub sound bank (TOWN1_V.ENS) when --wad is used (voices)")
    ap.add_argument('--no-cast', action='store_true', help='leave out the characters and their conversations')
    ap.add_argument('--no-body', action='store_true', help="leave out the player's body (Models/UA/Body)")
    ap.add_argument('--scale', type=float, default=0.0254,
                    help='metres per game unit (default 0.0254: Raven units are inches, a 72-unit hero ~ 1.83 m)')
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
        # fix 3: the solid materials OPAQUE (no stray holes in the furniture; ua_alpha.py)
        import ua_alpha
        solid, cut = ua_alpha.solidify(str(out / 'scene.glb'))
        for name in ('room.txt', 'room.json'):
            shutil.copyfile(room / name, out / name)
        for name in ('npcs.txt', 'report.txt'):
            shutil.copyfile(built / name, out / name)
        with open(out / 'report.txt', 'a') as f:
            f.write(f'materials opaque {len(solid)}, cut-out {len(cut)}: {", ".join(cut)}\n')

        if not a.no_cast:
            build_cast(a, pkg / MAP, work, out, env)

        if not a.no_body:
            notes = build_body(a, wad, work, out.parent / 'Body', env)
            with open(out / 'report.txt', 'a') as f:
                f.write('body ' + ('; '.join(notes) if notes else 'none (gloves in UA places)') + '\n')

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
            if not a.no_cast and (out / 'cast.txt').exists():
                run([a.blender, '-b', '--factory-startup', '--python', HERE / 'render_cast_preview.py', '--',
                     out, str(out) + os.sep], env)
                closeup_subtitle(out)
        print(f'\nSTARK TOWER ready in {out}')
        print('Start HERO VR with ua_locations on (the default), then: wrist -> ULTIMATE ALLIANCE EXPANSION -> STARK TOWER.')
    finally:
        if not a.work:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()

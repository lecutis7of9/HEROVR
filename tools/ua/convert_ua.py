#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION - build the UA rooms from YOUR OWN Marvel: Ultimate Alliance (PS2) disc.

HERO VR never ships Ultimate Alliance's maps (bring your own game): this script reads your disc image and
writes the converted room into your HERO VR install. Nothing is sent anywhere; the disc image is only read.

  python tools/ua/convert_ua.py --iso "D:/Games/Marvel - Ultimate Alliance (USA).iso"
                                --blender "C:/Program Files/Blender Foundation/Blender 4.4/blender.exe"
                                --igb-blender "D:/tools/igb-blender"
                                --out "<HERO VR folder>/Models/UA/StarkTower"
                                [--work <scratch dir>] [--previews]

--no-cast skips the characters, their lines and voices. (v4: no player body is built any more - MUA's Spider-Man
is retired; in a UA place HERO VR shows the player's own SM2 hero, include/spidy/ua_body.hpp.)

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
and, with --previews, walk_overlay.png (the walk grid over the floor plan), eye-level renders, cast_hub.png (the
hub with its characters) and cast_closeup.png (one character with a subtitle mock).
Then start the game with ua_locations on (the default) and open the wrist's ULTIMATE ALLIANCE EXPANSION page.
"""
import argparse, os, shutil, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'hideout074'))

# v5: Stark Tower is TWO converted MUA1 maps in ONE room: the hub (stark1) and Iron Man's Lab (stark2, behind the hub's
# waterfall). The lab stands LAB_X metres east of the hub (each has its own city and sky box, about 120 m round it, so
# the places must be 245 m or more apart; the room's black sphere hides what is past 190 m). `bank`: the sound bank
# (SOUNDS/T/O) with the map's conversation voices; `zone`: the map's own animation file (EA_ZONEn differs per map).
# The Act 5 maps (act5/doomstark/doomstark1a, 1b, 2: Doom's occupation of the tower) are combat levels: not converted.
LAB_X = 260.0
PLACES = [
    {'map': 'stark1', 'name': 'STARK TOWER', 'offset': (0.0, 0.0), 'tag': '', 'zone': 'zone_stark1', 'bank': 'TOWN1_V.ENS'},
    {'map': 'stark2', 'name': "IRON MAN'S LAB", 'offset': (LAB_X, 0.0), 'tag': 'lab_', 'zone': 'zone_stark2', 'bank': 'TOWN1B_V.ENS'},
]
for _p in PLACES:
    _p['engb'] = f"maps/act1/stark/{_p['map']}"
MAP = PLACES[0]['map']


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


CAST_CAP = 260000  # triangles in Cast/scene.glb: mesh_model::uaCastTriangles (frames dropped to fit)
HUB_BANK = PLACES[0]['bank']  # Act 1's hub voices (SOUNDS/T/O): every stark1 conversation line


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


def build_cast(a, pkgs, work, out, env):
    """The characters of every place (Blender: build_cast.py), their atlas model, cast.txt, talk.txt and the voice clips.
    pkgs: {map: unpacked package folder}."""
    import json
    import ua_cast
    cast, looks, talks_pkg = [], [], {}
    for pl in PLACES:
        pkg = pkgs[pl['map']]
        c = ua_cast.plan_cast(pkg, pl['engb'], a.scale, offset=pl['offset'], tag=pl['tag'], zone=pl['zone'])
        ls = ua_cast.looks_of(c)
        for l in ls:
            l['pkg'] = pkg
        cast += c
        looks += ls
        for n in {x['talk'] for x in c if x['talk']}:
            talks_pkg[n] = pkg
    plan = {'scale': a.scale, 'looks': [{'name': l['name'], 'skin_igb': str(l['pkg'] / 'actors' / f"{l['skin']}.igb"),
                                         'anim_igb': str(l['pkg'] / 'actors' / f"{l['zone']}.igb"), 'anim': l['anim'],
                                         'frames': 10 if l['principal'] else 6} for l in looks]}
    (work / 'cast_plan.json').write_text(json.dumps(plan, indent=1))
    built = work / 'cast'
    run([a.blender, '-b', '--factory-startup', '--python', HERE / 'build_cast.py', '--', work / 'cast_plan.json', built], env)
    looks = [ua_cast.load_built(l, built / f"{l['name']}.npz") for l in looks]
    lines, tris, sheets = ua_cast.pack_cast(looks, out / 'Cast', CAST_CAP)
    ua_cast.write_cast(out / 'cast.txt', lines, cast)
    print(f'cast: {len(cast)} characters, {len(looks)} looks, {tris} triangles, {sheets} atlas page(s)')
    talks = {}
    for pkg in dict.fromkeys(talks_pkg.values()):
        talks.update(ua_cast.read_talks(pkg, sorted(n for n, p in talks_pkg.items() if p == pkg)))
    bank = {}
    ens = [Path(x) for x in (a.sounds or [])]
    if a.iso:
        import ua_extract
        have = {x.name.lower() for x in ens}
        for pl in PLACES:
            if pl['bank'].lower() not in have:
                got = ua_extract.iso_extract_file(a.iso, pl['bank'], work / 'sounds')
                if got:
                    ens.append(Path(got))
    for k, e in enumerate(ens):
        if e.exists():
            bank.update(ua_cast.voice_bank(e, work / f'bank{k}'))
    if not bank:
        print('no sound bank: the characters talk in subtitles only (give --sounds TOWN1_V.ENS and TOWN1B_V.ENS)')
    index, durations = ua_cast.write_voices(out / 'Voice', talks, bank)
    stats = {}
    for pkg in dict.fromkeys(talks_pkg.values()):
        stats.update(ua_cast.read_npcstat(pkg / 'data' / 'npcstat.engb.xml'))
    ua_cast.write_talk(out / 'talk.txt', talks, ua_cast.speaker_names(talks, stats), index, durations)
    n = sum(len(t['lines']) for t in talks.values())
    print(f'talk: {len(talks)} conversations, {n} lines, {len(index)} voice clips ({sum(durations.values()):.0f} s)')
    with open(out / 'report.txt', 'a') as f:
        f.write((built / 'cast_report.txt').read_text() if (built / 'cast_report.txt').exists() else '')
        f.write(f'cast {len(cast)} characters, {tris} triangles, {sheets} atlas pages; talk {len(talks)} '
                f'conversations, {n} lines, {len(index)} voice clips\n')


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
    ap.add_argument('--sounds', action='append', help="a sound bank of the disc (TOWN1_V.ENS: the hub, TOWN1B_V.ENS: the lab) when --wad is used (voices); repeat for each")
    ap.add_argument('--no-cast', action='store_true', help='leave out the characters and their conversations')
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
        pkgs = {}
        for pl in PLACES:
            m = pl['map']
            hits = [n for n in wad.names if n.lower().endswith('.fb') and Path(n).stem.lower() == m]
            if not hits:
                raise SystemExit(f'no {m}.fb in {wad_path} - is this Marvel: Ultimate Alliance for PS2?')
            for n in hits:
                ua_extract.fb_unpack(wad.read(n), work / 'pkg' / Path(n).stem)
            pkgs[m] = work / 'pkg' / m
            ua_extract.decompile_xmlb(pkgs[m])
            igb, engb = pkgs[m] / '__combined.igb', pkgs[m] / (pl['engb'] + '.engb.xml')
            if not igb.exists() or not engb.exists():
                raise SystemExit(f'{m}: {igb.name} or {engb.name} missing after unpacking')

        env = blender_env(a.igb_blender, work)
        builts = []
        for pl in PLACES:
            m = pl['map']
            bd = work / f'built_{m}'
            run([a.blender, '-b', '--factory-startup', '--python', HERE / 'build_stark_tower.py', '--',
                 pkgs[m] / '__combined.igb', pkgs[m] / (pl['engb'] + '.engb.xml'), bd, '--scale', a.scale,
                 '--place', m, '--offset', pl['offset'][0], pl['offset'][1]], env)
            builts.append(bd)
        import combine_places
        built = work / 'built'
        combine_places.combine(built, builts)

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
            build_cast(a, pkgs, work, out, env)

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

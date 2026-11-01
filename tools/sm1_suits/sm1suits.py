"""HERO VR: our heroes as real Spider-Man Remastered suits, installed through the game's own asset table (toc),
the way Overstrike / ALERT install suit mods (Tkachov, GPL-3; the vendored dat1lib is ALERT's).

Run by the HERO VR launcher on the player's PC (the game's files are never shipped by us):
  sm1suits.py dump    --game <game folder> [--packs <dir>] [--out <dir>]   read only: what the game has, a dry run
  sm1suits.py install --game <game folder> --packs <dir> [--heroes a,b]     backs up toc, adds the suits
  sm1suits.py revert  --game <game folder>                                  puts the backed-up toc back
  sm1suits.py status  --game <game folder>
  sm1suits.py selftest                                                      synthetic model, no game needed
The last line printed is 'RESULT {json}' for the launcher.

What it writes (install): asset_archive\\toc (after a copy to asset_archive\\toc.herovr-sm1.bak) and its own
folder asset_archive\\HeroVR (one archive file + state.json). Nothing else: not SpidyVR's files, not the game's
other archives. Revert restores the toc byte for byte (sha256 checked) and deletes asset_archive\\HeroVR."""
import argparse
import datetime
import hashlib
import json
import os
import shutil
import struct
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import assets  # noqa: E402
import bcn  # noqa: E402
import heroes  # noqa: E402
import hsm1  # noqa: E402
import inject  # noqa: E402
import retarget  # noqa: E402
import toc_ops  # noqa: E402

TOOL_VERSION = 1
ARCHIVE_NAME = 'HeroVR\\herovr_suits'          # relative to asset_archive, like Overstrike's 'Suits\\<name>'
OUR_DIR = 'HeroVR'
BACKUP = 'toc.herovr-sm1.bak'
STATE = 'state.json'
CLASSIC_VANITY = 'configs/vanitytor1/vanity_spiderman_classic.config'  # CONFIRMED: ALERT's MSMR suit template path
ALIGN = 16


class Log(object):
    def __init__(self, path=None):
        self.lines = []
        self.f = open(path, 'a', encoding='utf-8') if path else None

    def __call__(self, msg):
        line = '%s %s' % (datetime.datetime.now().strftime('%H:%M:%S'), msg)
        self.lines.append(line)
        print(line)
        sys.stdout.flush()
        if self.f:
            self.f.write(line + '\n')
            self.f.flush()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def paths(game):
    aa = os.path.join(game, 'asset_archive')
    return {'aa': aa, 'toc': os.path.join(aa, 'toc'), 'bak': os.path.join(aa, BACKUP), 'dir': os.path.join(aa, OUR_DIR),
            'archive': os.path.join(aa, *ARCHIVE_NAME.split('\\')), 'state': os.path.join(aa, OUR_DIR, STATE)}


def read_state(P):
    try:
        with open(P['state'], 'r', encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def write_json_atomic(path, obj):
    tmp = path + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(obj, f, indent=1)
    os.replace(tmp, path)


def copy_atomic(src, dst):
    tmp = dst + '.herovr-tmp'
    shutil.copyfile(src, tmp)
    os.replace(tmp, dst)


# --- packs ---------------------------------------------------------------------------------------------

def load_pack(path):
    p = hsm1.Pack(path)
    groups = []
    for g in p.header['groups']:
        f = p.floats(g['pos'])
        pos = [tuple(f[i:i + 3]) for i in range(0, len(f), 3)]
        f = p.floats(g['nrm'])
        nrm = [tuple(f[i:i + 3]) for i in range(0, len(f), 3)]
        f = p.floats(g['uv'])
        uv = [tuple(f[i:i + 2]) for i in range(0, len(f), 2)]
        ix = p.u32s(g['idx'])
        j, w = p.u8s(g['jnt']), p.u8s(g['wgt'])
        groups.append({'pos': pos, 'nrm': nrm, 'uv': uv, 'tris': [tuple(ix[i:i + 3]) for i in range(0, len(ix) - 2, 3)],
                       'jnt': [list(j[4 * i:4 * i + 4]) for i in range(len(pos))],
                       'wgt': [list(w[4 * i:4 * i + 4]) for i in range(len(pos))],
                       'tex': g['tex'], 'tex_bytes': p.blob(g['tex']['blob'])})
    return p.header, groups


def _bc1_levels(w, h, mips):
    """(offset, blocks wide, blocks high, texel w, texel h) per level of a prepack BC1 mip chain."""
    out, off = [], 0
    for _ in range(mips):
        bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
        out.append((off, bw, bh, w, h))
        off += 8 * bw * bh
        w, h = max(1, w // 2), max(1, h // 2)
    return out


CELL = 16  # texels per side of a patch that holds one merged small texture


def _free_cells(g):
    """Cells of CELL x CELL texels in g's texture that no triangle's uv box touches (conservative)."""
    t = g['tex']
    cw, ch = t['width'] // CELL, t['height'] // CELL
    used = set()
    for tri in g['tris']:
        us = [min(max(g['uv'][i][0], 0.0), 1.0) for i in tri]
        vs = [min(max(g['uv'][i][1], 0.0), 1.0) for i in tri]
        x0, x1 = int(min(us) * cw) - 1, int(max(us) * cw) + 1
        y0, y1 = int(min(vs) * ch) - 1, int(max(vs) * ch) + 1
        for y in range(max(0, y0), min(ch, y1 + 1)):
            for x in range(max(0, x0), min(cw, x1 + 1)):
                used.add((x, y))
    used |= set(g.get('patches', []))
    return [(x, y) for y in range(ch) for x in range(cw) if (x, y) not in used]


def merge_small_groups(groups, slots, log):
    """SM1's suit model has a fixed number of mesh slots. When a pack has more groups than slots, fold each
    group with a tiny (<= 4x4) texture into a big one: its texels go into an unused 16x16 patch of the big
    texture (every mip level down to 4x4) and its uvs are pointed at that patch. CANDIDATE look (mips below
    the patch size blend with neighbours)."""
    groups = list(groups)
    while len(groups) > slots:
        small = [g for g in groups if g['tex']['width'] <= 4 and g['tex']['height'] <= 4]
        if not small:
            raise inject.ModelError('%d mesh groups but the suit model has only %d mesh slots, and none has a small '
                                    'texture to fold into another' % (len(groups), slots))
        s = small[0]
        targets = sorted([g for g in groups if g is not s and g['tex']['width'] >= 64 and g['tex']['height'] >= 64
                          and len(g['pos']) + len(s['pos']) <= 0xFFFF], key=lambda g: len(g['pos']))
        placed = False
        for g in targets:
            cells = _free_cells(g)
            if not cells:
                continue
            cx, cy = cells[0]
            t = g['tex']
            W, H = t['width'], t['height']
            texels = bcn.decode_bc1_block(s['tex_bytes'][:8])  # level 0 of a <= 4x4 texture is one block
            sw, sh = s['tex']['width'], s['tex']['height']
            buf = bytearray(g['tex_bytes'])
            for off, bw, bh, lw, lh in _bc1_levels(W, H, t['mips']):
                rs = CELL * lw // W  # patch size in texels at this level
                if rs < 4:
                    break
                nb = rs // 4
                for by in range(nb):
                    for bx in range(nb):
                        cols = []
                        for ty in range(4):
                            for tx in range(4):
                                # source texel under this destination texel (nearest)
                                sx = min(sw - 1, ((bx * 4 + tx) * sw) // rs)
                                sy = min(sh - 1, ((by * 4 + ty) * sh) // rs)
                                cols.append(texels[sy * 4 + sx])
                        avg = tuple(sum(c[i] for c in cols) // len(cols) for i in range(3))
                        gx, gy = cx * nb + bx, cy * nb + by
                        o = off + 8 * (gy * bw + gx)
                        buf[o:o + 8] = bcn.constant_block(71, avg + (255,))
            g['tex_bytes'] = bytes(buf)
            g.setdefault('patches', []).append((cx, cy))
            x0, y0 = cx * CELL + 1.0, cy * CELL + 1.0
            span = CELL - 2.0
            base = len(g['pos'])
            g['uv'] = list(g['uv']) + [((x0 + min(max(u, 0.0), 1.0) * span) / W, (y0 + min(max(v, 0.0), 1.0) * span) / H)
                                       for u, v in s['uv']]
            for k in ('pos', 'nrm', 'jnt', 'wgt'):
                g[k] = list(g[k]) + list(s[k])
            g['tris'] = list(g['tris']) + [(a + base, b + base, c + base) for a, b, c in s['tris']]
            groups.remove(s)
            log('folded a %d-vertex group with a %dx%d texture into a %dx%d one at patch (%d, %d) (CANDIDATE look)' % (
                len(s['pos']), sw, sh, W, H, cx, cy))
            placed = True
            break
        if not placed:
            raise inject.ModelError('%d mesh groups but only %d mesh slots, and no free texture patch to fold into' % (
                len(groups), slots))
    return groups


def pack_path(packs, suit_id):
    return os.path.join(packs, suit_id + '.hsm1')


# --- the plan: every asset we would write, built in memory --------------------------------------------

class Plan(object):
    def __init__(self, log):
        self.log = log
        self.assets = []       # (aid, span, bytes, path)
        self.report = {'heroes': {}, 'base': {}, 'notes': []}

    def add(self, path_or_aid, data, span=0, path=None):
        a = path_or_aid if isinstance(path_or_aid, int) else toc_ops.aid_of(path_or_aid)
        self.assets.append((a, span, bytes(data), path or (path_or_aid if isinstance(path_or_aid, str) else '%016X' % a)))


def read_required(toc, aid, what):
    data = toc.read_asset(aid, 0)
    if data is None:
        raise assets.AssetError('%s (%016X) is not in the toc' % (what, aid))
    return data


def short_texture_path(preferred, old, counter):
    if len(preferred) <= len(old):
        return preferred
    alt = 'hv/%d.texture' % counter
    if len(alt) <= len(old):
        return alt
    raise assets.AssetError('the game texture path %s is too short to hold a new one' % old)


def build_template(toc, log):
    """The classic suit's vanity config -> its model, read as our template (CONFIRMED path, ALERT)."""
    vdata = read_required(toc, toc_ops.aid_of(CLASSIC_VANITY), 'the classic suit vanity config')
    model_path = assets.vanity_configs(vdata)
    if not model_path:
        raise assets.AssetError('the classic suit vanity config has no ModelList model')
    mdata = read_required(toc, toc_ops.aid_of(model_path), 'the classic suit model ' + model_path)
    T = inject.Template(mdata)
    pts, sk = T.body_points()
    F = retarget.Fitter(pts, sk, retarget.joint_sides(T.joint_names()))
    log('template: %s, %d meshes, body meshes %s, reference %d, %d body points; frame %s; %s' % (
        model_path, len(T.meshes), T.body, T.ref, len(pts), F.frame.describe(), F.side_check))
    return model_path, mdata, T, F


def material_parts(toc, T, log):
    """The body material and the diffuse / normal textures it names (donors for ours)."""
    mpath = T.body_material_path()
    if not mpath:
        raise assets.AssetError('the template body mesh has no material path')
    mdata = read_required(toc, toc_ops.aid_of(mpath), 'the body material ' + mpath)
    texs = assets.material_textures(mdata)
    log('body material %s (magic %08X): %s' % (mpath, struct.unpack('<I', mdata[:4])[0],
                                               ', '.join('%s=%s' % (r, p) for p, r, _ in texs)))
    diff = [p for p, r, _ in texs if r == 'diffuse']
    nrm = [p for p, r, _ in texs if r == 'normal']
    if not diff:
        raise assets.AssetError('no diffuse texture found in the body material')
    out = {'material_path': mpath, 'material': mdata, 'diffuse': diff[0], 'normal': nrm[0] if nrm else None, 'all': texs}
    out['diffuse_tex'] = read_required(toc, toc_ops.aid_of(diff[0]), 'the body diffuse texture')
    out['diffuse_info'] = assets.texture_info(out['diffuse_tex'])
    if out['normal']:
        out['normal_tex'] = read_required(toc, toc_ops.aid_of(out['normal']), 'the body normal texture')
        out['normal_info'] = assets.texture_info(out['normal_tex'])
    return out


def plan_hero(plan, entry, packs, tpl, mat, counter):
    """One hero: model, materials, textures, the four suit configs and the icon. Raises on failure."""
    log = plan.log
    suit_id = entry['id']
    short = suit_id.replace('herovr_', '')
    header, groups = load_pack(pack_path(packs, suit_id))
    model_path_t, model_bytes, T, F = tpl
    rep = {'groups': len(groups), 'vertices': sum(len(g['pos']) for g in groups)}
    if len(groups) > len(T.meshes):
        groups = merge_small_groups(groups, len(T.meshes), log)
        rep['groups_after_fold'] = len(groups)
    out_groups, mat_paths = [], []
    flat_path = 'herovr/flat_n.texture'
    for gi, g in enumerate(groups):
        p, n, s = F.fit(header['ends'], g['pos'], g['nrm'], g['jnt'], g['wgt'])
        tris = [(a, c, b) for a, b, c in g['tris']] if F.mirrored else g['tris']
        out_groups.append({'pos': p, 'nrm': n, 'uv': g['uv'], 'tris': tris, 'skin': s})
        # texture: our BC1 mip chain in a copy of the body diffuse texture's header
        di = mat['diffuse_info']
        fmt = 72 if di['fmt'] in bcn.SRGB else 71
        tex_path = short_texture_path('herovr/%s/%dc.texture' % (short, gi), mat['diffuse'], counter[0])
        counter[0] += 1
        t = g['tex']
        plan.add(tex_path, assets.make_texture(mat['diffuse_tex'], fmt, t['width'], t['height'], t['mips'], g['tex_bytes'], log))
        repl = {mat['diffuse']: tex_path}
        flat = bcn.flat_normal(mat['normal_info']['fmt']) if mat['normal'] else None
        if mat['normal'] and flat is None and gi == 0:
            log('normal map format %d has no flat block here: the classic suit normal map stays (CANDIDATE look)' % mat['normal_info']['fmt'])
        if flat is not None:
            npath = short_texture_path(flat_path, mat['normal'], counter[0])
            if npath != flat_path:
                counter[0] += 1
            repl[mat['normal']] = npath
            if npath not in [a[3] for a in plan.assets]:
                ni = mat['normal_info']
                plan.add(npath, assets.make_texture(mat['normal_tex'], ni['fmt'], 4, 4, 1, flat, log))
        mp = 'herovr/%s/m%d.material' % (short, gi)
        plan.add(mp, assets.retarget_material(mat['material'], repl, log))
        mat_paths.append(mp)
    model_path = 'herovr/%s/%s.model' % (short, short)
    built = inject.build(model_bytes, out_groups, mat_paths, log)
    plan.add(model_path, built)
    left = []
    for old in [m['aid'] for m in T.materials()]:
        if bytes.fromhex(old)[::-1] in built:
            left.append(old)
    rep['old_material_ids_left_in_model'] = left
    for path, data in assets.suit_configs(suit_id, model_path).items():
        plan.add(path, data)
    rep['model'] = model_path
    rep['materials'] = mat_paths
    rep['model_bytes'] = len(built)
    return rep


def make_plan(toc, packs, wanted, log):
    plan = Plan(log)
    tpl = build_template(toc, log)
    plan.report['template'] = {'model': tpl[0], 'info': tpl[2].info(), 'fitter': tpl[3].report()}
    mat = material_parts(toc, tpl[2], log)
    plan.report['body_material'] = {'path': mat['material_path'], 'textures': mat['all'],
                                    'diffuse': mat['diffuse_info'], 'normal': mat.get('normal_info')}
    counter = [0]
    done = []
    for entry in heroes.SM1_SUITS:
        if wanted and entry['id'] not in wanted:
            continue
        n0 = len(plan.assets)
        try:
            if not os.path.isfile(pack_path(packs, entry['id'])):
                raise assets.AssetError('pack missing: ' + pack_path(packs, entry['id']))
            plan.report['heroes'][entry['id']] = plan_hero(plan, entry, packs, tpl, mat, counter)
            plan.report['heroes'][entry['id']]['ok'] = True
            done.append(entry)
            log('%s: ready' % entry['name'])
        except Exception as e:  # one hero failing never stops the others
            del plan.assets[n0:]
            plan.report['heroes'][entry['id']] = {'ok': False, 'error': '%s: %s' % (type(e).__name__, e)}
            log('%s: SKIPPED (%s: %s)' % (entry['name'], type(e).__name__, e))
    if not done:
        raise assets.AssetError('no hero could be built')

    # the suit icon: a copy of the first suit's tech web picture
    prog = read_required(toc, assets.AID_PROGRESSION, 'system_progression.config')
    icon = assets.first_suit_icon(prog)
    icon_data = toc.read_asset(toc_ops.aid_of(icon), 0) if icon else None
    plan.report['base']['icon_source'] = icon
    for e in done:
        if icon_data:
            plan.add(assets.suit_paths(e['id'])['icon'], icon_data)
    if not icon_data:
        log('no suit picture to copy: the tech web shows ours without one')

    # names: SUIT_<ID> in every copy of the localization table
    loc_idx = [i for i in toc.index().get(assets.AID_LOCALIZATION, [])]
    names = {}
    for e in done:
        names['SUIT_' + e['id'].upper()] = e['name']
        names['SUIT_' + e['id'].upper() + '_SHORT'] = e['name']
    has_name = False
    for i in loc_idx:
        span = toc.span_of(i)
        try:
            data = bytes(toc.toc.extract_asset(i))
            plan.add(assets.AID_LOCALIZATION, assets.edit_localization(data, names), span, 'localization (span %d)' % span)
            has_name = True
        except Exception as ex:
            log('localization in span %d not edited (%s): names fall back to an existing string' % (span, ex))
    plan.report['base']['localization_spans'] = [toc.span_of(i) for i in loc_idx]

    plan.add(assets.AID_PROGRESSION, assets.edit_progression(prog, [(e['id'], has_name) for e in done]), 0,
             'configs/system/system_progression.config')
    ll = read_required(toc, assets.AID_LOADOUTLIST, 'masteritemloadoutlist.config')
    plan.add(assets.AID_LOADOUTLIST, assets.edit_loadout_list(ll, [e['id'] for e in done]), 0,
             'configs/masteritemloadoutlist/masteritemloadoutlist.config')
    for a, what in ((assets.AID_VANITY_MASTER, 'vanity master list'), (assets.AID_VANITY_LAUNCH, 'vanity launch list')):
        d = toc.read_asset(a, 0)
        if d is not None:  # Overstrike re-points these two unchanged so the game reads them from the mod archive
            plan.add(a, d, 0, what)
        else:
            log('%s (%016X) not in the toc: left alone' % (what, a))
    plan.report['installed'] = [e['id'] for e in done]
    return plan


# --- commands --------------------------------------------------------------------------------------------

def open_toc(P):
    if not os.path.isfile(P['toc']):
        raise assets.AssetError('no asset_archive\\toc in the game folder')
    return toc_ops.Toc(P['toc'])


def cmd_status(P, log):
    st = read_state(P)
    if not os.path.isfile(P['toc']):
        return {'installed': False, 'error': 'no toc'}
    cur = sha256_file(P['toc'])
    if st is None:
        return {'installed': False, 'toc_sha256': cur}
    return {'installed': True, 'pending': st.get('pending', False), 'toc_matches_install': cur == st.get('toc_sha256_after'),
            'suits': st.get('suits', []), 'installed_at': st.get('installed_at'), 'toc_sha256': cur}


def cmd_revert(P, log):
    st = read_state(P)
    if st is None:
        if os.path.isdir(P['dir']) and os.path.isfile(P['toc']):
            t = open_toc(P)
            names = t.archive_names()
            t.close()
            if ARCHIVE_NAME in names:
                raise assets.AssetError('the toc uses our archive but there is no install record: use Steam > Verify integrity')
            shutil.rmtree(P['dir'], ignore_errors=True)
        log('nothing installed')
        return {'reverted': False, 'reason': 'nothing installed'}
    cur = sha256_file(P['toc'])
    before = st.get('toc_sha256_before')
    if cur == before:
        log('the toc is already the original one')
    elif cur == st.get('toc_sha256_after') or st.get('pending'):
        if not os.path.isfile(P['bak']) or sha256_file(P['bak']) != before:
            raise assets.AssetError('the backup %s is missing or changed: not touching the toc (Steam > Verify integrity fixes it)' % P['bak'])
        copy_atomic(P['bak'], P['toc'])
        if sha256_file(P['toc']) != before:
            raise assets.AssetError('restored toc does not match the backup')
        log('toc restored from %s' % BACKUP)
    else:
        t = open_toc(P)
        names = t.archive_names()
        t.close()
        if ARCHIVE_NAME in names:
            raise assets.AssetError('the toc changed after our install (another mod tool?) and still uses our archive: '
                                    'not touching it. Uninstall the other mods first, or Steam > Verify integrity.')
        log('the toc was replaced since our install (game update or file check) and no longer uses our files')
    shutil.rmtree(P['dir'], ignore_errors=True)
    try:
        os.remove(P['bak'])
    except OSError:
        pass
    log('HERO VR suits removed')
    return {'reverted': True}


def cmd_install(P, packs, wanted, log):
    st = read_state(P)
    if st is not None:
        log('a previous install is there: reverting it first')
        cmd_revert(P, log)
    toc = open_toc(P)
    try:
        if ARCHIVE_NAME in toc.archive_names():
            raise assets.AssetError('the toc already lists our archive but there is no install record: Steam > Verify integrity first')
        before = sha256_file(P['toc'])
        plan = make_plan(toc, packs, wanted, log)
    finally:
        toc.close()  # releases the game archive handles (Windows)
    if not plan.report['installed']:
        raise assets.AssetError('no hero could be built, nothing installed')

    os.makedirs(P['dir'], exist_ok=True)
    copy_atomic(P['toc'], P['bak'])
    if sha256_file(P['bak']) != before:
        raise assets.AssetError('backup copy does not match the toc')
    log('toc backed up to asset_archive\\%s' % BACKUP)
    try:
        placed, counts, blob_len = _write_install(P, toc, plan, before)
    except Exception:
        log('install failed part way: putting everything back')
        try:
            toc.close()
            cmd_revert(P, log)
        except Exception as e2:
            log('revert after the failure also failed: %s' % e2)
        raise
    # read back what we wrote
    chk = toc_ops.Toc(P['toc'])
    try:
        bad = [p for a, s, o, n, p in placed if chk.read_asset(a, s) is None]
    finally:
        chk.close()
    log('installed %d suits: %d assets added, %d re-pointed, %d bytes; read back: %s' % (
        len(plan.report['installed']), counts['added'], counts['rerouted'], blob_len, 'all present' if not bad else 'MISSING ' + ', '.join(bad)))
    if bad:
        cmd_revert(P, log)
        raise assets.AssetError('read back failed, reverted')
    plan.report['counts'] = counts
    return {'installed': plan.report['installed'], 'skipped': [k for k, v in plan.report['heroes'].items() if not v.get('ok')],
            'report': plan.report}


def _write_install(P, toc, plan, before):
    """Archive, pending state, toc. The caller reverts if this raises."""
    ai = toc.add_archive(ARCHIVE_NAME)
    blob = bytearray()
    placed = []
    for a, span, data, path in plan.assets:
        off = len(blob)
        blob += data
        blob += bytes((-len(blob)) % ALIGN)
        placed.append((a, span, off, len(data), path))
    with open(P['archive'] + '.tmp', 'wb') as f:
        f.write(blob)
    os.replace(P['archive'] + '.tmp', P['archive'])
    state = {'tool_version': TOOL_VERSION, 'installed_at': datetime.datetime.now().isoformat(timespec='seconds'),
             'toc_sha256_before': before, 'toc_sha256_after': None, 'pending': True, 'archive': ARCHIVE_NAME,
             'suits': plan.report['installed'], 'assets': [[('%016X' % a), s, o, n, p] for a, s, o, n, p in placed]}
    write_json_atomic(P['state'], state)
    counts = {'added': 0, 'rerouted': 0}
    for a, span, off, n, path in placed:
        counts[toc.put_asset(a, ai, off, n, span)] += 1
    toc.sort()
    toc.save(P['toc'])
    toc.close()
    state['toc_sha256_after'] = sha256_file(P['toc'])
    state['pending'] = False
    write_json_atomic(P['state'], state)
    return placed, counts, len(blob)


def cmd_dump(P, packs, out, raw, log):
    """Read only. What one run on the player's PC tells us: the toc's layout, every asset the install reads
    (present or not, size, sha256), the classic suit chain, the template model's sections / meshes / joints /
    materials, the body material's textures and their headers, the fitter's frame and joints, and a full dry
    run of the install (every hero built in memory, nothing written to the game)."""
    os.makedirs(out, exist_ok=True)
    toc = open_toc(P)
    rep = {'tool_version': TOOL_VERSION, 'toc_sha256': sha256_file(P['toc']), 'toc_bytes': os.path.getsize(P['toc']),
           'archives': toc.archive_names(), 'assets_count': len(toc.ids),
           'spans': [(e.asset_index, e.count) for e in toc.spans], 'status': cmd_status(P, log), 'reads': {}}
    want = {'vanity_classic': CLASSIC_VANITY, 'progression': assets.AID_PROGRESSION, 'loadoutlist': assets.AID_LOADOUTLIST,
            'vanity_master': assets.AID_VANITY_MASTER, 'vanity_launch': assets.AID_VANITY_LAUNCH,
            'localization': assets.AID_LOCALIZATION}
    for k, v in want.items():
        a = v if isinstance(v, int) else toc_ops.aid_of(v)
        idx = toc.index().get(a, [])
        ent = {'aid': '%016X' % a, 'copies': []}
        for i in idx:
            d = bytes(toc.toc.extract_asset(i))
            ent['copies'].append({'span': toc.span_of(i), 'bytes': len(d), 'sha256': hashlib.sha256(d).hexdigest(),
                                  'magic': '%08X' % struct.unpack('<I', d[:4])[0]})
            if raw:
                with open(os.path.join(out, '%s_span%d.bin' % (k, toc.span_of(i))), 'wb') as f:
                    f.write(d)
        rep['reads'][k] = ent
    try:
        prog = toc.read_asset(assets.AID_PROGRESSION, 0)
        rep['suits_in_tech_web'] = [i.get('Name') for i in assets.suits_in_progression(prog)] if prog else None
    except Exception as e:
        rep['suits_in_tech_web'] = 'error: %s' % e
    try:
        name, mbytes, T, F = build_template(toc, log)
        rep['template'] = {'model': name, 'bytes': len(mbytes), 'info': T.info(), 'fitter': F.report()}
        if raw:
            with open(os.path.join(out, 'template.model'), 'wb') as f:
                f.write(mbytes)
        mat = material_parts(toc, T, log)
        rep['body_material'] = {'path': mat['material_path'], 'textures': mat['all'], 'diffuse': mat['diffuse_info'],
                                'normal': mat.get('normal_info')}
        if raw:
            with open(os.path.join(out, 'body.material'), 'wb') as f:
                f.write(mat['material'])
            for k in ('diffuse_tex', 'normal_tex'):
                if k in mat:
                    with open(os.path.join(out, k + '_header.bin'), 'wb') as f:
                        f.write(mat[k][:mat[k.replace('_tex', '_info')]['pixels_at']])
    except Exception as e:
        rep['template_error'] = '%s: %s' % (type(e).__name__, e)
        rep['template_trace'] = traceback.format_exc()
    if packs and os.path.isdir(packs):
        try:
            plan = make_plan(toc, packs, None, log)
            rep['dry_run'] = {'report': plan.report, 'assets': [['%016X' % a, s, len(d), p] for a, s, d, p in plan.assets]}
        except Exception as e:
            rep['dry_run_error'] = '%s: %s' % (type(e).__name__, e)
            rep['dry_run_trace'] = traceback.format_exc()
    rep['log'] = log.lines
    toc.close()
    with open(os.path.join(out, 'herovr_sm1_dump.json'), 'w', encoding='utf-8') as f:
        json.dump(rep, f, indent=1, default=str)
    log('dump written to %s' % out)
    return {'dump': os.path.join(out, 'herovr_sm1_dump.json'), 'template_ok': 'template' in rep,
            'dry_run_ok': 'dry_run' in rep}


def cmd_selftest(packs, log):
    import synth
    src = pack_path(packs, 'herovr_superman')
    header, groups = load_pack(src)
    g = groups[0]
    bones = [g['jnt'][i][max(range(4), key=lambda k: g['wgt'][i][k])] for i in range(len(g['pos']))]
    res = {}
    for kind in ('zup', 'mirror'):
        data = synth.make_model(g['pos'], g['nrm'], g['uv'], g['tris'], bones, kind=kind)
        T = inject.Template(data)
        pts, sk = T.body_points()
        F = retarget.Fitter(pts, sk, retarget.joint_sides(T.joint_names()))
        h2, g2 = load_pack(pack_path(packs, 'herovr_captainamerica'))
        outg = []
        for gg in g2:
            p, n, s = F.fit(h2['ends'], gg['pos'], gg['nrm'], gg['jnt'], gg['wgt'])
            outg.append({'pos': p, 'nrm': n, 'uv': gg['uv'], 'tris': [(a, c, b) for a, b, c in gg['tris']] if F.mirrored else gg['tris'], 'skin': s})
        built = inject.build(data, outg, ['herovr/t/m%d.material' % i for i in range(len(outg))], log)
        T2 = inject.Template(built)
        res[kind] = {'mirrored': F.mirrored, 'vertexes': T2.info()['vertexes'], 'skin': T2.skin_notes}
    log('selftest ok: %s' % res)
    return res


def main(argv=None):
    ap = argparse.ArgumentParser(description='HERO VR suits for Spider-Man Remastered (toc install, Overstrike/ALERT style)')
    ap.add_argument('command', choices=['dump', 'install', 'revert', 'status', 'selftest'])
    ap.add_argument('--game', help='the game folder (holds Spider-Man.exe and asset_archive)')
    ap.add_argument('--packs', default=os.path.join(HERE, '..', '..', 'Models', 'SM1Suits'))
    ap.add_argument('--heroes', default='', help='comma separated suit ids (default: all)')
    ap.add_argument('--out', default=None)
    ap.add_argument('--log', default=None)
    ap.add_argument('--no-raw', action='store_true', help='dump: do not copy the read assets')
    a = ap.parse_args(argv)
    log = Log(a.log)
    try:
        if a.command == 'selftest':
            res = cmd_selftest(a.packs, log)
        else:
            if not a.game or not os.path.isdir(os.path.join(a.game, 'asset_archive')):
                raise assets.AssetError('--game must be the Spider-Man Remastered folder (with asset_archive)')
            P = paths(a.game)
            if a.command == 'status':
                res = cmd_status(P, log)
            elif a.command == 'revert':
                res = cmd_revert(P, log)
            elif a.command == 'install':
                wanted = set(x.strip() for x in a.heroes.split(',') if x.strip())
                res = cmd_install(P, a.packs, wanted, log)
                res = {k: v for k, v in res.items() if k != 'report'}
            else:
                res = cmd_dump(P, a.packs, a.out or os.path.join(os.getcwd(), 'herovr_sm1_dump'), not a.no_raw, log)
        print('RESULT ' + json.dumps({'ok': True, 'command': a.command, 'result': res}, default=str))
        return 0
    except Exception as e:
        log('FAILED: %s: %s' % (type(e).__name__, e))
        log(traceback.format_exc())
        print('RESULT ' + json.dumps({'ok': False, 'command': a.command, 'error': '%s: %s' % (type(e).__name__, e)}))
        return 1


if __name__ == '__main__':
    sys.exit(main())

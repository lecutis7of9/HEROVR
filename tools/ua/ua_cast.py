#!/usr/bin/env python3
"""ULTIMATE ALLIANCE EXPANSION - the hub's characters and what they say, from YOUR OWN MUA1 PS2 disc.

Pure Python (numpy + pillow; raven-formats for the sound banks). convert_ua.py drives it; the Blender half that
poses the characters is build_cast.py. Nothing of the game is in HERO VR's repository: every file this writes goes
into the player's own Models/UA/<Place> folder.

  plan_cast()   which characters stand where, in which skin and which of the map's own idle animations (the
                spawners of the map's .engb, npcstat's skins, the spawn scripts' spots and EA_ZONE animations)
  read_talks()  the map's conversations (conversations/*.engb.xml): every line, every reply and choice, the
                start conditions, run-once, unlock flags and the mission hooks, as one graph per conversation
  voice_bank()  the conversations' voice clips from the zone's sound bank (SOUNDS/T/O/TOWN1_V.ENS: PS2 ZSND,
                PS-ADPCM VAG) decoded to 16-bit WAV
  pack_cast()   the posed frames of build_cast.py into Cast/scene.glb (one texture atlas) + cast.txt
  write_talk()  talk.txt + Voice/v0000.wav ... (HERO VR plays them with its music player)

Formats HERO VR reads (include/spidy/ua_cast.hpp):

  cast.txt   HEROVR-UA-CAST 1
             look <name> <first triangle> <triangles a frame> <frames> <loop seconds> <turns 0|1>
             npc <id> <look> <x> <y> <z> <yaw deg> <group|-> <talk|-> <label...>
  talk.txt   HEROVR-UA-TALK 1
             speaker <TOKEN> <shown name...>
             conv <name>
             start <line> <once 0|1> <always|never|hero:<h>|not:<h,h,...>>
             line <i> <TOKEN> <voice|-1> <seconds> <text...>
             resp <line> <to|-1> <end 0|1> <none|missions|flag:<n>> <shown once flag n is set|0>
                  <always|never|hero:<h>|not:<h,...>> <text...>
"""
import hashlib, io, json, math, re, struct, wave
from pathlib import Path
from xml.etree import ElementTree as ET

CAST_HEADER = 'HEROVR-UA-CAST 1'
TALK_HEADER = 'HEROVR-UA-TALK 1'

# The player's hero for the start conditions (MUA's own hero ids; Spider-Man is one of MUA's heroes).
PLAYER_HERO = 'spiderman'
PLAYER_NAME = 'SPIDER-MAN'

# Speakers the conversations name that npcstat does not (MUA's heroes; their public names).
HERO_NAMES = {
    'SPIDERMAN': 'SPIDER-MAN', 'IRONMAN': 'IRON MAN', 'CAPTAINAMERICA': 'CAPTAIN AMERICA', 'THOR': 'THOR',
    'WOLVERINE': 'WOLVERINE', 'BLACKPANTHER': 'BLACK PANTHER', 'MRFANTASTIC': 'MR. FANTASTIC',
    'HUMANTORCH': 'HUMAN TORCH', 'INVISIBLEWOMAN': 'INVISIBLE WOMAN', 'THING': 'THE THING', 'STORM': 'STORM',
    'ICEMAN': 'ICEMAN', 'DAREDEVIL': 'DAREDEVIL', 'ELEKTRA': 'ELEKTRA', 'BLADE': 'BLADE', 'GHOSTRIDER': 'GHOST RIDER',
    'DEADPOOL': 'DEADPOOL', 'DRSTRANGE': 'DR. STRANGE', 'SPIDERWOMAN': 'SPIDER-WOMAN', 'WARBIRD': 'MS. MARVEL',
    'LUKECAGE': 'LUKE CAGE', 'SILVERSURFER': 'SILVER SURFER', 'MOONKNIGHT': 'MOON KNIGHT', 'NICKFURY': 'NICK FURY',
    'MENU': '', 'PLAYER': PLAYER_NAME,
}

# Office workers have no idle of their own on the PS2 disc (their 158_/159_ movesets are not packaged), so they
# borrow the hub's own zone animations: a woman listening, a man talking with his hands.
OFFICE_ANIM = {'f': 'zone10', 'm': 'zone7'}
SITTING = {'zone1', 'zone3', 'zone5', 'zone6', 'zone14'}  # seated performances: they do not turn to face him
DEFAULT_ANIM = 'zone10'


# ---------------------------------------------------------------- the map's entities
def vec(s):
    return [float(v) for v in (s or '0 0 0').split()]


def read_engb(path):
    """(instances {name: {'pos': [x,y,z], 'orient': [rx,ry,rz]}}, entities {name: attrs}) of a decompiled .engb."""
    root = ET.parse(str(path)).getroot()
    insts, ents = {}, {}
    for ei in root.iter('entinst'):
        for i in ei.iter('inst'):
            insts[i.get('name')] = {'type': ei.get('type'), 'pos': vec(i.get('pos')), 'orient': vec(i.get('orient'))}
    for e in root.iter('entity'):
        if e.get('name'):
            ents[e.get('name')] = dict(e.attrib)
    return insts, ents


def read_npcstat(path):
    root = ET.parse(str(path)).getroot()
    return {s.get('name'): dict(s.attrib) for s in root.iter('stats') if s.get('name')}


def script_text(pkg, rel):
    p = Path(pkg) / 'scripts' / (rel + '.py')
    return p.read_text(encoding='latin-1') if p.exists() else ''


def spot_anims(text):
    """The (spot or None, animation) pairs a spawn script stages, in order: copyOriginAndAngles(<who>, <spot>)
    then playanim("EA_ZONEn", ...)."""
    out, spot = [], None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#'):
            continue
        m = re.match(r'copyOriginAndAngles\(\s*"([^"]+)"\s*,\s*"([^"]+)"', line)
        if m:
            spot = m.group(2)
            continue
        m = re.match(r'playanim\s*\(\s*"EA_ZONE(\d+)"', line, re.I)
        if m:
            out.append((spot, f'zone{int(m.group(1))}'))
            spot = None
    return out


def last_conversation(text):
    """The conversation a talk script starts on a first visit (its last startConversation: the 'part1' branch)."""
    convs = re.findall(r'start(?:Char)?Conversation\(\s*"([^"]+)"', text)
    return convs[-1].split('/')[-1] if convs else None


def label_of(stat, character):
    name = (stat or {}).get('charactername') or character or ''
    return re.sub(r'\s+', ' ', name).strip().upper()


def plan_cast(pkg, engb_rel, scale, sex_of=None):
    """Every character spawner of the map: its skin, the idle it plays and where (room space, like npcs.txt)."""
    pkg = Path(pkg)
    insts, ents = read_engb(pkg / (engb_rel + '.engb.xml'))
    stats = read_npcstat(pkg / 'data' / 'npcstat.engb.xml')
    start = insts.get('player_start_all01') or next(v for k, v in insts.items() if k.startswith('player_start'))
    O = start['pos']

    def to_room(p):
        return [round((p[0] - O[0]) * scale, 3), round((p[2] - O[2]) * scale, 3), round(-(p[1] - O[1]) * scale, 3)]

    out = []
    for name, inst in sorted(insts.items()):
        e = ents.get(inst.get('type') or '', ents.get(name, {}))  # an entinst's instances share its type's entity
        if e.get('classname') != 'monsterspawnerent':
            continue
        character = e.get('character', '')
        stat = stats.get(character, {})
        skin = stat.get('skin', '')
        if not skin or skin == '0000':
            continue
        pos, yaw = inst['pos'], inst['orient'][2]
        pairs = spot_anims(script_text(pkg, e.get('monster_spawnscript', ''))) if e.get('monster_spawnscript') else []
        anim = None
        if pairs:
            spot, anim = pairs[-1]  # the likelier, later branch (Black Widow: the conference table's chair)
            if spot and spot in insts:
                pos, yaw = insts[spot]['pos'], insts[spot]['orient'][2]
        office = 'officeworker' in character.lower()
        if not anim:
            sex = (sex_of or {}).get(character) or ('f' if 'female' in character.lower() else 'm')
            anim = OFFICE_ANIM[sex] if office else DEFAULT_ANIM
        elif office:
            anim = OFFICE_ANIM['f' if 'female' in character.lower() else 'm']
        talk = None
        if e.get('monster_actonuse') == 'true' and e.get('monster_actscript'):
            talk = last_conversation(script_text(pkg, e['monster_actscript']))
        nid = name[3:] if name.startswith('sp_') else name
        group = '-'
        m = re.match(r'talker(\d+)$', nid)
        if m:  # rand_npcs.py: one pair at a time, talkers 2k-1 and 2k
            group = f'pair{(int(m.group(1)) + 1) // 2:02d}'
        out.append({'id': nid, 'character': character, 'skin': skin, 'anims': stat.get('characteranims', ''),
                    'anim': anim, 'at': to_room(pos), 'yaw': round(math.degrees(yaw), 1), 'group': group,
                    'talk': talk, 'label': label_of(stat, character), 'turns': anim not in SITTING})
    return out


def looks_of(cast):
    """The distinct (skin, animation) looks the cast needs, named like '2101_zone9'."""
    seen = {}
    for c in cast:
        key = f"{c['skin']}_{c['anim']}"
        c['look'] = key
        if key not in seen:
            seen[key] = {'name': key, 'skin': c['skin'], 'anim': c['anim'], 'principal': c['talk'] is not None,
                         'turns': c['turns']}
        seen[key]['principal'] = seen[key]['principal'] or c['talk'] is not None
    return list(seen.values())


# ---------------------------------------------------------------- conversations
def split_speaker(text):
    m = re.match(r'%([A-Za-z_]+)%\s*:\s*(.*)$', text or '', re.S)
    if m:
        return m.group(1).upper(), m.group(2).strip()
    m = re.match(r'%([A-Za-z_]+)%$', (text or '').strip())
    if m:
        return m.group(1).upper(), ''
    return None, (text or '').strip()


def condition_of(sc, pkg):
    """A start condition's test for the player's hero: always, never, hero:<h> or not:<h,...>."""
    return script_condition(sc.get('conditionscriptfile'), pkg)


def script_condition(f, pkg):
    """A condition script (a start's or a response's): always, never, hero:<h> or not:<h,...>."""
    if not f or 'disallowResponseOnVar' in f:
        return 'always'
    m = re.match(r'common/conv/([a-z_]+)$', f)
    if m:
        return 'hero:' + m.group(1)
    text = script_text(pkg, f)
    if 'disallowResponseOnVar' in text:
        return 'always'  # a flag gates it (needs_of), nobody's hero
    m = re.search(r'allowConvIfName\(\s*"_ACTIVATOR_"\s*,\s*"([a-z_]+)"', text)
    if m:
        return 'hero:' + m.group(1)
    m = re.search(r'isActorInList\(\s*"_ACTIVATOR_"\s*,\s*"([^"]+)"', text)
    if m and 'allowConversation("TRUE"' in text.replace(' ', '').replace("'", '"') or (m and 'allowConversation' in text):
        return 'not:' + ','.join(m.group(1).split())
    return 'never'  # story progress the hub cannot know (the laptop, run_once07, ...)


def action_of(resp, pkg=None):
    s = resp.get('chosenscriptfile') or ''
    if pkg is not None and re.fullmatch(r'[A-Za-z0-9_/]+', s) and not re.search(r'/load_[a-z0-9_]+$', s):
        s = script_text(pkg, s) or s  # a script file: what it does
    m = re.search(r"setGameFlag\(\s*'conv_vars'\s*,\s*(\d+)", s)
    if m:
        return f'flag:{int(m.group(1))}'
    if re.search(r'/load_[a-z0-9_]+$', s):
        return 'missions'
    return 'none'


def needs_of(resp, pkg=None):
    """The conv_vars flag this response waits for (disallowResponseOnVar: a follow-up shown only once the answer that
    sets it was heard - Fury's "troubles between S.H.I.E.L.D. and Stark" after "why Stark Tower"), 0 none."""
    s = resp.get('scriptfile') or ''
    if pkg is not None and re.fullmatch(r'[A-Za-z0-9_/]+', s):
        s = script_text(pkg, s)
    m = re.search(r"disallowResponseOnVar\(\s*'conv_vars'\s*,\s*(\d+)", s)
    return int(m.group(1)) if m else 0


def read_talk(path, pkg):
    """One conversation file as {'starts': [(line, once, cond)], 'lines': [...], 'resps': [...]}."""
    root = ET.parse(str(path)).getroot()
    lines, resps, tags, jumps = [], [], {}, []

    def add_line(el):
        i = len(lines)
        tok, text = split_speaker(el.get('text'))
        lines.append({'speaker': tok or 'MENU', 'text': text, 'sound': (el.get('soundtoplay') or '').upper()})
        if el.get('tagindex'):
            tags[el.get('tagindex')] = i
        for r in el.findall('response'):
            j = len(resps)
            resps.append({'line': i, 'to': -1, 'end': r.get('conversationend') == 'true', 'action': action_of(r, pkg),
                          'needs': needs_of(r, pkg), 'cond': script_condition(r.get('scriptfile'), pkg), 'text': (r.get('text') or '').strip()})
            child = r.find('line')
            if child is not None:
                resps[j]['to'] = add_line(child)
            elif r.get('tagjump'):
                jumps.append((j, r.get('tagjump')))
        return i

    starts = []
    for sc in root.findall('startCondition'):
        for part in sc.findall('participant'):
            for el in part.findall('line'):
                first = add_line(el)
                starts.append((first, sc.get('runonce') == 'true', condition_of(sc, pkg)))
    for j, tag in jumps:
        resps[j]['to'] = tags.get(tag, -1)
        if resps[j]['to'] < 0:
            resps[j]['end'] = True
    # a start whose first line is only a tag's menu header (no text of its own) is still a start
    return {'starts': starts, 'lines': lines, 'resps': resps}


def read_talks(pkg, names):
    out = {}
    for n in names:
        p = next(Path(pkg).rglob(f'conversations/**/{n}.engb.xml'), None)
        if p:
            out[n] = read_talk(p, pkg)
    return out


def speaker_names(talks, stats):
    names = dict(HERO_NAMES)
    for st in stats.values():
        cn = st.get('charactername') or ''
        tok = re.sub(r'[^A-Z]', '', cn.upper())
        if tok:
            names.setdefault(tok, cn.upper())
            if tok.startswith('THE'):
                names.setdefault(tok[3:], cn.upper())
    used = {l['speaker'] for t in talks.values() for l in t['lines']}
    return {tok: names.get(tok, tok.replace('_', ' ')) for tok in sorted(used)}


def clean(text):
    return re.sub(r'\s+', ' ', text.replace('\n', ' ')).strip()


def write_talk(path, talks, speakers, voice_index, durations):
    """talk.txt (see the module's docstring). voice_index: sound name -> clip number; durations: clip -> s."""
    out = [TALK_HEADER, f'# {sum(len(t["lines"]) for t in talks.values())} lines in {len(talks)} conversations']
    for tok, name in speakers.items():
        out.append(f'speaker {tok} {name}'.rstrip())
    for name, t in talks.items():
        out.append(f'conv {name}')
        for first, once, cond in t['starts']:
            out.append(f'start {first} {int(once)} {cond}')
        for i, l in enumerate(t['lines']):
            v = voice_index.get(l['sound'], -1)
            text = clean(l['text'])
            secs = durations.get(v) if v >= 0 else None
            if secs is None:
                secs = min(9.0, 1.2 + .055 * len(text)) if text else 0.0
            out.append(f'line {i} {l["speaker"]} {v} {secs:.2f} {text}'.rstrip())
        for r in t['resps']:
            out.append(f'resp {r["line"]} {r["to"]} {int(r["end"])} {r["action"]} {r["needs"]} {r.get("cond", "always")} '
                       f'{clean(r["text"])}'.rstrip())
    Path(path).write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')


# ---------------------------------------------------------------- voice (PS2 ZSND + VAG)
PSX_COEF = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


def decode_vag(data):
    """PS-ADPCM (a VAGp file or raw blocks) -> (rate, mono int16 list as bytes)."""
    import numpy as np
    rate = 22050
    if data[:4] == b'VAGp':
        rate = struct.unpack('>I', data[16:20])[0] or rate
        size = struct.unpack('>I', data[12:16])[0]
        body = data[48:48 + size] if 0 < size <= len(data) - 48 else data[48:]
    else:
        body = data
    n = len(body) // 16
    out = np.zeros(n * 28, dtype=np.int32)
    h1 = h2 = 0
    w = 0
    for b in range(n):
        blk = body[b * 16:b * 16 + 16]
        shift, pred = blk[0] & 15, (blk[0] >> 4) & 15
        shift = 9 if shift > 12 else shift  # the hardware's quirk for an oversized shift
        flags = blk[1]
        if flags == 7:  # end of the sample
            break
        c1, c2 = PSX_COEF[pred] if pred < 5 else (0, 0)
        for k in range(28):
            byte = blk[2 + k // 2]
            nib = (byte >> 4) if k & 1 else (byte & 15)
            if nib >= 8:
                nib -= 16
            s = (nib << 12) >> shift
            s += (h1 * c1 + h2 * c2) >> 6
            s = max(-32768, min(32767, s))
            out[w] = s
            w += 1
            h2, h1 = h1, s
    return rate, out[:w].astype('<i2').tobytes()


def wav_bytes(rate, pcm16):
    b = io.BytesIO()
    with wave.open(b, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(rate)
        wf.writeframes(pcm16)
    return b.getvalue()


def voice_bank(ens_path, scratch):
    """{SOUND NAME: vag bytes} of a ZSND bank (raven-formats reads it)."""
    from raven_formats.zsnd import read_zsnd
    scratch = Path(scratch)
    d = read_zsnd(Path(ens_path), scratch / 'bank.json')
    out = {}
    for s in d['sounds']:
        name = s['hash'] if isinstance(s['hash'], str) else str(s['hash'])
        idx = s.get('sample_index', -1)
        if 0 <= idx < len(d['samples']):
            f = Path(d['samples'][idx]['file'])
            f = f if f.is_absolute() else Path.cwd() / f
            if not f.exists():
                f = scratch / 'bank' / Path(d['samples'][idx]['file']).name
            if f.exists():
                out[name.upper()] = f.read_bytes()
    return out


def write_voices(folder, talks, bank):
    """Voice/v0000.wav ... for every line with a clip in `bank`; (index {sound: n}, durations {n: s})."""
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    for old in folder.glob('*.wav'):
        old.unlink()
    wanted = []
    for t in talks.values():
        for l in t['lines']:
            if l['sound'] and l['sound'] in bank and l['sound'] not in wanted:
                wanted.append(l['sound'])
    index, durations = {}, {}
    for n, name in enumerate(wanted):
        rate, pcm = decode_vag(bank[name])
        (folder / f'v{n:04d}.wav').write_bytes(wav_bytes(rate, pcm))
        index[name] = n
        durations[n] = len(pcm) / 2 / rate
    return index, durations


# ---------------------------------------------------------------- the cast model
def shelf_pack(sizes, page):
    """Places (w, h) boxes on page x page sheets (tallest first): [(sheet, x, y)] in the input order."""
    order = sorted(range(len(sizes)), key=lambda i: (-sizes[i][1], -sizes[i][0]))
    where = [None] * len(sizes)
    sheets = []  # [x, y, shelf height]
    for i in order:
        w, h = sizes[i]
        if w > page or h > page:
            raise ValueError('an image larger than the atlas page')
        for s, st in enumerate(sheets):
            if st[0] + w > page:
                st[0], st[1], st[2] = 0, st[1] + st[2], 0
            if st[1] + h <= page:
                where[i] = (s, st[0], st[1])
                st[0] += w
                st[2] = max(st[2], h)
                break
        else:
            sheets.append([w, 0, h])
            where[i] = (len(sheets) - 1, 0, 0)
    return where, len(sheets)


def glb_bytes(gltf, chunks):
    """A .glb from a glTF dict and its binary chunks (each 4-aligned); buffer 0 is the whole BIN."""
    blob = b''.join(chunks)
    gltf['buffers'] = [{'byteLength': len(blob)}]
    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    blob += b'\0' * (-len(blob) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    return (struct.pack('<III', 0x46546C67, 2, total) + struct.pack('<II', len(js), 0x4E4F534A) + js +
            struct.pack('<II', len(blob), 0x004E4942) + blob)


def pack_cast(looks, out_dir, cap, page=1024, pad=4):
    """looks: [{'name', 'frames': [N x 3 corners] (list of numpy (T*3,3) float32), 'uv': (T*3,2) glTF v-down,
    'image': (T,) int, 'images': [PIL images], 'seconds', 'turns', 'principal'}]. Drops frames (extras first, then
    principals) until every look fits under `cap` triangles. Writes scene.glb; returns the cast.txt look lines."""
    import numpy as np
    from PIL import Image
    tris = lambda: sum(len(l['image']) * len(l['frames']) for l in looks)
    while tris() > cap:
        pick = max((l for l in looks if len(l['frames']) > 1), default=None,
                   key=lambda l: (not l['principal'], len(l['frames'])))
        if pick is None:
            raise ValueError(f'the cast has {tris()} triangles even at one frame each, over {cap}')
        pick['frames'] = pick['frames'][::2]
    # the atlas: every image once (equal pixels shared), padded, on page x page sheets
    imgs, key_of = [], {}
    for l in looks:
        l['img_ids'] = []
        for im in l['images']:
            im = im.convert('RGBA')
            k = hashlib.sha1(im.tobytes() + bytes(str(im.size), 'ascii')).hexdigest()
            if k not in key_of:
                key_of[k] = len(imgs)
                imgs.append(im)
            l['img_ids'].append(key_of[k])
    where, sheets = shelf_pack([(im.width + 2 * pad, im.height + 2 * pad) for im in imgs], page)
    pages = [Image.new('RGBA', (page, page), (128, 128, 128, 255)) for _ in range(sheets)]
    for im, (s, x, y) in zip(imgs, where):
        tile = Image.new('RGBA', (im.width + 2 * pad, im.height + 2 * pad))
        tile.paste(im.resize((im.width + 2 * pad, im.height + 2 * pad)), (0, 0))  # the gutter: stretched edges
        tile.paste(im, (pad, pad))
        pages[s].paste(tile, (x, y))
    cutout = [bool(np.asarray(p)[:, :, 3].min() < 128) for p in pages]
    # one primitive per (look, frame, page): positions + atlas uvs, the triangle order the same every frame
    chunks, views, accessors, prims, off = [], [], [], [], 0

    def add(arr, target_type, comp, count, mn=None, mx=None):
        nonlocal off
        b = arr.astype('<f4').tobytes()
        chunks.append(b + b'\0' * (-len(b) % 4))
        views.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(b)})
        off += len(b) + (-len(b) % 4)
        acc = {'bufferView': len(views) - 1, 'componentType': 5126, 'count': count, 'type': target_type}
        if mn is not None:
            acc['min'], acc['max'] = mn, mx
        accessors.append(acc)
        return len(accessors) - 1

    lines, first = [], 0
    for l in looks:
        img_page = np.array([where[l['img_ids'][i]][0] if 0 <= i < len(l['img_ids']) else 0 for i in l['image']])
        order = np.argsort(img_page, kind='stable')
        corner_order = (order[:, None] * 3 + np.arange(3)[None, :]).reshape(-1)
        uv = l['uv'].astype(np.float64).copy()
        for t in range(len(l['image'])):
            i = l['image'][t]
            if not (0 <= i < len(l['img_ids'])):
                continue
            s, x, y = where[l['img_ids'][i]]
            im = imgs[l['img_ids'][i]]
            u = np.clip(uv[t * 3:t * 3 + 3, 0], .5 / im.width, 1 - .5 / im.width)
            v = np.clip(uv[t * 3:t * 3 + 3, 1], .5 / im.height, 1 - .5 / im.height)
            uv[t * 3:t * 3 + 3, 0] = (x + pad + u * im.width) / page
            uv[t * 3:t * 3 + 3, 1] = (y + pad + v * im.height) / page
        uv = uv[corner_order]
        pg = img_page[order]
        n = len(order)
        for f in l['frames']:
            pos = np.asarray(f, dtype=np.float64)[corner_order]
            for p in sorted(set(pg.tolist())):
                sel = np.repeat(pg == p, 3)
                P = pos[sel]
                a_pos = add(P, 'VEC3', 5126, len(P), P.min(0).tolist(), P.max(0).tolist())
                a_uv = add(uv[sel], 'VEC2', 5126, int(sel.sum()))
                prims.append({'attributes': {'POSITION': a_pos, 'TEXCOORD_0': a_uv}, 'material': p, 'mode': 4})
        lines.append(f"look {l['name']} {first} {n} {len(l['frames'])} {l['seconds']:.2f} {int(l['turns'])}")
        first += n * len(l['frames'])
    images, textures, materials = [], [], []
    for s, pg_img in enumerate(pages):
        b = io.BytesIO()
        pg_img.save(b, 'PNG')
        data = b.getvalue()
        chunks.append(data + b'\0' * (-len(data) % 4))
        views.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(data)})
        off += len(data) + (-len(data) % 4)
        images.append({'bufferView': len(views) - 1, 'mimeType': 'image/png'})
        textures.append({'source': s})
        m = {'name': f'cast_atlas_{s}', 'pbrMetallicRoughness': {'baseColorTexture': {'index': s},
                                                                  'metallicFactor': 0, 'roughnessFactor': 1},
             'extensions': {'KHR_materials_unlit': {}}}
        if cutout[s]:
            m['alphaMode'], m['alphaCutoff'] = 'MASK', .5
        materials.append(m)
    gltf = {'asset': {'version': '2.0', 'generator': 'HERO VR tools/ua/ua_cast.py (from the player\'s own disc)'},
            'extensionsUsed': ['KHR_materials_unlit'], 'scene': 0, 'scenes': [{'nodes': [0]}],
            'nodes': [{'mesh': 0, 'name': 'cast'}], 'meshes': [{'primitives': prims}], 'accessors': accessors,
            'bufferViews': views, 'images': images, 'textures': textures, 'materials': materials,
            'samplers': [{'magFilter': 9729, 'minFilter': 9987}]}
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    Path(out_dir, 'scene.glb').write_bytes(glb_bytes(gltf, chunks))
    return lines, first, sheets


def write_cast(path, look_lines, cast):
    out = [CAST_HEADER, '# look <name> <first> <triangles a frame> <frames> <loop s> <turns>',
           '# npc <id> <look> <x> <y> <z> <yaw deg> <group> <talk> <label>', *look_lines]
    for c in cast:
        x, y, z = c['at']
        out.append(f"npc {c['id']} {c['look']} {x:.3f} {y:.3f} {z:.3f} {c['yaw']:.1f} {c['group']} "
                   f"{c['talk'] or '-'} {c['label']}")
    Path(path).write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')


def load_built(look, npz_path):
    """A look (looks_of's) with build_cast.py's frames read back for pack_cast."""
    import numpy as np
    from PIL import Image
    d = np.load(npz_path)
    images = []
    k = 0
    while f'img{k}' in d:
        images.append(Image.fromarray(d[f'img{k}'], 'RGBA'))
        k += 1
    frames = list(d['frames'])
    lean = None
    if not look.get('principal'):
        frames, lean = straighten(frames)
    return dict(look, frames=frames, uv=d['uv'], image=d['image'], images=images,
                seconds=float(d['seconds']), lean=lean)


# The background people's talking / waiting loops (EA_ZONE7 / EA_ZONE10) hold the whole body pitched 11 to 23 degrees
# forward (the game's camera looks down from far above and never shows it; a pair is meant to lean in on each other).
# Close up, in VR, that reads as hunched and leaning at odd angles. Each such look is tilted back, as a whole and by
# one fixed angle for its loop (so its own motion stays), about its feet until its lean is `target` degrees (the
# principals' own idles lean 4 to 9) - and put back on the floor.
LEAN_TARGET = 5.0
LEAN_ABOVE = 9.0


def lean_of(frame):
    """(degrees from vertical, horizontal lean direction (x, z) unit) of a posed frame (glTF metres): its head
    (top 18 cm) over its feet (bottom 10 cm)."""
    import numpy as np
    P = np.asarray(frame, dtype=np.float64)
    y = P[:, 1]
    top, bot = P[y > y.max() - .18], P[y < y.min() + .10]
    v = top.mean(0) - bot.mean(0)
    h = float(np.hypot(v[0], v[2]))
    ang = float(np.degrees(np.arctan2(h, v[1])))
    return ang, (np.array([v[0], v[2]]) / h if h > 1e-6 else np.array([1.0, 0.0]))


def straighten(frames, target=LEAN_TARGET, above=LEAN_ABOVE):
    """frames (each (N, 3) glTF metres, y up, feet on y 0) -> (frames tilted back about the feet, the lean before) or
    the same frames when the loop's mean lean is `above` degrees or less."""
    import numpy as np
    leans = [lean_of(f) for f in frames]
    mean = float(np.mean([a for a, _ in leans]))
    if not (mean > above):
        return frames, mean
    out = []
    for f, (lean, dirv) in zip(frames, leans):
        # tilt back to `target` plus a quarter of this frame's own sway (its motion stays, the hunch goes)
        tilt = np.radians(max(lean - (target + .25 * (lean - mean)), 0.0))
        axis = np.array([dirv[1], 0.0, -dirv[0]])          # horizontal, across the lean
        c, s_ = np.cos(tilt), np.sin(tilt)
        P = np.asarray(f, dtype=np.float64)
        foot = P[P[:, 1] < P[:, 1].min() + .10].mean(0)
        foot[1] = P[:, 1].min()
        q = P - foot

        def rot(qq, sg):                                   # Rodrigues about `axis`
            return qq * c + np.cross(axis, qq) * (sg * s_) + np.outer(qq @ axis, axis) * (1 - c)
        best = min((rot(q, 1), rot(q, -1)), key=lambda r: lean_of(r + foot)[0])  # the turn that stands the head up
        R = best + foot
        R[:, 1] -= R[:, 1].min()                           # back on the floor
        out.append(R.astype(np.float32))
    return out, mean


# ---------------------------------------------------------------- talk.txt read back (previews, tests)
def read_talk_txt(path):
    """talk.txt -> ({token: name}, {conv: {'starts': [(line, once, cond)], 'lines': {i: (tok, voice, s, text)},
    'resps': [(line, to, end, action, needs, cond, text)]}})."""
    speakers, convs, cur = {}, {}, None
    for raw in Path(path).read_text(encoding='utf-8').splitlines():
        f = raw.split(' ')
        if not raw or raw.startswith('#') or f[0] == TALK_HEADER.split()[0]:
            continue
        if f[0] == 'speaker':
            speakers[f[1]] = ' '.join(f[2:])
        elif f[0] == 'conv':
            cur = convs.setdefault(f[1], {'starts': [], 'lines': {}, 'resps': []})
        elif f[0] == 'start':
            cur['starts'].append((int(f[1]), f[2] == '1', f[3]))
        elif f[0] == 'line':
            cur['lines'][int(f[1])] = (f[2], int(f[3]), float(f[4]), ' '.join(f[5:]))
        elif f[0] == 'resp':
            cur['resps'].append((int(f[1]), int(f[2]), f[3] == '1', f[4], int(f[5]), f[6], ' '.join(f[7:])))
    return speakers, convs


def passes(cond, hero=PLAYER_HERO):
    if cond == 'always':
        return True
    if cond.startswith('hero:'):
        return hero in cond[5:].split(',')
    if cond.startswith('not:'):
        return hero not in cond[4:].split(',')
    return False


def opening(conv, hero=PLAYER_HERO, used=()):
    """The line a conversation opens on for `hero` (the first start that passes and is not a spent run-once)."""
    for i, (line, once, cond) in enumerate(conv['starts']):
        if passes(cond, hero) and not (once and i in used):
            return i, line
    return -1, -1


def subtitle_mock(raw_png, out_png, speaker, text, hint='A: NEXT    B: END'):
    """The close-up with the subtitle panel HERO VR draws under a speaking character (a mock, for the preview)."""
    from PIL import Image, ImageDraw, ImageFont
    im = Image.open(raw_png).convert('RGBA')
    W, H = im.size
    layer = Image.new('RGBA', im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    try:
        big, small = ImageFont.load_default(size=int(H * .045)), ImageFont.load_default(size=int(H * .032))
    except TypeError:  # an old Pillow: its bitmap font
        big = small = ImageFont.load_default()
    words, rows, row = text.upper().split(), [], ''
    for wd in words:
        trial = (row + ' ' + wd).strip()
        if d.textlength(trial, font=small) > W * .74 and row:
            rows.append(row)
            row = wd
        else:
            row = trial
    rows.append(row)
    x0, x1 = int(W * .1), int(W * .9)
    y1 = int(H * .95)
    y0 = y1 - int(H * (.11 + .05 * len(rows)))
    d.rectangle([x0, y0, x1, y1], fill=(2, 8, 20, 205), outline=(15, 82, 140, 255), width=3)
    d.text((x0 + 24, y0 + 14), speaker.upper(), font=big, fill=(255, 179, 41, 255))
    for k, r in enumerate(rows):
        d.text((x0 + 24, y0 + 22 + int(H * .055) + k * int(H * .05)), r, font=small, fill=(204, 237, 255, 255))
    d.text((x1 - 24 - d.textlength(hint, font=small), y0 + 16), hint, font=small, fill=(64, 199, 255, 255))
    Image.alpha_composite(im, layer).convert('RGB').save(out_png)

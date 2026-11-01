"""HERO VR SM1 suits: the game files a suit needs, made the way Overstrike's MSMRSuitInstaller and ALERT's
suit exporter (Tkachov, GPL-3) make them: the four suit configs, the two base-game config edits (tech web
"Suits" list + unlock-for-free, and the vanity loadout list), the suit's name strings, and our own copies
of the suit material and its textures pointing at our hero's texture."""
import io
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
VENDOR = os.path.join(HERE, "vendor")
if VENDOR not in sys.path:
    sys.path.insert(0, VENDOR)
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import dat1lib  # noqa: E402
import dat1lib.crc32 as crc32  # noqa: E402
import dat1lib.crc64 as crc64  # noqa: E402
import dat1lib.types.config  # noqa: E402
import dat1lib.types.dat1  # noqa: E402
import dat1lib.types.sections.config.serialized as ser  # noqa: E402
import dat1lib.types.sections.config.references as refs  # noqa: E402
import bcn  # noqa: E402

# base-game assets (CONFIRMED ids: the ones Overstrike's MSMR installer edits)
AID_PROGRESSION = 0x9C9C72A303FCFA30   # configs\system\system_progression.config
AID_LOADOUTLIST = 0x9550E5741C2C7114   # configs\masteritemloadoutlist\masteritemloadoutlist.config
AID_VANITY_MASTER = 0x9CEADD22304ADD84
AID_VANITY_LAUNCH = 0x939887A999564798
AID_LOCALIZATION = 0xBE55D94F171BF8DE
CONFIG_MAGIC = 0x21A56F68
LOC_MAGIC = 0x122BB0AB
EXT_CONFIG = 0xA9F149C4
EXT_TEXTURE = 0x95A3A227
TEXTURE_MAGIC = 0x5C4580B9
MATERIAL_MAGIC = 0x1C04EF8C
T_TEX_HEADER = 0x4EDE3593
T_MAT_SERIALIZED = 0xF5260180
SLOTS_DIFFUSE = {0xABDAD780, 0x2EE380F1, 0x2E475254}
SLOTS_NORMAL = {0x7AE34E7A, 0xCAAA9AB5}


class AssetError(Exception):
    pass


def norm(path):
    return dat1lib.utils.normalize_path(path)


def aid(path):
    return crc64.hash(path)


# -- configs ---------------------------------------------------------------------------------------------

def make_config(type_name, content, references=()):
    """A fresh config asset, built exactly like ALERT's suit exporter (_make_config)."""
    cfg = dat1lib.types.config.Config.make()
    cfg.dat1.add_section_obj(ser.ConfigTypeSection.make({"Type": type_name}, [], cfg.dat1))
    cfg.dat1.add_section_obj(ser.ConfigContentSection.make(content, [], cfg.dat1))
    if references:
        rs = refs.ReferencesSection.make(cfg.dat1)
        cfg.dat1.add_section_obj(rs)
        for r in references:
            r = r.replace("/", "\\")
            rs.entries.append((crc64.hash(r), cfg.dat1.add_string(r), crc32.hash(os.path.splitext(r)[1])))
    f = io.BytesIO()
    cfg.save(f)
    return f.getvalue()


def read_config(data):
    if len(data) < 8 or struct.unpack('<I', data[:4])[0] != CONFIG_MAGIC:
        raise AssetError('not a config asset')
    return dat1lib.types.config.Config(io.BytesIO(data))


def save_config(cfg):
    """Writes a loaded config back: only sections dat1lib can serialise are rebuilt, the rest keep their bytes."""
    d = cfg.dat1
    for tag, ndx in d._sections_map.items():
        sec = d.sections[ndx]
        if sec is not None and hasattr(sec, 'save'):
            data = sec.save()
            if data is not None:
                d._sections_data[ndx] = bytearray(data)
    body = io.BytesIO()
    d.save(body)
    out = io.BytesIO()
    out.write(struct.pack('<II', cfg.magic, d.header.size))
    out.write(cfg.unk)
    out.write(body.getvalue())
    return out.getvalue()


def add_reference(cfg, path):
    """Overstrike's AddConfigReference: (crc64, path string, extension crc32), skipped if already there."""
    rs = cfg.get_references_section()
    if rs is None:
        rs = refs.ReferencesSection.make(cfg.dat1)
        cfg.dat1.add_section_obj(rs)
    a = crc64.hash(path)
    for e in rs.entries:
        if e[0] == a:
            return False
    ext = EXT_CONFIG if path.endswith('.config') else (EXT_TEXTURE if path.endswith('.texture') else 0)
    rs.entries.append((a, cfg.dat1.add_string(path), ext))
    return True


def suit_paths(suit_id):
    return {
        'reward': "configs\\inventory\\inv_reward_loadout_%s.config" % suit_id,
        'icon': "ui\\textures\\pause\\character\\suit_%s.texture" % suit_id,
        'equip': "configs\\equipment\\equip_techweb_suit_%s.config" % suit_id,
        'loadout': "configs\\masteritemloadoutlist\\itemloadout_spiderman_%s.config" % suit_id,
        'vanity': "configs/vanitytor1/vanity_spiderman_%s.config" % suit_id,
    }


def suit_configs(suit_id, model_path):
    """{path: bytes} of the four configs ALERT's MSMR suit exporter writes for one suit."""
    out = {}
    loadout = "configs/masteritemloadoutlist/itemloadout_spiderman_%s.config" % suit_id
    out["configs/inventory/inv_reward_loadout_%s.config" % suit_id] = make_config(
        "LoadoutRewardConfig", {"ItemLoadoutConfig": {"AssetPath": loadout, "Autoload": False}, "Name": suit_id, "Stackable": False})
    out["configs/equipment/equip_techweb_suit_%s.config" % suit_id] = make_config("TechItemConfig", {"Name": "Classic Suit Item"})
    body_type = "configs/VanityBodyType/VanityBody_SpiderMan.config"
    vanity_hed = "configs/VanityHED/VanityHEDSpiderMan1.config"
    vanity_tor1 = "configs/VanityTOR1/vanity_spiderman_%s.config" % suit_id
    out[loadout] = make_config("ItemLoadoutConfig", {
        "Loadout": {"ItemLoadoutLists": {"Items": [{"Item": body_type}, {"Item": vanity_hed}, {"Item": vanity_tor1}]}, "Name": suit_id}},
        [body_type, vanity_hed, "configs/vanitytor1/vanity_spiderman_classic.config", vanity_tor1])
    out["configs/vanitytor1/vanity_spiderman_%s.config" % suit_id] = make_config("VanityItemConfig", {
        "ModelList": {"Model": {"AssetPath": model_path, "Autoload": False}, "BodyType": "kAll"},
        "ShaderUpdater": {"Type": "SkinShaderUpdaterPrius"},
        "PartType": "kTypeBareTorsoAndArms",
        "Name": "Spider-Man Classic", "Category": "kDefaultItem", "Available": "kDefault"})
    return out


def _as_list(obj, key):
    """obj[key] as a list. A one-item array reads back as the item itself (dat1lib and Overstrike both treat
    an item count of 1 as a single value; the bytes are the same), so wrap it and store the list back."""
    v = obj.get(key) if isinstance(obj, dict) else None
    if v is None:
        return None
    if not isinstance(v, list):
        v = [v]
        obj[key] = v
    return v


def edit_progression(data, suits):
    """system_progression: one tech-web 'Suits' item per suit and each one unlocked for free (Overstrike)."""
    cfg = read_config(data)
    root = cfg.get_content_section().root
    lists = _as_list(root, "TechWebLists")
    if not isinstance(lists, list):
        raise AssetError('system_progression has no TechWebLists')
    target = None
    for e in lists:
        if isinstance(e, dict) and e.get("Description") == "Suits":
            target = e
            break
    if target is None or not isinstance(_as_list(target, "TechWebItems"), list):
        raise AssetError('system_progression has no "Suits" tech web list')
    items = target["TechWebItems"]
    free = _as_list(root, "UnlockForFree")
    if not isinstance(free, list):
        raise AssetError('system_progression has no UnlockForFree list')
    existing = set(i.get("Name") for i in items if isinstance(i, dict))
    for suit_id, has_name in suits:
        p = suit_paths(suit_id)
        for k in ('reward', 'icon', 'equip'):
            add_reference(cfg, p[k])
        name = "SUIT_" + suit_id
        key = ("SUIT_" + suit_id.upper()) if has_name else "CHARWEB_MAY"
        if name not in existing:
            items.append({"GivesItems": {"Item": norm(p['reward'])}, "SkillItem": norm(p['equip']), "UnhideByItem": norm(p['equip']),
                          "PreviewImage": norm(p['icon']), "DisplayName": key, "Description": key,
                          "Icon": "icon_tech_web_suit", "Name": name})
        if name not in free:
            free.append(name)
    return save_config(cfg)


def first_suit_icon(data):
    """The PreviewImage of the first suit already in the tech web (our icon is a copy of it)."""
    cfg = read_config(data)
    for e in _as_list(cfg.get_content_section().root, "TechWebLists") or []:
        if isinstance(e, dict) and e.get("Description") == "Suits":
            for i in _as_list(e, "TechWebItems") or []:
                if isinstance(i, dict) and i.get("PreviewImage"):
                    return i["PreviewImage"]
    return None


def suits_in_progression(data):
    cfg = read_config(data)
    for e in _as_list(cfg.get_content_section().root, "TechWebLists") or []:
        if isinstance(e, dict) and e.get("Description") == "Suits":
            return [i for i in _as_list(e, "TechWebItems") or [] if isinstance(i, dict)]
    return []


def edit_loadout_list(data, suit_ids):
    """masteritemloadoutlist: each suit's item loadout in the kVanity category (Overstrike)."""
    cfg = read_config(data)
    root = cfg.get_content_section().root
    cats = _as_list(root, "ItemLoadoutCategoryList")
    if not isinstance(cats, list):
        raise AssetError('masteritemloadoutlist has no ItemLoadoutCategoryList')
    van = None
    for c in cats:
        if isinstance(c, dict) and c.get("Category") == "kVanity":
            van = c
            break
    if van is None or not isinstance(_as_list(van, "ItemLoadoutConfigs"), list):
        raise AssetError('masteritemloadoutlist has no kVanity category')
    for suit_id in suit_ids:
        p = suit_paths(suit_id)['loadout']
        add_reference(cfg, p)
        if norm(p) not in van["ItemLoadoutConfigs"]:
            van["ItemLoadoutConfigs"].append(norm(p))
    return save_config(cfg)


def vanity_configs(data):
    """Reads the classic suit chain from the base configs for the dump: the classic vanity model path."""
    cfg = read_config(data)
    root = cfg.get_content_section().root
    try:
        return root["ModelList"]["Model"]["AssetPath"]
    except Exception:
        ml = root.get("ModelList")
        if isinstance(ml, list) and ml and isinstance(ml[0], dict):
            return ml[0].get("Model", {}).get("AssetPath")
    return None


# -- localization -----------------------------------------------------------------------------------------

T_LOC_COUNT, T_LOC_KEYS, T_LOC_KEYOFF, T_LOC_VALS, T_LOC_VALOFF = 0xD540A903, 0x4D73CEBD, 0xA4EA55B2, 0x70A382B8, 0xF80DEEB4
T_LOC_FLAGS, T_LOC_HASH, T_LOC_SHASH, T_LOC_SIDX = 0xB0653243, 0x06A58050, 0xC43731B5, 0x0CD2CFE9


def _strings(blob):
    out, start = {}, 0
    for i in range(len(blob)):
        if blob[i] == 0 or i == len(blob) - 1:
            end = i if blob[i] == 0 else i + 1
            out[start] = bytes(blob[start:end]).decode('utf-8', 'replace')
            start = i + 1
    return out


def edit_localization(data, entries):
    """Adds/sets key -> value strings and rebuilds the tables the way Overstrike's LocalizationHelper.Apply does."""
    magic, size = struct.unpack('<II', data[:8])
    if magic != LOC_MAGIC:
        raise AssetError('not a localization asset')
    unk = data[8:36]
    d = dat1lib.types.dat1.DAT1(io.BytesIO(data[36:36 + size]), None, ignore_sections_exceptions=True, version=dat1lib.VERSION_MSMR)
    need = (T_LOC_COUNT, T_LOC_KEYS, T_LOC_KEYOFF, T_LOC_VALS, T_LOC_VALOFF, T_LOC_FLAGS, T_LOC_HASH, T_LOC_SHASH, T_LOC_SIDX)
    for t in need:
        if t not in d._sections_map:
            raise AssetError('localization has no section %08X' % t)
    S = lambda t: bytes(d._sections_data[d._sections_map[t]])  # noqa: E731
    n = struct.unpack('<I', S(T_LOC_COUNT)[:4])[0]
    keys, vals = _strings(S(T_LOC_KEYS)), _strings(S(T_LOC_VALS))
    ko = struct.unpack('<%dI' % n, S(T_LOC_KEYOFF)[:4 * n])
    vo = struct.unpack('<%dI' % n, S(T_LOC_VALOFF)[:4 * n])
    flags = S(T_LOC_FLAGS)
    values, order = {}, []
    for i in range(n):
        k = keys.get(ko[i])
        if k is None:
            raise AssetError('localization key offset %d not found' % ko[i])
        if k not in values:
            order.append(k)
        values[k] = (vals.get(vo[i], ''), flags[i] if i < len(flags) else 0)
    for k, v in entries.items():
        values[k] = (v, values[k][1] if k in values else 0)
    n2 = len(values)
    kd, vd = bytearray(), bytearray()
    kmap, vmap = {}, {}

    def add(blob, m, s):
        if s in m:
            return m[s]
        off = len(blob)
        blob.extend(s.encode('utf-8') + b'\0')
        m[s] = off
        return off
    kofs, vofs, hashes, fl = [], [], [], bytearray()
    done = set()
    for k in (['INVALID'] if 'INVALID' in values else []) + sorted(values.keys(), key=lambda s: s.encode('utf-8')):
        if k in done:
            continue
        done.add(k)
        v, f = values[k]
        kofs.append(add(kd, kmap, k))
        vofs.append(add(vd, vmap, v))
        hashes.append(crc32.hash(k, normalize=False))
        fl.append(f & 0xFF)
    fl += bytes(4 * n2 - len(fl))
    pairs = sorted(((h, i) for i, h in enumerate(hashes)), key=lambda t: t[0])
    count_raw = bytearray(S(T_LOC_COUNT))
    count_raw[:4] = struct.pack('<I', n2)
    put = {T_LOC_COUNT: count_raw, T_LOC_KEYS: kd, T_LOC_VALS: vd,
           T_LOC_KEYOFF: struct.pack('<%dI' % n2, *kofs), T_LOC_VALOFF: struct.pack('<%dI' % n2, *vofs),
           T_LOC_HASH: struct.pack('<%dI' % n2, *hashes), T_LOC_FLAGS: fl,
           T_LOC_SHASH: struct.pack('<%dI' % n2, *[p[0] for p in pairs]), T_LOC_SIDX: struct.pack('<%dH' % n2, *[p[1] & 0xFFFF for p in pairs])}
    for t, b in put.items():
        d._sections_data[d._sections_map[t]] = bytearray(b)
    body = io.BytesIO()
    d.save(body)
    return struct.pack('<II', magic, d.header.size) + unk + body.getvalue()


def read_localization(data, key):
    """For tests and the dump: the value of one key (or None)."""
    magic, size = struct.unpack('<II', data[:8])
    d = dat1lib.types.dat1.DAT1(io.BytesIO(data[36:36 + size]), None, ignore_sections_exceptions=True, version=dat1lib.VERSION_MSMR)
    S = lambda t: bytes(d._sections_data[d._sections_map[t]])  # noqa: E731
    n = struct.unpack('<I', S(T_LOC_COUNT)[:4])[0]
    keys, vals = _strings(S(T_LOC_KEYS)), _strings(S(T_LOC_VALS))
    ko = struct.unpack('<%dI' % n, S(T_LOC_KEYOFF)[:4 * n])
    vo = struct.unpack('<%dI' % n, S(T_LOC_VALOFF)[:4 * n])
    for i in range(n):
        if keys.get(ko[i]) == key:
            return vals.get(vo[i])
    return None


# -- materials and textures ---------------------------------------------------------------------------------

_TEX_RE = re.compile(rb'(?<=\x00)[\x20-\x7e]{4,250}?\.texture(?=\x00)', re.I)


def material_textures(data):
    """[(path, role, slot hash or None)] of the textures a material names. Role is 'diffuse', 'normal' or
    'other': from the serialized-data slot hash when it is there (CANDIDATE slot ids), else from the name."""
    if len(data) < 40:
        raise AssetError('material too short')
    found = []
    for m in _TEX_RE.finditer(bytes(data)):
        p = m.group(0).decode('ascii')
        if p not in [f[0] for f in found]:
            found.append((p, None, None))
    slots = {}
    try:
        size = struct.unpack('<I', data[4:8])[0]
        d = dat1lib.types.dat1.DAT1(io.BytesIO(data[36:36 + size]), None, ignore_sections_exceptions=True, version=dat1lib.VERSION_MSMR)
        if T_MAT_SERIALIZED in d._sections_map:
            raw = bytes(d._sections_data[d._sections_map[T_MAT_SERIALIZED]])
            tc, tbs, tbe = struct.unpack('<III', raw[20:32])
            strings = raw[tbe:]
            for i in range(tc):
                spos, shash = struct.unpack('<II', raw[tbs + 8 * i:tbs + 8 * i + 8])
                end = strings.find(b'\0', spos)
                if end > spos:
                    slots[strings[spos:end].decode('ascii', 'replace')] = shash
    except Exception:
        pass
    out = []
    for p, _, _ in found:
        h = slots.get(p)
        low = p.lower()
        if h in SLOTS_DIFFUSE:
            role = 'diffuse'
        elif h in SLOTS_NORMAL:
            role = 'normal'
        elif h is None and re.search(r'(_c|_d|_diff|_diffuse|_albedo|_col|_color)\.texture$', low):
            role = 'diffuse'
        elif h is None and re.search(r'(_n|_nm|_nrm|_normal)\.texture$', low):
            role = 'normal'
        else:
            role = 'other'
        out.append((p, role, h))
    return out


def retarget_material(data, replace, log):
    """Our copy of a material: each old texture path in `replace` (old -> new, new no longer than old) is
    overwritten in place (zero padded, so no offset moves) and its crc64 asset id is swapped wherever it
    appears. Returns the new bytes."""
    b = bytearray(data)
    for old, new in replace.items():
        ob, nb = old.encode('ascii'), new.encode('ascii')
        if len(nb) > len(ob):
            raise AssetError('new texture path longer than the old one: %s' % new)
        pat = b'\x00' + ob + b'\x00'
        n_str = 0
        i = b.find(pat)
        while i >= 0:
            b[i + 1:i + 1 + len(ob)] = nb + bytes(len(ob) - len(nb))
            n_str += 1
            i = b.find(pat, i + 1)
        oa, na = struct.pack('<Q', crc64.hash(old)), struct.pack('<Q', crc64.hash(new))
        n_id = 0
        i = b.find(oa)
        while i >= 0:
            b[i:i + 8] = na
            n_id += 1
            i = b.find(oa, i + 8)
        log('material: %s -> %s (%d string(s), %d id(s))' % (old, new, n_str, n_id))
        if n_str == 0:
            raise AssetError('texture path not found in the material: %s' % old)
    return bytes(b)


def texture_info(data):
    magic, size = struct.unpack('<II', data[:8])
    d = dat1lib.types.dat1.DAT1(io.BytesIO(data[36:36 + size]), None, ignore_sections_exceptions=True, version=dat1lib.VERSION_MSMR)
    if T_TEX_HEADER not in d._sections_map:
        raise AssetError('texture without a header section')
    h = bytes(d._sections_data[d._sections_map[T_TEX_HEADER]])
    sd_len, hd_len, hd_w, hd_h, sd_w, sd_h, arr, stex, planes, fmt = struct.unpack('<IIHHHHHBBH', h[:22])
    sd_m, unk2, hd_m, unk3 = struct.unpack('<BBBB', h[30:34])
    return {'magic': '%08X' % magic, 'dat1_size': size, 'pixels_at': 36 + size, 'bytes': len(data), 'sd_len': sd_len, 'hd_len': hd_len,
            'hd': [hd_w, hd_h, hd_m], 'sd': [sd_w, sd_h, sd_m], 'array': arr, 'stex_format': stex, 'planes': planes, 'fmt': fmt,
            'unk_header': data[8:36].hex()}


def make_texture(donor, fmt, width, height, mips, pixels, log):
    """A texture asset with the whole mip chain in its own (SD) part and no HD part, using `donor` (a texture
    asset of the game) for every header byte we do not set. CANDIDATE: the game accepts SD-only textures."""
    magic, size = struct.unpack('<II', donor[:8])
    d = dat1lib.types.dat1.DAT1(io.BytesIO(donor[36:36 + size]), None, ignore_sections_exceptions=True, version=dat1lib.VERSION_MSMR)
    if T_TEX_HEADER not in d._sections_map:
        raise AssetError('donor texture has no header section')
    ndx = d._sections_map[T_TEX_HEADER]
    h = bytearray(d._sections_data[ndx])
    old_sd = struct.unpack('<I', h[:4])[0]
    struct.pack_into('<II', h, 0, len(pixels), 0)
    struct.pack_into('<HHHH', h, 8, width, height, width, height)
    struct.pack_into('<H', h, 20, fmt)
    h[30] = mips
    h[32] = 0
    # the header section sits in the donor's dat1 at a fixed spot: patch the bytes there (same length)
    off = [s.offset for s in d.header.sections if s.tag == T_TEX_HEADER][0]
    dat = bytearray(donor[36:36 + size])
    dat[off:off + len(h)] = h
    unk = bytearray(donor[8:36])
    for k in range(0, 28, 4):  # a copy of the SD length in the asset header, if there is one, follows it
        if struct.unpack('<I', unk[k:k + 4])[0] == old_sd and old_sd:
            struct.pack_into('<I', unk, k, len(pixels))
            log('texture header field +%d tracked the SD length (CANDIDATE)' % (8 + k))
    return struct.pack('<II', magic, size) + bytes(unk) + bytes(dat) + bytes(pixels)

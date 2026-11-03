# HERO VR 0.0.76 HARLEYS converter post-pass (Made by Agent Mulder): system python (numpy, PIL).
#   python3 tools/bike076/glbfix.py <mats.json>
# Blender exported the geometry with a 1 x 1 placeholder image per material; this puts in what the game reads
# (mesh_model: the base colour texture and the metallic / roughness FACTORS - it reads no other texture):
#  - each material's own base colour image, resized to texture_px (JPEG; PNG only where a cutout needs its alpha);
#    a near-uniform image becomes its mean colour in baseColorFactor (one texture layer fewer, the same look)
#  - a metal-rough source's metallicRoughness TEXTURE, which the game cannot read, sampled per triangle and folded
#    into factors: each primitive split by its triangles' metal and roughness (chrome stays chrome, the paint paint)
import sys, json, struct, io, hashlib
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
J = json.load(open(sys.argv[1]))
MATS, PX = J['mats'], J['texture_px']
_src_cache, _img_cache = {}, {}

def read_glb(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'glTF'
    n = struct.unpack('<I', b[12:16])[0]
    g = json.loads(b[20:20 + n])
    rest = b[20 + n:]
    binc = rest[8:8 + struct.unpack('<I', rest[:4])[0]] if len(rest) >= 8 else b''
    return g, binc

def source_image(ref):
    key = json.dumps(ref, sort_keys=True)
    if key in _img_cache: return _img_cache[key]
    if 'file' in ref:
        im = Image.open(ref['file'])
    else:
        if ref['glb'] not in _src_cache: _src_cache[ref['glb']] = read_glb(ref['glb'])
        g, binc = _src_cache[ref['glb']]
        bv = g['bufferViews'][g['images'][ref['image']]['bufferView']]
        o = bv.get('byteOffset', 0); im = Image.open(io.BytesIO(binc[o:o + bv['byteLength']]))
    im.load(); _img_cache[key] = im
    return im

def resized(im):
    w, h = im.size; s = min(1.0, PX / max(w, h))
    p2 = lambda v: 1 << max(2, int(round(np.log2(max(4, v * s)))))
    return im.resize((min(p2(w), PX), min(p2(h), PX)), Image.LANCZOS) if (w, h) != (min(p2(w), PX), min(p2(h), PX)) else im

def accessor(g, binc, i):
    a = g['accessors'][i]; bv = g['bufferViews'][a['bufferView']]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a['componentType']]
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', 0) or n * np.dtype(dt).itemsize
    raw = np.frombuffer(binc, np.uint8, count=stride * (a['count'] - 1) + n * np.dtype(dt).itemsize, offset=o)
    if stride == n * np.dtype(dt).itemsize: return raw.view(dt).reshape(a['count'], n)
    return np.stack([raw[k * stride:k * stride + n * np.dtype(dt).itemsize].view(dt) for k in range(a['count'])])

def fix(src, dst):
    g, binc = read_glb(src)
    chunks = []          # (bytes) for the new buffer, in bufferView order
    views = []
    for bv in g.get('bufferViews', []):
        o = bv.get('byteOffset', 0); chunks.append(binc[o:o + bv['byteLength']]); views.append(dict(bv))
    images, img_index, textures = [], {}, []
    used_views = set()
    def add_image(data, mime):
        h = hashlib.sha1(data).hexdigest()
        if h in img_index: return img_index[h]
        chunks.append(data); views.append(dict(buffer=0, byteLength=len(data)))
        images.append(dict(bufferView=len(views) - 1, mimeType=mime)); textures.append(dict(source=len(images) - 1, sampler=0))
        img_index[h] = len(textures) - 1
        return img_index[h]
    old_image_views = {im.get('bufferView') for im in g.get('images', [])}
    new_mats, layers, uniform = [], 0, 0
    mat_variant = {}     # (material index, metal bin, rough bin) -> new material index
    base_mat = {}        # old material index -> dict (pbr) for its variants
    for mi, m in enumerate(g.get('materials', [])):
        import re
        spec = MATS.get(re.sub(r'\.\d{3}$', '', m['name']))
        if spec is None: sys.exit('glbfix: no source material %r' % m['name'])
        f = list(spec['base']) + [1] * (4 - len(spec['base']))
        pbr = dict(metallicFactor=float(spec['metal']), roughnessFactor=float(spec['rough']))
        nm = dict(name=m['name'], doubleSided=m.get('doubleSided', False))
        if spec['alpha'] in ('MASK', 'BLEND'): nm['alphaMode'] = 'MASK'; nm['alphaCutoff'] = .5
        if spec['image']:
            im = source_image(spec['image'])
            cut = 'alphaMode' in nm and im.mode in ('RGBA', 'LA', 'P') and np.asarray(im.convert('RGBA'))[..., 3].min() < 250
            small = resized(im.convert('RGBA' if cut else 'RGB'))
            arr = np.asarray(small, np.float32) / 255
            if not cut and arr.reshape(-1, arr.shape[-1]).std(0).max() < 4 / 255:
                mean = arr.reshape(-1, 3).mean(0); f = [f[0] * mean[0], f[1] * mean[1], f[2] * mean[2], f[3]]; uniform += 1
            else:
                bio = io.BytesIO()
                if cut: small.save(bio, 'PNG', optimize=True); mime = 'image/png'
                else: small.save(bio, 'JPEG', quality=90, optimize=True); mime = 'image/jpeg'
                pbr['baseColorTexture'] = dict(index=add_image(bio.getvalue(), mime))
        if 'alphaMode' in nm and 'baseColorTexture' not in pbr: del nm['alphaMode'], nm['alphaCutoff']
        pbr['baseColorFactor'] = [round(float(c), 4) for c in f]
        nm['pbrMetallicRoughness'] = pbr
        base_mat[mi] = (nm, spec)
    # the primitives: a metal-rough source's MR texture folded per triangle into factors
    new_meshes = []
    def variant(mi, metal, rough):
        nm, spec = base_mat[mi]
        mb, rb = int(metal >= .5), min(int(rough / .25), 3)
        key = (mi, mb, rb)
        if key not in mat_variant:
            v = json.loads(json.dumps(nm))
            if spec['mr']:
                v['pbrMetallicRoughness']['metallicFactor'] = round(float(metal), 3)
                v['pbrMetallicRoughness']['roughnessFactor'] = round(float(rough), 3)
                v['name'] = '%s_m%d_r%d' % (nm['name'], mb, rb)
            mat_variant[key] = len(new_mats); new_mats.append(v)
        return mat_variant[key]
    mr_cache = {}
    for mesh in g.get('meshes', []):
        prims = []
        for p in mesh['primitives']:
            mi = p.get('material')
            spec = base_mat[mi][1] if mi is not None else None
            if mi is None or not spec['mr'] or 'TEXCOORD_0' not in p['attributes']:
                q = dict(p)
                if mi is not None: q['material'] = variant(mi, spec['metal'], spec['rough'])
                prims.append(q); continue
            idx = accessor(g, binc, p['indices']).reshape(-1, 3).astype(np.int64)
            uv = accessor(g, binc, p['attributes']['TEXCOORD_0']).astype(np.float64)
            key = json.dumps(spec['mr'], sort_keys=True)
            if key not in mr_cache:
                mr_cache[key] = np.asarray(resized(source_image(spec['mr']).convert('RGB')), np.float32) / 255
            mr = mr_cache[key]; H, W = mr.shape[:2]
            c = uv[idx].mean(1) % 1.0
            px = mr[np.clip((c[:, 1] * H).astype(int), 0, H - 1), np.clip((c[:, 0] * W).astype(int), 0, W - 1)]
            metal = px[:, 2] * spec['metal']; rough = px[:, 1] * spec['rough']
            groups = {}
            for t, (mm, rr) in enumerate(zip(metal, rough)):
                groups.setdefault((int(mm >= .5), min(int(rr / .25), 3)), []).append(t)
            for (mb, rb), ts in sorted(groups.items()):
                ts = np.array(ts)
                data = idx[ts].astype(np.uint32).tobytes()
                chunks.append(data); views.append(dict(buffer=0, byteLength=len(data), target=34963))
                g['accessors'].append(dict(bufferView=len(views) - 1, componentType=5125, count=len(ts) * 3, type='SCALAR'))
                q = dict(p); q['indices'] = len(g['accessors']) - 1
                q['material'] = variant(mi, float(metal[ts].mean()), float(rough[ts].mean()))
                prims.append(q)
        mesh['primitives'] = prims
    # drop the placeholder images, write the buffer
    keep_views = [i for i in range(len(views)) if i not in old_image_views]
    remap = {o: n for n, o in enumerate(keep_views)}
    out = bytearray(); new_views = []
    for o in keep_views:
        v = views[o]; v['byteOffset'] = len(out); v['buffer'] = 0; out += chunks[o]
        while len(out) % 4: out += b'\0'
        new_views.append(v)
    for a in g['accessors']:
        if 'bufferView' in a: a['bufferView'] = remap[a['bufferView']]
    for im in images: im['bufferView'] = remap[im['bufferView']]
    g['bufferViews'] = new_views; g['buffers'] = [dict(byteLength=len(out))]
    g['materials'] = new_mats
    if images:
        g['images'], g['textures'], g['samplers'] = images, textures, [dict(magFilter=9729, minFilter=9987, wrapS=10497, wrapT=10497)]
    else:
        for k in ('images', 'textures', 'samplers'): g.pop(k, None)
    g.pop('extensionsUsed', None); g.pop('extensionsRequired', None)
    g['asset'] = dict(version='2.0', generator='HERO VR 0.0.76 tools/bike076')
    js = json.dumps(g, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    total = 12 + 8 + len(js) + 8 + len(out)
    with open(dst, 'wb') as fo:
        fo.write(struct.pack('<4sII', b'glTF', 2, total)); fo.write(struct.pack('<I4s', len(js), b'JSON')); fo.write(js)
        fo.write(struct.pack('<I4s', len(out), b'BIN\0')); fo.write(out)
    tris = sum(g['accessors'][p['indices']]['count'] // 3 for m in g['meshes'] for p in m['primitives'])
    print('GLB %s: %d triangles, %d materials, %d images (%d near-uniform folded into their colour), %.1f MB'
          % (dst, tris, len(new_mats), len(images), uniform, total / 1e6))

for src, dst in J['jobs']:
    fix(src, dst)

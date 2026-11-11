#!/usr/bin/env python3
"""M0 extraction for Marvel: Ultimate Alliance (PS2) — read-only on the user's own ISO.

  ua_extract.py wad   <game.iso> <out_dir>           copy Z/ASSETSFB.WAD out of the ISO (ISO9660/UDF, no mount)
  ua_extract.py list  <assetsfb.wad|game.iso> [--maps] [--grep TEXT]
                                                     list the archive; --maps: only map packages, grouped
  ua_extract.py map   <assetsfb.wad|game.iso> <name> <out_dir>
                                                     pull every .fb whose path matches <name> (e.g. stark_tower),
                                                     unpack it (raven-formats FB layout), decompile .engb/.xmlb
                                                     to .xml, and print the map's .igb files.
  ua_extract.py fb    <package.fb> <out_dir>         unpack one FB package

The .wad is read with zipfile; if its central directory is non-standard, a fallback walks the local
PK\x03\x04 headers (stored/deflate). FB layout (raven-formats, nikita488): repeated
[128s path][64s type][u32 size LE][data]. Nothing is ever written back to the ISO or the .wad.
"""
import argparse, io, os, re, struct, sys, zipfile, zlib
from pathlib import Path

FB_HDR = struct.Struct('<128s64sI')


# ---------- ISO ----------
def _iso_find_wad(iso_path):
    import pycdlib
    iso = pycdlib.PyCdlib()
    iso.open(str(iso_path))
    try:
        hits = []
        # PS2 discs: ISO9660 level 1/2 (+ UDF bridge). Walk ISO9660 names.
        for dirname, _dirs, files in iso.walk(iso_path='/'):
            for f in files:
                name = f.split(';')[0]
                if name.lower() == 'assetsfb.wad':
                    hits.append(f"{dirname.rstrip('/')}/{f}")
        if not hits:
            raise SystemExit('assetsfb.wad not found on the ISO (expected under /Z/)')
        return iso, hits[0]
    except Exception:
        iso.close()
        raise


def iso_extract_wad(iso_path, out_dir):
    out_dir = Path(out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    iso, path = _iso_find_wad(iso_path)
    dst = out_dir / 'assetsfb.wad'
    with open(dst, 'wb') as f:
        iso.get_file_from_iso_fp(f, iso_path=path)
    iso.close()
    print(f'{path} -> {dst} ({dst.stat().st_size / 1e6:.1f} MB)')
    return dst


def resolve_wad(src, work):
    src = Path(src)
    if src.suffix.lower() == '.iso':
        cached = Path(work) / 'assetsfb.wad'
        return cached if cached.exists() else iso_extract_wad(src, work)
    return src


# ---------- WAD (zip-style) ----------
class Wad:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}  # name -> (offset of local header, method, csize, usize)
        self.zip = None
        try:
            self.zip = zipfile.ZipFile(self.path)
            self.names = [i.filename for i in self.zip.infolist() if not i.is_dir()]
        except zipfile.BadZipFile:
            self._scan()
            self.names = list(self.entries)

    def _scan(self):
        data = self.path.read_bytes()
        pos = 0
        while (pos := data.find(b'PK\x03\x04', pos)) >= 0:
            (_sig, _ver, flag, method, _t, _d, _crc, csize, usize, nlen, xlen) = struct.unpack_from('<IHHHHHIIIHH', data, pos)
            name = data[pos + 30:pos + 30 + nlen].decode('latin-1')
            start = pos + 30 + nlen + xlen
            if flag & 8 or (csize == 0 and usize == 0 and not name.endswith('/')):
                print(f'warning: {name}: data descriptor / unknown size, skipped', file=sys.stderr)
            elif not name.endswith('/'):
                self.entries[name] = (start, method, csize, usize)
            pos = start + max(csize, 0)
        self._data = data
        print(f'(fallback scan: {len(self.entries)} entries)', file=sys.stderr)

    def read(self, name):
        if self.zip:
            return self.zip.read(name)
        start, method, csize, _usize = self.entries[name]
        raw = self._data[start:start + csize]
        return raw if method == 0 else zlib.decompress(raw, -15)


# ---------- FB ----------
def fb_unpack(data, out_dir):
    out_dir = Path(out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    files, pos = [], 0
    manifest = ['<packagedef>']
    while pos + FB_HDR.size <= len(data):
        p, t, size = FB_HDR.unpack_from(data, pos)
        pos += FB_HDR.size
        p = p.split(b'\0', 1)[0].decode('latin-1').replace('\\', '/')
        t = t.split(b'\0', 1)[0].decode('latin-1')
        body = data[pos:pos + size]; pos += size
        manifest.append(f'    <{t} filename="{p}" />')
        if not p or size == 0:
            continue
        rel = Path(p)
        if rel.is_absolute() or '..' in rel.parts:
            print(f'skipped unsafe path {p}', file=sys.stderr); continue
        dst = out_dir / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(body)
        files.append((p, t, size))
    manifest.append('</packagedef>')
    (out_dir / '_package.xml').write_text('\n'.join(manifest) + '\n')
    if pos != len(data):
        print(f'warning: {len(data) - pos} trailing bytes', file=sys.stderr)
    return files


def decompile_xmlb(folder):
    try:
        from raven_formats import xmlb
    except ImportError:
        print('raven-formats missing: .engb/.xmlb left binary', file=sys.stderr); return
    for p in Path(folder).rglob('*'):
        if p.suffix.lower() in ('.engb', '.xmlb', '.chrb', '.boyb', '.pkgb') and p.is_file():
            try:
                xmlb.decompile(p, p.with_suffix(p.suffix + '.xml'), True)
            except Exception as e:  # .chrb/.boyb/.navb are not all xmlb
                print(f'  (not xmlb or failed: {p.name}: {e.__class__.__name__})', file=sys.stderr)


# ---------- commands ----------
MAP_RE = re.compile(r'(^|/)maps/', re.I)


def cmd_list(args):
    wad = Wad(resolve_wad(args.src, args.work))
    names = sorted(wad.names)
    if args.grep:
        names = [n for n in names if args.grep.lower() in n.lower()]
    if args.maps:
        fbs = [n for n in names if n.lower().endswith('.fb')]
        groups = {}
        for n in fbs:
            stem = Path(n).stem
            groups.setdefault(str(Path(n).parent), []).append(stem)
        for d, stems in sorted(groups.items()):
            print(f'{d}/  ({len(stems)})')
            for s in sorted(stems):
                print('   ', s)
        # map packages are named after their map; also show the maps.xmlb-style list if present
        print(f'\n{len(fbs)} .fb packages; tip: --grep stark  /  --grep act1')
    else:
        for n in names:
            print(n)
        print(f'\n{len(names)} entries', file=sys.stderr)


def cmd_map(args):
    wad = Wad(resolve_wad(args.src, args.work))
    want = args.name.lower()
    hits = [n for n in wad.names if n.lower().endswith('.fb') and want in n.lower()]
    exact = [n for n in hits if Path(n).stem.lower() == want]
    if exact and not args.all:
        hits = exact  # e.g. 'stark_tower' alone, not 'sim_stark_tower' too (use --all for every match)
    if not hits:
        raise SystemExit(f'no .fb package matches "{args.name}" — run: ua_extract.py list <wad> --maps --grep {args.name}')
    out = Path(args.out)
    for n in hits:
        sub = out / Path(n).stem
        files = fb_unpack(wad.read(n), sub)
        print(f'{n}: {len(files)} files -> {sub}')
        decompile_xmlb(sub)
    igbs = sorted(p for p in out.rglob('*.igb') if MAP_RE.search(str(p).replace(os.sep, '/')))
    print('\nmap geometry candidates:')
    for p in igbs or sorted(out.rglob('*.igb')):
        print(f'   {p}  ({p.stat().st_size / 1e6:.2f} MB)')


def cmd_fb(args):
    files = fb_unpack(Path(args.fb).read_bytes(), args.out)
    decompile_xmlb(args.out)
    for p, t, s in files:
        print(f'{t:16s} {s:10d}  {p}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--work', default=str(Path(__file__).resolve().parent.parent / 'work'), help='cache dir for assetsfb.wad')
    sp = ap.add_subparsers(dest='cmd', required=True)
    a = sp.add_parser('wad'); a.add_argument('iso'); a.add_argument('out')
    a.set_defaults(fn=lambda x: iso_extract_wad(x.iso, x.out))
    a = sp.add_parser('list'); a.add_argument('src'); a.add_argument('--maps', action='store_true'); a.add_argument('--grep')
    a.set_defaults(fn=cmd_list)
    a = sp.add_parser('map'); a.add_argument('src'); a.add_argument('name'); a.add_argument('out'); a.add_argument('--all', action='store_true')
    a.set_defaults(fn=cmd_map)
    a = sp.add_parser('fb'); a.add_argument('fb'); a.add_argument('out')
    a.set_defaults(fn=cmd_fb)
    args = ap.parse_args()
    args.fn(args)


if __name__ == '__main__':
    main()

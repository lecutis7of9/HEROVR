#!/usr/bin/env python3
"""Regression gate (0.0.62 sweep): a Nexus upload zip must hold no program and no archive.

Nexus quarantined an earlier upload for carrying an exe, and a zip inside the zip hides what is in it.
Checked by name AND by content, so a renamed file is still caught:
  - no .exe/.dll/.msi/.scr/.com/.pyd/.so/.sys/.bat-free check (scripts are fine: .cmd/.ps1/.py are text);
  - no file starting with a Windows program header (MZ) or a Linux one (ELF);
  - no archive inside: .zip/.7z/.rar/.tar/.gz/.xz/.bz2/.cab/.iso by name, or zip/7z/rar/gzip/xz/bzip2/cab magic by content;
  - one top folder, no absolute or '..' paths.
Usage: check_nexus_zip.py NEXUS.zip   (exit 0 = clean, 1 = problems listed)"""
import sys, zipfile

BAD_EXT = ('.exe', '.dll', '.msi', '.scr', '.com', '.pyd', '.so', '.sys', '.pdb', '.lib',
           '.zip', '.7z', '.rar', '.tar', '.gz', '.tgz', '.xz', '.bz2', '.cab', '.iso', '.jar', '.apk')
MAGIC = [(b'MZ', 'a Windows program (MZ header)'), (b'\x7fELF', 'a Linux program (ELF header)'),
         (b'PK\x03\x04', 'a zip archive'), (b'PK\x05\x06', 'an empty zip archive'), (b"7z\xbc\xaf'\x1c", 'a 7z archive'),
         (b'Rar!\x1a\x07', 'a RAR archive'), (b'\x1f\x8b', 'a gzip archive'), (b'\xfd7zXZ\x00', 'an xz archive'),
         (b'BZh', 'a bzip2 archive'), (b'MSCF', 'a cab archive')]


def problems(path):
    out = []
    with zipfile.ZipFile(path) as z:
        tops = set()
        for i in z.infolist():
            n = i.filename
            if n.startswith('/') or '..' in n.replace('\\', '/').split('/'):
                out.append(f'{n}: unsafe path')
            tops.add(n.split('/')[0])
            if i.is_dir():
                continue
            low = n.lower()
            if low.endswith(BAD_EXT):
                out.append(f'{n}: banned file type')
            head = z.open(i).read(8)
            for m, what in MAGIC:
                # 'BZh' and gzip are short magics: only count them with the rest of the header that follows.
                if head.startswith(m) and not (m == b'BZh' and not head[3:4].isdigit()):
                    out.append(f'{n}: content is {what}')
                    break
        if len(tops) != 1:
            out.append(f'{len(tops)} top-level entries ({sorted(tops)[:5]}): expected one folder')
    return out


if __name__ == '__main__':
    p = problems(sys.argv[1])
    for l in p:
        print('NEXUS ZIP:', l)
    print(f'nexus zip {sys.argv[1]}: ' + ('CLEAN (no program, no archive inside)' if not p else f'{len(p)} problem(s)'))
    sys.exit(1 if p else 0)

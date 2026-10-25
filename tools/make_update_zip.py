"""Update 34.1: makes an update zip whose file times can never be in the player's future.

A zip stores local clock times with no time zone. Update 34's carried this box's (UTC) times,
which on a PC behind UTC were hours ahead, and ninja then stopped with "manifest 'build.ninja'
still dirty after 100 tries". Every entry here is dated two days before now, earlier than any
clock on Earth, and tools/build.ps1 also re-dates future files before it configures.

    python3 tools/make_update_zip.py LIST_FILE OUT.zip
"""
import datetime
import pathlib
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def safe_time(now=None):
    t = (now or datetime.datetime.now(datetime.timezone.utc)) - datetime.timedelta(days=2)
    return (t.year, t.month, t.day, t.hour, t.minute, t.second // 2 * 2)


def make(names, out, now=None):
    stamp = safe_time(now)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for name in names:
            info = zipfile.ZipInfo(name, stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, (ROOT / name).read_bytes())
    return stamp


if __name__ == '__main__':
    names = [n.strip() for n in pathlib.Path(sys.argv[1]).read_text().splitlines() if n.strip()]
    print(len(names), 'files dated', make(names, sys.argv[2]))

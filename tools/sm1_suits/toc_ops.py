"""Reads and edits Spider-Man Remastered's asset_archive\\toc (the game's own mod mechanism).

Uses the vendored dat1lib (ALERT by Tkachov, GPL-3). The toc edits follow what Overstrike / ALERT's suit
installer do (append an archive entry, add or re-point asset entries, keep each span sorted), with two fixes:
ArchiveFileEntry is built by hand (its make() is broken) and the span table is written back (dat1lib has no
SpansSection.save). Asset ids are crc64 of the lower-case, back-slash-free path (dat1lib.crc64.hash)."""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
VENDOR = os.path.join(HERE, "vendor")
if VENDOR not in sys.path:
    sys.path.insert(0, VENDOR)

import dat1lib  # noqa: E402
import dat1lib.crc32 as crc32  # noqa: E402
import dat1lib.crc64 as crc64  # noqa: E402
import dat1lib.types.toc  # noqa: E402
import dat1lib.types.sections.toc.archives as s_arch  # noqa: E402
import dat1lib.types.sections.toc.sizes as s_sizes  # noqa: E402
import dat1lib.types.sections.toc.offsets as s_offs  # noqa: E402
import dat1lib.types.sections.toc.spans as s_spans  # noqa: E402
import dat1lib.types.sections.toc.asset_ids as s_ids  # noqa: E402

TAG_ARCHIVES = s_arch.ArchivesSection.TAG
TAG_IDS = s_ids.AssetIdsSection.TAG
TAG_SIZES = s_sizes.SizesSection.TAG
TAG_OFFSETS = s_offs.OffsetsSection.TAG
TAG_SPANS = s_spans.SpansSection.TAG
TOC_MAGIC = 0x77AF12AF


def aid_of(path):
    """Asset id of a game path (CONFIRMED rule: crc64 of the normalized path, as ALERT and Overstrike compute it)."""
    return crc64.hash(path)


def ext_hash(path):
    return crc32.hash(os.path.splitext(path)[1])


class Toc:
    def __init__(self, toc_path):
        self.path = toc_path
        self.archives_dir = os.path.dirname(os.path.abspath(toc_path))
        with open(toc_path, "rb") as f:
            magic = struct.unpack("<I", f.read(4))[0]
            if magic != TOC_MAGIC:
                raise ValueError("not a Spider-Man Remastered toc (magic %08X)" % magic)
            f.seek(0)
            self.toc = dat1lib.types.toc.TOC(f, version=dat1lib.VERSION_MSMR)
        self.toc.set_archives_dir(self.archives_dir)
        d = self.toc.dat1
        for tag in (TAG_ARCHIVES, TAG_IDS, TAG_SIZES, TAG_OFFSETS, TAG_SPANS):
            if d.get_section(tag) is None:
                raise ValueError("toc has no section %08X" % tag)
        self._index = None

    # --- sections ---
    @property
    def ids(self): return self.toc.dat1.get_section(TAG_IDS).ids
    @property
    def sizes(self): return self.toc.dat1.get_section(TAG_SIZES).entries
    @property
    def offsets(self): return self.toc.dat1.get_section(TAG_OFFSETS).entries
    @property
    def spans(self): return self.toc.dat1.get_section(TAG_SPANS).entries
    @property
    def archives(self): return self.toc.dat1.get_section(TAG_ARCHIVES).archives

    def archive_names(self):
        out = []
        for a in self.archives:
            fn = bytes(a.filename)
            out.append(fn.split(b"\0", 1)[0].decode("ascii", "replace"))
        return out

    def span_of(self, index):
        for s, e in enumerate(self.spans):
            if e.asset_index <= index < e.asset_index + e.count:
                return s
        return -1

    def index(self):
        if self._index is None:
            m = {}
            for i, a in enumerate(self.ids):
                m.setdefault(a, []).append(i)
            self._index = m
        return self._index

    def find(self, aid, span=None):
        """Index of the asset (in `span` if given, else the first one), or -1."""
        for i in self.index().get(aid, []):
            if span is None or self.span_of(i) == span:
                return i
        return -1

    def read_asset(self, aid, span=None):
        i = self.find(aid, span)
        if i < 0:
            return None
        return bytes(self.toc.extract_asset(i))

    # --- edits ---
    def add_archive(self, name):
        """Index of archive `name` (e.g. 'HeroVR\\\\models'), appending it like Overstrike's suit tool does if missing."""
        names = self.archive_names()
        if name in names:
            return names.index(name)
        raw = name.encode("ascii")
        if len(raw) >= 64:
            raise ValueError("archive name too long")
        sec = self.toc.dat1.get_section(TAG_ARCHIVES)
        index = len(sec.archives)
        data = struct.pack("<II", 0, 10000 + index) + raw + b"\0" * (64 - len(raw))
        sec.archives.append(s_arch.ArchiveFileEntry(data, sec.version))
        return index

    def put_asset(self, aid, archive_index, offset, size, span=0):
        """Re-point an existing asset (first one found in `span`) or add it to `span`. Returns 'added'/'rerouted'."""
        i = self.find(aid, span)
        state = "rerouted"
        if i < 0:
            state = "added"
            sp = self.spans[span]
            i = sp.asset_index + sp.count
            sp.count += 1
            for e in self.spans[span + 1:]:
                e.asset_index += 1
            self.ids.insert(i, aid)
            self.sizes.insert(i, s_sizes.SizeEntry(struct.pack("<III", 1, 0, i)))
            self.offsets.insert(i, s_offs.OffsetEntry(struct.pack("<II", 0, 0)))
            self._index = None
        self.sizes[i].value = size
        self.offsets[i].archive_index = archive_index
        self.offsets[i].offset = offset
        return state

    def sort(self):
        ids, sizes, offs = self.ids, self.sizes, self.offsets
        for sp in self.spans:
            r = range(sp.asset_index, sp.asset_index + sp.count)
            vals = sorted(((ids[i], sizes[i], offs[i]) for i in r), key=lambda v: v[0])
            for k, i in enumerate(r):
                ids[i], sizes[i], offs[i] = vals[k]
        for i in range(len(ids)):
            sizes[i].index = i
        self._index = None

    def save(self, out_path):
        d = self.toc.dat1
        spans = d.get_section(TAG_SPANS)
        spans._raw = b"".join(struct.pack("<II", e.asset_index, e.count) for e in spans.entries)
        for tag in (TAG_ARCHIVES, TAG_IDS, TAG_SIZES, TAG_OFFSETS, TAG_SPANS):
            d.refresh_section_data(tag)
        d.recalculate_section_headers()
        tmp = out_path + ".herovr-tmp"
        with open(tmp, "wb") as f:
            self.toc.save(f)
        os.replace(tmp, out_path)

    def close(self):
        self.toc.set_archives_dir(self.archives_dir)

"""Helpers for tests that read lv_font_conv binaries the way the firmware's loader does."""
import os
import shutil
import struct
from pathlib import Path


def converter_available() -> bool:
    return bool(os.environ.get('LV_FONT_CONV') or shutil.which('lv_font_conv') or shutil.which('npx'))


def font_facts(path: Path) -> dict:
    """Read an lv_font_conv binary the way lv_font_load does: the head table, then the codepoints the
    cmap tables cover."""
    b = path.read_bytes()
    head_len, tag = struct.unpack('<I4s', b[:8])
    (_version, _tables, size, _asc, _desc, _tasc, _tdesc, _tgap, _miny, _maxy, _adv, _kscale, _i2l, _gid, _advfmt,
     bpp, _xy, _wh, _advbits, compression, _sub, _pad, _upos, _uthick) = struct.unpack_from('<IHHhhHhHhhHHBBBBBBBBBBhH', b, 8)
    cmap_at = head_len
    _, cmap_tag, n = struct.unpack('<I4sI', b[cmap_at:cmap_at + 12])
    assert cmap_tag == b'cmap', cmap_tag
    codepoints = set()
    for i in range(n):
        at = cmap_at + 12 + i * 16
        data_offset, start, length, _gid, count, fmt, _pad = struct.unpack('<IIHHHBB', b[at:at + 16])
        if fmt in (0, 2):                                   # a contiguous range
            codepoints.update(range(start, start + length))
        else:                                               # sparse: 16-bit offsets from `start`
            offs = struct.unpack(f'<{count}H', b[cmap_at + data_offset:cmap_at + data_offset + 2 * count])
            codepoints.update(start + o for o in offs)
    return {'tag': tag, 'size': size, 'bpp': bpp, 'compression': compression, 'codepoints': codepoints}


def font_table_bytes(path: Path) -> int:
    """Bytes accounted for by an lv_font_conv binary's tables (head, cmap, loca, glyf, kern): each starts with its
    own length, so together they must add up to the file. LVGL's loader does not check how much it read, so a
    truncated font loads and draws garbage; this is what notices."""
    b = path.read_bytes()
    at = 0
    while at + 8 <= len(b):
        length, = struct.unpack('<I', b[at:at + 4])
        if length < 8:
            break
        at += length
    return at

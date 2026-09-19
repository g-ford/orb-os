"""Writes the PNGs tests/png_decode_test.cpp reads. Standard library only (zlib + struct),
so the test needs no Pillow."""
import os, random, struct, sys, zlib


def chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def png(width, height, color_type, rows, plte=None):
    raw = b"".join(b"\x00" + r for r in rows)  # filter type 0 on every scanline
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0))
    if plte:
        out += chunk(b"PLTE", plte)
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    return out + chunk(b"IEND", b"")


def main(d):
    # Pixel (x, y) = (x*60, y*80, 200-x*10) with alpha 255-x*40.
    rows = [b"".join(bytes((x * 60, y * 80, 200 - x * 10, 255 - x * 40)) for x in range(4)) for y in range(3)]
    open(os.path.join(d, "rgba_4x3.png"), "wb").write(png(4, 3, 6, rows))

    # Incompressible content so the IDAT is large enough to truncate mid-stream.
    rnd = random.Random(7)
    big_rows = [bytes(rnd.randrange(256) for _ in range(96 * 4)) for _ in range(96)]
    big = png(96, 96, 6, big_rows)
    open(os.path.join(d, "noise_96x96.png"), "wb").write(big)
    # Header and the start of IDAT intact, the rest gone: opens fine, cannot decode.
    open(os.path.join(d, "noise_96x96_truncated.png"), "wb").write(big[: len(big) // 2])

    # Palette image (color type 3): valid, but not the 8-bit RGBA the loaders expect.
    plte = bytes(v for i in range(4) for v in (i * 40, i * 30, i * 20))
    prow = [bytes((x % 4) for x in range(4)) for _ in range(3)]
    open(os.path.join(d, "palette_4x3.png"), "wb").write(png(4, 3, 3, prow, plte=plte))


main(sys.argv[1])

#!/usr/bin/env python3
from __future__ import annotations

import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THEME_ROOT = ROOT / "src" / "theme"
OUT_ROOT = ROOT / "src" / "theme_assets"


def parse_ints(text: str):
    tokens = re.findall(r"0x[0-9A-Fa-f]+|\d+", text)
    out = []
    for token in tokens:
        if token.lower().startswith("0x"):
            out.append(int(token, 16))
        else:
            out.append(int(token, 10))
    return out


def write_png(path: Path, width: int, height: int, rgba_bytes: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        start = y * stride
        raw.extend(rgba_bytes[start:start + stride])

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack("!I", len(data))
            + tag
            + data
            + struct.pack("!I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack("!IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))
    path.write_bytes(bytes(png))


def rgb565_to_rgb888(value: int) -> tuple[int, int, int]:
    r5 = (value >> 11) & 0x1F
    g6 = (value >> 5) & 0x3F
    b5 = value & 0x1F
    r = (r5 << 3) | (r5 >> 2)
    g = (g6 << 2) | (g6 >> 4)
    b = (b5 << 3) | (b5 >> 2)
    return r, g, b


def render_rgb565_array(values: list[int], width: int, height: int, out_path: Path):
    rgba = bytearray()
    for v in values[: width * height]:
        r, g, b = rgb565_to_rgb888(v)
        rgba.extend((r, g, b, 255))
    write_png(out_path, width, height, bytes(rgba))


def render_rgba_bytes(values: list[int], width: int, height: int, out_path: Path):
    rgba = bytearray()
    for i in range(0, min(len(values), width * height * 3), 3):
        lo = values[i]
        hi = values[i + 1]
        alpha = values[i + 2]
        rgb565 = lo | (hi << 8)
        r, g, b = rgb565_to_rgb888(rgb565)
        rgba.extend((r, g, b, alpha))
    write_png(out_path, width, height, bytes(rgba))


def is_png_blob(values: list[int]) -> bool:
    return len(values) >= 8 and values[:8] == [137, 80, 78, 71, 13, 10, 26, 10]


def strip_symbol_suffixes(name: str) -> str:
    for suffix in ("_PNG", "_IMG", "_MAP", "_ARRAY"):
        if name.endswith(suffix):
            return name[: -len(suffix)]
    return name


def find_dims_for_symbol(symbol: str, macro_map: dict[str, int]):
    base = strip_symbol_suffixes(symbol)
    candidates = [symbol, base, f"{base}_IMG", f"{base}_MAP", f"{base}_PNG"]
    for candidate in candidates:
        width = macro_map.get(f"{candidate}_W")
        height = macro_map.get(f"{candidate}_H")
        if width is not None and height is not None:
            return width, height
    return None, None


def out_dir_for(path: Path, symbol: str) -> Path:
    path_text = str(path).lower()
    symbol_text = symbol.lower()
    if "office" in path_text or "office" in symbol_text:
        return OUT_ROOT / "office"
    if "custom" in path_text or "dial" in symbol_text or "hand_" in symbol_text:
        return OUT_ROOT / "default"
    return OUT_ROOT / "default"


def export_png_blob(values: list[int], out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(values))


def build_macro_map() -> dict[str, int]:
    macro_map: dict[str, int] = {}
    for file in sorted(THEME_ROOT.rglob("*.h")):
        text = file.read_text(encoding="utf-8", errors="ignore")
        for match in re.finditer(r"#define\s+([A-Z0-9_]+)\s+(\d+)", text):
            name = match.group(1)
            if name.endswith("_W") or name.endswith("_H"):
                macro_map[name] = int(match.group(2))
    return macro_map


def export_file(path: Path, macro_map: dict[str, int]):
    text = path.read_text(encoding="utf-8", errors="ignore")
    matches = list(re.finditer(r"static\s+const\s+(?:uint8_t|uint16_t|unsigned\s+char)\s+([A-Z0-9_]+)\s*(?:\[[^\]]*\])?\s*=\s*\{(.*?)\}\s*;", text, re.S))
    if not matches:
        return

    for match in matches:
        symbol = match.group(1)
        values = parse_ints(match.group(2))
        if not values:
            continue

        out_dir = out_dir_for(path, symbol)
        out_path = out_dir / f"{symbol.lower()}.png"

        if is_png_blob(values):
            export_png_blob(values, out_path)
            continue

        width, height = find_dims_for_symbol(symbol, macro_map)
        if width is None or height is None:
            continue

        if len(values) >= width * height * 3 and len(values) % 3 == 0:
            render_rgba_bytes(values, width, height, out_path)
        elif len(values) >= width * height:
            render_rgb565_array(values, width, height, out_path)


def main():
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    for folder in (OUT_ROOT / "default", OUT_ROOT / "office"):
        if folder.exists():
            for child in folder.iterdir():
                if child.is_file():
                    child.unlink()
    macro_map = build_macro_map()
    for file in sorted(THEME_ROOT.rglob("*.h")):
        export_file(file, macro_map)
    print(f"Exported canonical theme previews under {OUT_ROOT}")


if __name__ == "__main__":
    main()

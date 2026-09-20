#!/usr/bin/env python3
"""Draws the artwork for the Fallout theme into src/theme_assets/fallout/.

    python3 tools/fallout_art.py            # write the PNGs
    python3 tools/fallout_art.py --preview  # also write a contact sheet to build/fallout_preview.png

Needs Pillow and numpy (pip3 install pillow numpy). Original artwork in the spirit of a wrist-worn
terminal from the games: green phosphor on a black tube, scanlines, a fine grid, tick marks. No text
is drawn into the images (the firmware draws all text with its own fonts), and none of the games'
art, logos or mascots are used.

It is deliberately GREEN and only green: tests/test_fallout_theme.py fails on any pixel whose red or
blue channel is above its green one, which is what keeps amber from creeping in.

The drawing machinery (supersampling, glow, masks, the dial circle) is portal_art.py's; this file is
only the look. Everything is drawn at 3x and scaled down, and every plate is black outside the
inscribed circle the round display shows.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from portal_art import (CX, CY, N, SS, W, compose_clock, disc, finish, glow_of, line_polar, new, over,
                        radial, ring_mask, sc, sector_mask)

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'src' / 'theme_assets' / 'fallout'

# palette, brightest first. theme.yaml states the same values.
HI = (27, 255, 128)         # 0x1BFF80  the lit phosphor
PALE = (182, 255, 210)      # 0xB6FFD2  hot core of a bright line
MID = (17, 178, 90)         # 0x11B25A
LO = (11, 122, 62)          # 0x0B7A3E
DIM = (6, 64, 33)           # 0x064021
DEEP = (2, 26, 12)          # 0x021A0C  the tube's own glow, the fill of a hollow shape


def rgba(color, alpha=255):
    return tuple(color) + (alpha,)


# ---- the tube --------------------------------------------------------------------------

def screen() -> Image.Image:
    """The glass: a faint green glow in the middle, falling to black at the edge."""
    return over(new((0, 0, 0, 255)), radial((4, 44, 22), (0, 4, 2), 0, 233), disc(233))


def lit(layer: Image.Image, r: float = 2.0) -> Image.Image:
    """A drawn layer with phosphor bloom: a wide soft halo, a tight one, then the line itself."""
    return over(over(glow_of(layer, r * 3.2, 1.5), glow_of(layer, r, 1.3)), layer)


def scanlines(img: Image.Image) -> Image.Image:
    """Every third row a little dimmer, like the raster of a CRT. Done at final size so it is crisp."""
    a = np.asarray(img.convert('RGBA')).copy()
    a[2::3, :, :3] = (a[2::3, :, :3] * 0.72).astype(np.uint8)
    return Image.fromarray(a, 'RGBA')


def plate(img: Image.Image) -> Image.Image:
    return scanlines(finish(img))


# ---- motifs ----------------------------------------------------------------------------

def ellipse_box(r: float, cx: float = CX, cy: float = CY) -> list:
    return [sc(cx - r), sc(cy - r), sc(cx + r), sc(cy + r)]


def ring(r: float, color=LO, alpha=210, width=1.6, cx: float = CX, cy: float = CY) -> Image.Image:
    layer = new()
    ImageDraw.Draw(layer).ellipse(ellipse_box(r, cx, cy), outline=rgba(color, alpha), width=sc(width))
    return layer


def dashed_ring(r: float, color=LO, alpha=210, width=1.6, dash=4, gap=3) -> Image.Image:
    layer = new()
    d = ImageDraw.Draw(layer)
    for a in range(0, 360, dash + gap):
        d.arc(ellipse_box(r), a, a + dash, fill=rgba(color, alpha), width=sc(width))
    return layer


def grid(step: float, color=DIM, alpha=110, width=1.0, clip_r=214) -> Image.Image:
    """Square graticule through the middle, clipped to a circle."""
    layer = new()
    d = ImageDraw.Draw(layer)
    for i in range(-int(233 // step) - 1, int(233 // step) + 2):
        p = sc(CX + i * step)
        d.line([(p, 0), (p, N)], fill=rgba(color, alpha), width=sc(width))
        d.line([(0, p), (N, p)], fill=rgba(color, alpha), width=sc(width))
    return over(new(), layer, disc(clip_r))


def polar(d: ImageDraw.ImageDraw, r0, r1, deg, color, width, cx: float = CX, cy: float = CY):
    a = math.radians(deg - 90)
    d.line([(sc(cx + r0 * math.cos(a)), sc(cy + r0 * math.sin(a))),
            (sc(cx + r1 * math.cos(a)), sc(cy + r1 * math.sin(a)))], fill=color, width=sc(width))


def rule(y: float, x0: float, x1: float, color=HI, width=1.8, cap=7, alpha=255) -> Image.Image:
    """A horizontal rule with a short upright tick at each end, the frame of a terminal panel."""
    layer = new()
    d = ImageDraw.Draw(layer)
    c = rgba(color, alpha)
    d.line([(sc(x0), sc(y)), (sc(x1), sc(y))], fill=c, width=sc(width))
    for x in (x0, x1):
        d.line([(sc(x), sc(y - cap)), (sc(x), sc(y + cap))], fill=c, width=sc(width))
    return layer


def chord_rule(y: float, r: float = 206, **kw) -> Image.Image:
    """A rule as long as the circle of radius r is wide at height y, so it stops short of the bezel."""
    half = math.sqrt(max(r * r - (y - CY) ** 2, 0))
    return rule(y, CX - half, CX + half, **kw)


def chevron(x: float, y: float, h: float, facing: int, color=HI, width=3.0) -> Image.Image:
    """A '<' (facing -1) or '>' (facing 1)."""
    layer = new()
    d = ImageDraw.Draw(layer)
    tip = x + facing * h * 0.5
    d.line([(sc(x - facing * h * 0.5), sc(y - h)), (sc(tip), sc(y)), (sc(x - facing * h * 0.5), sc(y + h))],
           fill=rgba(color), width=sc(width), joint='curve')
    return layer


def bezel(r_in: float, r_out: float = 233.0) -> Image.Image:
    """The dark housing round the glass: a bright inner edge and a ring of notches."""
    room = r_out - r_in
    out = over(new(), radial((4, 40, 20), (0, 10, 5), r_in, r_out), ring_mask(r_in, r_out))
    edge = new()
    d = ImageDraw.Draw(edge)
    r = r_in - 1.2
    d.ellipse(ellipse_box(r), outline=rgba(HI), width=sc(1.8))
    for deg in range(0, 360, 10):
        major = deg % 90 == 0
        polar(d, r_in + room * 0.2, r_in + room * (0.7 if major else 0.55 if deg % 30 == 0 else 0.4), deg,
              rgba(HI if major else MID), 1.6 if major else 1.2)
    return over(out, lit(edge, 1.6))


def ticks(r_out: float, minor: float, hour: float, quarter: float, minor_step=6, glow=True) -> Image.Image:
    """Dial ticks reaching in from r_out: minor every `minor_step` degrees, hour every 30, quarter every 90."""
    layer = new()
    d = ImageDraw.Draw(layer)
    for deg in range(0, 360, minor_step):
        if deg % 90 == 0:
            polar(d, r_out - quarter, r_out, deg, rgba(HI), 4.4)
        elif deg % 30 == 0:
            polar(d, r_out - hour, r_out, deg, rgba(HI), 3.0)
        else:
            polar(d, r_out - minor, r_out, deg, rgba(LO), 1.4)
    return lit(layer, 1.8) if glow else layer


def corner_brackets(r: float, half_deg: float = 7, width=3.0) -> Image.Image:
    """Four short arcs on the diagonals: the corners of a targeting frame, bent round the tube."""
    layer = new()
    d = ImageDraw.Draw(layer)
    for centre in (45, 135, 225, 315):
        d.arc(ellipse_box(r), centre - 90 - half_deg, centre - 90 + half_deg, fill=rgba(HI), width=sc(width))
    return lit(layer, 1.8)


# ---- the sprites -----------------------------------------------------------------------

def halo(big: Image.Image, blur: float, gain: float = 2.0) -> Image.Image:
    """`big` with a soft glow of its own colour underneath, kept inside the sprite's bounds."""
    soft = big.filter(ImageFilter.GaussianBlur(blur))
    soft.putalpha(soft.getchannel('A').point(lambda v: min(255, int(v * gain))))
    return Image.alpha_composite(soft, big)


def hollow_hand(w: int, h: int, pivot_y: int, blade_w: float, taper: float, tail: float,
                inset: float = 2.2) -> Image.Image:
    """A hand pointing up, pivot at (w/2, pivot_y): an outlined blade, bright edge and dark middle, and a
    hub. Drawn as an outline because a solid slab reads as a block of light on a phosphor screen."""
    big = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    cx = w * SS / 2
    top, base = sc(4), sc(pivot_y)
    tip_w, base_w = sc(blade_w * taper) / 2, sc(blade_w) / 2
    tail_y, tail_w = base + sc(tail), base_w * 0.8

    def outline(i: float):
        i = sc(i)
        return [(cx - tip_w + i, top + i * 1.6), (cx + tip_w - i, top + i * 1.6), (cx + base_w - i, base),
                (cx + tail_w - i, tail_y - i), (cx - tail_w + i, tail_y - i), (cx - base_w + i, base)]

    d.polygon(outline(0), fill=rgba(HI))
    d.polygon(outline(inset), fill=rgba(DEEP))
    hub = sc(blade_w * 0.7)
    d.ellipse([cx - hub, base - hub, cx + hub, base + hub], fill=rgba(HI))
    d.ellipse([cx - hub * 0.55, base - hub * 0.55, cx + hub * 0.55, base + hub * 0.55], fill=rgba(DEEP))
    d.ellipse([cx - hub * 0.2, base - hub * 0.2, cx + hub * 0.2, base + hub * 0.2], fill=rgba(PALE))
    return halo(big, sc(1.4)).resize((w, h), Image.LANCZOS)


def second_hand() -> Image.Image:
    """A thin bright needle with a short heavier tail and a ring at the pivot."""
    w, h, pivot = 14, 246, 206
    big = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    cx = w * SS / 2
    d.line([(cx, sc(4)), (cx, sc(pivot))], fill=rgba(HI), width=sc(2.4))
    d.line([(cx, sc(pivot)), (cx, sc(238))], fill=rgba(MID), width=sc(3.6))
    d.rectangle([cx - sc(3.4), sc(228), cx + sc(3.4), sc(240)], fill=rgba(HI))
    r = sc(5)
    d.ellipse([cx - r, sc(pivot) - r, cx + r, sc(pivot) + r], fill=rgba(HI))
    d.ellipse([cx - r * 0.6, sc(pivot) - r * 0.6, cx + r * 0.6, sc(pivot) + r * 0.6], fill=rgba(DEEP))
    return halo(big, sc(1.2)).resize((w, h), Image.LANCZOS)


def clock_hands() -> dict[str, tuple[Image.Image, tuple[int, int]]]:
    return {'clock_hand_hour.png': (hollow_hand(26, 156, 132, 15, 0.40, 22), (13, 132)),
            'clock_hand_minute.png': (hollow_hand(20, 220, 198, 12, 0.32, 22, inset=2.0), (10, 198)),
            'clock_hand_second.png': (second_hand(), (7, 206))}


def blip(size: int = 26) -> Image.Image:
    """An arrowhead pointing up, the aircraft mark of a terminal map. Rotated to the aircraft's heading
    by the firmware. Drawn 12x so a sprite this small stays clean."""
    k = 12
    big = Image.new('RGBA', (size * k, size * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    c = size * k / 2

    def pt(x, y):
        return (c + x * k, c + y * k)

    d.polygon([pt(0, -10.5), pt(8.6, 10), pt(0, 5.2), pt(-8.6, 10)], fill=rgba(HI))
    d.polygon([pt(0, -5.4), pt(4.4, 6.0), pt(0, 3.4), pt(-4.4, 6.0)], fill=rgba(DEEP))
    d.ellipse([c - 1.3 * k, c - 0.6 * k, c + 1.3 * k, c + 2.0 * k], fill=rgba(PALE))
    return halo(big, k * 1.2).resize((size, size), Image.LANCZOS)


# ---- the plates ------------------------------------------------------------------------

def radar_plate() -> Image.Image:
    """Polar scope: fine graticule, range rings, crosshair and a bearing scale. Bezel starts at r=217
    and the firmware masks aircraft outside r=214 (theme.yaml radar.zones); change both together."""
    img = over(screen(), grid(29, DIM, 100))
    rings = new()
    d = ImageDraw.Draw(rings)
    for r in (58, 116, 174):
        d.ellipse(ellipse_box(r), outline=rgba(LO, 220), width=sc(1.6))
    for deg in (0, 90, 180, 270):
        polar(d, 0, 200, deg, rgba(MID, 200), 1.2)
    img = over(img, rings)
    img = over(img, ticks(212, 5, 9, 14, minor_step=10))
    north = new()
    ImageDraw.Draw(north).polygon([(sc(CX), sc(CY - 196)), (sc(CX - 6), sc(CY - 184)), (sc(CX + 6), sc(CY - 184))],
                                  fill=rgba(HI))
    img = over(img, lit(north, 1.6))
    return plate(over(img, bezel(217)))


def weather_plate() -> Image.Image:
    """The weather scope: a coarser grid, dashed rings and targeting corners, so it does not read as
    the same screen as the flight radar. Same bezel and mask radius as the radar."""
    img = over(screen(), grid(33, DIM, 120))
    img = over(img, dashed_ring(87, LO, 200))
    img = over(img, dashed_ring(174, LO, 200))
    img = over(img, corner_brackets(200))
    img = over(img, ticks(212, 4, 8, 12, minor_step=15, glow=False))
    return plate(over(img, bezel(217)))


def menu_plate() -> Image.Image:
    """Two rules frame the current app's name, with a chevron either side. The name is drawn large
    across the middle (x 45-421, y 195-270), and the firmware draws the neighbouring apps' names above
    and below it, at about y 160 and y 291: the rules sit between, so nothing is struck through."""
    img = screen()
    img = over(img, lit(rule(191, 78, 388), 1.8))
    img = over(img, lit(rule(275, 78, 388), 1.8))
    img = over(img, lit(chevron(34, 233, 15, -1), 1.8))
    img = over(img, lit(chevron(432, 233, 15, 1), 1.8))
    return plate(over(img, bezel(222)))


def settings_plate() -> Image.Image:
    """A gauge scale down each side, like the margin of a list. The rows and their highlight bar sit
    in the middle (about x 103-363), so the centre column is left clear."""
    img = screen()
    scale = new()
    d = ImageDraw.Draw(scale)
    for x, inward in ((62, 1), (404, -1)):
        d.line([(sc(x), sc(70)), (sc(x), sc(396))], fill=rgba(LO, 230), width=sc(1.6))
        for i, y in enumerate(range(70, 397, 17)):
            long = i % 4 == 0
            d.line([(sc(x), sc(y)), (sc(x + inward * (12 if long else 6)), sc(y))],
                   fill=rgba(HI if long else MID), width=sc(1.6 if long else 1.2))
    img = over(img, lit(scale, 1.6), disc(222))
    return plate(over(img, bezel(222)))


def splash() -> Image.Image:
    """A scope coming online in the upper two thirds: rings, a bearing scale, a sweep and a few
    contacts. Version, network, credits and theme name are drawn at y 321-419, so that band stays dark."""
    cx, cy = CX, 168
    img = screen()
    face = new()
    d = ImageDraw.Draw(face)
    for r in (40, 80, 120):
        d.ellipse(ellipse_box(r, cx, cy), outline=rgba(LO, 230), width=sc(1.6))
    for deg in (0, 90, 180, 270):
        polar(d, 0, 124, deg, rgba(MID, 200), 1.2, cx, cy)
    for deg in range(0, 360, 6):
        polar(d, 125, 131 if deg % 30 else 137, deg, rgba(HI if deg % 30 == 0 else LO), 1.5 if deg % 30 else 2.4, cx, cy)
    sweep = new()
    sd = ImageDraw.Draw(sweep)
    for i in range(24):                                       # a wedge that brightens toward its leading edge
        a0 = 300 - 90 + i * 1.6
        sd.pieslice(ellipse_box(120, cx, cy), a0, a0 + 2.2, fill=rgba(HI, 4 + i * 3))
    polar(sd, 0, 120, 338, rgba(PALE), 2.0, cx, cy)
    for x, y in ((cx + 44, cy - 50), (cx - 62, cy + 22), (cx + 18, cy + 84)):
        sd.ellipse([sc(x - 4), sc(y - 4), sc(x + 4), sc(y + 4)], fill=rgba(HI))
    keep = Image.new('L', (N, N), 255)
    ImageDraw.Draw(keep).rectangle([0, sc(312), N, N], fill=0)
    img = over(img, lit(face, 1.8), keep)
    img = over(img, lit(sweep, 2.0), keep)
    img = over(img, lit(rule(314, 118, 348, DIM, 1.2, 4), 1.2))
    return plate(over(img, bezel(222)))


def intel_plate() -> Image.Image:
    """The headline title sits at y 65 between two rules, the text has 68 px margins, and the age line
    is at y 409."""
    img = screen()
    img = over(img, lit(chord_rule(41), 2.0))
    img = over(img, lit(chord_rule(90), 2.0))
    margins = new()
    d = ImageDraw.Draw(margins)
    for x in (52, 414):
        d.line([(sc(x), sc(112)), (sc(x), sc(380))], fill=rgba(DIM, 230), width=sc(1.4))
    d.line([(sc(140), sc(390)), (sc(326), sc(390))], fill=rgba(DIM, 230), width=sc(1.4))
    img = over(img, margins, disc(222))
    return plate(over(img, bezel(222)))


def ticker_plate() -> Image.Image:
    """Two rings hold the scrolling strip between them (radius 196); name, price and change sit
    inside at y 186, 222 and 286."""
    img = over(screen(), grid(33, DIM, 70, clip_r=182))
    img = over(img, lit(ring(204, LO, 230, 1.6), 1.4))
    img = over(img, lit(ring(188, LO, 230, 1.6), 1.4))
    return plate(over(img, bezel(220)))


def clock_plate() -> Image.Image:
    """The dial: a graticule, a tick scale, and nothing else. No numerals, since the firmware draws
    the digital readout with the theme's font."""
    img = over(screen(), grid(29, DIM, 70, clip_r=168))
    img = over(img, ring(60, DIM, 200, 1.4))
    img = over(img, ring(150, DIM, 200, 1.4))
    img = over(img, ticks(208, 7, 15, 22))
    return plate(over(img, bezel(217)))


ASSETS = {
    'clock_plate.png': clock_plate,
    'radar_plate.png': radar_plate,
    'weather_plate.png': weather_plate,
    'menu_plate.png': menu_plate,
    'settings_plate.png': settings_plate,
    'splash.png': splash,
    'intel_plate.png': intel_plate,
    'ticker_plate.png': ticker_plate,
}


def save_png(img: Image.Image, path: Path) -> None:
    """Level 9, NOT optimize=True. The firmware's PNG decoder (PNGdec's bundled zlib) mis-decodes some
    compressed streams: from one column to the end of a row it reads every channel one byte out of step,
    so black comes out pure red. The same pixels decode correctly or wrongly depending only on how the
    stream was compressed, and optimize=True produced two bad plates here (menu and weather). Which
    settings are safe is luck, not a rule, so tests/test_fallout_theme.py decodes every image with the
    firmware's own decoder; if it fails after a change, try another compress_level."""
    img.save(path, compress_level=9)


def main(argv) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    made = {}
    for name, fn in ASSETS.items():
        made[name] = fn()
        save_png(made[name], OUT / name)
    hands = clock_hands()
    for name, (img, _pivot) in hands.items():
        save_png(img, OUT / name)
    save_png(blip(), OUT / 'radar_blip.png')             # pivot (13, 13): its centre
    print(f'wrote {len(ASSETS) + len(hands) + 1} images to {OUT}')
    if '--preview' in argv:
        sheet = Image.new('RGB', (W * 3, W * 3), (40, 40, 40))
        order = ['clock_plate.png', 'radar_plate.png', 'weather_plate.png', 'menu_plate.png',
                 'settings_plate.png', 'splash.png', 'intel_plate.png', 'ticker_plate.png']
        for i, name in enumerate(order):
            sheet.paste(made[name].convert('RGB'), ((i % 3) * W, (i // 3) * W))
        sheet.paste(compose_clock(made['clock_plate.png'], hands).convert('RGB'), (2 * W, 2 * W))
        target = REPO / 'build' / 'fallout_preview.png'
        target.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(target)
        print(f'preview: {target}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))

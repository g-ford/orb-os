#!/usr/bin/env python3
"""Draws the artwork for the Portal theme into src/theme_assets/portal/.

    python3 tools/portal_art.py            # write the PNGs
    python3 tools/portal_art.py --preview  # also write a contact sheet to build/portal_preview.png

Needs Pillow and numpy (pip3 install pillow numpy). Original artwork in the spirit of the game's
test chambers: white wall panels, dark steel, hazard stripes, an orange and a blue portal, a red
sentry dot. No text is drawn into the images (the firmware draws all text with its own fonts), and
none of the game's own art or logos are used.

Everything is drawn at 3x and scaled down so the edges are smooth. The round display shows only
the inscribed circle, so every plate is black outside it.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'src' / 'theme_assets' / 'portal'

SS = 3                    # supersampling
W = 466
N = W * SS
CX = CY = 233

# palette
ORANGE, ORANGE_HI, ORANGE_LO = (255, 154, 31), (255, 196, 110), (194, 94, 0)
BLUE, BLUE_HI, BLUE_LO = (31, 162, 255), (130, 206, 255), (11, 95, 168)
RED = (255, 45, 45)
PANEL, SEAM, SEAM_DARK = (233, 235, 237), (197, 202, 207), (150, 157, 164)
INK = (38, 43, 49)
DARK, DARK2, STEEL = (11, 14, 17), (20, 25, 30), (58, 64, 72)


def sc(v: float) -> int:
    return int(round(v * SS))


def new(fill=(0, 0, 0, 0)) -> Image.Image:
    return Image.new('RGBA', (N, N), fill)


def disc(r: float, cx: float = CX, cy: float = CY) -> Image.Image:
    m = Image.new('L', (N, N), 0)
    ImageDraw.Draw(m).ellipse([sc(cx - r), sc(cy - r), sc(cx + r), sc(cy + r)], fill=255)
    return m


def ellipse_mask(cx: float, cy: float, rx: float, ry: float) -> Image.Image:
    m = Image.new('L', (N, N), 0)
    ImageDraw.Draw(m).ellipse([sc(cx - rx), sc(cy - ry), sc(cx + rx), sc(cy + ry)], fill=255)
    return m


def ring_mask(r_in: float, r_out: float) -> Image.Image:
    return ImageChops.subtract(disc(r_out), disc(r_in))


def radial(inner, outer, r_in: float, r_out: float) -> Image.Image:
    """Opaque image whose colour goes from `inner` at r_in to `outer` at r_out."""
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    d = np.hypot(xx - sc(CX), yy - sc(CY)) / SS
    t = np.clip((d - r_in) / max(r_out - r_in, 1e-3), 0, 1)[..., None]
    a, b = np.array(inner, np.float32), np.array(outer, np.float32)
    rgb = (a + (b - a) * t).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, np.full((N, N), 255, np.uint8)]), 'RGBA')


def over(base: Image.Image, layer: Image.Image, mask: Image.Image | None = None) -> Image.Image:
    if mask is not None:
        layer = layer.copy()
        layer.putalpha(ImageChops.multiply(layer.getchannel('A'), mask))
    return Image.alpha_composite(base, layer)


def glow_of(layer: Image.Image, radius: float, gain: float = 1.0) -> Image.Image:
    """Blur `layer` into a soft light of the same colours.

    Blurred premultiplied, so the halo keeps the ring's colour as it fades instead of going grey
    toward the transparent black around it. A thin ring blurred wide has a low peak alpha, so
    `gain` is what makes the light visible rather than a tint."""
    arr = np.asarray(layer, np.float32) / 255.0
    a = arr[..., 3]
    blur = ImageFilter.GaussianBlur(sc(radius))

    def soft(channel: np.ndarray) -> np.ndarray:
        img = Image.fromarray((np.clip(channel, 0, 1) * 255).astype(np.uint8), 'L').filter(blur)
        return np.asarray(img, np.float32) / 255.0

    a2 = soft(a)
    rgb = np.stack([soft(arr[..., i] * a) for i in range(3)], axis=-1)
    with np.errstate(divide='ignore', invalid='ignore'):
        rgb = np.where(a2[..., None] > 1e-4, rgb / a2[..., None], 0)
    out = np.dstack([np.clip(rgb, 0, 1), np.clip(a2 * gain, 0, 1)]) * 255
    # dither: a wide, faint gradient otherwise quantises into visible contour rings. Seeded, so the
    # art is the same every run.
    out = out + np.random.default_rng(7).uniform(-0.5, 0.5, out.shape)
    return Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8), 'RGBA')


def finish(img: Image.Image, opaque: bool = True) -> Image.Image:
    """Scale down; an opaque plate is black outside the dial circle."""
    if opaque:
        black = Image.new('RGBA', (N, N), (0, 0, 0, 255))
        img = over(black, img, disc(233))
        # RGBA, not RGB: the firmware's PNG decoders draw anything but 8-bit RGBA as black
        return img.resize((W, W), Image.LANCZOS).convert('RGBA')
    return img.resize((W, W), Image.LANCZOS)


# ---- motifs ----------------------------------------------------------------------------

def portal(cx, cy, rx, ry, color, hi, lo, ring=6.0, tilt=0.0) -> Image.Image:
    """An elliptical portal: glow, dark void, bright rim, thin highlight."""
    box = [sc(cx - rx), sc(cy - ry), sc(cx + rx), sc(cy + ry)]
    inner = [box[0] + sc(ring), box[1] + sc(ring), box[2] - sc(ring), box[3] - sc(ring)]
    rim, hl, void = new(), new(), new()
    ImageDraw.Draw(rim).ellipse(box, outline=color + (255,), width=sc(ring))
    ImageDraw.Draw(hl).ellipse([b + sc(ring * 0.9) * s for b, s in zip(box, (1, 1, -1, -1))],
                               outline=hi + (190,), width=max(1, sc(ring * 0.28)))
    ImageDraw.Draw(void).ellipse(inner, fill=lo + (150,))
    # three lights, wide to tight: a soft spill onto the wall, a halo, and a bloom on the rim itself
    out = over(glow_of(rim, ring * 4.5, 2.6), glow_of(rim, ring * 1.7, 2.2))
    out = over(out, glow_of(rim, ring * 0.7, 1.6))
    out = over(out, void)
    # ... and the light falling into the void, brightest against the rim
    inside = glow_of(rim, ring * 2.4, 1.5)
    out = over(out, inside, ellipse_mask(cx, cy, rx - ring * 0.4, ry - ring * 0.4))
    for part in (rim, hl):
        out = over(out, part)
    if tilt:
        out = out.rotate(tilt, center=(sc(cx), sc(cy)), resample=Image.BICUBIC)
    return out


def steel_rim(r_in: float, r_out: float) -> Image.Image:
    """Bevelled dark-steel bezel: bright outer lip, dark body."""
    body = radial((22, 26, 31), (74, 81, 90), r_in, r_out)
    lip = ring_mask(r_out - 3, r_out)
    out = over(new(), body, ring_mask(r_in, r_out))
    hi = Image.new('RGBA', (N, N), (120, 128, 138, 255))
    return over(out, hi, ImageChops.multiply(lip, Image.new('L', (N, N), 160)))


def split_ring(r_mid: float, width: float, gap_deg: float = 6.0) -> Image.Image:
    """Orange left half, blue right half, glowing: the two portals as a bezel light."""
    bbox = [sc(CX - r_mid), sc(CY - r_mid), sc(CX + r_mid), sc(CY + r_mid)]
    out = new()
    d = ImageDraw.Draw(out)
    d.arc(bbox, 90 + gap_deg, 270 - gap_deg, fill=ORANGE + (255,), width=sc(width))
    d.arc(bbox, 270 + gap_deg, 450 - gap_deg, fill=BLUE + (255,), width=sc(width))
    return over(over(glow_of(out, width * 3.6, 2.6), glow_of(out, width * 1.3, 1.8)), out)


def panels(color=PANEL, seam=SEAM, tile=93, seam_w=2.0) -> Image.Image:
    """Chamber wall: flat panel colour with a grid of seams."""
    img = Image.new('RGBA', (N, N), color + (255,))
    d = ImageDraw.Draw(img)
    for i in range(-3, 8):
        p = sc(CX - tile / 2 + (i - 2) * tile)
        d.line([(p, 0), (p, N)], fill=seam + (255,), width=sc(seam_w))
        d.line([(0, p), (N, p)], fill=seam + (255,), width=sc(seam_w))
    return img


def hazard(mask: Image.Image, stripe: float = 8.0, r_mid: float = 214.0) -> Image.Image:
    """Yellow/black stripes slanted 45 degrees to the arc they sit on, wherever on the dial that is."""
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    dx, dy = xx - sc(CX), yy - sc(CY)
    r = np.hypot(dx, dy) / SS
    along = np.arctan2(dy, dx) * r_mid          # distance along the arc, in px
    on = (np.floor((along + (r - r_mid)) / stripe).astype(np.int32) % 2).astype(bool)
    rgb = np.where(on[..., None], np.array([242, 194, 0], np.uint8), np.array([24, 26, 28], np.uint8))
    img = Image.fromarray(np.dstack([rgb, np.full((N, N), 255, np.uint8)]), 'RGBA')
    return over(new(), img, mask)


def sector_mask(r_in, r_out, a0, a1) -> Image.Image:
    """Annular sector between angles a0..a1 (degrees, 0 = 3 o'clock, clockwise)."""
    m = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(m)
    bb = [sc(CX - r_out), sc(CY - r_out), sc(CX + r_out), sc(CY + r_out)]
    d.pieslice(bb, a0, a1, fill=255)
    inner = [sc(CX - r_in), sc(CY - r_in), sc(CX + r_in), sc(CY + r_in)]
    d.ellipse(inner, fill=0)
    return m


def line_polar(d, r0, r1, deg, color, width):
    a = math.radians(deg - 90)
    d.line([(sc(CX + r0 * math.cos(a)), sc(CY + r0 * math.sin(a))),
            (sc(CX + r1 * math.cos(a)), sc(CY + r1 * math.sin(a)))], fill=color, width=sc(width))


def vignette(img: Image.Image, strength: int = 70) -> Image.Image:
    shade = radial((0, 0, 0), (0, 0, 0), 120, 233)
    a = radial((0, 0, 0), (strength, strength, strength), 120, 233).getchannel('R')
    shade.putalpha(a)
    return over(img, shade)


def little_man(cx: float, cy: float, h: float, color=(255, 255, 255), clip=None) -> Image.Image:
    """The test-subject pictogram, mid-stride: round head, a torso, one arm up and one down, legs apart.
    Drawn from round-capped strokes. `clip` is an optional mask (the portal's inside) so a limb
    never pokes out of the ring."""
    layer = new()
    d = ImageDraw.Draw(layer)
    w = sc(h * 0.085)

    def stroke(*pts):
        d.line([(sc(cx + x * h), sc(cy + y * h)) for x, y in pts], fill=color + (255,), width=w, joint='curve')
        for x, y in (pts[0], pts[-1]):
            r = w / 2
            d.ellipse([sc(cx + x * h) - r, sc(cy + y * h) - r, sc(cx + x * h) + r, sc(cy + y * h) + r],
                      fill=color + (255,))

    hr = sc(h * 0.105)
    d.ellipse([sc(cx) - hr, sc(cy - 0.40 * h) - hr, sc(cx) + hr, sc(cy - 0.40 * h) + hr], fill=color + (255,))
    stroke((0, -0.27), (0, 0.03))                                  # torso
    stroke((0, -0.22), (-0.17, -0.31), (-0.22, -0.40))             # arm reaching up
    stroke((0, -0.22), (0.15, -0.12), (0.22, -0.02))               # arm swinging down
    stroke((0, 0.03), (-0.13, 0.18), (-0.20, 0.36))                # leg back
    stroke((0, 0.03), (0.16, 0.14), (0.15, 0.36))                  # leg forward
    return over(new(), layer, clip) if clip is not None else layer


def sentry_droid(size: int = 26) -> Image.Image:
    """A little sentry droid seen from above, nose up: white shell, two gun pods, three legs and a
    red eye at the front. Rotated to the aircraft's heading by the firmware, so the eye leads."""
    k = 12                                           # supersampling for a sprite this small
    big = Image.new('RGBA', (size * k, size * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    c = size * k / 2

    def px(v):
        return v * k

    leg = (120, 128, 138, 255)
    for x, y in ((-8.5, 8.0), (8.5, 8.0), (0, 11.5)):                 # three splayed legs
        d.line([(c, c + px(1.5)), (c + px(x), c + px(y))], fill=leg, width=int(px(1.5)))
        d.ellipse([c + px(x) - px(1.3), c + px(y) - px(1.3), c + px(x) + px(1.3), c + px(y) + px(1.3)], fill=leg)
    for sx in (-1, 1):                                                # gun pods, dark barrels forward
        d.rounded_rectangle([c + px(sx * 7.6) - px(1.9), c - px(6.5), c + px(sx * 7.6) + px(1.9), c + px(2.5)],
                            radius=int(px(1.6)), fill=(198, 204, 210, 255))
        d.rectangle([c + px(sx * 7.6) - px(0.9), c - px(9.4), c + px(sx * 7.6) + px(0.9), c - px(6.2)],
                    fill=(40, 46, 52, 255))
    d.ellipse([c - px(5.2), c - px(8.6), c + px(5.2), c + px(8.6)], fill=(236, 239, 241, 255))   # shell
    d.ellipse([c - px(3.3), c - px(5.0), c + px(3.3), c + px(6.2)], fill=(214, 219, 224, 255))   # panel
    d.ellipse([c - px(2.9), c - px(7.6), c + px(2.9), c - px(1.8)], fill=(28, 32, 37, 255))      # eye socket
    d.ellipse([c - px(1.9), c - px(6.6), c + px(1.9), c - px(2.8)], fill=RED + (255,))            # red eye
    d.ellipse([c - px(0.7), c - px(5.8), c + px(0.7), c - px(4.6)], fill=(255, 190, 190, 255))    # glint
    return big.resize((size, size), Image.LANCZOS)


# ---- the plates ------------------------------------------------------------------------

def clock_plate() -> Image.Image:
    img = over(new((0, 0, 0, 255)), steel_rim(214, 233))
    face = vignette(panels(), 55)
    img = over(img, face, disc(214))
    ticks = new()
    d = ImageDraw.Draw(ticks)
    for m in range(60):
        if m % 5:
            line_polar(d, 202, 193, m * 6, SEAM_DARK + (255,), 1.6)
    for h in range(12):
        col = ORANGE if h == 0 else BLUE if h == 6 else INK
        line_polar(d, 203, 176 if h % 3 else 168, h * 30, col + (255,), 6.0 if h % 3 else 7.5)
    img = over(img, ticks)
    img = over(img, split_ring(209, 3.2))
    # a portal at 12 (orange) with the little man stepping through it, and one at 6 (blue)
    img = over(img, portal(233, 114, 19, 40, ORANGE, ORANGE_HI, ORANGE_LO, 4.0))
    img = over(img, little_man(233, 116, 52, clip=ellipse_mask(233, 114, 19 - 3.5, 40 - 3.5)))
    img = over(img, portal(233, 352, 19, 40, BLUE, BLUE_HI, BLUE_LO, 4.0))
    return finish(img)


def hand(w: int, h: int, pivot_y: int, blade_w: float, tip_color, body=INK, tail=22, taper=0.55,
         cap=None) -> Image.Image:
    """A hand pointing up, pivot at (w/2, pivot_y). Tapered blade, coloured tip, round hub."""
    big = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    cx = w * SS / 2
    top, base = sc(4), sc(pivot_y)
    tip_w, base_w = sc(blade_w * taper) / 2, sc(blade_w) / 2
    d.polygon([(cx - tip_w, top), (cx + tip_w, top), (cx + base_w, base), (cx - base_w, base)], fill=body + (255,))
    d.polygon([(cx - base_w, base), (cx + base_w, base), (cx + base_w * 0.8, base + sc(tail)),
               (cx - base_w * 0.8, base + sc(tail))], fill=body + (255,))
    if tip_color:
        tip_len = sc((pivot_y - 4) * 0.22)
        d.polygon([(cx - tip_w, top), (cx + tip_w, top),
                   (cx + tip_w + (base_w - tip_w) * 0.22, top + tip_len),
                   (cx - tip_w - (base_w - tip_w) * 0.22, top + tip_len)], fill=tip_color + (255,))
    hub = sc(blade_w * 0.62)
    d.ellipse([cx - hub, sc(pivot_y) - hub, cx + hub, sc(pivot_y) + hub], fill=(200, 205, 210, 255))
    d.ellipse([cx - hub * 0.45, sc(pivot_y) - hub * 0.45, cx + hub * 0.45, sc(pivot_y) + hub * 0.45],
              fill=(cap or DARK) + (255,))
    return big.resize((w, h), Image.LANCZOS)


def clock_hands() -> dict[str, tuple[Image.Image, tuple[int, int]]]:
    hour = hand(26, 156, 132, 15, ORANGE)
    minute = hand(20, 220, 198, 11, ORANGE)
    # the second hand is a thin orange needle with a blue counterweight
    sec_big = Image.new('RGBA', (14 * SS, 246 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(sec_big)
    cx = 7 * SS
    d.rectangle([cx - sc(1.4), sc(4), cx + sc(1.4), sc(212)], fill=ORANGE + (255,))
    d.rectangle([cx - sc(1.8), sc(212), cx + sc(1.8), sc(240)], fill=BLUE + (255,))
    d.ellipse([cx - sc(5), sc(240) - sc(5), cx + sc(5), sc(240) + sc(5)], fill=BLUE + (255,))
    d.ellipse([cx - sc(4.5), sc(206) - sc(4.5), cx + sc(4.5), sc(206) + sc(4.5)], fill=(235, 238, 240, 255))
    d.ellipse([cx - sc(2), sc(206) - sc(2), cx + sc(2), sc(206) + sc(2)], fill=RED + (255,))
    second = sec_big.resize((14, 246), Image.LANCZOS)
    return {'clock_hand_hour.png': (hour, (13, 132)),
            'clock_hand_minute.png': (minute, (10, 198)),
            'clock_hand_second.png': (second, (7, 206))}


def scope_plate(ring_color, accent, tint) -> Image.Image:
    """Dark scope with range rings, crosshair, degree ticks and a split portal bezel."""
    img = radial(tint[0], tint[1], 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    grid = new()
    d = ImageDraw.Draw(grid)
    for r in (58, 116, 174):
        d.ellipse([sc(CX - r), sc(CY - r), sc(CX + r), sc(CY + r)], outline=ring_color + (200,), width=sc(1.6))
    for a in (0, 90):
        line_polar(d, 0, 205, a, ring_color + (150,), 1.2)
        line_polar(d, 0, 205, a + 180, ring_color + (150,), 1.2)
    for deg in range(0, 360, 10):
        line_polar(d, 205, 212 if deg % 30 else 216, deg, accent + (200,) if deg % 90 == 0 else ring_color + (230,), 1.6)
    img = over(img, grid)
    img = over(img, steel_rim(217, 233))
    return over(img, split_ring(214, 2.6))


def radar_plate() -> Image.Image:
    img = scope_plate((30, 64, 96), ORANGE, ((15, 22, 30), (7, 10, 13)))
    # the sentry: a small red dot at the north mark
    top = new()
    ImageDraw.Draw(top).ellipse([sc(CX - 4), sc(20 - 4), sc(CX + 4), sc(20 + 4)], fill=RED + (255,))
    img = over(img, over(glow_of(top, 4, 2.0), top))
    return finish(img)


def weather_plate() -> Image.Image:
    img = scope_plate((28, 78, 120), BLUE, ((9, 22, 36), (5, 9, 14)))
    return finish(img)


def menu_plate() -> Image.Image:
    img = radial((24, 30, 37), (9, 11, 14), 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    seams = panels((0, 0, 0), (34, 41, 49), 93, 1.6)
    seams.putalpha(seams.getchannel('R').point(lambda v: 0 if v == 0 else 255))
    img = over(img, seams, disc(233))
    # thin and near the edge: the current app's name is drawn large across the middle, and
    # "Calibration" spans x=61..404, so the portals must stay outside about x=45 / x=421
    img = over(img, portal(30, 233, 15, 112, ORANGE, ORANGE_HI, ORANGE_LO, 5))
    img = over(img, portal(436, 233, 15, 112, BLUE, BLUE_HI, BLUE_LO, 5))
    img = over(img, steel_rim(222, 233))
    return finish(img)


def settings_plate() -> Image.Image:
    img = radial((22, 28, 34), (8, 10, 13), 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    stripes = new()
    d = ImageDraw.Draw(stripes)
    for x in range(-3, 8):
        d.line([(sc(CX - 93 * 2.5 + x * 93), 0), (sc(CX - 93 * 2.5 + x * 93), N)], fill=(30, 37, 45, 255), width=sc(1.6))
    img = over(img, stripes, disc(233))
    img = over(img, hazard(sector_mask(206, 222, 20, 70)))       # lower right
    img = over(img, hazard(sector_mask(206, 222, 110, 160)))     # lower left
    img = over(img, hazard(sector_mask(206, 222, 200, 250)))     # upper left
    img = over(img, hazard(sector_mask(206, 222, 290, 340)))     # upper right
    img = over(img, portal(30, 233, 12, 52, BLUE, BLUE_HI, BLUE_LO, 5))
    img = over(img, portal(436, 233, 12, 52, ORANGE, ORANGE_HI, ORANGE_LO, 5))
    img = over(img, steel_rim(222, 233))
    return finish(img)


def splash() -> Image.Image:
    img = radial((26, 32, 40), (5, 7, 9), 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    # two portals facing each other, the light of each falling across the floor
    beam = new()
    d = ImageDraw.Draw(beam)
    d.polygon([(sc(112), sc(120)), (sc(354), sc(120)), (sc(354), sc(252)), (sc(112), sc(252))], fill=(255, 255, 255, 16))
    img = over(img, glow_of(beam, 20))
    img = over(img, portal(112, 186, 34, 92, ORANGE, ORANGE_HI, ORANGE_LO, 8, tilt=0))
    img = over(img, portal(354, 186, 34, 92, BLUE, BLUE_HI, BLUE_LO, 8, tilt=0))
    img = over(img, steel_rim(224, 233))
    return finish(img)


def intel_plate() -> Image.Image:
    img = radial((24, 29, 35), (9, 11, 14), 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    band = new()
    d = ImageDraw.Draw(band)
    d.rectangle([0, sc(40), N, sc(41.6)], fill=ORANGE + (255,))
    d.rectangle([0, sc(90), N, sc(91.6)], fill=ORANGE + (255,))
    img = over(img, over(glow_of(band, 3, 1.3), band), disc(232))
    img = over(img, hazard(sector_mask(208, 222, 55, 125)))      # a hazard arc across the bottom
    img = over(img, steel_rim(222, 233))
    return finish(img)


def ticker_plate() -> Image.Image:
    img = radial((20, 25, 31), (7, 9, 12), 0, 233)
    img = over(new((0, 0, 0, 255)), img, disc(233))
    track = new()
    d = ImageDraw.Draw(track)
    for r, col in ((204, STEEL), (188, STEEL)):
        d.ellipse([sc(CX - r), sc(CY - r), sc(CX + r), sc(CY + r)], outline=col + (255,), width=sc(1.6))
    img = over(img, track)
    img = over(img, split_ring(212, 2.6))
    img = over(img, steel_rim(220, 233))
    return finish(img)


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


def compose_clock(plate: Image.Image, hands) -> Image.Image:
    """Plate plus hands at 10:10:30, for looking at."""
    out = plate.convert('RGBA')
    for name, angle in (('clock_hand_hour.png', 305), ('clock_hand_minute.png', 60), ('clock_hand_second.png', 180)):
        img, (px, py) = hands[name][0], hands[name][1]
        pad = Image.new('RGBA', (W * 2, W * 2), (0, 0, 0, 0))
        pad.paste(img, (W - px, W - py))
        pad = pad.rotate(-angle, center=(W, W), resample=Image.BICUBIC)
        out = Image.alpha_composite(out, pad.crop((W - 233, W - 233, W + 233, W + 233)))
    return out


def main(argv) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    made = {}
    for name, fn in ASSETS.items():
        made[name] = fn()
        made[name].save(OUT / name, optimize=True)
    hands = clock_hands()
    for name, (img, _pivot) in hands.items():
        img.save(OUT / name, optimize=True)
    sentry_droid().save(OUT / 'radar_blip.png', optimize=True)      # pivot (13, 13): its centre
    print(f'wrote {len(ASSETS) + len(hands) + 1} images to {OUT}')
    if '--preview' in argv:
        sheet = Image.new('RGB', (W * 3, W * 3), (40, 40, 40))
        order = ['clock_plate.png', 'radar_plate.png', 'weather_plate.png', 'menu_plate.png',
                 'settings_plate.png', 'splash.png', 'intel_plate.png', 'ticker_plate.png']
        for i, name in enumerate(order):
            sheet.paste(made[name], ((i % 3) * W, (i // 3) * W))
        sheet.paste(compose_clock(made['clock_plate.png'], hands).convert('RGB'), (2 * W, 2 * W))
        target = REPO / 'build' / 'portal_preview.png'
        target.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(target)
        print(f'preview: {target}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))

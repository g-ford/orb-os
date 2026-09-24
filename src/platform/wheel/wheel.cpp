#include "wheel.h"
#include "wheel_layout.h"
#include "config.h"          // SCREEN_W / SCREEN_H
#include <math.h>
#include <string.h>
#include <stdio.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
#include <stdlib.h>
#endif

namespace {

lv_obj_t   *s_canvas = nullptr;
lv_color_t *s_buf    = nullptr;

// The rectangle this frame actually wrote into, and the one the last frame did.
//
// The canvas is the whole panel, and refresh() marked the whole panel dirty, so changing
// one word repainted all 217,156 pixels. Measured on the hardware: every menu detent
// reported "repainted 100% of the screen" and took 146-244 ms to reach the glass, on a
// screen that is three short lines of text over a background that never changes.
//
// LVGL is perfectly willing to repaint a small rectangle; it was being told not to. So the
// two functions that write pixels record where they wrote, and refresh() invalidates the
// union of this frame and the last one. The last one matters as much as this one: the text
// that is going away has to be repainted to erase it.
//
// Tracked at WRITE time rather than derived from the layout, because the layout already has
// wrapping, alignment, rotation and a glow reach folded into it, and a bounding box
// calculated a second way is a bounding box that can be subtly wrong in exactly the cases
// that matter.
int s_dx0 = 0, s_dy0 = 0, s_dx1 = -1, s_dy1 = -1;         // this frame, empty when x1 < x0
int s_pdx0 = 0, s_pdy0 = 0, s_pdx1 = -1, s_pdy1 = -1;     // the frame before

inline void mark_px(int x, int y) {
    if (s_dx1 < s_dx0) { s_dx0 = s_dx1 = x; s_dy0 = s_dy1 = y; return; }
    if (x < s_dx0) s_dx0 = x;
    if (x > s_dx1) s_dx1 = x;
    if (y < s_dy0) s_dy0 = y;
    if (y > s_dy1) s_dy1 = y;
}

// --- Straight-line glyph blit + glow, copied from radar_view.cpp's rtext_*
// functions (same technique clock_view.cpp's draw_baked_text uses): reads a
// 4-bpp glyph alpha bitmap (lv_font_conv --bpp 4 --no-compress layout, same as
// the LVGL builtin fonts) and blends it into this module's own RGB565+alpha
// canvas buffer. No curve needed here (the menu overlay is flat, not a scope),
// so only the straight-layout half of radar's renderer is needed.
inline float glyph_alpha4(const uint8_t *bmp, int bw, int x, int y) {
    const int bit = (y * bw + x) * 4;
    const uint8_t byte = bmp[bit >> 3];
    const uint8_t nib = (bit & 4) ? (byte & 0x0F) : (byte >> 4);
    return nib * 17.0f;
}
// overwrite=true means "this is the sharp text, it sits on top". Without it the
// max-alpha rule below let a saturated glow block the very letters it was drawn for:
// the glow reached alpha 255, the text also wanted 255, 255 is not greater than 255, so
// the text was skipped entirely and all you saw was a white fuzzy blob.
void blit_glyph(const uint8_t *bmp, int bw, int bh, float destCx, float destCy,
                lv_color_t col, lv_opa_t maxOpa, bool overwrite = false) {
    if (!bmp || bw <= 0 || bh <= 0 || !s_buf) return;
    uint8_t *buf = (uint8_t *)s_buf;
    const float reach = sqrtf((bw * 0.5f) * (bw * 0.5f) + (bh * 0.5f) * (bh * 0.5f)) + 1.0f;
    const int x0 = (int)fmaxf(0.0f, destCx - reach), x1 = (int)fminf((float)SCREEN_W - 1, destCx + reach);
    const int y0 = (int)fmaxf(0.0f, destCy - reach), y1 = (int)fminf((float)SCREEN_H - 1, destCy + reach);
    const float pivotX = bw * 0.5f, pivotY = bh * 0.5f;
    for (int dy = y0; dy <= y1; ++dy) {
        const float sy = dy - destCy + pivotY;
        for (int dx = x0; dx <= x1; ++dx) {
            const float sx = dx - destCx + pivotX;
            const int ix = (int)floorf(sx), iy = (int)floorf(sy);
            if (ix < -1 || iy < -1 || ix >= bw || iy >= bh) continue;
            const float fx = sx - ix, fy = sy - iy;
            const float a00 = (ix >= 0 && iy >= 0 && ix < bw && iy < bh) ? glyph_alpha4(bmp, bw, ix, iy) : 0.0f;
            const float a10 = (ix + 1 >= 0 && iy >= 0 && ix + 1 < bw && iy < bh) ? glyph_alpha4(bmp, bw, ix + 1, iy) : 0.0f;
            const float a01 = (ix >= 0 && iy + 1 >= 0 && ix < bw && iy + 1 < bh) ? glyph_alpha4(bmp, bw, ix, iy + 1) : 0.0f;
            const float a11 = (ix + 1 >= 0 && iy + 1 >= 0 && ix + 1 < bw && iy + 1 < bh) ? glyph_alpha4(bmp, bw, ix + 1, iy + 1) : 0.0f;
            float a = a00 * (1 - fx) * (1 - fy) + a10 * fx * (1 - fy) + a01 * (1 - fx) * fy + a11 * fx * fy;
            a = a * (float)maxOpa / 255.0f;
            if (a < 8.0f) continue;
            const int px = (dy * SCREEN_W + dx) * 3;
            if (!overwrite && (uint8_t)a <= buf[px + 2]) continue;
            buf[px] = (uint8_t)(col.full & 0xFF);
            buf[px + 1] = (uint8_t)(col.full >> 8);
            buf[px + 2] = (uint8_t)a;
            mark_px(dx, dy);
        }
    }
}
// ---- glow -------------------------------------------------------------------
// Glow used to be 24 offset copies of every glyph (3 rings x 8 directions), each one a
// full bilinear float resample. Measured cost: 358-829 ms for a single menu refresh,
// which is what made every knob detent take about a second.
//
// It is now drawn the way blurs are actually done: rasterise the whole string ONCE into
// a one-byte-per-pixel alpha stencil, blur that stencil, then paint the blurred shape in
// the glow colour. Two properties make it cheap:
//
//   - Separable. Blurring horizontally then vertically is mathematically the same as a
//     2D blur but costs radius+radius lookups per pixel instead of radius squared.
//   - Running sum. Sliding the window one pixel right means adding the pixel entering
//     and subtracting the one leaving: two operations, no matter how wide the blur.
//
// So the cost no longer depends on the glow radius at all. A glow of 40 costs what a
// glow of 4 costs, which turns glow back into a free design choice.
// Ceiling on how opaque the halo may get. The old stamped version peaked around 90 of
// 255 (rings at opacity 90/45/30), which is the "aged, bleeding into the page" density
// worth preserving. Left uncapped, a blurred stroke centre reaches full opacity and the
// glow stops being a glow and becomes a blob. Raise for a heavier bleed, lower for a
// subtler one: this is the one number to tune by eye.
constexpr int GLOW_MAX_ALPHA = 115;

struct Stencil {
    uint8_t *a = nullptr;          // alpha only, 1 byte per pixel
    int x0 = 0, y0 = 0, w = 0, h = 0;   // placement in canvas coordinates
};

Stencil s_sten;

bool stencil_alloc(int x0, int y0, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    const size_t bytes = (size_t)w * h;
#if defined(ESP_PLATFORM)
    s_sten.a = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
#else
    s_sten.a = (uint8_t *)malloc(bytes);
#endif
    if (!s_sten.a) return false;
    memset(s_sten.a, 0, bytes);
    s_sten.x0 = x0; s_sten.y0 = y0; s_sten.w = w; s_sten.h = h;
    return true;
}

void stencil_free() {
    if (!s_sten.a) return;
#if defined(ESP_PLATFORM)
    heap_caps_free(s_sten.a);
#else
    free(s_sten.a);
#endif
    s_sten.a = nullptr; s_sten.w = s_sten.h = 0;
}

// Stamp one glyph into the stencil. Integer placement on purpose: this is the blurred
// layer, so sub-pixel precision is invisible once it has been smeared, and dropping the
// bilinear filter here removes four lookups and ~10 float ops from every pixel. The
// sharp text on top still uses the filtered path, which is where precision shows.
void stencil_glyph(const uint8_t *bmp, int bw, int bh, float destCx, float destCy) {
    if (!s_sten.a || !bmp || bw <= 0 || bh <= 0) return;
    const int gx = (int)lroundf(destCx - bw * 0.5f) - s_sten.x0;
    const int gy = (int)lroundf(destCy - bh * 0.5f) - s_sten.y0;
    for (int sy = 0; sy < bh; ++sy) {
        const int dy = gy + sy;
        if (dy < 0 || dy >= s_sten.h) continue;
        uint8_t *row = s_sten.a + (size_t)dy * s_sten.w;
        for (int sx = 0; sx < bw; ++sx) {
            const int dx = gx + sx;
            if (dx < 0 || dx >= s_sten.w) continue;
            const uint8_t a = (uint8_t)glyph_alpha4(bmp, bw, sx, sy);
            if (a > row[dx]) row[dx] = a;
        }
    }
}

// One separable box-blur pass in each axis, using a sliding window sum so the per-pixel
// cost is constant regardless of `r`. Two passes approximate a Gaussian closely enough
// for a glow, and match the shape of the browser's shadowBlur that Launch Kit's editor
// has always previewed.
void stencil_blur(int r) {
    if (!s_sten.a || r <= 0) return;
    const int w = s_sten.w, h = s_sten.h;
    const int win = 2 * r + 1;
    uint8_t *tmp = nullptr;
#if defined(ESP_PLATFORM)
    tmp = (uint8_t *)heap_caps_malloc((size_t)w * h, MALLOC_CAP_SPIRAM);
#else
    tmp = (uint8_t *)malloc((size_t)w * h);
#endif
    if (!tmp) return;   // no scratch: leave the stencil sharp rather than dropping glow

    for (int y = 0; y < h; ++y) {            // horizontal
        const uint8_t *src = s_sten.a + (size_t)y * w;
        uint8_t *dst = tmp + (size_t)y * w;
        int sum = 0;
        for (int x = -r; x <= r; ++x) sum += src[x < 0 ? 0 : (x >= w ? w - 1 : x)];
        for (int x = 0; x < w; ++x) {
            dst[x] = (uint8_t)(sum / win);
            const int add = x + r + 1, sub = x - r;
            sum += src[add >= w ? w - 1 : add];
            sum -= src[sub < 0 ? 0 : sub];
        }
    }
    for (int x = 0; x < w; ++x) {            // vertical
        int sum = 0;
        for (int y = -r; y <= r; ++y) sum += tmp[(size_t)(y < 0 ? 0 : (y >= h ? h - 1 : y)) * w + x];
        for (int y = 0; y < h; ++y) {
            s_sten.a[(size_t)y * w + x] = (uint8_t)(sum / win);
            const int add = y + r + 1, sub = y - r;
            sum += tmp[(size_t)(add >= h ? h - 1 : add) * w + x];
            sum -= tmp[(size_t)(sub < 0 ? 0 : sub) * w + x];
        }
    }
#if defined(ESP_PLATFORM)
    heap_caps_free(tmp);
#else
    free(tmp);
#endif
}

// Paint the blurred stencil into the canvas in the glow colour.
void stencil_composite(lv_color_t glowCol, int strength) {
    if (!s_sten.a || !s_buf) return;
    uint8_t *buf = (uint8_t *)s_buf;
    const uint8_t lo = (uint8_t)(glowCol.full & 0xFF), hi = (uint8_t)(glowCol.full >> 8);
    for (int y = 0; y < s_sten.h; ++y) {
        const int dy = s_sten.y0 + y;
        if (dy < 0 || dy >= SCREEN_H) continue;
        const uint8_t *srow = s_sten.a + (size_t)y * s_sten.w;
        for (int x = 0; x < s_sten.w; ++x) {
            const int dx = s_sten.x0 + x;
            if (dx < 0 || dx >= SCREEN_W) continue;
            int a = srow[x] * strength / 255;
            if (a < 8) continue;
            if (a > GLOW_MAX_ALPHA) a = GLOW_MAX_ALPHA;
            const int px = (dy * SCREEN_W + dx) * 3;
            if ((uint8_t)a <= buf[px + 2]) continue;
            buf[px] = lo; buf[px + 1] = hi; buf[px + 2] = (uint8_t)a;
            mark_px(dx, dy);
        }
    }
}
void draw_straight(const lv_font_t *font, const char *str, float bx, float by,
                   lv_color_t col, int glow, lv_color_t glowCol, int align, lv_opa_t opa = 255) {
    if (!font || !str || !str[0]) return;
    const int n = (int)strlen(str), cap = n < 80 ? n : 80;
    float w[80], total = 0.0f;
    for (int i = 0; i < cap; ++i) {
        lv_font_glyph_dsc_t g;
        w[i] = lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)str[i], 0) ? (float)g.adv_w : 0.0f;
        total += w[i];
    }
    const float startX = (align == 1) ? (bx - total / 2.0f) : (align == 2) ? (bx - total) : bx;
    const float lineH = (float)lv_font_get_line_height(font), desc = (float)font->base_line;
    const float halfMid = (lineH - 2.0f * desc) * 0.5f;
    // Glow first, for the WHOLE string at once: one stencil, one blur, one composite,
    // instead of 24 resampled copies per glyph. Then the sharp text goes on top.
    if (glow > 0) {
        const int pad = glow * 2 + 4;   // room for the blur to spread past the glyphs
        const int sx0 = (int)startX - pad;
        const int sy0 = (int)(by + halfMid - lineH) - pad;
        const int sw  = (int)total + 2 * pad;
        const int sh  = (int)lineH * 2 + 2 * pad;
        if (stencil_alloc(sx0, sy0, sw, sh)) {
            float gx = startX;
            for (int i = 0; i < cap; ++i) {
                lv_font_glyph_dsc_t g;
                if (lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)str[i], 0)) {
                    const uint8_t *bmp = lv_font_get_glyph_bitmap(font, (uint32_t)(uint8_t)str[i]);
                    if (bmp && g.box_w && g.box_h)
                        stencil_glyph(bmp, g.box_w, g.box_h,
                                      gx + (float)g.ofs_x + (float)g.box_w * 0.5f,
                                      by + halfMid - (float)g.ofs_y - (float)g.box_h * 0.5f);
                }
                gx += w[i];
            }
            stencil_blur(glow);
            // No boost: a blurred stroke centre already reaches full alpha, and
            // GLOW_MAX_ALPHA is what keeps it from swamping the text.
            stencil_composite(glowCol, 255);
            stencil_free();
        }
    }

    float x = startX;
    for (int i = 0; i < cap; ++i) {
        lv_font_glyph_dsc_t g;
        if (lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)str[i], 0)) {
            const uint8_t *bmp = lv_font_get_glyph_bitmap(font, (uint32_t)(uint8_t)str[i]);
            if (bmp && g.box_w && g.box_h) {
                const float destCx = x + (float)g.ofs_x + (float)g.box_w * 0.5f;
                const float destCy = by + halfMid - (float)g.ofs_y - (float)g.box_h * 0.5f;
                blit_glyph(bmp, g.box_w, g.box_h, destCx, destCy, col, opa, true);
            }
        }
        x += w[i];
    }
}

} // namespace

namespace wheel {

void acquire(lv_obj_t *parent) {
    if (s_canvas || !parent) return;
#ifndef ARDUINO
    // Desktop only. SIM_WHEEL_NO_CANVAS=1 behaves as a failed PSRAM allocation, so the plain-label fallback of
    // every wheel screen can be photographed.
    if (getenv("SIM_WHEEL_NO_CANVAS")) return;
#endif
    const size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H);
#if defined(ESP_PLATFORM)
    s_buf = (lv_color_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
#else
    s_buf = (lv_color_t *)malloc(sz);
#endif
    if (!s_buf) {
#ifdef ARDUINO
        Serial.printf("[wheel] canvas alloc FAILED (%u bytes) - callers fall back to plain labels\n", (unsigned)sz);
#else
        printf("[wheel] canvas alloc FAILED (%u bytes) - callers fall back to plain labels\n", (unsigned)sz);
#endif
        return;
    }
    memset(s_buf, 0, sz);   // all-zero bytes are transparent black in this format
    s_canvas = lv_canvas_create(parent);
    lv_obj_clear_flag(s_canvas, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_canvas_set_buffer(s_canvas, s_buf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_center(s_canvas);
    s_dx0 = s_dy0 = s_pdx0 = s_pdy0 = 0;
    s_dx1 = s_dy1 = s_pdx1 = s_pdy1 = -1;
}

void release() {
    if (s_canvas) { lv_obj_del(s_canvas); s_canvas = nullptr; }
    if (s_buf) {
#if defined(ESP_PLATFORM)
        heap_caps_free(s_buf);
#else
        free(s_buf);
#endif
        s_buf = nullptr;
    }
}

bool available() { return s_canvas != nullptr; }

void clear() {
    if (!s_canvas) return;
    memset(s_buf, 0, LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H));
    s_dx0 = s_dy0 = s_pdx0 = s_pdy0 = 0;
    s_dx1 = s_dy1 = s_pdx1 = s_pdy1 = -1;
    lv_obj_invalidate(s_canvas);
}

bool fit(const lv_font_t *font, const char *text, float maxW, char *out, size_t cap) {
    if (!out || cap == 0) return false;
    out[0] = '\0';
    if (!font || !text) return false;
    constexpr size_t MAXC = 80;
    const size_t full = strlen(text);
    const size_t n = full < MAXC ? full : MAXC;
    float adv[MAXC];
    for (size_t i = 0; i < n; ++i) {
        lv_font_glyph_dsc_t g;
        adv[i] = lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)text[i], 0) ? (float)g.adv_w : 0.0f;
    }
    lv_font_glyph_dsc_t gd;
    const float dot = lv_font_get_glyph_dsc(font, &gd, (uint32_t)'.', 0) ? (float)gd.adv_w : 4.0f;
    const size_t keep = wheel_layout::ellipsis_keep(adv, text, n, dot, maxW);
    const bool cut = keep < full;
    size_t w = keep < cap - 1 ? keep : cap - 1;
    memcpy(out, text, w);
    if (cut) for (int k = 0; k < 3 && w + 1 < cap; ++k) out[w++] = '.';
    out[w] = '\0';
    return cut;
}

void draw(const char *const *rows, int count, int sel, const Look &look) {
    if (!s_canvas || !rows) return;
    // A straight wipe at memory speed: the buffer is 3 bytes per pixel and transparent black is all zeros.
    // lv_canvas_fill_bg did the same through LVGL's per-pixel API and measured 143 ms.
    memset(s_buf, 0, LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H));
    // Where the text now being erased was, so it is repainted too.
    s_pdx0 = s_dx0; s_pdy0 = s_dy0; s_pdx1 = s_dx1; s_pdy1 = s_dy1;
    s_dx0 = 0; s_dy0 = 0; s_dx1 = -1; s_dy1 = -1;   // empty; the draws below fill it

    for (int i = 0; i < count; ++i) {
        const wheel_layout::Row r = wheel_layout::row(i, sel);
        if (r.offDial || r.opa == 0 || !rows[i] || !rows[i][0]) continue;
        const bool isSel = (i == sel);
        const lv_font_t *font = isSel ? look.selFont : look.itemFont;
        char text[88];
        fit(font, rows[i], r.maxW, text, sizeof(text));
        draw_straight(font, text, wheel_layout::PANEL_C + r.sx, wheel_layout::PANEL_C + r.sy,
                      isSel ? look.selColor : look.itemColor,
                      isSel ? look.selGlow : 0, look.glowColor, 1 /* centred */, (lv_opa_t)r.opa);
    }

    // Only what changed: what was just drawn plus what was just erased, padded by one pixel because the canvas
    // is composited with alpha and LVGL's rounder can widen a flush area.
    int x0 = s_dx0, y0 = s_dy0, x1 = s_dx1, y1 = s_dy1;
    if (s_pdx1 >= s_pdx0) {
        if (x1 < x0) { x0 = s_pdx0; y0 = s_pdy0; x1 = s_pdx1; y1 = s_pdy1; }
        else {
            if (s_pdx0 < x0) x0 = s_pdx0;
            if (s_pdy0 < y0) y0 = s_pdy0;
            if (s_pdx1 > x1) x1 = s_pdx1;
            if (s_pdy1 > y1) y1 = s_pdy1;
        }
    }
    if (x1 < x0) {
        lv_obj_invalidate(s_canvas);              // nothing tracked: fall back to all of it
    } else {
        lv_area_t a;
        a.x1 = (lv_coord_t)(x0 > 0 ? x0 - 1 : 0);
        a.y1 = (lv_coord_t)(y0 > 0 ? y0 - 1 : 0);
        a.x2 = (lv_coord_t)(x1 < SCREEN_W - 1 ? x1 + 1 : SCREEN_W - 1);
        a.y2 = (lv_coord_t)(y1 < SCREEN_H - 1 ? y1 + 1 : SCREEN_H - 1);
        lv_obj_invalidate_area(s_canvas, &a);
    }
}

} // namespace wheel

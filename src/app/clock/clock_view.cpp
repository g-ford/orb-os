// Clock app for the shell. One face: the theme's own (plate, hands, overlay and text), with
// the built-in look drawn from the palette wherever a theme ships no image. See
// compose_custom() and the drawn face below.
//
// Time comes from the system clock (RTC-seeded, NTP-synced; see main.cpp). TZ is
// applied at boot, so getLocalTime() returns local time.
#include "clock_view.h"
#include "display.h"      // orb_screen_covered(): do not redraw under a cover
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
// Desktop/native build (no ESP32 core): shim the two Arduino-only calls this
// file uses so it can run in the LVGL simulator for real screenshots.
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
static bool getLocalTime(struct tm *info, uint32_t = 0) {
    time_t now = time(nullptr);
    if (getenv("SIM_NO_TIME")) return false;   // photograph the not-yet-set face (see s_noTime)
    return localtime_r(&now, info) != nullptr;
}
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } void println(const char *s) const { puts(s); } } Serial;
static void *heap_caps_malloc(size_t sz, int) { return malloc(sz); }
static void  heap_caps_free(void *p) { free(p); }
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#endif
#include <lvgl.h>
#include <time.h>
#include <sys/time.h>   // gettimeofday: the sweeping hand needs the fraction of a second
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

// NO TIME YET. Until the RTC or NTP has set the clock, getLocalTime() says no, and this
// screen used to draw nothing at all: a black disc, on a theme whose dial is drawn here.
// The first stranger to power-cycle an Orb without a coin cell in the RTC read that as a
// broken clock (CanadianAvenger, 2.16.17), and it did look like one. A clock that has not
// been set shows its face at twelve, which every oven and microwave has taught people to
// read correctly, so that is what this draws: the dial, the hands at 12:00 with the seconds
// running, and no date, because a date would be an invented one. The real time replaces it
// on the tick after it arrives.
static bool s_noTime = false;

static void time_for_face(struct tm *ti) {
    if (getLocalTime(ti, 0)) { s_noTime = false; return; }
    time_t now; time(&now);
    localtime_r(&now, ti);
    ti->tm_hour = 0; ti->tm_min = 0;   // tm_sec keeps running from the system clock
    s_noTime = true;
}
#include "custom_hands.h"   // CUSTOM_HAS_* / CUSTOM_*_PIVOT_* / CUSTOM_*_BLEND / CUSTOM_HAND_ORDER
#include "custom_text.h"    // CUSTOM_HAS_TEXT* (compile-time show/hide gate) / CUSTOM_TEXT*_FONT (compiled glyphs, not per-theme — see theme_style.h)
#include "custom_sprite.h"  // custom_plate()/custom_overlay()/custom_hand()
#include "clock_face.h"     // the drawn face's geometry, pure math (tests/clock_face_test.cpp)
#include "theme_style.h"
#include "theme_font.h"   // per-theme fonts, with the compiled font as fallback    // per-theme bg/text position/color/format — the runtime half of custom_text.h's macros (theme_style.h explains what stays compile-time and why)

// ---- palette ----------------------------------------------------------------
static const lv_color_t COL_BLACK = LV_COLOR_MAKE(0x00, 0x00, 0x00);

static constexpr float CX = SCREEN_CX;   // 233 (main dial centre)
static constexpr float CY = SCREEN_CY;   // 233
static constexpr float DEG2RAD = 3.14159265358979f / 180.0f;

static lv_obj_t   *s_screen = nullptr;
static lv_obj_t   *s_canvas = nullptr;
static lv_color_t *s_buf    = nullptr;

// ---- drawing helpers --------------------------------------------------------
static inline lv_point_t P(float x, float y) {
    lv_point_t p;
    p.x = (lv_coord_t)lroundf(x);
    p.y = (lv_coord_t)lroundf(y);
    return p;
}

static void draw_disc(float ccx, float ccy, float r, lv_color_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col;
    d.bg_opa   = LV_OPA_COVER;
    d.radius   = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_canvas, (lv_coord_t)lroundf(ccx - r), (lv_coord_t)lroundf(ccy - r),
                        (lv_coord_t)lroundf(2*r), (lv_coord_t)lroundf(2*r), &d);
}

// A thin needle (line) pivoting at (pxc,pyc), for the sub-seconds hand.
static void draw_needle_at(float pxc, float pyc, float angDeg, float len, float tail,
                           float width, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
    const float a  = angDeg * DEG2RAD;
    const float dx = sinf(a), dy = -cosf(a);
    lv_point_t sp[2] = { P(pxc - tail*dx, pyc - tail*dy), P(pxc + len*dx, pyc + len*dy) };
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = col;
    ld.opa   = opa;
    ld.width = (lv_coord_t)lroundf(width);
    ld.round_start = 1;
    ld.round_end   = 1;
    lv_canvas_draw_line(s_canvas, sp, 2, &ld);
}

static inline void unpack565(uint16_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (uint8_t)(((v >> 11) & 0x1F) << 3);
    g = (uint8_t)(((v >> 5)  & 0x3F) << 2);
    b = (uint8_t)((v & 0x1F) << 3);
}

// ---- CUSTOM face (pushed from Launch Kit) -----------------------------------
// One live text banner, rendered with the design's real typeface baked into the
// firmware (a proper LVGL font in custom_font*.c) rather than a shipped glyph
// image. The glow the editor draws with canvas shadowBlur is reproduced here as
// a firmware effect: the string is drawn several times in the glow colour at a
// ring of offsets with falling opacity, then the sharp fill goes on top. Centred
// at (bx,by) to match the editor (textAlign centre, textBaseline middle).
// One live text banner in the design's real baked typeface, hard-left anchored at
// (bx,by): the string always starts at the same x, laid out with each glyph's own
// natural advance width. A digit that's narrower or wider than its predecessor
// (e.g. "1" -> "8") only pushes the tail end of the string further right — the
// start never moves, so there's no left-right wobble as the seconds tick. Glow is
// a few rings of the same layout at falling opacity, offset outward, under the
// sharp fill on top.
// align: 0 left (bx is the start — a digit changing width only shifts the tail,
// so a live value never wobbles), 1 center (bx is the middle), 2 right (bx is
// the end). Mirrors the editor's alignedStartX().
// Why a banner drew nothing, said once.
//
// Both banner painters bail on a null font or a format strftime will not take, and both
// used to do it in silence. A theme asking for "%a %b %-d" — the GNU no-padding flag, which
// this newlib does not have — therefore lost its whole date line with no symptom anywhere:
// not on the screen, not on the wire, not in this log. Finding that cost an afternoon.
//
// Rate-limited to one line per distinct reason, because this runs inside a once-a-second
// redraw and a fault that repeats 3600 times an hour is noise, not a diagnosis.
static void banner_silent(const char *which, const char *why, const char *fmt) {
#ifdef ARDUINO
    static char s_said[2][40] = { "", "" };
    const int slot = (which[5] == '2') ? 1 : 0;
    char now[40];
    snprintf(now, sizeof(now), "%s:%s", why, fmt ? fmt : "");
    if (!strcmp(s_said[slot], now)) return;
    snprintf(s_said[slot], sizeof(s_said[slot]), "%s", now);
    Serial.printf("[clock] %s drew nothing: %s (fmt \"%s\")\n", which, why, fmt ? fmt : "");
#else
    (void)which; (void)why; (void)fmt;
#endif
}

// A rounded rectangle blended into the canvas in one pass at one opacity. Corner
// coverage comes from the distance to the corner's circle centre, with a one-pixel ramp
// so the curve is smooth rather than stepped. Radius is clamped to half the shorter side.
static void fill_plate(int x, int y, int w, int h, int radius, lv_color_t col, lv_opa_t opa) {
    if (!s_buf || w <= 0 || h <= 0 || opa == 0) return;
    float r = (float)radius;
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r < 0) r = 0;
    const int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int x1 = (x + w > SCREEN_W) ? SCREEN_W : x + w;
    const int y1 = (y + h > SCREEN_H) ? SCREEN_H : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            float cov = 1.0f;
            if (r > 0.5f) {
                // Which corner, if any, this pixel sits in; centre of that corner's arc.
                const float cx = (px < x + r) ? x + r : (px >= x + w - r) ? x + w - r : -1.0f;
                const float cy = (py < y + r) ? y + r : (py >= y + h - r) ? y + h - r : -1.0f;
                if (cx >= 0 && cy >= 0) {
                    const float dx = (px + 0.5f) - cx, dy = (py + 0.5f) - cy;
                    const float d = sqrtf(dx * dx + dy * dy);
                    cov = r + 0.5f - d;            // 1 inside, 0 outside, a one-pixel ramp between
                    if (cov <= 0.0f) continue;
                    if (cov > 1.0f) cov = 1.0f;
                }
            }
            const lv_opa_t a = (lv_opa_t)lroundf(opa * cov);
            if (!a) continue;
            lv_color_t *dst = &s_buf[py * SCREEN_W + px];
            *dst = lv_color_mix(col, *dst, a);
        }
    }
}

static void draw_baked_text(const lv_font_t *font, const char *fmt, int bx, int by,
                            uint32_t color, int glow, uint32_t glowColor, int align,
                            const struct tm *ti, const char *which, lv_opa_t opa = LV_OPA_COVER,
                            uint32_t bg = 0, int bgOpa = 0, int bgRadius = 0) {
    if (!font)          { banner_silent(which, "no font loaded for this slot", fmt); return; }
    if (!fmt || !fmt[0]) { banner_silent(which, "empty format", fmt); return; }
    if (s_noTime)        return;   // nothing true to print yet; see time_for_face()
    char buf[48];
    // 0 means strftime refused the format outright — almost always a flag or a conversion
    // this libc does not implement, since 48 bytes is ample for anything a banner shows.
    if (strftime(buf, sizeof(buf), fmt, ti) == 0) {
        banner_silent(which, "strftime rejected the format, or it produced nothing", fmt);
        return;
    }
    const int n = (int)strlen(buf);
    float w[48], total = 0.0f;
    for (int i = 0; i < n && i < 48; ++i) {
        lv_font_glyph_dsc_t g;
        w[i] = lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)buf[i], 0) ? (float)g.adv_w : 0.0f;
        total += w[i];
    }
    const float startX = (align == 1) ? (bx - total / 2.0f) : (align == 2) ? (bx - total) : (float)bx;
    const int y0 = (int)lroundf(by - lv_font_get_line_height(font) * 0.5f);

    // The plate behind the words, THEME_CAPS 33. This screen draws into an LVGL canvas
    // rather than through curved_text, so it gets LVGL's own rounded rectangle instead of
    // the raster filler that serves the other screens. Same padding, 8 across and 2 down, so
    // a design that moves a line between screens keeps the shape it drew against.
    if (bgOpa > 0) {
        // Drawn by hand, not with lv_canvas_draw_rect. LVGL 8.4's rounded rectangle with a
        // background opacity under 253 paints its corner rows and its middle block down two
        // different paths, and on this canvas the middle came out wrong: missing entirely in
        // the simulator, and on the glass a dark seam across the words (canoejohn, a date box
        // at 99% opacity, 2026-09-18). The theme tool's preview had no such line, because a browser
        // draws a rounded rectangle in one pass. So does this: one coverage value per pixel,
        // rounded corners included, blended once.
        const lv_coord_t lh = (lv_coord_t)lv_font_get_line_height(font);
        fill_plate((int)lroundf(startX) - 8, y0 - 2, (int)lroundf(total) + 16, lh + 4,
                   bgRadius, lv_color_hex(bg), (lv_opa_t)bgOpa);
    }
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    ld.font  = font;
    ld.align = LV_TEXT_ALIGN_LEFT;
    const auto paint = [&](int ox, int oy, lv_color_t col, lv_opa_t opa) {
        ld.color = col; ld.opa = opa;
        float x = startX;
        for (int i = 0; i < n && i < 48; ++i) {
            char c[2] = { buf[i], 0 };
            lv_canvas_draw_text(s_canvas, (lv_coord_t)lroundf(x) + ox, y0 + oy, (lv_coord_t)lroundf(w[i] + 4), &ld, c);
            x += w[i];
        }
    };
    if (glow > 0) {
        static const float dirs[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{0.707f,0.707f},{-0.707f,0.707f},{0.707f,-0.707f},{-0.707f,-0.707f} };
        const lv_color_t gc = lv_color_hex(glowColor);
        const int rings = 3;
        for (int ri = 1; ri <= rings; ++ri) {
            const int r = (int)lroundf((float)glow * ri / rings);
            if (r <= 0) continue;
            const lv_opa_t opa = (lv_opa_t)(90 / ri);   // fainter the further out
            for (int di = 0; di < 8; ++di)
                paint((int)lroundf(dirs[di][0] * r), (int)lroundf(dirs[di][1] * r), gc, opa);
        }
    }
    paint(0, 0, lv_color_hex(color), opa);
}

// Read a 4-bpp (16-level) glyph alpha bitmap (as lv_font_conv --bpp 4 --no-compress
// emits it): continuous bitstream, MSB-first, box_w px per row, no row padding.
static inline float glyph_alpha4(const uint8_t *bmp, int bw, int x, int y) {
    const int bit = (y * bw + x) * 4;
    const uint8_t byte = bmp[bit >> 3];
    const uint8_t nib = (bit & 4) ? (byte & 0x0F) : (byte >> 4);
    return nib * 17.0f;   // 0..15 -> 0..255
}

// Rotate one glyph's alpha bitmap around its own centre by angleDeg (clockwise,
// screen space) and alpha-blend it into the canvas in a solid colour, with its
// box centred at (destCx,destCy). Bilinear sampled so rotated edges stay smooth.
static void blit_glyph_rot(const uint8_t *bmp, int bw, int bh, float destCx, float destCy, float angleDeg, lv_color_t col, lv_opa_t opa = LV_OPA_COVER) {
    if (!bmp || bw <= 0 || bh <= 0) return;
    const float th = angleDeg * DEG2RAD, ct = cosf(th), st = sinf(th);
    const float pivotX = bw * 0.5f, pivotY = bh * 0.5f;
    const float reach = sqrtf(pivotX * pivotX + pivotY * pivotY) + 1.0f;
    const int x0 = (int)fmaxf(0.0f, destCx - reach), x1 = (int)fminf((float)SCREEN_W - 1, destCx + reach);
    const int y0 = (int)fmaxf(0.0f, destCy - reach), y1 = (int)fminf((float)SCREEN_H - 1, destCy + reach);
    for (int dy = y0; dy <= y1; ++dy) {
        const float oy = dy - destCy;
        for (int dx = x0; dx <= x1; ++dx) {
            const float ox = dx - destCx;
            const float sxf = ox * ct + oy * st + pivotX;
            const float syf = -ox * st + oy * ct + pivotY;
            const int ix = (int)floorf(sxf), iy = (int)floorf(syf);
            if (ix < -1 || iy < -1 || ix >= bw || iy >= bh) continue;
            const float fx = sxf - ix, fy = syf - iy;
            const float a00 = (ix >= 0 && iy >= 0 && ix < bw && iy < bh) ? glyph_alpha4(bmp, bw, ix, iy) : 0.0f;
            const float a10 = (ix + 1 >= 0 && iy >= 0 && ix + 1 < bw && iy < bh) ? glyph_alpha4(bmp, bw, ix + 1, iy) : 0.0f;
            const float a01 = (ix >= 0 && iy + 1 >= 0 && ix < bw && iy + 1 < bh) ? glyph_alpha4(bmp, bw, ix, iy + 1) : 0.0f;
            const float a11 = (ix + 1 >= 0 && iy + 1 >= 0 && ix + 1 < bw && iy + 1 < bh) ? glyph_alpha4(bmp, bw, ix + 1, iy + 1) : 0.0f;
            const float a = (a00 * (1 - fx) * (1 - fy) + a10 * fx * (1 - fy) + a01 * (1 - fx) * fy + a11 * fx * fy)
                          * (float)opa / 255.0f;
            if (a < 8.0f) continue;
            lv_color_t *d = &s_buf[dy * SCREEN_W + dx];
            *d = lv_color_mix(col, *d, (lv_opa_t)lroundf(fminf(255.0f, a)));
        }
    }
}

// A curved banner: lay each glyph along an arc of radius R centred on arcDeg (a
// clock angle, 0 = 12 o'clock), advancing by the glyph's own width AND tilting
// each glyph tangent to the arc — the same geometry as the editor's
// drawCurvedText (textAlign centre, textBaseline middle). Glow isn't applied on
// the curve.
static void draw_baked_arc_text(const lv_font_t *font, const char *fmt, float R, float arcDeg,
                                uint32_t color, const struct tm *ti, const char *which,
                                lv_opa_t opa = LV_OPA_COVER) {
    if (!font)           { banner_silent(which, "no font loaded for this slot", fmt); return; }
    if (!fmt || !fmt[0])  { banner_silent(which, "empty format", fmt); return; }
    if (R < 1.0f)        { banner_silent(which, "curved, but sitting on the dial centre", fmt); return; }
    if (s_noTime)        return;
    char buf[48];
    if (strftime(buf, sizeof(buf), fmt, ti) == 0) {
        banner_silent(which, "strftime rejected the format, or it produced nothing", fmt);
        return;
    }
    const int n = (int)strlen(buf);
    float w[48]; float total = 0.0f;
    for (int i = 0; i < n && i < 48; ++i) {
        char c[2] = { buf[i], 0 }; lv_point_t s;
        lv_txt_get_size(&s, c, font, 0, 0, LV_COORD_MAX, 0);
        w[i] = s.x; total += s.x;
    }
    const float norm = fmodf(fmodf(arcDeg, 360.0f) + 360.0f, 360.0f);
    const bool bottom = (norm > 90.0f && norm < 270.0f);
    const float dir = bottom ? -1.0f : 1.0f;                 // read L->R at the bottom
    const float base = arcDeg * DEG2RAD;
    const lv_color_t col = lv_color_hex(color);
    // Baseline "middle": vertical centre of the em box sits on the arc point.
    const float lineH = (float)lv_font_get_line_height(font), desc = (float)font->base_line;
    const float halfMid = (lineH - 2.0f * desc) * 0.5f;      // (ascent - descent)/2
    float cursor = -total / 2.0f;
    for (int i = 0; i < n && i < 48; ++i) {
        const float mid = cursor + w[i] / 2.0f, ang = base + dir * mid / R;
        const float ax = CX + sinf(ang) * R, ay = CY - cosf(ang) * R;   // arc anchor
        const float rot = ang + (bottom ? 3.14159265358979f : 0.0f);    // glyph tilt (clockwise)
        cursor += w[i];
        lv_font_glyph_dsc_t g;
        if (!lv_font_get_glyph_dsc(font, &g, (uint32_t)(uint8_t)buf[i], 0)) continue;
        const uint8_t *bmp = lv_font_get_glyph_bitmap(font, (uint32_t)(uint8_t)buf[i]);
        if (!bmp || g.box_w == 0 || g.box_h == 0) continue;
        // Glyph box centre offset from the arc anchor in the upright (unrotated)
        // text frame, then rotated by the tilt into screen space.
        const float offY = halfMid - (float)g.ofs_y - (float)g.box_h * 0.5f;
        const float cr = cosf(rot), sr = sinf(rot);
        const float destCx = ax - offY * sr, destCy = ay + offY * cr;
        blit_glyph_rot(bmp, g.box_w, g.box_h, destCx, destCy, rot / DEG2RAD, col, opa);
    }
}

// A shadow is one flat colour behind a shape, so it needs none of the colour machinery a
// hand needs. blend_custom_hand reconstructs each pixel from four RGB565 neighbours: four
// unpacks and nine multiplies per pixel, over a box as wide as the sprite's reach. For
// a 429x429 minute hand — the size the retired Aviator face shipped, and one a pushed
// design can still ask for — that is the whole 466x466 screen, and running it a SECOND
// time for the shadow doubled the most expensive loop on the clock.
//
// That is what turned the dial black. Not the art — the shadow sprites are clean
// silhouettes covering 3% of their box — but a frame that no longer finished inside its own
// tick, so the canvas was never completely composited before it was flushed.
//
// This samples alpha only, nearest-neighbour, and mixes one constant colour. A blurred
// silhouette has no detail for bilinear to preserve, so nothing is lost, and it costs
// roughly a third of the full path.
// The box everything on this dial is allowed to touch.
//
// Full screen except while a smooth second hand is sweeping, when only the hand's own
// rectangle is repainted. Every full-screen pass on this face costs real time — the plate
// copy is 28.7 ms and the overlay 26.1 — and both scale straight down with the area, so a
// hand covering a quarter of the dial costs a quarter of them.
//
// A file-static rather than a parameter on nine functions: it is the same box for every
// layer in one pass, and threading it through by hand is how one helper ends up drawing
// outside it and smearing the frame.
static int s_clipX0 = 0, s_clipY0 = 0, s_clipX1 = SCREEN_W - 1, s_clipY1 = SCREEN_H - 1;
// Per-row runs, when the box is not tight enough.
//
// A sweep frame wipes only the runs the second hand and its shadow actually cover, not the
// rectangle around them. Anything redrawn afterwards — a hand the design orders ABOVE the
// second hand — must go back over exactly those runs and no further, or it would be blended
// a second time onto pixels that still had it from the last frame, and a semi-transparent
// hand would darken a little more every frame.
static const int *s_runLo = nullptr, *s_runHi = nullptr;
static inline void clip_reset() {
    s_clipX0 = 0; s_clipY0 = 0; s_clipX1 = SCREEN_W - 1; s_clipY1 = SCREEN_H - 1;
    s_runLo = nullptr; s_runHi = nullptr;
}
// The x range this row may touch, box and run together.
static inline void clip_row(int dy, int &lo, int &hi) {
    if (lo < s_clipX0) lo = s_clipX0;
    if (hi > s_clipX1) hi = s_clipX1;
    if (s_runLo && dy >= 0 && dy < SCREEN_H) {
        if (lo < s_runLo[dy]) lo = s_runLo[dy];
        if (hi > s_runHi[dy]) hi = s_runHi[dy];
    }
}

// The run of dx, within one row, whose source coordinates land inside the sprite.
//
// Both rotating blits need this and only one of them had it. blend_custom_hand got the
// treatment first and went from 234 ms to 16; blend_shadow kept sweeping the whole square,
// and on a design with shadows switched on it then cost more than every hand put together —
// 38 ms of a 57 ms sweep frame, for one hand's shadow. Shared now, so they cannot diverge
// again.
//
// sxf and syf are linear in dx within a row, so each axis clips the run to an interval and
// the answer is the intersection. Callers keep their own per-pixel guard: this narrows the
// loop, it does not decide what is drawn.
static inline void row_span(float a, float b, float L, float &lo, float &hi, bool &dead) {
    if (fabsf(a) < 1e-6f) { if (b < 0.0f || b > L) dead = true; return; }
    float t0 = (0.0f - b) / a, t1 = (L - b) / a;
    if (t0 > t1) { const float t = t0; t0 = t1; t1 = t; }
    if (t0 > lo) lo = t0;
    if (t1 < hi) hi = t1;
}

static void blend_shadow(const uint8_t *src, int sw, int sh, int pivotX, int pivotY,
                         float cx, float cy, float angleDeg) {
    if (!src || !s_buf) return;
    const float th = angleDeg * DEG2RAD, ct = cosf(th), st = sinf(th);
    const float reach = sqrtf(fmaxf((float)pivotX, (float)(sw - pivotX)) * fmaxf((float)pivotX, (float)(sw - pivotX))
                            + fmaxf((float)pivotY, (float)(sh - pivotY)) * fmaxf((float)pivotY, (float)(sh - pivotY)));
    int x0 = (int)fmaxf(0.0f, cx - reach), x1 = (int)fminf((float)SCREEN_W - 1, cx + reach);
    int y0 = (int)fmaxf(0.0f, cy - reach), y1 = (int)fminf((float)SCREEN_H - 1, cy + reach);
    if (x0 < s_clipX0) x0 = s_clipX0;
    if (y0 < s_clipY0) y0 = s_clipY0;
    if (x1 > s_clipX1) x1 = s_clipX1;
    if (y1 > s_clipY1) y1 = s_clipY1;
    for (int dy = y0; dy <= y1; ++dy) {
        const float oy = dy - cy;
        const float ax = oy * st + pivotX, ay = oy * ct + pivotY;
        float uLo = (float)(x0 - cx), uHi = (float)(x1 - cx);
        bool dead = false;
        row_span(ct,  ax, (float)(sw - 1), uLo, uHi, dead);
        row_span(-st, ay, (float)(sh - 1), uLo, uHi, dead);
        if (dead || uHi < uLo) continue;
        int rx0 = (int)floorf(cx + uLo) - 1, rx1 = (int)ceilf(cx + uHi) + 1;
        if (rx0 < x0) rx0 = x0;
        if (rx1 > x1) rx1 = x1;
        clip_row(dy, rx0, rx1);
        for (int dx = rx0; dx <= rx1; ++dx) {
            const float ox = dx - cx;
            const int sx = (int)(ox * ct + oy * st + pivotX);
            const int sy = (int)(-ox * st + oy * ct + pivotY);
            if (sx < 0 || sy < 0 || sx >= sw || sy >= sh) continue;
            const uint8_t *p = src + ((size_t)sy * sw + sx) * 3;
            const uint8_t a = p[2];
            if (a < 8) continue;                       // same floor the hand blit uses
            lv_color_t sc; sc.full = (uint16_t)(p[0] | (p[1] << 8));
            lv_color_t *dst = &s_buf[dy * SCREEN_W + dx];
            *dst = lv_color_mix(sc, *dst, a);
        }
    }
}

// Rotate a hand sprite (RGB565+alpha, 3 B/px) around the dial centre by angleDeg
// and composite it into the canvas with the given blend (0 normal, 1 multiply,
// 2 screen) — the same rotation math as blend_shadow above, which samples alpha only,
// carried over to full colour + a blend mode so a pushed hand lands exactly where the
// editor drew it.
// A layer that never turns is a straight copy: one source pixel onto one screen pixel.
//
// Worth its own path because the layers that use it are FULL SCREEN. Sending 217k pixels
// through the rotating blit below costs four texel fetches and a dozen floats each, which
// is the same full-screen bilinear pass that once left the dial black (blend_shadow's note
// above). This is the overlay loop's cost instead, and the frame budget already carries
// one of those.
static void blit_upright(const uint8_t *src, int sw, int sh, int pivotX, int pivotY,
                         int cx, int cy, int blend) {
    const int offX = cx - pivotX, offY = cy - pivotY;
    int x0 = offX < 0 ? 0 : offX, y0 = offY < 0 ? 0 : offY;
    int x1 = (offX + sw < SCREEN_W ? offX + sw : SCREEN_W);
    int y1 = (offY + sh < SCREEN_H ? offY + sh : SCREEN_H);
    if (x0 < s_clipX0) x0 = s_clipX0;
    if (y0 < s_clipY0) y0 = s_clipY0;
    if (x1 > s_clipX1 + 1) x1 = s_clipX1 + 1;
    if (y1 > s_clipY1 + 1) y1 = s_clipY1 + 1;
    for (int dy = y0; dy < y1; ++dy) {
        const uint8_t *row = src + ((size_t)(dy - offY) * sw) * 3;
        lv_color_t *dstRow = &s_buf[dy * SCREEN_W];
        for (int dx = x0; dx < x1; ++dx) {
            const uint8_t *p = row + (size_t)(dx - offX) * 3;
            const uint8_t a = p[2];
            if (a < 8) continue;                       // same floor the rotating blit uses
            lv_color_t sc; sc.full = (uint16_t)(p[0] | (p[1] << 8));
            lv_color_t *dst = &dstRow[dx];
            if (blend) {
                uint8_t sr, sg, sb, dr, dg, db;
                unpack565(sc.full, sr, sg, sb);
                unpack565(dst->full, dr, dg, db);
                if (blend == 1) { sr = (uint8_t)(sr * dr / 255); sg = (uint8_t)(sg * dg / 255); sb = (uint8_t)(sb * db / 255); }
                else if (blend == 2) { sr = (uint8_t)(255 - (255 - sr) * (255 - dr) / 255); sg = (uint8_t)(255 - (255 - sg) * (255 - dg) / 255); sb = (uint8_t)(255 - (255 - sb) * (255 - db) / 255); }
                sc = LV_COLOR_MAKE(sr, sg, sb);
            }
            *dst = lv_color_mix(sc, *dst, a);
        }
    }
}

static void blend_custom_hand(const uint8_t *src, int sw, int sh, int pivotX, int pivotY, float cx, float cy, float angleDeg, int blend) {
    if (!src || !s_buf) return;
    // The static layers (kinds 3 and 4) are always here, and a hand passing 12 lands here
    // for one frame, which is free.
    if (fabsf(angleDeg) < 0.01f && cx == floorf(cx) && cy == floorf(cy)) {
        blit_upright(src, sw, sh, pivotX, pivotY, (int)cx, (int)cy, blend);
        return;
    }
    const float th = angleDeg * DEG2RAD, ct = cosf(th), st = sinf(th);
    const float reach = sqrtf(fmaxf((float)pivotX, (float)(sw - pivotX)) * fmaxf((float)pivotX, (float)(sw - pivotX))
                            + fmaxf((float)pivotY, (float)(sh - pivotY)) * fmaxf((float)pivotY, (float)(sh - pivotY)));
    int x0 = (int)fmaxf(0.0f, cx - reach), x1 = (int)fminf((float)SCREEN_W - 1, cx + reach);
    int y0 = (int)fmaxf(0.0f, cy - reach), y1 = (int)fminf((float)SCREEN_H - 1, cy + reach);
    if (x0 < s_clipX0) x0 = s_clipX0;
    if (y0 < s_clipY0) y0 = s_clipY0;
    if (x1 > s_clipX1) x1 = s_clipX1;
    if (y1 > s_clipY1) y1 = s_clipY1;
    // ONLY THE PIXELS THE HAND ACTUALLY LANDS ON.
    //
    // The box above is a square as wide as the hand's whole swing, because `reach` is the
    // distance from the pivot to the furthest corner. A hand is a long thin rectangle, so
    // most of that square is empty: a 60 x 230 hand pivoting near one end sweeps a 460 px
    // square, 212,000 pixels, to cover about 14,000 of them. Every one of the other 198,000
    // still paid for two multiplies, a floor, and a bounds test before being skipped.
    //
    // Measured on the Beige face before this: 234 ms of a 290 ms redraw was the three hands,
    // 81% of the whole dial. Nothing about a smooth second hand is possible at that price.
    //
    // Within one row the source coordinates are LINEAR in dx, so the range of dx that lands
    // inside the sprite is just two intervals intersected. Solve, clamp, and walk only that.
    // The original per-pixel test is kept below as the authority: this narrows the loop, it
    // does not decide what gets drawn, so an off-by-one here costs a wasted iteration rather
    // than a wrong pixel.
    const float lx = (float)(sw - 1), ly = (float)(sh - 1);
    for (int dy = y0; dy <= y1; ++dy) {
        const float oy = dy - cy;
        // sxf(u) = u*ct + ax and syf(u) = -u*st + ay, for u = dx - cx.
        const float ax = oy * st + pivotX, ay = oy * ct + pivotY;
        float uLo = (float)(x0 - cx), uHi = (float)(x1 - cx);
        bool empty = false;
        row_span(ct,  ax, lx, uLo, uHi, empty);
        row_span(-st, ay, ly, uLo, uHi, empty);
        if (empty || uHi < uLo) continue;
        // One pixel of slack each way, because the bounds above are on the sampled point and
        // the guard below tests the texel pair around it.
        int rx0 = (int)floorf(cx + uLo) - 1, rx1 = (int)ceilf(cx + uHi) + 1;
        if (rx0 < x0) rx0 = x0;
        if (rx1 > x1) rx1 = x1;
        clip_row(dy, rx0, rx1);
        if (rx1 < rx0) continue;
        // Stepped, not recomputed. sxf advances by ct and syf by -st for every pixel across
        // a row, which turns four multiplies and four adds per pixel into two adds.
        float sxf = (rx0 - cx) * ct + ax;
        float syf = -(rx0 - cx) * st + ay;
        for (int dx = rx0; dx <= rx1; ++dx, sxf += ct, syf -= st) {
            const int sx0 = (int)floorf(sxf), sy0 = (int)floorf(syf), sx1 = sx0 + 1, sy1 = sy0 + 1;
            if (sx0 < 0 || sy0 < 0 || sx1 >= sw || sy1 >= sh) continue;
            const float fx = sxf - sx0, fy = syf - sy0;
            const uint8_t *p00 = src + ((size_t)sy0 * sw + sx0) * 3, *p10 = src + ((size_t)sy0 * sw + sx1) * 3;
            const uint8_t *p01 = src + ((size_t)sy1 * sw + sx0) * 3, *p11 = src + ((size_t)sy1 * sw + sx1) * 3;
            const float w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy), w01 = (1 - fx) * fy, w11 = fx * fy;
            const float aF = p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11;
            if (aF < 8) continue;
            const float aw00 = p00[2] * w00, aw10 = p10[2] * w10, aw01 = p01[2] * w01, aw11 = p11[2] * w11;
            const float aSum = aw00 + aw10 + aw01 + aw11;
            // ONE reciprocal, not three divides. Colour is weighted by alpha and then
            // normalised on all three channels, and a float division is the most expensive
            // arithmetic on this chip by a distance: three of them per pixel, over the forty
            // thousand a sweeping hand touches, is the difference between a hand that glides
            // and one Zion can see stepping.
            const float invA = 1.0f / aSum;
            uint8_t r, g, b, r2, g2, b2, r3, g3, b3, r4, g4, b4;
            unpack565((uint16_t)(p00[0] | (p00[1] << 8)), r, g, b);
            unpack565((uint16_t)(p10[0] | (p10[1] << 8)), r2, g2, b2);
            unpack565((uint16_t)(p01[0] | (p01[1] << 8)), r3, g3, b3);
            unpack565((uint16_t)(p11[0] | (p11[1] << 8)), r4, g4, b4);
            float rF = (r * aw00 + r2 * aw10 + r3 * aw01 + r4 * aw11) * invA;
            float gF = (g * aw00 + g2 * aw10 + g3 * aw01 + g4 * aw11) * invA;
            float bF = (b * aw00 + b2 * aw10 + b3 * aw01 + b4 * aw11) * invA;
            lv_color_t *dst = &s_buf[dy * SCREEN_W + dx];
            uint8_t dr, dg, db; unpack565(dst->full, dr, dg, db);
            if (blend == 1) { rF = rF * dr / 255.0f; gF = gF * dg / 255.0f; bF = bF * db / 255.0f; }        // multiply
            else if (blend == 2) { rF = 255 - (255 - rF) * (255 - dr) / 255.0f; gF = 255 - (255 - gF) * (255 - dg) / 255.0f; bF = 255 - (255 - bF) * (255 - db) / 255.0f; } // screen
            lv_color_t sc = LV_COLOR_MAKE((uint8_t)rF, (uint8_t)gF, (uint8_t)bF);
            *dst = lv_color_mix(sc, *dst, (lv_opa_t)lroundf(aF));
        }
    }
}

// Composite the editor's exact pixels: plate (background) -> live text -> hand
// sprites (rotated, in the editor's draw order/blend) -> overlay (hub, rim, glass).
// Copy the plate rotated about the screen centre. Used only by themes whose background
// "rotates with" a hand (Launch Kit's Rotate-with control): the crescent border in the
// Modern theme is meant to trail the minute hand, and a static plate made it line up once
// an hour by coincidence.
//
// Nearest-neighbour on purpose. The artwork this exists for is a soft gradient, where
// bilinear buys nothing visible, and the same choice on the radar sweep earlier roughly
// doubled that screen's frame rate. Full-screen, so it is worth not paying for.
// The rotated plate, kept between frames. Rotating is ~217k pixel lookups; a clock with a
// second hand redraws ~33x a second, and the minute-follow angle moves 0.1 deg in that
// time — so all but one of those rotations reproduced the previous image exactly.
// Measured cost of getting this wrong: 993 ms of LVGL time per second, i.e. the CPU
// pinned inside the graphics library, which starved knob input and read as a sluggish
// encoder. Now the rotation happens only when the angle has actually moved, and every
// other frame is a memcpy.
static void blit_plate_rot_slow(const uint16_t *src, float angleDeg);

static uint16_t *s_rotCache      = nullptr;
static float     s_rotCacheAngle = 1e9f;    // no cached angle yet
static const uint16_t *s_rotCacheSrc = nullptr;

static void blit_plate_rot(const uint16_t *src, float angleDeg) {
    const size_t bytes = (size_t)SCREEN_W * SCREEN_H * sizeof(uint16_t);
    if (!s_rotCache) {
#ifdef ESP_PLATFORM
        s_rotCache = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        s_rotCache = (uint16_t *)malloc(bytes);
#endif
    }
    if (s_rotCache) {
        // A quarter of a degree is well under one pixel of movement at this radius, so
        // re-rotating below that threshold buys nothing visible. At minute-follow it means
        // a real rotation roughly every 2.5 s instead of 33 times a second.
        if (s_rotCacheSrc == src && fabsf(angleDeg - s_rotCacheAngle) < 0.25f) {
            memcpy(s_buf, s_rotCache, bytes);
            return;
        }
        blit_plate_rot_slow(src, angleDeg);
        memcpy(s_rotCache, s_buf, bytes);
        s_rotCacheAngle = angleDeg;
        s_rotCacheSrc   = src;
        return;
    }
    blit_plate_rot_slow(src, angleDeg);   // no cache buffer: correct, just slower
}

static void blit_plate_rot_slow(const uint16_t *src, float angleDeg) {
    const float th = angleDeg * DEG2RAD, ct = cosf(th), st = sinf(th);
    const float cx = SCREEN_W * 0.5f, cy = SCREEN_H * 0.5f;
    uint16_t *dst = (uint16_t *)s_buf;
    for (int dy = 0; dy < SCREEN_H; ++dy) {
        const float oy = dy - cy;
        for (int dx = 0; dx < SCREEN_W; ++dx) {
            const float ox = dx - cx;
            const int sx = (int)(ox * ct + oy * st + cx);
            const int sy = (int)(-ox * st + oy * ct + cy);
            dst[(size_t)dy * SCREEN_W + dx] =
                (sx < 0 || sy < 0 || sx >= SCREEN_W || sy >= SCREEN_H) ? 0 : src[(size_t)sy * SCREEN_W + sx];
        }
    }
}

// ---- the drawn face ---------------------------------------------------------
// For any element a palette-mode theme ships no image for (see compose_custom): the dial when there is no plate,
// a hand when there is no sprite for it. Colours are the theme's roles, so a theme that is only a palette has a
// clock that matches the rest of its screens. It ticks once a second: no sweep, no cache, nothing to keep in step.
static inline lv_color_t role_colour(theme_roles::Role r) { return lv_color_hex(theme_style::palette().v[r]); }

static void draw_palette_dial(const struct tm *ti) {
    const clock_face::Layout &L = clock_face::DEFAULT_LAYOUT;
    {   // the ring: a circle outline, drawn as a rectangle whose corners are the whole radius
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_opa = LV_OPA_TRANSP;
        d.border_opa = LV_OPA_COVER;
        d.border_color = role_colour(theme_roles::R_primary);
        d.border_width = (lv_coord_t)lroundf(L.ringW);
        d.radius = LV_RADIUS_CIRCLE;
        lv_canvas_draw_rect(s_canvas, (lv_coord_t)lroundf(L.cx - L.ringR), (lv_coord_t)lroundf(L.cy - L.ringR),
                            (lv_coord_t)lroundf(2 * L.ringR), (lv_coord_t)lroundf(2 * L.ringR), &d);
    }
    for (int i = 0; i < 60; ++i) {          // sixty ticks, every fifth long
        const clock_face::Segment t = clock_face::tick(L, i);
        lv_point_t pts[2] = { P(t.x0, t.y0), P(t.x1, t.y1) };
        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        const bool major = clock_face::is_major(i);
        ld.color = role_colour(major ? theme_roles::R_primary : theme_roles::R_dim);
        ld.width = (lv_coord_t)lroundf(major ? L.majorW : L.minorW);
        ld.opa = LV_OPA_COVER;
        lv_canvas_draw_line(s_canvas, pts, 2, &ld);
    }
    if (!s_noTime) {                        // the day and date, low on the dial where the hands are furthest away
        char ds[16];
        strftime(ds, sizeof(ds), "%a %d", ti);
        lv_draw_label_dsc_t ld;
        lv_draw_label_dsc_init(&ld);
        ld.color = role_colour(theme_roles::R_muted);
        ld.font = &lv_font_montserrat_18;
        ld.align = LV_TEXT_ALIGN_CENTER;
        lv_canvas_draw_text(s_canvas, (lv_coord_t)lroundf(L.cx - 60), (lv_coord_t)lroundf(L.cy + 112), 120, &ld, ds);
    }
}

// A tapered blade with an outline in the background colour so it reads against the ticks.
static void draw_blade(float ang, float len, float tail, float hw, lv_color_t fill, lv_color_t edge) {
    for (int pass = 0; pass < 2; ++pass) {
        const float grow = pass == 0 ? 1.4f : 0.0f;
        const clock_face::Blade b = clock_face::blade(CX, CY, ang, len + grow, tail + grow, hw + grow);
        lv_point_t pts[4] = { P(b.x[0], b.y[0]), P(b.x[1], b.y[1]), P(b.x[2], b.y[2]), P(b.x[3], b.y[3]) };
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = pass == 0 ? edge : fill;
        d.bg_opa = LV_OPA_COVER;
        lv_canvas_draw_polygon(s_canvas, pts, 4, &d);
    }
}

// k: 0 hour, 1 minute, 2 second. The lengths fit inside the ticks (the ring's inner edge is at about 198 px).
static void draw_palette_hand(int k, float angDeg) {
    if (k == 2) {
        draw_needle_at(CX, CY, angDeg, 200, 40, 3, role_colour(theme_roles::R_secondary));
        return;
    }
    const lv_color_t fill = role_colour(theme_roles::R_primary), edge = role_colour(theme_roles::R_bg);
    if (k == 0) draw_blade(angDeg, 120, 22, 8.0f, fill, edge);
    else        draw_blade(angDeg, 186, 26, 6.0f, fill, edge);
}

static void draw_palette_hub() {
    draw_disc(CX, CY, 9, role_colour(theme_roles::R_primary));
    draw_disc(CX, CY, 3, role_colour(theme_roles::R_bg));
}

// Compose the dial.
//
// Two flags, and both exist for the smooth second hand. `skipSecond` leaves the sweeping
// hand out, which is what makes a cached face possible: everything that only changes once a
// minute is composited once and kept. `withOverlay` leaves the glass off, because the
// overlay has to go back on TOP of the second hand and so cannot be baked into that cache.
static void compose_custom(const struct tm *ti, bool skipSecond, bool withOverlay) {
    // Decode the plate first: it's the whole visible dial and the largest buffer,
    // so it gets first claim on PSRAM. (Text is now a baked font, not a giant
    // atlas, so the old "overlay first" ordering is no longer needed.) The overlay
    // is decoded next and blitted at the end (over the hands).
    const uint16_t *plate = custom_plate();
    const uint8_t *overlay = custom_overlay();
    // Angles are needed before the plate now: a theme can ask the plate to rotate with a
    // hand, in which case the straight copy below becomes a rotated one.
    const float p_sec = ti->tm_sec, p_min = ti->tm_min + p_sec / 60.0f;
    const float p_hr = (ti->tm_hour % 12) + p_min / 60.0f;
    const float followAng[4] = { 0.0f, p_hr * 30.0f, p_min * 6.0f, p_sec * 6.0f };
    const int pf = theme_style::clock().plateFollow;
    if (plate) {
        if (pf > 0 && pf < 4) blit_plate_rot(plate, followAng[pf]);
        else memcpy(s_buf, plate, (size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t));
    }
    else {
        lv_canvas_fill_bg(s_canvas, lv_color_hex(theme_style::clock().bg), LV_OPA_COVER);
        if (theme_style::paletteOn()) draw_palette_dial(ti);   // no plate: draw the dial from the palette
    }

    // Live text banners in the design's real baked font (+ firmware glow). A curved banner
    // arcs along the rim instead of sitting on a straight baseline.
    //
    // Used to stay behind CUSTOM_HAS_TEXT{1,2}, a compile-time gate baked in by whichever
    // theme push happened to run last. `show` is the runtime gate now, same as every
    // other layer here.
    //
    // A LAMBDA because the design chooses which side of the hands these fall on. Drawn before
    // them the hands sweep over the words, which is what a watch does and what this firmware
    // has always done; drawn after, the words sit on top, which a date window or a signature
    // across the dial wants. THEME_CAPS 43.
    auto draw_banners = [&]() {
        {
            const theme_style::ClockText &t = theme_style::clock().text1;
            if (t.show) {
                if (t.curved) draw_baked_arc_text(theme_font::clock_text1(), t.fmt, (float)t.curveR, t.arcDeg, t.color, ti, "text1", (lv_opa_t)t.opa);
                else draw_baked_text(theme_font::clock_text1(), t.fmt, t.x, t.y, t.color, t.glow, t.glowColor, t.align, ti, "text1", (lv_opa_t)t.opa, t.bg, t.bgOpa, t.radius);
            }
        }
        {
            const theme_style::ClockText &t = theme_style::clock().text2;
            if (t.show) {
                if (t.curved) draw_baked_arc_text(theme_font::clock_text2(), t.fmt, (float)t.curveR, t.arcDeg, t.color, ti, "text2", (lv_opa_t)t.opa);
                else draw_baked_text(theme_font::clock_text2(), t.fmt, t.x, t.y, t.color, t.glow, t.glowColor, t.align, ti, "text2", (lv_opa_t)t.opa, t.bg, t.bgOpa, t.radius);
            }
        }
    };
    if (!theme_style::clock().textOverHands) draw_banners();

    // kind 3/4 = the two static image layers — same pivot/center/blend metadata as
    // a hand, just always angle 0 (they never rotate, see custom_sprite.cpp).
    const float sec = ti->tm_sec, mins = ti->tm_min + sec / 60.0f, hrs = (ti->tm_hour % 12) + mins / 60.0f;
    const float ang[5] = { hrs * 30.0f, mins * 6.0f, sec * 6.0f, 0.0f, 0.0f };
    // Geometry, draw order, and the per-hand show gate come from the active theme at
    // runtime (theme_style, fed by /themes/<slug>/clock_style.json) rather than from the
    // compile-time CUSTOM_* macros, so hands travel with the theme like every other
    // layer. The macros are still the seed defaults inside theme_style::load().
    const theme_style::Clock &cs = theme_style::clock();
    // Every shadow first, then every hand.
    //
    // Interleaving them would let the minute hand's shadow fall across the hour hand drawn
    // below it, which is what really happens but reads as a smudge on a 466 px dial. Laying
    // all the shadows on the face and then standing the hands on top is the same choice a
    // watch photographer makes with a diffuser, and it costs a second short loop.
    if (cs.shadowOn) {
        bool sawSecondSh = false;
        for (int i = 0; i < cs.orderN; ++i) {
            const int k = cs.order[i];
            if (skipSecond && k == 2) { sawSecondSh = true; continue; }
            if (skipSecond && sawSecondSh) continue;   // above the sweeping hand: drawn per frame
            if (k < 0 || k > 2) continue;              // statics do not cast; they are the face
            const theme_style::Hand &hd = cs.hand[k];
            if (!hd.show) continue;
            CustomSprite sh = custom_shadow(k);
            // Say so when a shadow was asked for and did not arrive.
            //
            // This has now silently gone missing twice, both times right after a theme
            // install, and both times a reboot cured it before anything could be learned:
            // the sprites are decoded lazily into PSRAM and the hour and minute shadows are
            // the two largest allocations on the dial (294 KB and 539 KB), so they are the
            // first things to fail when an install has just left memory tight. The loader
            // reports its own failures, but only at the moment they happen, and by the time
            // anyone notices a missing shadow that line is long gone.
            //
            // Latched, not per frame: this runs sixty times a second and the interesting
            // event is the transition, not the state.
            static bool s_warned[3] = { false, false, false };
            if (k >= 0 && k < 3) {
                if (!sh.data && !s_warned[k]) {
                    s_warned[k] = true;
#if defined(ESP_PLATFORM)
                    Serial.printf("[clock] shadow %d wanted but not loaded - PSRAM free %u KB\n",
                                  k, (unsigned)(ESP.getFreePsram() / 1024));
#endif
                } else if (sh.data && s_warned[k]) {
                    s_warned[k] = false;
#if defined(ESP_PLATFORM)
                    Serial.printf("[clock] shadow %d is back\n", k);
#endif
                }
            }
            // Same art, same angle, same pivot as the hand — only the centre moves, and it
            // moves in SCREEN space, which is the whole reason the light appears to stay put
            // while the hand goes round.
            if (sh.data) blend_shadow(sh.data, sh.w, sh.h, hd.pivotX, hd.pivotY,
                                      (float)(hd.centerX + cs.shadowDX),
                                      (float)(hd.centerY + cs.shadowDY), ang[k]);
        }
    }
    bool drewHand = false;
    bool sawSecond = false;
    for (int i = 0; i < cs.orderN; ++i) {
        const int k = cs.order[i];
        if (k < 0 || k > 4) continue;
        // Stop at the second hand. Everything from there up is redrawn on every sweep frame,
        // clipped to the hand's own box, which is what lets a design order a hand ABOVE its
        // second hand and still sweep. Zion: "there's never a reason to not have a feature
        // we get working not work for all the themes."
        if (skipSecond && sawSecond) continue;
        if (skipSecond && k == 2) { sawSecond = true; continue; }
        const theme_style::Hand &hd = cs.hand[k];
        if (!hd.show) continue;
        CustomSprite spr = custom_hand(k);
        if (spr.data) blend_custom_hand(spr.data, spr.w, spr.h, hd.pivotX, hd.pivotY,
                                        (float)hd.centerX, (float)hd.centerY, ang[k], hd.blend);
        else if (k < 3 && theme_style::paletteOn()) {          // no image for this hand: draw it
            draw_palette_hand(k, ang[k]);
            drewHand = true;
        }
    }
    if (drewHand) draw_palette_hub();
    if (cs.textOverHands) draw_banners();

    // Row by row and inside the clip, so a sweeping hand pays for its own box rather than
    // all 217,156 pixels. Full screen this is 26 ms, the second largest cost on the dial.
    if (overlay && withOverlay) {
        for (int dy = s_clipY0; dy <= s_clipY1; ++dy) {
            const int base = dy * SCREEN_W;
            for (int dx = s_clipX0; dx <= s_clipX1; ++dx) {
                const int i = base + dx;
                const uint8_t a = overlay[i * 3 + 2];
                if (!a) continue;
                lv_color_t sc; sc.full = (uint16_t)(overlay[i * 3] | (overlay[i * 3 + 1] << 8));
                s_buf[i] = lv_color_mix(sc, s_buf[i], a);
            }
        }
    }
}

static void draw_custom(const struct tm *ti) { compose_custom(ti, false, true); }

// ---- the smooth second hand -------------------------------------------------
//
// The dial without its second hand and without its glass, kept between frames. Everything in
// it changes at most once a minute, so it is composed once and then only the sweeping hand's
// own rectangle is rebuilt: copy that box back out of here, blend the hand into it, put the
// glass over it, and invalidate nothing else.
//
// Measured, which is why it is built this way. A full custom redraw is 72 ms, and 55 of those
// are the two full-screen passes: 28.7 ms copying the plate and 26.1 ms mixing the overlay
// across 217,156 pixels. Both scale with area, so a hand covering a quarter of the dial costs
// a quarter of each, and a sweep becomes affordable rather than impossible.
static lv_timer_t *s_tick = nullptr;
// A rolling average of what one sweep frame costs, in milliseconds, measured end to end
// including everything LVGL then does with it.
static float s_sweepMs = 45.0f;
static uint32_t s_tickPeriod = 0;

// 30 a second is the ceiling: the hand turns six degrees a second, so a step is a fifth of a
// degree, well under a pixel at the tip, and asking for more would spend the whole device on
// motion nobody can see. 200 ms is the floor, for a design heavy enough that anything faster
// would be a promise the renderer cannot keep.
static uint32_t sweep_period() {
    // Twice the compositing, because LVGL then renders the invalidated box and pushes it over
    // QSPI, roughly as much again. Measured: 26 ms of work held 13 frames a second at a 70 ms
    // period, and 52 ms of work could not hold 25 at a 40 ms one.
    float ms = s_sweepMs * 2.0f;
    if (ms < 33.0f) ms = 33.0f;
    if (ms > 150.0f) ms = 150.0f;
    return (uint32_t)ms;
}

static lv_color_t *s_under = nullptr;
static int  s_underMin = -1, s_underHr = -1;   // what minute this cache is of
static bool s_prevSecValid = false;
static lv_area_t s_prevSec = { 0, 0, 0, 0 };
static float s_prevAng = 0.0f;
// The frame after a cache rebuild repaints everything, because everything changed.
static bool s_fullNext = true;

// Can this design sweep at all?
//
// The cache holds everything BELOW the hand, so anything a theme draws ABOVE it — a static
// layer ordered on top, or banners set to sit over the hands — would be composited under the
// sweeping hand and come out in the wrong order. Rather than draw it wrongly, such a design
// keeps its tick. The glass is the one exception, because it is re-applied per frame.
// -1 auto (the theme decides), 0 force tick, 1 force sweep. Never persisted: an instrument
// for proving this on the glass before any theme carries the flag.
static int s_forceSweep = -1;

static const char *s_sweepWhyNot = "";

static bool sweep_possible() {
    const theme_style::Clock &cs = theme_style::clock();
    s_sweepWhyNot = "";
    if (s_forceSweep == 0) { s_sweepWhyNot = "forced off"; return false; }
    if (s_forceSweep < 0 && !cs.secondSweep) { s_sweepWhyNot = "the design does not ask for it"; return false; }
    if (!cs.hand[2].show) return (s_sweepWhyNot = "the second hand is hidden", false);
    if (!custom_hand(2).data) return (s_sweepWhyNot = "the second hand is drawn, not an image", false);
    if (cs.textOverHands && (cs.text1.show || cs.text2.show)) return (s_sweepWhyNot = "the words sit over the hands", false);
    // A layer ABOVE the second hand used to rule this out, because the cache held the whole
    // face and the sweeping hand would have landed on top of things meant to cover it. The
    // cache stops at the second hand now and everything above is redrawn each frame, inside
    // the runs that were wiped, so any order works. Zion's Beige dial was refused for exactly
    // this and his objection was the right one: a feature that works should work everywhere.
    bool seenSecond = false;
    for (int i = 0; i < cs.orderN; ++i) if (cs.order[i] == 2) seenSecond = true;
    if (!seenSecond) s_sweepWhyNot = "the second hand is not in the draw order";
    return seenSecond;
}

// The box the second hand can reach at this angle, padded by a pixel for the bilinear tap.
static lv_area_t second_box(float angDeg) {
    const theme_style::Hand &hd = theme_style::clock().hand[2];
    CustomSprite spr = custom_hand(2);
    const int sw = spr.data ? spr.w : 0, sh = spr.data ? spr.h : 0;
    const float px = (float)hd.pivotX, py = (float)hd.pivotY;
    const float th = angDeg * DEG2RAD, ct = cosf(th), st = sinf(th);
    // The four corners of the sprite, turned about the pivot. A rectangle, not a disc: the
    // whole point of the box is that it is smaller than the swing.
    const float xs[4] = { -px, sw - px, sw - px, -px };
    const float ys[4] = { -py, -py, sh - py, sh - py };
    float lo_x = 1e9f, hi_x = -1e9f, lo_y = 1e9f, hi_y = -1e9f;
    for (int i = 0; i < 4; ++i) {
        const float rx = xs[i] * ct - ys[i] * st + (float)hd.centerX;
        const float ry = xs[i] * st + ys[i] * ct + (float)hd.centerY;
        if (rx < lo_x) lo_x = rx;
        if (rx > hi_x) hi_x = rx;
        if (ry < lo_y) lo_y = ry;
        if (ry > hi_y) hi_y = ry;
    }
    lv_area_t a;
    a.x1 = (lv_coord_t)fmaxf(0.0f, floorf(lo_x) - 2.0f);
    a.y1 = (lv_coord_t)fmaxf(0.0f, floorf(lo_y) - 2.0f);
    a.x2 = (lv_coord_t)fminf((float)SCREEN_W - 1, ceilf(hi_x) + 2.0f);
    a.y2 = (lv_coord_t)fminf((float)SCREEN_H - 1, ceilf(hi_y) + 2.0f);
    return a;
}

static void area_join(lv_area_t &a, const lv_area_t &b) {
    if (b.x1 < a.x1) a.x1 = b.x1;
    if (b.y1 < a.y1) a.y1 = b.y1;
    if (b.x2 > a.x2) a.x2 = b.x2;
    if (b.y2 > a.y2) a.y2 = b.y2;
}

// The run of dx, within one row, that a sprite at this angle and centre can touch.
//
// The same arithmetic the blits use, lifted out so the RESTORE and the GLASS can be as
// tight as the drawing is. A second hand is a thin diagonal; its bounding box is three or
// four times its own area, and copying and re-glassing that whole rectangle was costing
// more than the hand itself.
static bool sprite_span(int dy, float ang, float cx, float cy, int pivotX, int pivotY,
                        int sw, int sh, int &lo, int &hi) {
    const float th = ang * DEG2RAD, ct = cosf(th), st = sinf(th);
    const float oy = dy - cy;
    const float ax = oy * st + pivotX, ay = oy * ct + pivotY;
    float uLo = -4000.0f, uHi = 4000.0f;
    bool dead = false;
    row_span(ct,  ax, (float)(sw - 1), uLo, uHi, dead);
    row_span(-st, ay, (float)(sh - 1), uLo, uHi, dead);
    if (dead || uHi < uLo) return false;
    lo = (int)floorf(cx + uLo) - 2;
    hi = (int)ceilf(cx + uHi) + 2;
    return true;
}

// Compose the dial without its second hand and keep it. Once a minute, not once a frame.
static bool rebuild_under(const struct tm *ti) {
    if (!s_buf) return false;
    if (!s_under) {
        const size_t bytes = (size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t);
#if defined(ESP_PLATFORM)
        s_under = (lv_color_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        s_under = (lv_color_t *)malloc(bytes);
#endif
        // No cache, no sweep. Falling back to a tick is a slower clock; drawing without the
        // cache would be a wrong one.
        if (!s_under) return false;
    }
    clip_reset();
    compose_custom(ti, true, false);
    memcpy(s_under, s_buf, (size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t));
    s_underHr = ti->tm_hour;
    s_underMin = ti->tm_min;
    s_prevSecValid = false;
    s_fullNext = true;
    return true;
}

// One frame of sweep: restore the hand's box out of the cache, turn the hand into it, put
// the glass back over that box, and invalidate only that.
static void sweep_frame(float secs) {
#if defined(ESP_PLATFORM)
    const uint32_t t0 = micros();
#endif
    const theme_style::Clock &cs = theme_style::clock();
    const theme_style::Hand &hd = cs.hand[2];
    const float ang = secs * 6.0f;

    lv_area_t box = second_box(ang);
    if (s_prevSecValid) area_join(box, s_prevSec);
    s_prevSec = second_box(ang);
    s_prevSecValid = true;

    s_clipX0 = box.x1; s_clipY0 = box.y1; s_clipX1 = box.x2; s_clipY1 = box.y2;

    // What has to be put back, row by row: everywhere the hand WAS and everywhere it is
    // going, and the same for its shadow. Anything else in the box was never touched.
    //
    // The bounding rectangle of a thin diagonal is three or four times its own area, so
    // restoring and re-glassing the whole box was 18 ms of a 52 ms frame to repair pixels
    // nothing had drawn on.
    CustomSprite spr0 = custom_hand(2);
    const int sw = spr0.data ? spr0.w : 0, sh = spr0.data ? spr0.h : 0;
    const bool shadow = cs.shadowOn && custom_shadow(2).data;
    // STATIC, not on the stack. Two arrays of 466 ints is 3.7 KB, and this runs on the LVGL
    // task whose headroom is measured in single kilobytes; a frame that overflows it would
    // look like a random crash somewhere else entirely. There is only ever one sweep in
    // flight, so one copy is enough.
    static int runLo[SCREEN_H], runHi[SCREEN_H];
    for (int y = box.y1; y <= box.y2; ++y) {
        int lo = 1 << 20, hi = -(1 << 20), a, b;
        if (!s_fullNext && sw > 0) {
            const float angs[2] = { s_prevAng, ang };
            for (int i = 0; i < 2; ++i) {
                if (sprite_span(y, angs[i], (float)hd.centerX, (float)hd.centerY,
                                hd.pivotX, hd.pivotY, sw, sh, a, b)) {
                    if (a < lo) lo = a;
                    if (b > hi) hi = b;
                }
                if (shadow && sprite_span(y, angs[i], (float)(hd.centerX + cs.shadowDX),
                                          (float)(hd.centerY + cs.shadowDY),
                                          hd.pivotX, hd.pivotY, sw, sh, a, b)) {
                    if (a < lo) lo = a;
                    if (b > hi) hi = b;
                }
            }
        } else {
            lo = box.x1; hi = box.x2;
        }
        if (lo < box.x1) lo = box.x1;
        if (hi > box.x2) hi = box.x2;
        runLo[y] = lo; runHi[y] = hi;
        if (hi < lo) continue;
        memcpy(&s_buf[y * SCREEN_W + lo], &s_under[y * SCREEN_W + lo],
               (size_t)(hi - lo + 1) * sizeof(lv_color_t));
    }
    s_fullNext = false;
    s_prevAng = ang;
    // From here on, every layer is confined to exactly what was wiped.
    s_runLo = runLo; s_runHi = runHi;
    CustomSprite spr = custom_hand(2);
    if (cs.shadowOn) {
        CustomSprite sh = custom_shadow(2);
        if (sh.data) blend_shadow(sh.data, sh.w, sh.h, hd.pivotX, hd.pivotY,
                                  (float)(hd.centerX + cs.shadowDX), (float)(hd.centerY + cs.shadowDY), ang);
    }
    if (spr.data) blend_custom_hand(spr.data, spr.w, spr.h, hd.pivotX, hd.pivotY,
                                    (float)hd.centerX, (float)hd.centerY, ang, hd.blend);

    // Everything the design draws ABOVE its second hand, put back over it, inside the box.
    //
    // The cache stops at the second hand, so these are not in it. Redrawing them here is
    // what makes the sweep work on any layer order rather than only on designs that happen
    // to put the second hand on top. They are clipped, so this costs their overlap with the
    // hand's box and nothing more; on a design with nothing above the hand it is an empty
    // loop. The angles are the same ones the cache was built with, so they land exactly
    // where the minute has them.
    {
        struct tm ti;
        time_for_face(&ti);
        {
            const float p_sec = (float)ti.tm_sec, p_min = ti.tm_min + p_sec / 60.0f;
            const float p_hr = (ti.tm_hour % 12) + p_min / 60.0f;
            const float above[5] = { p_hr * 30.0f, p_min * 6.0f, ang, 0.0f, 0.0f };
            bool past = false;
            for (int i = 0; i < cs.orderN; ++i) {
                const int k = cs.order[i];
                if (k < 0 || k > 4) continue;
                if (k == 2) { past = true; continue; }
                if (!past) continue;
                const theme_style::Hand &oh = cs.hand[k];
                if (!oh.show) continue;
                if (cs.shadowOn && k <= 2) {
                    CustomSprite osh = custom_shadow(k);
                    if (osh.data) blend_shadow(osh.data, osh.w, osh.h, oh.pivotX, oh.pivotY,
                                               (float)(oh.centerX + cs.shadowDX),
                                               (float)(oh.centerY + cs.shadowDY), above[k]);
                }
                CustomSprite ospr = custom_hand(k);
                if (ospr.data) blend_custom_hand(ospr.data, ospr.w, ospr.h, oh.pivotX, oh.pivotY,
                                                 (float)oh.centerX, (float)oh.centerY,
                                                 above[k], oh.blend);
            }
        }
    }
    if (const uint8_t *overlay = custom_overlay()) {
        for (int dy = s_clipY0; dy <= s_clipY1; ++dy) {
            const int base = dy * SCREEN_W;
            // Exactly the pixels that were restored. Glassing anything else would be
            // re-mixing the overlay onto a pixel that already has it.
            int gx0 = s_clipX0, gx1 = s_clipX1;
            clip_row(dy, gx0, gx1);
            for (int dx = gx0; dx <= gx1; ++dx) {
                const int i = base + dx;
                const uint8_t a = overlay[i * 3 + 2];
                if (!a) continue;
                lv_color_t sc; sc.full = (uint16_t)(overlay[i * 3] | (overlay[i * 3 + 1] << 8));
                s_buf[i] = lv_color_mix(sc, s_buf[i], a);
            }
        }
    }
    clip_reset();
    // The box only. Invalidating the whole canvas would hand back every pixel this exists to
    // avoid touching.
    lv_obj_invalidate_area(s_canvas, &box);

    // THE WORK, not the interval between frames.
    //
    // This measured the gap between one frame and the next, which is a feedback loop with
    // the very thing it sets: a longer period makes a longer gap, which asks for a longer
    // period again. It walked straight up to its own ceiling and the hand fell to four
    // frames a second, worse than the fixed number it replaced. Compositing cost does not
    // depend on how often it is asked for, so that is what gets measured.
#if defined(ESP_PLATFORM)
    {
        const uint32_t took2 = micros() - t0;
        if (took2 < 400000UL) s_sweepMs += 0.1f * ((float)took2 / 1000.0f - s_sweepMs);
        const uint32_t want = sweep_period();
        if (s_tick && want != s_tickPeriod) {
            s_tickPeriod = want;
            lv_timer_set_period(s_tick, want);
        }
    }
#endif
}

// A shadow cast by the second hand has to be inside the box too, or it smears. Widen by the
// light's offset so the restore covers wherever the shadow landed last frame.
static void sweep_pad_for_shadow() {
    const theme_style::Clock &cs = theme_style::clock();
    if (!cs.shadowOn) return;
    const int dx = cs.shadowDX < 0 ? -cs.shadowDX : cs.shadowDX;
    const int dy = cs.shadowDY < 0 ? -cs.shadowDY : cs.shadowDY;
    s_prevSec.x1 = (lv_coord_t)((s_prevSec.x1 - dx) < 0 ? 0 : s_prevSec.x1 - dx);
    s_prevSec.y1 = (lv_coord_t)((s_prevSec.y1 - dy) < 0 ? 0 : s_prevSec.y1 - dy);
    s_prevSec.x2 = (lv_coord_t)((s_prevSec.x2 + dx) > SCREEN_W - 1 ? SCREEN_W - 1 : s_prevSec.x2 + dx);
    s_prevSec.y2 = (lv_coord_t)((s_prevSec.y2 + dy) > SCREEN_H - 1 ? SCREEN_H - 1 : s_prevSec.y2 + dy);
}

// ---- tick + face management -------------------------------------------------
static void redraw(const struct tm *ti) {
    if (!s_canvas || !s_buf) return;
    draw_custom(ti);
    lv_obj_invalidate(s_canvas);
}

// Seconds within the minute, with the fraction. A sweeping hand needs to know where it is
// between ticks, and getLocalTime only ever answers in whole seconds.
static float second_now() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm ti;
    const time_t t = (time_t)tv.tv_sec;
    localtime_r(&t, &ti);
    return (float)ti.tm_sec + (float)tv.tv_usec / 1000000.0f;
}

// A tick a second, or a frame every 40 ms while sweeping.
//
// Reset whenever a theme is applied, because whether this design sweeps is the theme's
// answer and not a fixed property of the screen.
static void retime(void) {
    if (!s_tick) return;
    // The sweep asks for whatever it has been managing, not a number chosen in advance.
    //
    // A fixed period was wrong twice over. 40 ms asked for frames the device could not draw,
    // so the timer was late every time and lv_timer_handler saturated for no extra frames.
    // 70 ms was measured honestly, and then the frame got two and a half times cheaper and
    // 70 became a ceiling holding back a hand that could have moved twice as often.
    //
    // s_sweepMs is an average of what a frame actually costs on THIS design — a plain dial
    // is a quarter of the work of a busy one — with a fifth on top so the timer, not the
    // renderer, decides when frames happen. That is radar_view's rule about even arrival,
    // applied without having to guess the number.
    lv_timer_set_period(s_tick, sweep_possible() ? sweep_period() : 1000);
}

static void tick_cb(lv_timer_t * /*t*/) {
    if (lv_scr_act() != s_screen) return;
    // ...and not while something is drawn over the top of it. The guard above catches
    // another APP being on screen, because a switch changes the active screen. It does not
    // catch a full-screen panel on the top layer, which leaves this the active screen while
    // hiding every pixel of it, and that is exactly what the wind screen is.
    //
    // Measured, not assumed: 655 ms of canvas work here, once a second, every second
    // somebody spent winding, with all of it discarded because a hidden object's
    // invalidation is dropped. It was the whole of the hitch.
    if (orb_screen_covered()) return;
    struct tm ti;
    time_for_face(&ti);

    if (sweep_possible()) {
        // The cache is of one minute. When the minute rolls, the hour and minute hands have
        // moved and everything under the second hand has to be composed again.
        if (!s_under || ti.tm_min != s_underMin || ti.tm_hour != s_underHr) {
            if (!rebuild_under(&ti)) { redraw(&ti); return; }
            // First frame after a rebuild repaints everything, because everything changed.
            // Same path as any other frame, just with the whole dial as its box.
            s_prevSec.x1 = 0; s_prevSec.y1 = 0;
            s_prevSec.x2 = SCREEN_W - 1; s_prevSec.y2 = SCREEN_H - 1;
            s_prevSecValid = true;
        }
        sweep_pad_for_shadow();
        sweep_frame(second_now());
        return;
    }
    redraw(&ti);
}

// Redraw now, whatever the second says. For coming back from a screen that covered this one
// for a while: the canvas still holds the face as it was when the cover went up, so without
// this the clock shows the wrong time for up to a second after it reappears.
void clockview::setSweep(int mode) {
    s_forceSweep = mode;
    s_underMin = -1; s_underHr = -1; s_prevSecValid = false;
    retime();
#if defined(ESP_PLATFORM)
    const bool ok = sweep_possible();
    Serial.printf("[clock] sweep -> %s (possible: %s%s%s)\n",
                  mode < 0 ? "auto" : (mode ? "forced on" : "forced off"),
                  ok ? "yes" : "no", ok ? "" : " - ", ok ? "" : s_sweepWhyNot);
#endif
}

void clockview::refresh() {
    if (!s_screen) return;
    struct tm ti;
    time_for_face(&ti);
    redraw(&ti);
}

static void apply_face() {
    // The cache belongs to the old theme's dial. Dropping the minute stamp forces a rebuild
    // rather than sweeping a new second hand over somebody else's face.
    s_underMin = -1; s_underHr = -1; s_prevSecValid = false;
    // ...and what the last face COST. Nothing about how often this screen redraws is stored
    // with a design or carried between them: it is measured, here, from whatever is on the
    // glass now. A heavy dial must not leave a light one running at its pace.
    s_sweepMs = 45.0f; s_tickPeriod = 0;
    retime();
    struct tm ti;
    time_for_face(&ti);
    redraw(&ti);
}

// The custom face's plate/overlay/hand sprites decode once into PSRAM and were
// never freed, so they sat resident even while some other app (radar, weather,
// intel) was on screen. Now the shell calls this on the way out, so that memory
// (up to ~1 MB for a photo-background design) goes back to whatever's shown next;
// custom_plate()/custom_overlay()/custom_hand() re-decode lazily the next time
// draw_custom() runs (see the timing it logs).
// Allocate the canvas here rather than in init(). See the header for the measurement that
// prompted it. Safe to call repeatedly: it only allocates what is missing.
void clockview::onEnter() {
    if (!s_screen) return;
    if (!s_buf) {
        const size_t bufBytes = (size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t);
        s_buf = (lv_color_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_buf) { Serial.println("[clock] PSRAM alloc for clock canvas failed"); return; }
        s_canvas = lv_canvas_create(s_screen);
        lv_canvas_set_buffer(s_canvas, s_buf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR);
        lv_obj_center(s_canvas);
        lv_obj_move_background(s_canvas);
        lv_canvas_fill_bg(s_canvas, COL_BLACK, LV_OPA_COVER);
    }
}

void clockview::onExit() {
    custom_sprite_release();
    // The canvas and the rotation cache go too. The canvas object stays, pointing at
    // nothing until the next onEnter refills it: deleting and rebuilding an LVGL object
    // every switch is churn, and its z-order is re-asserted there anyway.
    // DELETE the object, do not hand it a null buffer.
    //
    // The first version called lv_canvas_set_buffer(s_canvas, nullptr, 1, 1, ...) to detach
    // it before freeing. LVGL does not accept that: it hung the UI thread on the very next
    // switch to another app, twice, while core 0 carried on logging happily, which is what
    // a wedged LVGL task looks like from the outside. Deleting the object costs one
    // allocation on the way back in and cannot be misread by the library.
    if (s_canvas) { lv_obj_del(s_canvas); s_canvas = nullptr; }
    if (s_buf)      { heap_caps_free(s_buf);      s_buf = nullptr; }
    if (s_rotCache) { heap_caps_free(s_rotCache); s_rotCache = nullptr; }
}

// ---- build ------------------------------------------------------------------
void clockview::init() {
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COL_BLACK, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // The canvas is NOT allocated here any more; onEnter() takes it when the app is shown
    // and onExit() gives it back. This is the boot app, so it is taken moments later
    // regardless, and the difference is that it is released the moment you leave.

    apply_face();
    s_tick = lv_timer_create(tick_cb, 1000, nullptr);
    retime();
}

lv_obj_t *clockview::screen() {
    return s_screen;
}

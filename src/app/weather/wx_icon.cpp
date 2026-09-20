#include "wx_icon.h"
#include <math.h>
#include <stdint.h>

namespace {

// Named colours, the icon's own palette. They are not the theme's: an outlook reads as an
// outlook (sun is yellow, rain is blue) whatever accent the design chose.
constexpr uint32_t COL_SUN        = 0xFFC02E;
constexpr uint32_t COL_MOON       = 0xDDE6F2;
constexpr uint32_t COL_MOON_SHADE = 0xB4C0D0;
constexpr uint32_t COL_CLOUD      = 0xD5DDE6;
constexpr uint32_t COL_CLOUD_DARK = 0x8A97A6;
constexpr uint32_t COL_RAIN       = 0x4DDCFF;
constexpr uint32_t COL_SNOW       = 0xFFFFFF;
constexpr uint32_t COL_BOLT       = 0xFFD84D;
constexpr uint32_t COL_FOG        = 0x9AA5B1;

constexpr int    SUN_RAYS      = 8;
constexpr float  PI_F          = 3.14159265f;
constexpr int    MIN_LINE_PX   = 2;

// Everything is placed in a unit square and scaled, so one description serves every size.
struct Pen {
    lv_draw_ctx_t *dc;
    lv_coord_t x0, y0;
    float s;
    lv_coord_t X(float u) const { return (lv_coord_t)(x0 + lroundf(u * s)); }
    lv_coord_t Y(float u) const { return (lv_coord_t)(y0 + lroundf(u * s)); }
    lv_coord_t W(float u) const { const int w = (int)lroundf(u * s); return (lv_coord_t)(w < MIN_LINE_PX ? MIN_LINE_PX : w); }
};

void disc(const Pen &p, float cx, float cy, float r, uint32_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(col);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    lv_area_t a;
    a.x1 = p.X(cx - r); a.y1 = p.Y(cy - r);
    a.x2 = p.X(cx + r) - 1; a.y2 = p.Y(cy + r) - 1;
    lv_draw_rect(p.dc, &d, &a);
}

void slab(const Pen &p, float x1, float y1, float x2, float y2, uint32_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(col);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    lv_area_t a;
    a.x1 = p.X(x1); a.y1 = p.Y(y1); a.x2 = p.X(x2) - 1; a.y2 = p.Y(y2) - 1;
    lv_draw_rect(p.dc, &d, &a);
}

void bar(const Pen &p, float x1, float y1, float x2, float y2, float widthU, uint32_t col) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(col);
    d.width = p.W(widthU);
    d.opa = LV_OPA_COVER;
    d.round_start = 1;
    d.round_end = 1;
    lv_point_t a = { p.X(x1), p.Y(y1) };
    lv_point_t b = { p.X(x2), p.Y(y2) };
    lv_draw_line(p.dc, &d, &a, &b);
}

// A cloud is a flat base with three overlapping humps. (ox, oy) shift it, k scales it about
// the unit square's centre, so the same shape serves as the front cloud, a smaller one tucked
// behind a sun, or a darker storm cloud.
void cloud(const Pen &p, float ox, float oy, float k, uint32_t col) {
    auto U = [&](float v, float o) { return 0.5f + (v - 0.5f) * k + o; };
    slab(p, U(0.14f, ox), U(0.50f, oy), U(0.88f, ox), U(0.74f, oy), col);
    disc(p, U(0.34f, ox), U(0.52f, oy), 0.17f * k, col);
    disc(p, U(0.54f, ox), U(0.42f, oy), 0.22f * k, col);
    disc(p, U(0.72f, ox), U(0.54f, oy), 0.15f * k, col);
}

void sun(const Pen &p, float cx, float cy, float r, bool rays) {
    if (rays) {
        for (int i = 0; i < SUN_RAYS; ++i) {
            const float a = (2.0f * PI_F * i) / SUN_RAYS;
            const float c = cosf(a), s = sinf(a);
            bar(p, cx + c * r * 1.45f, cy + s * r * 1.45f, cx + c * r * 1.95f, cy + s * r * 1.95f, 0.05f, COL_SUN);
        }
    }
    disc(p, cx, cy, r, COL_SUN);
}

void moon(const Pen &p, float cx, float cy, float r) {
    disc(p, cx, cy, r, COL_MOON);
    disc(p, cx - r * 0.35f, cy - r * 0.25f, r * 0.18f, COL_MOON_SHADE);
    disc(p, cx + r * 0.30f, cy + r * 0.30f, r * 0.13f, COL_MOON_SHADE);
}

void drops(const Pen &p, int n, uint32_t col, float len, float width) {
    const float xs[3] = { 0.34f, 0.50f, 0.66f };
    for (int i = 0; i < n && i < 3; ++i)
        bar(p, xs[i] + 0.05f, 0.80f, xs[i] - 0.03f, 0.80f + len, width, col);
}

void draw_kind(const Pen &p, WxIconKind k) {
    switch (k) {
    case WxIconKind::ClearDay:
        sun(p, 0.5f, 0.5f, 0.22f, true);
        break;
    case WxIconKind::ClearNight:
        moon(p, 0.5f, 0.5f, 0.30f);
        break;
    case WxIconKind::PartlyDay:
        sun(p, 0.36f, 0.36f, 0.15f, true);
        cloud(p, 0.06f, 0.10f, 0.85f, COL_CLOUD);
        break;
    case WxIconKind::PartlyNight:
        moon(p, 0.36f, 0.36f, 0.20f);
        cloud(p, 0.06f, 0.10f, 0.85f, COL_CLOUD);
        break;
    case WxIconKind::Cloudy:
        cloud(p, -0.10f, -0.10f, 0.78f, COL_CLOUD_DARK);
        cloud(p, 0.04f, 0.06f, 0.92f, COL_CLOUD);
        break;
    case WxIconKind::Fog:
        cloud(p, 0.0f, -0.12f, 0.85f, COL_CLOUD);
        bar(p, 0.22f, 0.78f, 0.78f, 0.78f, 0.05f, COL_FOG);
        bar(p, 0.30f, 0.88f, 0.86f, 0.88f, 0.05f, COL_FOG);
        break;
    case WxIconKind::Drizzle:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD);
        drops(p, 3, COL_RAIN, 0.10f, 0.03f);
        break;
    case WxIconKind::Rain:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD_DARK);
        drops(p, 3, COL_RAIN, 0.17f, 0.05f);
        break;
    case WxIconKind::Showers:
        sun(p, 0.30f, 0.30f, 0.12f, false);
        cloud(p, 0.06f, -0.04f, 0.85f, COL_CLOUD);
        drops(p, 2, COL_RAIN, 0.15f, 0.05f);
        break;
    case WxIconKind::Snow:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD);
        disc(p, 0.34f, 0.86f, 0.045f, COL_SNOW);
        disc(p, 0.50f, 0.94f, 0.045f, COL_SNOW);
        disc(p, 0.66f, 0.86f, 0.045f, COL_SNOW);
        break;
    case WxIconKind::Thunder:
        cloud(p, 0.0f, -0.10f, 0.88f, COL_CLOUD_DARK);
        bar(p, 0.54f, 0.62f, 0.42f, 0.82f, 0.06f, COL_BOLT);
        bar(p, 0.42f, 0.82f, 0.58f, 0.82f, 0.06f, COL_BOLT);
        bar(p, 0.58f, 0.82f, 0.46f, 1.00f, 0.06f, COL_BOLT);
        break;
    case WxIconKind::Count:
        break;
    }
}

void draw_cb(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    Pen p;
    p.dc = lv_event_get_draw_ctx(e);
    p.x0 = a.x1;
    p.y0 = a.y1;
    p.s = (float)lv_obj_get_width(o);
    draw_kind(p, (WxIconKind)(uintptr_t)lv_obj_get_user_data(o));
}

}  // namespace

lv_obj_t *wx_icon::create(lv_obj_t *parent, int sizePx) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, sizePx, sizePx);
    lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_user_data(o, (void *)(uintptr_t)WxIconKind::Cloudy);
    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

void wx_icon::set(lv_obj_t *icon, WxIconKind kind) {
    if (!icon) return;
    if ((WxIconKind)(uintptr_t)lv_obj_get_user_data(icon) == kind) return;
    lv_obj_set_user_data(icon, (void *)(uintptr_t)kind);
    lv_obj_invalidate(icon);
}

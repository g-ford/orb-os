// Aircraft drawing: blips, glyphs, trails, off-range arrows, labels and the selection ring.
// Split out of radar_view.cpp; the shared palette, tunables and state are in radar_internal.h.
#include "radar_internal.h"

namespace radar_impl {

// =============================== aircraft ====================================

// The SWEEP IS NEVER MASKED, by decision (the owner, 2026-08-18), and this is not an oversight
// to be tidied up later. It is the one moving part of the instrument, and a hand that
// blinks out over a piece of artwork reads as a fault rather than as a design. Where a
// sweep needs to stop short of a border, sweepLength already does that honestly, by making
// the hand shorter rather than by hiding part of it.
//
// Masked, by contrast: aircraft and their off-range arrows, their trails and flow tracks,
// the rings and crosshair (baked into the plate by the editors), and the etched map.

static void draw_trail(lv_draw_ctx_t *d, const AcDraw &ac, lv_color_t col) {
    const int n = (int)ac.trail.size();
    if (n < 2) return;
    lv_draw_line_dsc_t t;
    lv_draw_line_dsc_init(&t);
    t.color = col;
    t.width = 2;
    for (int i = 1; i < n; ++i) {
        t.opa = (lv_opa_t)(10 + 45 * i / n);
        lv_point_t a = ac.trail[i - 1], b = ac.trail[i];
        // Hiding the aircraft but still drawing its track across the decoration would
        // defeat the point, so a segment is dropped if either end sits in a zone.
        if (in_excluded_zone(a.x, a.y) || in_excluded_zone(b.x, b.y)) continue;
        lv_draw_line(d, &t, &a, &b);
    }
}

static void draw_ball(lv_draw_ctx_t *d, const AcDraw &ac) {
    // emitted waves: several expanding rings (sonar-ping look)
    lv_draw_arc_dsc_t w;
    lv_draw_arc_dsc_init(&w);
    w.color = ORB_ACCENT;
    w.width = 3;
    for (int wv = 0; wv < 3; ++wv) {
        float ph = s_wavePhase + (float)wv * 0.34f;
        if (ph >= 1.0f) ph -= 1.0f;
        w.opa = scale_opa((lv_opa_t)((1.0f - ph) * 245.0f), ac.freshness);
        if (w.opa > 6) lv_draw_arc(d, &w, &ac.pos, (uint16_t)(BALL_R + 3 + ph * WAVE_EXPAND), 0, 360);
    }

    // the ball
    lv_draw_rect_dsc_t b;
    lv_draw_rect_dsc_init(&b);
    b.bg_color = ac.emergency ? ORB_EMERG : ORB_BLIP;
    b.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
    b.radius = LV_RADIUS_CIRCLE;
    b.border_color = lv_color_hex(0x7A5A00);
    b.border_width = 1;
    b.border_opa = scale_opa(150, ac.freshness);
    lv_area_t r = { (lv_coord_t)(ac.pos.x - BALL_R), (lv_coord_t)(ac.pos.y - BALL_R),
                    (lv_coord_t)(ac.pos.x + BALL_R), (lv_coord_t)(ac.pos.y + BALL_R) };
    lv_draw_rect(d, &b, &r);

    // glossy highlight
    lv_draw_rect_dsc_t hl;
    lv_draw_rect_dsc_init(&hl);
    hl.bg_color = lv_color_hex(0xFFFBCC);
    hl.bg_opa = scale_opa(170, ac.freshness);
    hl.radius = LV_RADIUS_CIRCLE;
    lv_area_t hr = { (lv_coord_t)(ac.pos.x - 5), (lv_coord_t)(ac.pos.y - 6),
                     (lv_coord_t)(ac.pos.x - 1), (lv_coord_t)(ac.pos.y - 2) };
    lv_draw_rect(d, &hl, &hr);
}

static void draw_offrange(lv_draw_ctx_t *d, const AcDraw &ac) {
    // small ball at the rim
    lv_draw_rect_dsc_t b;
    lv_draw_rect_dsc_init(&b);
    b.bg_color = ac.emergency ? ORB_EMERG : ORB_BLIP;
    b.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
    b.radius = LV_RADIUS_CIRCLE;
    lv_area_t r = { (lv_coord_t)(ac.pos.x - 5), (lv_coord_t)(ac.pos.y - 5),
                    (lv_coord_t)(ac.pos.x + 5), (lv_coord_t)(ac.pos.y + 5) };
    lv_draw_rect(d, &b, &r);

    // small orange triangle just outside it, pointing toward the aircraft's bearing
    const lv_coord_t ox = (lv_coord_t)lroundf(ac.pos.x + 12.0f * sinf(ac.bearingDeg * (float)M_PI / 180.0f));
    const lv_coord_t oy = (lv_coord_t)lroundf(ac.pos.y - 12.0f * cosf(ac.bearingDeg * (float)M_PI / 180.0f));
    lv_point_t tri[3] = { rot_pt(0, -7, ac.bearingDeg, ox, oy),
                          rot_pt(5, 4, ac.bearingDeg, ox, oy),
                          rot_pt(-5, 4, ac.bearingDeg, ox, oy) };
    lv_draw_rect_dsc_t td;
    lv_draw_rect_dsc_init(&td);
    td.bg_color = ORB_ACCENT;
    td.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
    lv_draw_polygon(d, &td, tri, 3);
}

// Approximate a canvas shadowBlur glow: concentric filled circles behind the
// real shape, opacity falling off with radius — the same "soft ring" trick
// draw_ball's wave animation and the sweep's fading trail already use.
void draw_glow(lv_draw_ctx_t *d, lv_point_t pos, float baseR, float glowPx, lv_color_t color) {
    if (glowPx <= 0.5f) return;
    const int steps = 5;
    lv_draw_rect_dsc_t g;
    lv_draw_rect_dsc_init(&g);
    g.bg_color = color;
    g.radius = LV_RADIUS_CIRCLE;
    for (int i = steps; i >= 1; --i) {
        const float t = (float)i / (float)steps;
        const float r = baseR + glowPx * t;
        const lv_opa_t opa = (lv_opa_t)((1.0f - t) * (1.0f - t) * 130.0f);
        if (opa < 3) continue;
        g.bg_opa = opa;
        lv_area_t a = { (lv_coord_t)lroundf(pos.x - r), (lv_coord_t)lroundf(pos.y - r),
                        (lv_coord_t)lroundf(pos.x + r), (lv_coord_t)lroundf(pos.y + r) };
        lv_draw_rect(d, &g, &a);
    }
}

// Mirrors the editor's radarKitePoints(): kite=0 is today's notched kite (a
// tail), kite=1 collapses the notch flush with the wings into a plain triangle.
static inline void custom_kite_points(float kite, float *gx, float *gy) {
    const float t = kite < 0.0f ? 0.0f : (kite > 1.0f ? 1.0f : kite);
    const float notchY = 8.0f - 3.0f * t;
    gx[0] = 0.0f; gx[1] = 7.0f; gx[2] = 0.0f; gx[3] = -7.0f;
    gy[0] = -11.0f; gy[1] = 5.0f; gy[2] = notchY; gy[3] = 5.0f;
}

// A pushed design's own blip/selection/off-range/center look, used for every
// theme once a design is active (replaces both Orb's ball-with-waves and the
// phosphor kite/triangle paths below). Trails stay on regardless of style —
// the editor has no live data to preview motion history with, but it's a
// harmless, useful device-only extra, not a visual regression from the design.
// Phase 0 instrumentation for Flight Tracker. Every aircraft on screen gets its icon
// rotated to heading with antialiasing on, which is the most expensive primitive LVGL
// has, and there can be two dozen of them. Print the real cost before optimising it:
// the alternative is pre-rotating the icon into cached bitmaps, which is real work and
// should not be built on an estimate.
static void draw_custom_ac(lv_draw_ctx_t *d) {
#ifdef ARDUINO
    const uint32_t t_ac0 = micros();
    int acDrawn = 0;
#endif
    const theme_style::Radar &rs = theme_style::radar();
    for (const AcDraw &ac : s_acs) {
        // Masked by an exclusion zone: draw nothing for it at all, icon or off-range
        // arrow. It reappears the moment it clears the far side.
        if (ac_masked(ac)) continue;
        if (!ac.inRange) {
            if (!rs.offRangeEnabled) continue;
            const lv_color_t oc = lv_color_hex(rs.offRangeColor);
            lv_draw_rect_dsc_t b;
            lv_draw_rect_dsc_init(&b);
            b.bg_color = oc; b.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness); b.radius = LV_RADIUS_CIRCLE;
            const lv_coord_t sz = (lv_coord_t)rs.offRangeSize;
            lv_area_t r = { (lv_coord_t)(ac.pos.x - sz), (lv_coord_t)(ac.pos.y - sz),
                            (lv_coord_t)(ac.pos.x + sz), (lv_coord_t)(ac.pos.y + sz) };
            lv_draw_rect(d, &b, &r);
            const lv_coord_t ox = (lv_coord_t)lroundf(ac.pos.x + 12.0f * sinf(ac.bearingDeg * (float)M_PI / 180.0f));
            const lv_coord_t oy = (lv_coord_t)lroundf(ac.pos.y - 12.0f * cosf(ac.bearingDeg * (float)M_PI / 180.0f));
            lv_point_t tri[3] = { rot_pt(0, -7, ac.bearingDeg, ox, oy),
                                  rot_pt(5, 4, ac.bearingDeg, ox, oy),
                                  rot_pt(-5, 4, ac.bearingDeg, ox, oy) };
            lv_draw_rect_dsc_t td;
            lv_draw_rect_dsc_init(&td);
            td.bg_color = oc; td.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
            lv_draw_polygon(d, &td, tri, 3);
            continue;
        }

        draw_trail(d, ac, ac.color);

        lv_color_t blipColor;
        if (rs.blipFixedColorMode) {
            blipColor = lv_color_hex(rs.blipFixedColor);
        } else {
            if (ac.onGround)           blipColor = lv_color_hex(rs.blipAltGround);
            else if (ac.altFt < 3000)  blipColor = lv_color_hex(rs.blipAltLow);
            else if (ac.altFt < 10000) blipColor = lv_color_hex(rs.blipAltMid);
            else if (ac.altFt < 20000) blipColor = lv_color_hex(rs.blipAltHigh);
            else if (ac.altFt < 30000) blipColor = lv_color_hex(rs.blipAltCruise);
            else                       blipColor = lv_color_hex(rs.blipAltJet);
        }
        // Selection style 1 (Glow) / 2 (Recolor) change the selected aircraft's
        // OWN icon draw below instead of adding a separate ring — style 0
        // (Ring, the original/default look) leaves blipColor/glow untouched
        // here and draws the ring afterward, exactly as before this field existed.
        const bool isSelected = rs.selEnabled && !s_selHex.empty() && s_selHex == ac.hex;
        if (isSelected && rs.selStyle == 2) blipColor = lv_color_hex(rs.selColor);
        // The blip icon itself (dot/kite/image) — off-range arrow and
        // selection ring below have their own separate enabled toggles.
        if (rs.blipEnabled) {
#ifdef ARDUINO
        ++acDrawn;
#endif
        // Selection style 1 (Glow): boost this aircraft's own glow to at
        // least a visible amount (a 0 selGlow default would otherwise be
        // invisible the moment you switch to Glow) using the selection's
        // color, same "boost the blip's own draw" approach the editor
        // preview uses (see drawBlipImage/drawBlipVector in app.js).
        float selBlipGlowAmt = (float)rs.blipGlow;
        lv_color_t selBlipGlowColor = lv_color_hex(rs.blipGlowColor);
        if (isSelected && rs.selStyle == 1) {
            const float boosted = rs.selGlow > 0 ? (float)rs.selGlow : 18.0f;
            if (boosted > selBlipGlowAmt) selBlipGlowAmt = boosted;
            selBlipGlowColor = lv_color_hex(rs.selGlowColor);
        }
        draw_glow(d, ac.pos, (float)rs.blipSize, selBlipGlowAmt * ac.freshness, selBlipGlowColor);

        if (rs.blipTypeImage) {
            // The uploaded aircraft icon, rotated to heading and (optionally) recolored
            // by altitude band via LVGL's own image recolor mix — the icon's alpha was
            // already derived from its Blend mode client-side (see exportRadarLayers'
            // blipIcon step), so this is always a plain alpha-over rotate here, no
            // blend-mode handling needed at draw time. Pivot/icon bitmap themselves
            // stay compile-time (coupled to whichever icon PNG is actually baked in —
            // see theme_style.h) — only color/size/tint travel per theme here.
            if (const lv_img_dsc_t *icon = radar_custom_blip_icon()) {
                // "Rotate to heading" off: the icon always shows at its own
                // baseline angle, it just moves with the aircraft's position.
                const float headingDeg = rs.blipRotate ? ((ac.track != ac.track) ? 0.0f : ac.track) : 0.0f;
                lv_draw_img_dsc_t idsc;
                lv_draw_img_dsc_init(&idsc);
                idsc.angle = (int16_t)lroundf((headingDeg + CUSTOM_BLIP_IMAGE_BASELINE_DEG) * 10.0f);
                // Theme data first, welded macro as the fallback. A theme that ships its
                // own blip sprite has to be able to say where that sprite turns.
                const int bpx = rs.blipPivotX >= 0 ? rs.blipPivotX : CUSTOM_RADAR_BLIP_PIVOT_X;
                const int bpy = rs.blipPivotY >= 0 ? rs.blipPivotY : CUSTOM_RADAR_BLIP_PIVOT_Y;
                idsc.pivot.x = bpx;
                idsc.pivot.y = bpy;
                idsc.opa = scale_opa(LV_OPA_COVER, ac.freshness);
                idsc.antialias = 1;
                // Selection style 2 (Recolor) forces the tint on for this one
                // aircraft even if the design normally leaves the icon
                // untinted — blipColor is already overridden to selColor
                // above, so this recolors it, same as the editor preview.
                if (rs.blipImageTint || (isSelected && rs.selStyle == 2)) {
                    idsc.recolor = blipColor;
                    idsc.recolor_opa = LV_OPA_COVER;
                }
                const lv_coord_t x0 = (lv_coord_t)(ac.pos.x - bpx);
                const lv_coord_t y0 = (lv_coord_t)(ac.pos.y - bpy);
                lv_area_t r = { x0, y0, (lv_coord_t)(x0 + icon->header.w - 1), (lv_coord_t)(y0 + icon->header.h - 1) };
                lv_draw_img(d, &idsc, &r, icon);
            } else {
                // Design says "image" but nothing decoded (no icon uploaded, or the SD/
                // flash asset failed) — fall back to the plain dot so a blip is never
                // silently invisible.
                lv_draw_rect_dsc_t g;
                lv_draw_rect_dsc_init(&g);
                g.bg_color = blipColor; g.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness); g.radius = LV_RADIUS_CIRCLE;
                const lv_coord_t sz = (lv_coord_t)rs.blipSize;
                lv_area_t r = { (lv_coord_t)(ac.pos.x - sz), (lv_coord_t)(ac.pos.y - sz),
                                (lv_coord_t)(ac.pos.x + sz), (lv_coord_t)(ac.pos.y + sz) };
                lv_draw_rect(d, &g, &r);
            }
        } else if (rs.blipKiteShape) {
            const float th = (rs.blipRotate ? ((ac.track != ac.track) ? 0.0f : ac.track) : 0.0f) * (float)M_PI / 180.0f;
            const float cth = cosf(th), sth = sinf(th);
            float gx[4], gy[4];
            custom_kite_points((float)rs.blipKiteT / 100.0f, gx, gy);
            const float scale = (float)rs.blipSize / 9.0f;
            lv_point_t pts[4];
            for (int i = 0; i < 4; ++i) {
                const float x = (gx[i] * scale) * cth - (gy[i] * scale) * sth;
                const float y = (gx[i] * scale) * sth + (gy[i] * scale) * cth;
                pts[i].x = (lv_coord_t)(ac.pos.x + (lv_coord_t)lroundf(x));
                pts[i].y = (lv_coord_t)(ac.pos.y + (lv_coord_t)lroundf(y));
            }
            lv_draw_rect_dsc_t g;
            lv_draw_rect_dsc_init(&g);
            g.bg_color = blipColor; g.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
            lv_draw_polygon(d, &g, pts, 4);
        } else {
            lv_draw_rect_dsc_t g;
            lv_draw_rect_dsc_init(&g);
            g.bg_color = blipColor; g.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness); g.radius = LV_RADIUS_CIRCLE;
            const lv_coord_t sz = (lv_coord_t)rs.blipSize;
            lv_area_t r = { (lv_coord_t)(ac.pos.x - sz), (lv_coord_t)(ac.pos.y - sz),
                            (lv_coord_t)(ac.pos.x + sz), (lv_coord_t)(ac.pos.y + sz) };
            lv_draw_rect(d, &g, &r);
        }
        } // rs.blipEnabled

        if (isSelected && rs.selStyle == 0) {
            draw_glow(d, ac.pos, (float)rs.selDiameter / 2.0f, (float)rs.selGlow, lv_color_hex(rs.selGlowColor));
            lv_draw_arc_dsc_t sr;
            lv_draw_arc_dsc_init(&sr);
            sr.color = lv_color_hex(rs.selColor); sr.width = rs.selWidth; sr.opa = 240;
            lv_draw_arc(d, &sr, &ac.pos, (uint16_t)(rs.selDiameter / 2), 0, 360);
        }
    }

    // Center marker, drawn last so it sits over every blip — matches the editor's z-order.
    if (rs.centerEnabled) {
        lv_draw_rect_dsc_t cd;
        lv_draw_rect_dsc_init(&cd);
        cd.bg_color = lv_color_hex(rs.centerColor); cd.bg_opa = LV_OPA_COVER; cd.radius = LV_RADIUS_CIRCLE;
        lv_area_t cr = { (lv_coord_t)(s_cx - rs.centerRadius), (lv_coord_t)(s_cy - rs.centerRadius),
                         (lv_coord_t)(s_cx + rs.centerRadius), (lv_coord_t)(s_cy + rs.centerRadius) };
        lv_draw_rect(d, &cd, &cr);
        lv_draw_rect_dsc_t ci;
        lv_draw_rect_dsc_init(&ci);
        ci.bg_color = lv_color_hex(rs.centerInnerColor); ci.bg_opa = LV_OPA_COVER; ci.radius = LV_RADIUS_CIRCLE;
        lv_area_t cir = { (lv_coord_t)(s_cx - rs.centerInnerRadius), (lv_coord_t)(s_cy - rs.centerInnerRadius),
                          (lv_coord_t)(s_cx + rs.centerInnerRadius), (lv_coord_t)(s_cy + rs.centerInnerRadius) };
        lv_draw_rect(d, &ci, &cir);
    }
#ifdef ARDUINO
    {   // Rate-limited: this runs every frame and the log itself must not become the cost.
        static uint32_t s_lastLog = 0;
        const uint32_t now = millis();
        if (now - s_lastLog > 2000) {
            s_lastLog = now;
            Serial.printf("[perf] radar aircraft draw: %lu us for %d aircraft (%s icons)\n",
                          (unsigned long)(micros() - t_ac0), acDrawn,
                          rs.blipTypeImage ? (rs.blipRotate ? "rotated image" : "image, no rotate")
                                           : "vector");
        }
    }
#endif
}

void ac_draw_cb(lv_event_t *e) {
    RADAR_PHASE(RP_AC);
    // Once per frame, and on THIS layer rather than the grid: a custom design bakes its
    // rings into the plate, so the grid layer is never drawn and the report never spoke.
    radar_phase_report();
    lv_draw_ctx_t *d = lv_event_get_draw_ctx(e);
    if (customStyled()) { draw_custom_ac(d); return; }
    const bool drg = orb();
    int balls = 0, arrows = 0;

    for (const AcDraw &ac : s_acs) {
        if (ac_masked(ac)) continue;   // exclusion zones apply to the stock scopes too
        if (drg) {
            if (ac.inRange) {
                if (balls >= ORB_BLIPS) continue;   // up to 7 in-range balls
                draw_trail(d, ac, ORB_FLOW);
                draw_ball(d, ac);
                balls++;
            } else {
                if (arrows >= ORB_ARROWS) continue;  // up to 8 off-range arrows
                draw_offrange(d, ac);
                arrows++;
            }
        } else {
            if (!ac.inRange) continue;            // phosphor shows in-range traffic only
            draw_trail(d, ac, ac.color);
            const float th = ((ac.track != ac.track) ? 0.0f : ac.track) * (float)M_PI / 180.0f;
            const float c = cosf(th), s = sinf(th);
            const float *gx = GX, *gy = GY;
            if (aviator()) {                     // little bombers vs little fighters (both convex kites)
                if (is_big_type(ac.type)) { gx = BOMBER_X;  gy = BOMBER_Y; }
                else                      { gx = FIGHTER_X; gy = FIGHTER_Y; }
            }
            lv_point_t pts[4];
            for (int i = 0; i < 4; ++i) {
                const float x = gx[i] * c - gy[i] * s;
                const float y = gx[i] * s + gy[i] * c;
                pts[i].x = (lv_coord_t)(ac.pos.x + (lv_coord_t)lroundf(x));
                pts[i].y = (lv_coord_t)(ac.pos.y + (lv_coord_t)lroundf(y));
            }
            lv_draw_rect_dsc_t g;
            lv_draw_rect_dsc_init(&g);
            g.bg_color = ac.color;
            g.bg_opa = scale_opa(LV_OPA_COVER, ac.freshness);
            lv_draw_polygon(d, &g, pts, 4);
            if (ac.emergency) {
                lv_draw_arc_dsc_t h;
                lv_draw_arc_dsc_init(&h);
                h.color = COL_EMERG; h.width = 2; h.opa = scale_opa(200, ac.freshness);
                lv_draw_arc(d, &h, &ac.pos, 16, 0, 360);
            }
        }

        // selection ring(s)
        if (!s_selHex.empty() && s_selHex == ac.hex) {
            lv_draw_arc_dsc_t sr;
            lv_draw_arc_dsc_init(&sr);
            sr.width = 2;
            sr.opa = 240;
            if (drg) {
                sr.color = ORB_ACCENT;
                lv_draw_arc(d, &sr, &ac.pos, 15, 0, 360);
                lv_draw_arc(d, &sr, &ac.pos, 23, 0, 360);
            } else {
                sr.color = ac.emergency ? COL_EMERG : s_cInk;
                lv_draw_arc(d, &sr, &ac.pos, 19, 0, 360);
            }
        }

        // floating labels (phosphor only; orb keeps clean balls + the tap card)
        if (!drg) {
            lv_draw_label_dsc_t lc;
            lv_draw_label_dsc_init(&lc);
            lc.font = s_bigText ? &lv_font_montserrat_18 : &lv_font_montserrat_14;
            lc.color = s_cInk;
            lv_area_t a1 = { (lv_coord_t)(ac.pos.x + 12), (lv_coord_t)(ac.pos.y - 14),
                             (lv_coord_t)(ac.pos.x + 168), (lv_coord_t)(ac.pos.y + 4) };
            if (ac.call[0]) lv_draw_label(d, &lc, &a1, ac.call, NULL);
            lv_draw_label_dsc_t la;
            lv_draw_label_dsc_init(&la);
            la.font = s_bigText ? &lv_font_montserrat_16 : &lv_font_montserrat_12;
            la.color = ac.color;
            lv_area_t a2 = { a1.x1, (lv_coord_t)(ac.pos.y + 4), a1.x2, (lv_coord_t)(ac.pos.y + 26) };
            if (ac.altTxt[0]) lv_draw_label(d, &la, &a2, ac.altTxt, NULL);
        }
    }
}

}  // namespace radar_impl

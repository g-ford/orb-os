// The rotating sweep: the hand, its wake, the weather sweep and the frame-time pacing that drives them.
// Split out of radar_view.cpp; the shared palette, tunables and state are in radar_internal.h.
#include "radar_internal.h"

namespace radar_impl {

// =============================== sweep =======================================
// A custom design's sweep is a live parameter set (color/leadColor/trailDeg/
// opacity/length), not baked into the plate — it animates, so it has to stay a
// real draw callback either way. Orb's plain grid has no sweep by default, but
// a pushed design's own sweep.enabled should still apply regardless of theme.
static inline float sweepLenPx()    { return customStyled() ? (float)theme_style::radar().sweepLength  : (float)RADAR_R_OUTER_PX; }
static inline float sweepTrailDeg() { return customStyled() ? (float)theme_style::radar().sweepTrailDeg : SWEEP_TRAIL_DEG; }

// The weather map's sweep. Its own object, its own colours, its own settings file.
//
// Written separately rather than by parameterising the Flight Tracker's, deliberately. The
// scope's sweep is the one that took days to get smooth and is the one thing on this device
// most worth not breaking; threading a second look through it to save thirty lines would
// put that at risk for nothing. The two also diverge: no hub here, no aircraft, no
// selection, and this one will grow its own image slot.
//
// What IS shared is the only thing that ever mattered for smoothness: s_sweepDeg, advanced
// by real elapsed time in sweep_timer_cb, on a timer created once and never paused. Two
// objects hang off that as easily as one when only one is ever visible.
void wx_sweep_draw_cb(lv_event_t *e) {
    RADAR_PHASE(RP_WX);
    const theme_style::Weather &ws = theme_style::weather();
    if (!ws.sweepEnabled) return;
    lv_draw_ctx_t *dctx = lv_event_get_draw_ctx(e);
    const lv_point_t center = { s_cx, s_cy };
    const float R = (float)(ws.sweepLength < 20 ? 20 : (ws.sweepLength > 233 ? 233 : ws.sweepLength));
    const float trailDeg = (float)(ws.sweepTrailDeg < 1 ? 1 : (ws.sweepTrailDeg > 180 ? 180 : ws.sweepTrailDeg));
    const float trailOpaMax = (float)ws.sweepOpacity * 2.55f;
    // Clamped rather than trusted: a theme is a file on an SD card and a zero here would
    // divide by zero two lines down.
    const int steps = ws.sweepTrailSteps < 1 ? 1 : (ws.sweepTrailSteps > 60 ? 60 : ws.sweepTrailSteps);

    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(ws.sweepColor);
    ld.width = (lv_coord_t)(ws.sweepTrailWidth < 1 ? 1 : ws.sweepTrailWidth);
    ld.round_start = 1; ld.round_end = 1;
    for (int i = steps; i >= 1; --i) {
        const float frac = 1.0f - (float)i / (float)steps;
        const float ang  = s_wxSweepDeg - (float)i * (trailDeg / (float)steps);
        ld.opa = (lv_opa_t)(frac * frac * trailOpaMax);
        if (ld.opa < 2) continue;
        lv_point_t p2 = rim_point(ang, R);
        lv_draw_line(dctx, &ld, &center, &p2);
    }
    lv_draw_line_dsc_t le;
    lv_draw_line_dsc_init(&le);
    le.color = lv_color_hex(ws.sweepLeadColor);
    le.width = (lv_coord_t)(ws.sweepLeadWidth < 1 ? 1 : ws.sweepLeadWidth);
    le.opa = 217;
    le.round_start = 1; le.round_end = 1;
    lv_point_t lead = rim_point(s_wxSweepDeg, R);
    lv_draw_line(dctx, &le, &center, &lead);
}

void sweep_draw_cb(lv_event_t *e) {
    RADAR_PHASE(RP_SWEEP);
    if (s_loadingPending) return;   // no hand until there is something to sweep over
    if (!customStyled() && orb()) return;
    if (customStyled() && !theme_style::radar().sweepEnabled) return;
    // Image-type sweep is a separate rotating lv_img object (s_sweepImg, see
    // init()/refreshCustomStyle()/sweep_timer_cb) — this vector wedge stays
    // hidden/skipped whenever that's the active look.
    if (customStyled() && theme_style::radar().sweepTypeImage) return;
    lv_draw_ctx_t *dctx = lv_event_get_draw_ctx(e);
    const lv_point_t center = { s_cx, s_cy };
    const float R = sweepLenPx();
    const float trailDeg = sweepTrailDeg();
    const lv_color_t trailColor = customStyled() ? lv_color_hex(theme_style::radar().sweepColor) : s_cRing;
    const lv_color_t leadColor  = customStyled() ? lv_color_hex(theme_style::radar().sweepLeadColor) : s_cLead;
    const float trailOpaMax = customStyled() ? ((float)theme_style::radar().sweepOpacity * 2.55f) : (float)SWEEP_TRAIL_OPA;

    // The trail's line work. Clamped rather than trusted: a theme is a file on an SD card
    // and a zero step count here would divide by zero two lines down.
    int steps = customStyled()
        ? (theme_style::radar().sweepTrailSteps < 1 ? 1 : (theme_style::radar().sweepTrailSteps > 60 ? 60 : theme_style::radar().sweepTrailSteps))
        : SWEEP_TRAIL_STEPS;
    // Overridable over the cable, so the trade between how many lines the fan has and what
    // a frame costs can be swept on a running Orb instead of reasoned about. Not persisted.
    if (s_forceTrailSteps > 0) steps = s_forceTrailSteps;
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = trailColor;
    ld.width = customStyled() ? (lv_coord_t)theme_style::radar().sweepTrailWidth : 5;
    ld.round_start = 1;
    ld.round_end = 1;
    for (int i = steps; i >= 1; --i) {
        const float frac = 1.0f - (float)i / (float)steps;
        const float ang  = s_sweepDeg - (float)i * (trailDeg / (float)steps);
        ld.opa = (lv_opa_t)(frac * frac * trailOpaMax);
        if (ld.opa < 2) continue;
        lv_point_t p2 = rim_point(ang, R);
        lv_draw_line(dctx, &ld, &center, &p2);
    }
    lv_draw_line_dsc_t le;
    lv_draw_line_dsc_init(&le);
    le.color = leadColor;
    le.width = customStyled() ? (lv_coord_t)theme_style::radar().sweepLeadWidth : 2;
    le.opa = 217;
    le.round_start = 1;
    le.round_end = 1;
    lv_point_t lead = rim_point(s_sweepDeg, R);
    lv_draw_line(dctx, &le, &center, &lead);

    // The hub, last so it caps the lines rather than being crossed by them. Part of THIS
    // layer on purpose: it is the point the hand turns about, so it belongs to the hand and
    // moves with it through the stack. The aircraft layer's centre mark is a different
    // thing that happens to sit in the same place.
    if (customStyled()) {
        const theme_style::Radar &rs = theme_style::radar();
        if (rs.sweepHubOn && rs.sweepHubRadius > 0) {
            const float hr = (float)rs.sweepHubRadius;
            draw_glow(dctx, center, hr, (float)rs.sweepHubGlow, lv_color_hex(rs.sweepHubGlowColor));
            lv_draw_rect_dsc_t hd;
            lv_draw_rect_dsc_init(&hd);
            hd.bg_color = lv_color_hex(rs.sweepHubColor);
            hd.bg_opa = LV_OPA_COVER;
            hd.radius = LV_RADIUS_CIRCLE;
            lv_area_t ha = { (lv_coord_t)lroundf(center.x - hr), (lv_coord_t)lroundf(center.y - hr),
                             (lv_coord_t)lroundf(center.x + hr), (lv_coord_t)lroundf(center.y + hr) };
            lv_draw_rect(dctx, &hd, &ha);
        }
    }
}

// The rectangle a wedge of `trailDeg` ending at `deg` actually occupies, walked round the
// arc rather than guessed at. The centre is always in it, because the wedge starts there.
//
// This is what keeps the sweep cheap: without it every step of the hand would mark all
// 217,156 pixels as needing recomputing, when the hand covers a fraction of that. The
// rectangle is bigger than the wedge (a diagonal wedge needs a much larger box than a
// vertical one, since a box has to be axis-aligned), and it is still far smaller than the
// screen at any angle.
static void wedge_bbox_at(float deg, float trailDeg, float R, lv_area_t *out) {
    lv_coord_t minx = s_cx, maxx = s_cx, miny = s_cy, maxy = s_cy;
    const int steps = 10;
    for (int i = 0; i <= steps; ++i) {
        const float a = deg - trailDeg * (float)i / (float)steps;
        const lv_point_t p = rim_point(a, R);
        if (p.x < minx) minx = p.x;
        if (p.x > maxx) maxx = p.x;
        if (p.y < miny) miny = p.y;
        if (p.y > maxy) maxy = p.y;
    }
    // Generous on purpose. Describing the changed region even slightly too small does not
    // fail loudly: it leaves a smear of the previous hand behind, which looks like a fault
    // in the artwork rather than in the invalidation.
    const lv_coord_t pad = 6;
    out->x1 = minx - pad; out->y1 = miny - pad;
    out->x2 = maxx + pad; out->y2 = maxy + pad;
}

static void wedge_bbox(float deg, lv_area_t *out) {
    wedge_bbox_at(deg, sweepTrailDeg(), sweepLenPx(), out);
}

// The same, for the weather map's sweep, which has its own trail, its own reach and its own
// angle. Its geometry comes from the weather theme rather than the radar's: two apps.
static void wx_wedge_bbox(float deg, lv_area_t *out) {
    const theme_style::Weather &ws = theme_style::weather();
    const float trailDeg = (float)(ws.sweepTrailDeg < 1 ? 1 : (ws.sweepTrailDeg > 180 ? 180 : ws.sweepTrailDeg));
    const float R = (float)(ws.sweepLength < 20 ? 20 : (ws.sweepLength > 233 ? 233 : ws.sweepLength));
    wedge_bbox_at(deg, trailDeg, R, out);
}

// Ask both sweeps to repaint just the ground their hands covered between the last frame and
// this one.
//
// The weather map's was not being asked AT ALL on this path. The only invalidate it had sat
// inside the image-sweep branch above, which returns early, so on the ordinary vector path
// the weather sweep repainted only when a new radar frame happened to redraw the tile
// underneath it. That is once a second against the timer's dozen, and it is why that hand
// moved in steps while the Flight Tracker's did not.
static void invalidate_sweeps(float prevDeg, float deg, float wxPrevDeg, float wxDeg) {
    lv_area_t a, b, area;
    if (s_sweep) {
        wedge_bbox(prevDeg, &a);
        wedge_bbox(deg, &b);
        area.x1 = LV_MIN(a.x1, b.x1); area.y1 = LV_MIN(a.y1, b.y1);
        area.x2 = LV_MAX(a.x2, b.x2); area.y2 = LV_MAX(a.y2, b.y2);
        lv_obj_invalidate_area(s_sweep, &area);
    }
    // Invalidating an object on a tile that is not showing costs nothing: LVGL discards it.
    if (s_wxSweep) {
        wx_wedge_bbox(wxPrevDeg, &a);
        wx_wedge_bbox(wxDeg, &b);
        area.x1 = LV_MIN(a.x1, b.x1); area.y1 = LV_MIN(a.y1, b.y1);
        area.x2 = LV_MAX(a.x2, b.x2); area.y2 = LV_MAX(a.y2, b.y2);
        lv_obj_invalidate_area(s_wxSweep, &area);
    }
}

// How far one aircraft's mark can reach from its own position, in px.
//
// blipSize describes the VECTOR shapes only. An image blip is drawn at the sprite's own
// pixel size, about its pivot, so its reach is the distance from that pivot to the far
// corner: a 100 px icon padded as if it were a 10 px dot leaves the invalidation area
// short, and the glide smears the parts that fall outside it. Rotation is why the corner
// matters rather than the edge, and an off-centre pivot is why both sides are measured.
static inline int blip_reach(const theme_style::Radar &rs) {
    if (rs.blipTypeImage) {
        if (const lv_img_dsc_t *icon = radar_custom_blip_icon()) {
            const int w = icon->header.w, h = icon->header.h;
            const int bpx = rs.blipPivotX >= 0 ? rs.blipPivotX : CUSTOM_RADAR_BLIP_PIVOT_X;
            const int bpy = rs.blipPivotY >= 0 ? rs.blipPivotY : CUSTOM_RADAR_BLIP_PIVOT_Y;
            const float dx = (float)LV_MAX(bpx, w - bpx);
            const float dy = (float)LV_MAX(bpy, h - bpy);
            return (int)lroundf(sqrtf(dx * dx + dy * dy));
        }
    }
    return rs.blipSize;
}

// glyph + label bounding box (for partial invalidation during the glide).
// Must cover the label areas drawn in the aircraft layer (they grew for large-text mode).
static inline lv_area_t glyph_bbox(lv_point_t p) {
    lv_area_t a;
    if (customStyled()) {
        // No floating call/alt labels in custom style (those are the separate
        // selection-banner system) — just cover the blip + its glow + the
        // selection ring + its glow, generously, so the glide never trails ghosts.
        const theme_style::Radar &rs = theme_style::radar();
        const int pad = 16 + blip_reach(rs) + rs.blipGlow + rs.selDiameter / 2 + rs.selGlow;
        a.x1 = p.x - pad; a.y1 = p.y - pad; a.x2 = p.x + pad; a.y2 = p.y + pad;
    } else if (orb()) { a.x1 = p.x - 30; a.y1 = p.y - 30; a.x2 = p.x + 30;  a.y2 = p.y + 30; }
    else          { a.x1 = p.x - 22; a.y1 = p.y - 22; a.x2 = p.x + 174; a.y2 = p.y + 32; }
    return a;
}
static inline void area_union(lv_area_t &d, const lv_area_t &s) {
    d.x1 = LV_MIN(d.x1, s.x1); d.y1 = LV_MIN(d.y1, s.y1);
    d.x2 = LV_MAX(d.x2, s.x2); d.y2 = LV_MAX(d.y2, s.y2);
}

// Advance each glyph from its previous position toward the new target (ease-out),
// invalidating only the small region each one occupies. Self-limiting: when a plane
// barely moves (far away / slow), nx==pos and it's skipped — near-zero cost.
static void interp_step(void) {
#if MOTION_INTERP
    if (!s_acLayer || s_acs.empty()) return;
    // Positions still advance when nobody can see them; the REPAINT REQUESTS do not.
    //
    // That split is the lesson from the wind screen on 2026-09-04. A hidden object's
    // invalidation is thrown away by LVGL, so asking for one costs only the asking, but the
    // work of getting there is paid in full either way. Here that work is a bounding box per
    // contact and a union per contact, with up to 28 of them, on a timer that runs on every
    // screen the Orb has. Small next to the 655 ms the clock was spending, and free to stop.
    //
    // The position is NOT skipped, because it is state rather than drawing: it is a pure
    // function of elapsed time, and a scope switched to mid-glide should show its aircraft
    // where they are now, not where they were when somebody last looked.
    const bool seen = lv_obj_is_visible(s_acLayer);
    const uint32_t now = lv_tick_get();
    int moved = 0; long pixels = 0;
    float t = s_pollMs ? (float)(now - s_animStartMs) / (float)s_pollMs : 1.0f;
    if (t > 1.0f) t = 1.0f;
    // LINEAR, not eased. An ease-out is right for a thing arriving somewhere and stopping;
    // an aircraft is not doing that. It is flying at a roughly constant speed, and the two
    // endpoints of this interpolation are two real reports of where it was. Easing between
    // them made it dart most of the way in the first half of the interval and then crawl,
    // which reads as a lurch rather than as flight. Straight line, constant rate, arriving
    // exactly as the next report does.
    const float e = t;
    for (AcDraw &ac : s_acs) {
        const lv_coord_t nx = ac.from.x + (lv_coord_t)lroundf((float)(ac.to.x - ac.from.x) * e);
        const lv_coord_t ny = ac.from.y + (lv_coord_t)lroundf((float)(ac.to.y - ac.from.y) * e);
        if (nx == ac.pos.x && ny == ac.pos.y) continue;
        ++moved;
        pixels += labs((long)nx - ac.pos.x) + labs((long)ny - ac.pos.y);
        lv_point_t np; np.x = nx; np.y = ny;
        if (!seen) { ac.pos = np; continue; }
        lv_area_t inv = glyph_bbox(ac.pos);
        area_union(inv, glyph_bbox(np));
        ac.pos = np;
        lv_obj_invalidate_area(s_acLayer, &inv);
    }
#ifdef ARDUINO
    // What the glyphs ACTUALLY did, so the question "does it look smooth" can be answered
    // from here instead of by asking. Many small moves is a glide; one large move every ten
    // seconds and nothing in between is a snap.
    if (seen) {
        static uint32_t at = 0; static int steps = 0, movers = 0; static long px = 0;
        ++steps; movers += moved; px += pixels;
        if (now - at > 5000) {
            if (at) Serial.printf("[glide] %d steps in %lu ms, %d glyph moves, %ld px total\n",
                                  steps, (unsigned long)(now - at), movers, px);
            at = now; steps = 0; movers = 0; px = 0;
        }
    }
#endif
#endif
}

// Re-derive every contact's freshness from the time it was last heard, so a contact keeps
// dimming while no new snapshot arrives. freshness used to be computed only inside update(),
// which only runs when a poll SUCCEEDS: a dead feed left the last snapshot frozen at the
// brightness it had at that moment. Deliberately not under MOTION_INTERP, which is about
// where glyphs are, not how sure we are that they are still there.
//
// Only a glyph whose brightness moved by a whole opacity step is repainted, so a scope that
// is either fully fresh or fully faded costs nothing here.
static void age_step(void) {
    if (!s_acLayer || s_acs.empty()) return;
    const bool seen = lv_obj_is_visible(s_acLayer);
    const uint32_t now = lv_tick_get();
    for (AcDraw &ac : s_acs) {
        const float f = ac_freshness(aging::age(now, ac.lastUpdateMs));
        if (fabsf(f - ac.freshness) < 1.0f / 255.0f) continue;
        ac.freshness = f;
        if (!seen) continue;
        lv_area_t inv = glyph_bbox(ac.pos);
        lv_obj_invalidate_area(s_acLayer, &inv);
    }
}

void sweep_timer_cb(lv_timer_t *t) {
    (void)t;
    // Selection mode auto-times-out: 5s with no knob input drops back to the
    // populated default view (deselect + release the knob) so the scope doesn't
    // stay pinned on one aircraft. Runs before the early returns below.
    if (s_selectMode && (uint32_t)(lv_tick_get() - s_selActivityMs) >= SELECT_IDLE_MS) radar_exit_select();
    {   // aircraft glyph motion, throttled: see AC_INTERP_MS for why this is slow on purpose
        static uint32_t s_lastInterpMs = 0;
        const uint32_t nowIms = lv_tick_get();
        const uint32_t gate = s_acInterpMs ? s_acInterpMs : (uint32_t)AC_INTERP_MS;
        if (!s_lastInterpMs || (uint32_t)(nowIms - s_lastInterpMs) >= gate) {
            s_lastInterpMs = nowIms;
            interp_step();
            age_step();
        }
    }
    if (!customStyled() && orb()) {
        // animate the blip waves (invalidate only the ball areas)
        s_wavePhase += 0.05f;
        if (s_wavePhase >= 1.0f) s_wavePhase -= 1.0f;
        if (!s_acLayer) return;
        int balls = 0;
        for (const AcDraw &ac : s_acs) {
            if (!ac.inRange) continue;
            if (balls >= ORB_BLIPS) break;
            balls++;
            lv_area_t a = { (lv_coord_t)(ac.pos.x - 44), (lv_coord_t)(ac.pos.y - 44),
                            (lv_coord_t)(ac.pos.x + 44), (lv_coord_t)(ac.pos.y + 44) };
            lv_obj_invalidate_area(s_acLayer, &a);
        }
        return;
    }
    // sweep disabled (live toggle, or the pushed design's own sweep.enabled): glyph interpolation above still runs
    // (this used to read the compile-time CUSTOM_SWEEP_ENABLED/CUSTOM_SWEEP_SPEED
    // macros directly — a leftover from before theme_style existed, so a theme
    // switch could leave the sweep animating at a stale/wrong-theme speed even
    // though sweep_draw_cb's own coloring/trail already followed theme_style)
    if (!s_sweepEnabled || (customStyled() && !theme_style::radar().sweepEnabled)) return;
    // Held still until the scope has data. Re-seeding the pacing state here means the
    // first step after the wait is measured from the first real frame, not from the
    // seconds-long projection stall that preceded it.
    if (s_loadingPending) {
        s_lastSweepMs = 0; s_emaDtMs = 0.0f;
        // Repaint only when the displayed second actually changes: this timer fires every
        // SWEEP_FRAME_MS (100 ms), and re-laying-out a label ten times a second for a number
        // that only moves once a second would just be a different way to burn the frame
        // budget this whole file exists to protect.
        if (s_loadTicker) {
            const int sec = (int)((lv_tick_get() - s_loadStartMs) / 1000U);
            if (sec != s_loadShownSec) {
                s_loadShownSec = sec;
                char t[24];
                snprintf(t, sizeof(t), "%ds", sec);
                lv_label_set_text(s_loadTicker, t);
                show(s_loadTicker, true);
            }
        }
        return;
    }
    s_prevSweepDeg = s_sweepDeg;
    const float speedDps = customStyled() ? (float)theme_style::radar().sweepSpeed : (360.0f * 1000.0f / (float)SWEEP_PERIOD_MS);
    // Advance by REAL elapsed time, not by an assumed SWEEP_FRAME_MS per tick. LVGL
    // timers fire when lv_timer_handler() reaches them, so on a loaded frame they run
    // late; stepping a fixed amount each time made the sweep rotate at whatever fraction
    // of real time the render loop was achieving. Measured 5 fps against a 33 fps target
    // on Flight Tracker, which is exactly the "sweep is slower than Launch Kit shows"
    // The owner spotted. Elapsed-time stepping makes the rotation correct at any frame rate
    // (it just gets chunkier as frames drop, which is honest rather than wrong).
    const uint32_t nowMs = lv_tick_get();
    uint32_t dtMs = s_lastSweepMs ? (uint32_t)(nowMs - s_lastSweepMs) : (uint32_t)SWEEP_FRAME_MS;
    s_lastSweepMs = nowMs;
    // The RAW gap, kept before the clamp below touches it, because the clamp is a safety
    // measure and a safety measure must never be what an instrument reads.
    //
    // This was one number doing both jobs and the statistics block below could therefore not
    // see a single stall: every gap longer than half a second was written down as a perfect
    // 100 ms. It reported 60 frames in a 15 second window, which is a real gap of 250 ms
    // each, as min/avg/max 100/102/105. The instrument said the sweep was flawless while a
    // third of its frames were arriving half a second late.
    //
    // It was blind to a real fault, too. On 2026-09-04 the clock was found redrawing its
    // whole face under the wind screen, 655 ms once a second, which is exactly the stall
    // this clamp was erasing from the record.
    const uint32_t rawDtMs = dtMs;
    if (dtMs > 500) dtMs = SWEEP_FRAME_MS;   // returning from a stall shouldn't teleport the sweep
    // Advance by a SMOOTHED frame time, not the raw one. Raw elapsed-time stepping keeps
    // the rotation speed exactly right, but when frame times wobble (66-100 ms on this
    // hardware) the angular step wobbles with them, +-40%, and that variance IS the
    // stutter the eye picks up. The owner's stated priority is explicit: perfectly even
    // motion beats exactly correct speed. An EMA drifts the speed by a few percent
    // while it adapts, which nobody can see; uneven steps are what everybody sees.
    // Criterion for "the sweep must not look like it stutters": what the eye catches is not
    // a low frame rate, it is UNEVEN steps. So measure the spread of frame times directly
    // rather than trusting the average — 12 fps that arrives every 83 ms looks smooth, and
    // 12 fps that arrives 40/120/60/140 does not, and both report the same fps.
    // Only while somebody can actually see it. The question this number answers is whether
    // the sweep looks smooth, and it looks like nothing at all when the scope is not on the
    // glass. Sampling off-screen mixed the unloaded case into the figure and flattered it.
    // Whichever sweep object this theme actually shows. A design with an image sweep hand
    // HIDES s_sweep and shows s_sweepImg in its place, so testing s_sweep alone reported
    // "not visible" on every such theme and the statistics never printed at all. Caught on
    // the Steam Punk face within minutes of shipping it, which is the argument for reading
    // the visible object rather than the one that happens to be first in the file.
    lv_obj_t *shown = (customStyled() && theme_style::radar().sweepTypeImage) ? s_sweepImg : s_sweep;
    if (shown && lv_obj_is_visible(shown)) {
        static uint32_t s_jMin = 0xFFFFFFFF, s_jMax = 0, s_jAt = 0, s_jN = 0, s_jSum = 0, s_jStall = 0;
        if (rawDtMs < s_jMin) s_jMin = rawDtMs;
        if (rawDtMs > s_jMax) s_jMax = rawDtMs;
        if (rawDtMs > 500) ++s_jStall;
        s_jSum += rawDtMs; ++s_jN;
        if (lv_tick_get() - s_jAt > 15000) {
            if (s_jAt && s_jN) {
                const uint32_t avg = s_jSum / s_jN;
                // Stalls counted separately as well as included in max, because one stall and
                // twenty read the same in a maximum and mean completely different things.
                Serial.printf("[sweep] frames=%lu dt min/avg/max=%lu/%lu/%lu ms spread=%lu ms, %lu stalled\n",
                              (unsigned long)s_jN, (unsigned long)s_jMin,
                              (unsigned long)avg, (unsigned long)s_jMax,
                              (unsigned long)(s_jMax - s_jMin), (unsigned long)s_jStall);
            }
            s_jAt = lv_tick_get(); s_jMin = 0xFFFFFFFF; s_jMax = 0; s_jN = 0; s_jSum = 0; s_jStall = 0;
        }
    }
    // ASK FOR AS MANY FRAMES AS THE SCREEN CAN ACTUALLY DRAW.
    //
    // SWEEP_FRAME_MS is 100 and it was measured honestly, on a design whose frame cost 166 ms.
    // The owner's Modern dial costs about a quarter of that — its trail is ONE line, so the fan
    // this file spends most of its worry on is not even in play — and the sweep was still
    // being asked for ten frames a second while the device had room for three times as many.
    // Ten a second is a sweep moving three degrees a step, which is the stutter.
    //
    // Measured off the WORK, never off the gap. Two earlier attempts fed an EMA of the
    // measured interval back into the period; a gap can never be shorter than the period, so
    // that can only ever push it up, and both walked to their ceiling. lvgl_us divided by
    // frames is time spent rendering per screen frame, which does not depend on how often
    // the sweep asks — each frame draws it once either way.
    //
    // Half as much again as the work, so the timer rather than the renderer decides when
    // frames happen, which is this file's own rule about even arrival.
    //
    // Device only: display.cpp is not built for the simulator, where these counters do not
    // exist and a desktop's frame time would say nothing about an ESP32's anyway.
#ifdef ARDUINO
    {
        static uint32_t lastUs = 0, lastFrames = 0, asked = 0;
        static float ema = 0.0f;
        // Forget the last design's measurement rather than adapt away from it. A theme
        // change reboots the Orb, so this only matters for a live style refresh, but the
        // guarantee is worth making plainly: nothing about how often this screen redraws is
        // ever carried from one design to another, or stored with one.
        if (s_pacingStale) { s_pacingStale = false; lastUs = 0; lastFrames = 0; asked = 0; ema = 0.0f; }
        const uint32_t us = display_lvgl_us(), fr = display_frames();
        if (lastFrames && fr > lastFrames) {
            const float perFrame = (float)(us - lastUs) / (float)(fr - lastFrames) / 1000.0f;
            if (perFrame > 0.5f && perFrame < 400.0f)
                ema = (ema <= 0.0f) ? perFrame : ema + 0.15f * (perFrame - ema);
        }
        lastUs = us; lastFrames = fr;
        if (ema > 0.0f) {
            uint32_t want = (uint32_t)(ema * 1.5f + 0.5f);
            // 40 ms is the floor on purpose. A sweep is a slow hand; past 25 a second the
            // extra frames buy nothing an eye can see and cost the whole screen.
            if (want < 40) want = 40;
            if (want > 200) want = 200;
            if (s_timer && (asked == 0 || want > asked + 6 || want + 6 < asked)) {
                asked = want;
                lv_timer_set_period(s_timer, want);
            }
        }
    }
#endif
    if (s_emaDtMs <= 0.0f) s_emaDtMs = (float)dtMs;
    s_emaDtMs += 0.08f * ((float)dtMs - s_emaDtMs);
    if (s_emaDtMs < 20.0f) s_emaDtMs = 20.0f;
    if (s_emaDtMs > 400.0f) s_emaDtMs = 400.0f;
    s_sweepDeg += speedDps * s_emaDtMs / 1000.0f;
    {
        // The same smoothed dt, so it cannot judder independently of the other one.
        const theme_style::Weather &ws = theme_style::weather();
        const float wxDps = (float)(ws.sweepSpeed < 1 ? 1 : (ws.sweepSpeed > 360 ? 360 : ws.sweepSpeed));
        s_wxPrevSweepDeg = s_wxSweepDeg;
        s_wxSweepDeg += wxDps * s_emaDtMs / 1000.0f;
        if (s_wxSweepDeg >= 360.0f) s_wxSweepDeg -= 360.0f;
    }
    if (s_sweepDeg >= 360.0f) s_sweepDeg -= 360.0f;
    // Image-type sweep: same angle, rotated as a real lv_img instead of the
    // vector wedge's manual bounding-box invalidation below.
    if (customStyled() && theme_style::radar().sweepTypeImage) {
        if (s_sweepImg) lv_img_set_angle(s_sweepImg, (int16_t)lroundf(s_sweepDeg * 10.0f));
        // The Flight Tracker's hand is an image here and LVGL handles its own invalidation
        // for a rotation. The weather map's is a vector wedge either way, so it gets the
        // same box treatment as on the path below rather than a whole-screen repaint.
        invalidate_sweeps(s_sweepDeg, s_sweepDeg, s_wxPrevSweepDeg, s_wxSweepDeg);
        return;
    }
    invalidate_sweeps(s_prevSweepDeg, s_sweepDeg, s_wxPrevSweepDeg, s_wxSweepDeg);
}

}  // namespace radar_impl

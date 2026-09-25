// Selection: hit-testing, the info card, the knob's select mode, and the theme's custom selection text.
// Split out of radar_view.cpp; the shared palette, tunables and state are in radar_internal.h.
#include "radar_internal.h"

namespace radar {

int hitTest(int x, int y) {
    int best = -1;
    long bestD = (long)TAP_RADIUS_PX * TAP_RADIUS_PX;
    const bool drg = orb();
    int balls = 0, arrows = 0;
    for (size_t i = 0; i < s_acs.size(); ++i) {
        if (drg) {
            if (s_acs[i].inRange) { if (balls >= ORB_BLIPS) continue; balls++; }
            else { if (arrows >= ORB_ARROWS) continue; arrows++; }
        } else if (!s_acs[i].inRange) continue;
        const long dx = (long)s_acs[i].pos.x - x;
        const long dy = (long)s_acs[i].pos.y - y;
        const long dd = dx * dx + dy * dy;
        if (dd <= bestD) { bestD = dd; best = (int)i; }
    }
    return best;
}

static void fill_info(const AcDraw &a, AcInfo &out) {
    snprintf(out.hex, sizeof(out.hex), "%s", a.hex);
    snprintf(out.call, sizeof(out.call), "%s", a.call);
    snprintf(out.type, sizeof(out.type), "%s", a.type);
    out.altFt = a.altFt; out.onGround = a.onGround;
    out.vsFpm = a.vsFpm; out.gsKt = a.gsKt;
    out.distKm = a.distKm; out.bearingDeg = a.bearingDeg;
    out.squawk = a.squawk; out.emergency = a.emergency;
}

#if CUSTOM_HAS_RADAR
// Substitute {token} placeholders against one aircraft's live info — the exact
// same token set the Launch Kit editor's own radarFmt()/radarTokens() use, so a
// format string written there means the same thing here. {from}/{to} come from
// the same async route lookup the native detail card already uses.
// The parser moved to text_tokens.cpp when the Weather map got text slots of its own, so
// both screens walk a format string the same way. These two names stay because they are
// spelled all over this file and mean exactly what they always did.
using RadarTok = text_tokens::Tok;
static inline void radar_fmt_toks(char *out, size_t outSz, const char *fmt, const RadarTok *toks, size_t nToks) {
    text_tokens::expand(out, outSz, fmt, toks, nToks);
}
// Returns false (leaves `out` empty) when the format needs {from}/{to} and the
// route lookup hasn't produced both yet — the caller skips drawing that banner
// entirely rather than showing a "?" placeholder while it's still resolving (or
// blank/half-blank if this particular callsign genuinely has no route on file).
static bool radar_fmt(char *out, size_t outSz, const char *fmt, const AcInfo &in) {
    char altS[16], spdS[16], distS[16], hdgS[8], sqkS[8];
    snprintf(altS, sizeof(altS), "%.0f", (double)in.altFt);
    snprintf(spdS, sizeof(spdS), "%.0f", (double)(isnan(in.gsKt) ? 0.0f : in.gsKt));
    snprintf(distS, sizeof(distS), "%.1f", (double)in.distKm);
    snprintf(hdgS, sizeof(hdgS), "%.0f", (double)in.bearingDeg);
    if (in.squawk < 0) snprintf(sqkS, sizeof(sqkS), "-");
    else                snprintf(sqkS, sizeof(sqkS), "%04d", in.squawk);
    const bool needsRoute = strstr(fmt, "{from}") || strstr(fmt, "{to}");
    char rfrom[40] = "", rto[40] = "";
    if (needsRoute && in.call[0]) {
        route_request(in.call);
        route_get(in.call, rfrom, sizeof(rfrom), rto, sizeof(rto));
        // The moment a route first completes for the aircraft on screen, restart the idle
        // countdown. This runs on the LVGL task (route_store does not), so the timer is
        // touched from the same task that reads it. Without this the route could arrive
        // with a second left on the clock and vanish as it registered — the card looking
        // "too fast" when really the text had only just turned up.
        static char s_routeShownFor[12] = "";
        if (rfrom[0] && rto[0] && strncmp(s_routeShownFor, in.call, sizeof(s_routeShownFor) - 1) != 0) {
            snprintf(s_routeShownFor, sizeof(s_routeShownFor), "%s", in.call);
            noteSelectionDetailArrived();
        }
    }
    if (needsRoute && (!rfrom[0] || !rto[0])) { if (outSz) out[0] = 0; return false; }
    RadarTok toks[] = {
        { "callsign", in.call[0] ? in.call : "-" }, { "type", in.type },
        { "alt", altS }, { "spd", spdS }, { "dist", distS }, { "hdg", hdgS }, { "sqk", sqkS },
        { "from", rfrom }, { "to", rto },
    };
    radar_fmt_toks(out, outSz, fmt, toks, sizeof(toks) / sizeof(toks[0]));
    return true;
}
// The scope-range banner: describes the radar's own configured radius (the
// Range slider), not a selected aircraft — always shown when CUSTOM_HAS_RTEXT4,
// regardless of selection. s_lastRangeKm (declared near the other statics
// above) is kept current by update().
static void radar_range_fmt(char *out, size_t outSz, const char *fmt) {
    char rangeS[16]; snprintf(rangeS, sizeof(rangeS), "%.0f", (double)s_lastRangeKm);
    RadarTok toks[] = { { "range", rangeS } };
    radar_fmt_toks(out, outSz, fmt, toks, 1);
}
// Selection banners render into their own transparent canvas (s_textCanvas), not LVGL
// labels — that is what lets a banner curve along an arc (LVGL has no curved-text
// primitive) and glow (canvas shadowBlur is not a firmware effect either).
//
// Both are drawn by the shared curved_text primitive.
//
// This file used to carry its own copy of the glyph-rotation and arc-layout maths, which
// was itself copied out of clock_view.cpp. The Headlines screen wanting the same thing made
// that two copies about to become three, so it moved to curved_text.cpp and this is now the
// call site rather than a third implementation. The arithmetic there is byte for byte what
// was here: these banners are tuned against designs that already exist.
static void rtext_draw_curved(const lv_font_t *font, const char *str, float R, float arcDeg,
                              lv_color_t col, int glow, lv_color_t glowCol, lv_opa_t opa) {
    const curved_text::Target dst = { (uint8_t *)s_textBuf, SCREEN_W, SCREEN_H };
    curved_text::draw_arc(dst, font, str, (float)s_cx, (float)s_cy, R, arcDeg, col, glow, glowCol, opa);
}

static void rtext_draw_straight(const lv_font_t *font, const char *str, float bx, float by,
                                lv_color_t col, int glow, lv_color_t glowCol, int align, lv_opa_t opa,
                                const curved_text::Pill &pill = curved_text::Pill()) {
    const curved_text::Target dst = { (uint8_t *)s_textBuf, SCREEN_W, SCREEN_H };
    curved_text::draw_straight(dst, font, str, bx, by, col, glow, glowCol, align, opa, pill);
}

// Refresh the 4 selection banners for whatever's currently selected — the
// canvas is cleared fully transparent when nothing is, so a design with no
// aircraft picked shows a clean scope, matching the editor's own show/hide.
void refresh_custom_text() {
    if (!s_textCanvas) return;
    AcInfo in;
    const bool have = selected(in);
    const theme_style::Radar &rs = theme_style::radar();
    // `show` is the gate now, not CUSTOM_HAS_RTEXT{n}.
    //
    // Those macros are baked in by whichever theme push last compiled the firmware,
    // so a theme installed as FILES ALONE — which is every theme the theme tool makes — could
    // ship a selection line and have the Orb refuse to draw it, for no reason it could see
    // or state. Exactly the bug the clock's own text1/text2 had (see clock_view.cpp), and
    // exactly the same fix: the theme decides, at runtime.
    bool need = false;
    if (have) for (int i = 0; i < 3; ++i) if (rs.rtext[i].show) need = true;
    if (rs.rtext[3].show) need = true;   // the range banner is scope-wide, selection or not
    // The card, and where it sits. Placed before the text so the banners riding it have a
    // centre to be measured from.
    //
    // Always 180 degrees from the selected aircraft's own bearing: the thing just picked is
    // never underneath the words describing it, however the traffic moves. Recomputed on
    // every refresh, which is also every position update, so the card chases the far side
    // of the dial live rather than being placed once and left there.
    float cardCx = (float)s_cx, cardCy = (float)s_cy;
    const bool cardOn = rs.card.enabled && have;
    if (cardOn) {
        const float opp = (in.bearingDeg + 180.0f) * (float)M_PI / 180.0f;
        cardCx = (float)s_cx + sinf(opp) * (float)rs.card.radius;
        cardCy = (float)s_cy - cosf(opp) * (float)rs.card.radius;
    }
    if (s_cardImg && s_cardObj) {
        const lv_img_dsc_t *art = (cardOn && rs.card.typeImage) ? radar_custom_card() : nullptr;
        if (art) {
            lv_img_set_src(s_cardImg, art);
            lv_obj_set_pos(s_cardImg, (lv_coord_t)lroundf(cardCx - art->header.w / 2.0f),
                                      (lv_coord_t)lroundf(cardCy - art->header.h / 2.0f));
            lv_obj_set_style_img_opa(s_cardImg, (lv_opa_t)rs.card.opacity, 0);
            show(s_cardImg, true);
            show(s_cardObj, false);
        } else if (cardOn) {
            // Drawn card, or an image card whose art failed to decode: a plate is better
            // than words floating over the scope with nothing behind them.
            lv_obj_set_size(s_cardObj, (lv_coord_t)rs.card.w, (lv_coord_t)rs.card.h);
            lv_obj_set_pos(s_cardObj, (lv_coord_t)lroundf(cardCx - rs.card.w / 2.0f),
                                      (lv_coord_t)lroundf(cardCy - rs.card.h / 2.0f));
            lv_obj_set_style_bg_color(s_cardObj, lv_color_hex(rs.card.color), 0);
            lv_obj_set_style_bg_opa(s_cardObj, (lv_opa_t)rs.card.opacity, 0);
            lv_obj_set_style_radius(s_cardObj, (lv_coord_t)rs.card.corner, 0);
            lv_obj_set_style_border_color(s_cardObj, lv_color_hex(rs.card.borderColor), 0);
            lv_obj_set_style_border_width(s_cardObj, (lv_coord_t)rs.card.borderWidth, 0);
            lv_obj_set_style_border_opa(s_cardObj, LV_OPA_COVER, 0);
            show(s_cardObj, true);
            show(s_cardImg, false);
        } else {
            show(s_cardObj, false);
            show(s_cardImg, false);
        }
    }
    if (!need) { canvas_release(s_textCanvas, s_textBuf); return; }
    if (!canvas_acquire(s_textCanvas, s_textBuf, "text")) return;
    lv_canvas_fill_bg(s_textCanvas, lv_color_black(), LV_OPA_TRANSP);
    // CUSTOM_HAS_RTEXT{n} (whether this banner exists at all) and each FONT stay
    // compile-time (see theme_style.h); position/color/glow/format/align/curve now
    // follow the active SD theme.
    if (have) {
        for (int i = 0; i < 3; ++i) {
            const theme_style::RadarText &t = rs.rtext[i];
            if (!t.show) continue;
            char buf[64];
            if (!radar_fmt(buf, sizeof(buf), t.fmt, in)) continue;
            if (t.curved) { rtext_draw_curved(theme_font::radar_text(i), buf, (float)t.curveR, t.arcDeg, lv_color_hex(t.color), t.glow, lv_color_hex(t.glowColor), (lv_opa_t)t.opa); continue; }
            // A line riding the card reads x/y as an offset from the card's own centre, so
            // it travels with it. Only when a card is actually showing: a line pinned to a
            // card that is switched off would otherwise land at an offset from the middle
            // of the scope, which is not where anyone put it.
            const float lx = (t.onCard && cardOn) ? cardCx + (float)(t.x - SCREEN_W / 2) : (float)t.x;
            const float ly = (t.onCard && cardOn) ? cardCy + (float)(t.y - SCREEN_H / 2) : (float)t.y;
            rtext_draw_straight(theme_font::radar_text(i), buf, lx, ly, lv_color_hex(t.color), t.glow, lv_color_hex(t.glowColor), t.align, (lv_opa_t)t.opa, curved_text::pill_of(t));
        }
    }
    // The range banner describes the scope itself (its configured radius), not a
    // selected aircraft, so it's outside the `have` gate above — it stays on the
    // whole time a custom design is active, matching the editor's own preview.
    if (rs.rtext[3].show) {
      const theme_style::RadarText &t = rs.rtext[3];
      char buf[64]; radar_range_fmt(buf, sizeof(buf), t.fmt);
      if (t.curved) rtext_draw_curved(theme_font::radar_text(3), buf, (float)t.curveR, t.arcDeg, lv_color_hex(t.color), t.glow, lv_color_hex(t.glowColor), (lv_opa_t)t.opa);
      else rtext_draw_straight(theme_font::radar_text(3), buf, (float)t.x, (float)t.y, lv_color_hex(t.color), t.glow, lv_color_hex(t.glowColor), t.align, (lv_opa_t)t.opa, curved_text::pill_of(t));
    }
    lv_obj_invalidate(s_textCanvas);
}
#else
void refresh_custom_text() {}
#endif

void select(int idx) {
    if (idx < 0 || idx >= (int)s_acs.size()) s_selHex.clear();
    else s_selHex = s_acs[idx].hex;
    if (s_acLayer) lv_obj_invalidate(s_acLayer);
    refresh_custom_text();
}

// Cycle through in-range aircraft, with an explicit "none selected" stop at
// position 0 so turning wraps all the way back around to it. Builds the
// in-range list fresh each call (aircraft come and go every poll), finds where
// the current selection sits in it (or the "none" stop if nothing/no-longer
// in range), and steps by dir with wraparound.
void selectNext(int dir) {
    std::vector<int> inRangeIdx;
    // Masked aircraft are excluded from the knob's cycle: landing a selection on a
    // contact the person cannot see reads as the knob doing nothing.
    for (int i = 0; i < (int)s_acs.size(); ++i)
        if (s_acs[i].inRange && !ac_masked(s_acs[i])) inRangeIdx.push_back(i);
    const int n = (int)inRangeIdx.size();
    if (n == 0) { select(-1); return; }
    int pos = 0;   // 0 = "none"; 1..n = inRangeIdx[pos-1]
    if (!s_selHex.empty()) {
        for (int k = 0; k < n; ++k) if (s_acs[inRangeIdx[k]].hex == s_selHex) { pos = k + 1; break; }
    }
    pos = ((pos + dir) % (n + 1) + (n + 1)) % (n + 1);
    select(pos == 0 ? -1 : inRangeIdx[pos - 1]);
}

// --- Flight Tracker knob handlers (wired identically by the device + simulator) ---

// onEnter tail: re-attach the pushed plate/overlay (freed on the last onExit) and
// land in the DEFAULT VIEW — nothing selected, knob released, so a turn opens the
// app switcher. A push is what enters selection mode (knobPress below). Unconditional
// now that knobPress() always supports selection mode (not gated on CUSTOM_HAS_RADAR
// any more) — this reset has to run every entry regardless, or stale selection state
// from a prior visit could leak through in a stock (no custom design) build.
void knobEnter() {
    refreshCustomStyle();
    s_selectMode = false;
    select(-1);
    app_shell::setCaptured(false);
    // Data now polls continuously from boot (see main.cpp's adsb_task), so this banner can
    // already be primed to fire the instant the screen appears: staleness kept accumulating
    // the whole time nobody was here to see it. setFeedStatus() re-evaluates on its own
    // very next tick and will show it again if the feed genuinely is still down, but that
    // is a fresh, honest read taken now, not a verdict reached before this screen existed.
    if (s_feedWarn) show(s_feedWarn, false);
    // Only when there is actually nothing on the scope. Coming back to a Flight Tracker
    // that still holds its last snapshot has nothing to wait for, and flashing a loading
    // notice over a working display would be its own kind of lie.
    if (s_acs.empty()) {
        s_loadingPending = true;
        s_loadStartMs = lv_tick_get();
        s_loadShownSec = -1;
        if (s_loading) { show(s_loading, true); lv_obj_move_foreground(s_loading); }
        if (s_sweepImg) show(s_sweepImg, false);
        if (s_sweep)    lv_obj_invalidate(s_sweep);   // clear the vector wedge's last frame
    }
}

// Push: toggle selection. From the default view it grabs the knob and selects the
// first in-range aircraft (nothing to select -> stays in the default view). From
// selection mode it drops straight back to the default view (the manual version of
// the 5s idle timeout). Aircraft selection doesn't depend on a custom design being
// active — it used to be stock-only-vs-cycle-the-scope-skin here, but that legacy
// theme-cycle gesture was a hidden, undiscoverable knob-press with no Settings entry
// at all, confusingly named the same as actual themes. Retired in favor of
// the real Settings "Design" picker (theme_select) — see its header for why.
// Nothing. Selecting an aircraft is what a TURN does now, so the button has no job on this
// screen, and giving it a second way to do the same thing would only invite the question of
// what the difference is. Left as an empty handler rather than unregistered so the shape of
// the app table stays readable.
void knobPress() {}

// A turn selects. Straight in, with no press to arm it first.
//
// This used to be a mode you entered by pressing: press to take the knob, turn to step
// through aircraft, press again to leave. That put the one thing people most want to do on
// this screen behind a button that takes real force, and made a turn — the obvious gesture
// on a dial — do nothing at all until it had been asked permission.
//
// The first turn from nothing selected picks the nearest contact rather than stepping from
// an arbitrary index, so the box lands somewhere sensible. After SELECT_IDLE_MS of stillness
// it clears itself; the Rock gesture leaves the app entirely and clears it on the way out.
void knobTurn(int dir) {
    if (!s_selectMode) {
        if (countInRange() <= 0) return;   // an empty sky has nothing to select
        s_selectMode = true;
        selectNext(1);
    } else {
        selectNext(dir > 0 ? 1 : -1);
    }
    s_selActivityMs = lv_tick_get();
}

// onExit: free the decoded plate/overlay PSRAM and drop selection mode so the idle
// timer can't fire against a scope that's no longer on screen.
void knobExit() {
    radar_sprite_release();
    s_selectMode = false;
    s_loadingPending = false;
    if (s_loading)    show(s_loading, false);    // never leave it stranded over another app
    if (s_loadTicker) show(s_loadTicker, false); // same for its elapsed-time line
    if (s_feedWarn)   show(s_feedWarn, false);   // same for the feed banner
}

bool selected(AcInfo &out) {
    if (s_selHex.empty()) return false;
    for (const AcDraw &a : s_acs)
        if (s_selHex == a.hex) { fill_info(a, out); return true; }
    return false;
}

int count() { return (int)s_acs.size(); }

int countInRange() {
    int n = 0;
    for (const AcDraw &a : s_acs) if (a.inRange) ++n;
    return n;
}

bool info(int idx, AcInfo &out) {
    if (idx < 0 || idx >= (int)s_acs.size()) return false;
    fill_info(s_acs[idx], out);
    return true;
}

} // namespace radar

// Radar scope (M1) + aircraft (M2) + selection (M3) + selectable themes (M4).
// Pure LVGL, portable. Visual reference: upstream's mockup describes the STOCK skin only;
// a theme redraws nearly all of this from theme_style.h.
//   THEME_ORB   : Orb scope: green gradient, square grid, the 7 nearest
//                    aircraft as yellow balls (emitting waves) + off-range arrows.
#include "radar_internal.h"   // the shared block that used to sit here: palette, tunables, state, helpers

// ---- the definitions of the state declared in radar_internal.h ----
namespace radar_impl { uint32_t s_acInterpMs = 0; }
namespace radar_impl { int s_forceGlide = -1; }
namespace radar_impl { int s_forceTrailSteps = 0; }
namespace radar_impl { bool s_pacingStale = true; }
namespace radar_impl { int s_theme = THEME_AVIATOR; }
namespace radar_impl { void (*s_themeCb)(int) = nullptr; }
namespace radar_impl { lv_color_t s_cRing = COL_GREEN, s_cLead = COL_LEAD, s_cInk = COL_INK, s_cSoft = COL_SOFT; }
namespace radar_impl { lv_obj_t *s_loading = nullptr; }
namespace radar_impl { bool s_loadingPending = false; }
namespace radar_impl { uint32_t s_loadStartMs = 0; }
namespace radar_impl { int s_loadShownSec = -1; }
namespace radar_impl { lv_obj_t *s_themeLabel = nullptr; }
namespace radar_impl { lv_timer_t *s_themeLabelTimer = nullptr; }
namespace radar_impl { lv_obj_t *s_parent = nullptr; }
namespace radar_impl { lv_obj_t *s_gridLayer = nullptr; }
namespace radar_impl { lv_obj_t *s_sweep = nullptr; }
namespace radar_impl { lv_obj_t *s_wxSweep = nullptr; }
namespace radar_impl { lv_obj_t *s_sweepImg = nullptr; }
namespace radar_impl { lv_obj_t *s_acLayer = nullptr; }
namespace radar_impl { lv_obj_t *s_flowCanvas = nullptr; }
namespace radar_impl { lv_color_t *s_flowBuf = nullptr; }
namespace radar_impl { lv_obj_t *s_rose[4] = {nullptr, nullptr, nullptr, nullptr}; }
namespace radar_impl { lv_obj_t *s_centerDot = nullptr; }
namespace radar_impl { lv_obj_t *s_pulse = nullptr; }
namespace radar_impl { lv_obj_t *s_rangeLbl = nullptr; }
namespace radar_impl { bool s_rangeLblVisible = true; }
namespace radar_impl { bool s_sweepEnabled = true; }
namespace radar_impl { bool s_airportsEnabled = true; }
namespace radar_impl { int s_maxOnScreen = 12; }
namespace radar_impl { bool s_bigText = false; }
namespace radar_impl { int s_trailMax = TRAIL_MAX; }
namespace radar_impl { int s_flowMax = FLOW_MAX; }
namespace radar_impl { int s_flowGenMax = 14; }
namespace radar_impl { lv_timer_t *s_timer = nullptr; }
namespace radar_impl { float s_sweepDeg = 0.0f; }
namespace radar_impl { float s_wxSweepDeg = 0.0f; }
namespace radar_impl { uint32_t s_lastSweepMs = 0; }
namespace radar_impl { float s_emaDtMs = 0.0f; }
namespace radar_impl { float s_prevSweepDeg = 0.0f; }
namespace radar_impl { float s_wxPrevSweepDeg = 0.0f; }
namespace radar_impl { float s_wavePhase = 0.0f; }
namespace radar_impl { uint32_t s_lastUpdateMs = 0; }
namespace radar_impl { uint32_t s_animStartMs = 0; }
namespace radar_impl { uint32_t s_pollMs = POLL_INTERVAL_MS; }
namespace radar_impl { int s_frameCtr = 0; }
namespace radar_impl { lv_coord_t s_cx = SCREEN_CX, s_cy = SCREEN_CY; }
namespace radar_impl { std::string s_selHex; }
namespace radar_impl { bool s_selectMode = false; }
namespace radar_impl { uint32_t s_selActivityMs = 0; }
namespace radar_impl { float s_lastRangeKm = 0.0f; }
namespace radar_impl { lv_obj_t *s_feedWarn = nullptr; }
namespace radar_impl { lv_obj_t *s_simBadge = nullptr; }
namespace radar_impl { lv_obj_t *s_loadTicker = nullptr; }
namespace radar_impl { lv_obj_t *s_textCanvas = nullptr; }
namespace radar_impl { lv_obj_t *s_cardObj = nullptr; }
namespace radar_impl { lv_obj_t *s_cardImg = nullptr; }
namespace radar_impl { lv_color_t *s_textBuf = nullptr; }
namespace radar_impl { lv_obj_t *s_plateImg = nullptr; }
namespace radar_impl { lv_obj_t *s_ringsImg = nullptr; }
namespace radar_impl { lv_obj_t *s_overlayImg = nullptr; }
namespace radar_impl { lv_obj_t *s_staticImg[2] = { nullptr, nullptr }; }
namespace radar_impl { lv_obj_t *s_dimLayer = nullptr; }
namespace radar_impl { std::deque<FlowSeg> s_flow; }
namespace radar_impl { int s_flowRedrawCtr = 0; }
namespace radar_impl { uint16_t s_flowGen = 0; }
namespace radar_impl { std::vector<AcDraw> s_acs; }
namespace radar_impl { std::set<std::string> s_tracked; }
namespace radar_impl { std::map<std::string, std::vector<lv_point_t>> s_trails; }


bool radar_impl::is_big_type(const char *t) {
    if (!t || !t[0]) return false;
    static const char *kBig[] = {
        "B7", "B4", "A3", "A2", "MD1", "MD9", "DC1", "DC9", "DC8",
        "IL9", "IL7", "C5", "C17", "KC1", "KC4", "E3", "E4", "P8", "B52", "B1", "B2"
    };
    for (const char *p : kBig) if (strncmp(t, p, strlen(p)) == 0) return true;
    return false;
}

void radar_impl::show(lv_obj_t *o, bool v) {
    if (!o) return;
    if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void radar_impl::hide_theme_label_cb(lv_timer_t * /*t*/) {
    show(s_themeLabel, false);
    s_themeLabelTimer = nullptr;   // the one-shot timer already deleted itself
}

void radar_impl::show_theme_label(const char *name) {
    if (!s_themeLabel) return;
    // Never on a custom design. This banner names the STOCK scope skin (Phosphor/Orb/
    // Aviator/...), which is meaningless once a theme is driving the screen —
    // it was appearing as a black "AVIATOR" pill floating over the Steam Punk dial,
    // because radar::init() ends with setTheme() and setTheme() flashes the name.
    if (customStyled()) return;
    lv_label_set_text(s_themeLabel, name);
    show(s_themeLabel, true);
    lv_obj_move_foreground(s_themeLabel);
    if (s_themeLabelTimer) { lv_timer_del(s_themeLabelTimer); s_themeLabelTimer = nullptr; }
    s_themeLabelTimer = lv_timer_create(hide_theme_label_cb, 2000, nullptr);
    lv_timer_set_repeat_count(s_themeLabelTimer, 1);
}

lv_color_t radar_impl::alt_color(float altFt, bool onGround) {
    if (aviator()) {                            // warm rust->ivory ramp, same low->high order
        if (onGround)      return lv_color_hex(0x8A7F6B);
        if (altFt < 3000)  return lv_color_hex(0xB0402C);
        if (altFt < 10000) return lv_color_hex(0xC97A2E);
        if (altFt < 20000) return lv_color_hex(0xC9A227);
        if (altFt < 30000) return lv_color_hex(0xE8DCC0);
        return lv_color_hex(0xF2ECDD);
    }
    if (onGround)      return lv_color_hex(0x888888);
    if (altFt < 3000)  return lv_color_hex(0xFF5A3C);
    if (altFt < 10000) return lv_color_hex(0xFFB23C);
    if (altFt < 20000) return lv_color_hex(0xC8FF3C);
    if (altFt < 30000) return lv_color_hex(0x39FF14);
    return lv_color_hex(0x3CE0FF);
}

float radar_impl::ac_freshness(uint32_t ageMs) { return aging::freshness(ageMs); }

// =============================== flow map ====================================
// ---- lazy full-screen canvases ---------------------------------------------
// The flow (aircraft-trail) canvas and the selection-banner text canvas are each a
// 636 KB PSRAM buffer AND a full-screen alpha layer that every sweep frame must blend
// through. Both were allocated at boot and held forever, which (a) fragmented PSRAM —
// largest free block measured at 423 KB while Flight Tracker was open, below the 651 KB
// the menu's own canvas needs, which is exactly the black-menu-with-white-text failure —
// and (b) taxed every frame for features that are usually inactive: trails are a
// settings toggle, and the banners only exist while an aircraft is selected.
//
// Lifecycle now matches everything else on this device: acquire when there is something
// to show, release when there is not. While unbuffered, the canvas object stays HIDDEN,
// so LVGL also skips it entirely during composition.
bool radar_impl::canvas_acquire(lv_obj_t *canvas, lv_color_t *&buf, const char *tag) {
    if (!canvas) return false;
    if (buf) return true;
    const size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(SCREEN_W, SCREEN_H);
#if defined(ESP_PLATFORM)
    buf = (lv_color_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
#else
    buf = (lv_color_t *)malloc(sz);
#endif
    if (!buf) {
        printf("[radar] %s canvas alloc FAILED (%u bytes) — feature skipped this session\n",
               tag, (unsigned)sz);
        return false;
    }
    lv_canvas_set_buffer(canvas, buf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void radar_impl::canvas_release(lv_obj_t *canvas, lv_color_t *&buf) {
    if (!buf) return;
    if (canvas) {
        lv_img_set_src(canvas, (const void *)NULL);   // detach before freeing: LVGL must never repaint from a freed buffer
        lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    }
#if defined(ESP_PLATFORM)
    heap_caps_free(buf);
#else
    free(buf);
#endif
    buf = nullptr;
}

static void flow_draw_seg(const FlowSeg &s) {
    if (!s_flowCanvas || !s_flowBuf) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = orb() ? ORB_FLOW : s_cRing;
    d.width = 2;
    d.opa = FLOW_OPA;
    lv_point_t pts[2] = { s.a, s.b };
    lv_canvas_draw_line(s_flowCanvas, pts, 2, &d);
}

static void flow_redraw_all(void) {
    if (!s_flowCanvas) return;
    if (s_flow.empty()) { canvas_release(s_flowCanvas, s_flowBuf); return; }
    if (!canvas_acquire(s_flowCanvas, s_flowBuf, "flow")) return;
    lv_canvas_fill_bg(s_flowCanvas, lv_color_black(), LV_OPA_TRANSP);
    for (const FlowSeg &s : s_flow) flow_draw_seg(s);
}

// =============================== grid ========================================
// Where a radar frame goes.
//
// The scope runs at 8 or 9 a second on the Modern design, and its sweep arrives every 100 ms
// at best and 260 at worst — a spread of 100 to 160 ms in a 15 second window. That variance
// is the stutter the owner can see, and radar_view's own comment already says why: what the eye
// catches is not a low frame rate, it is uneven steps.
//
// The same question the clock's face answered last night, asked of this screen: which layer
// owns the frame. Four micros() calls a frame and one line every ten seconds, and it settles
// what to do instead of another round of reasoning about it.
#ifdef ARDUINO
static uint32_t s_rpFrames = 0, s_rpAt = 0, s_rpLvgl = 0;
static const char *RP_NAME[RP_N] = { "grid", "sweep", "aircraft", "wx" };
void radar_impl::radar_phase_report(void) {
    ++s_rpFrames;
    const uint32_t now = millis();
    if (now - s_rpAt <= 10000) return;
    if (s_rpAt && s_rpFrames) {
        uint32_t tot = 0;
        for (int i = 0; i < RP_N; ++i) tot += s_rp[i];
        Serial.printf("[rframe] %lu frames, %lu us each:", (unsigned long)s_rpFrames,
                      (unsigned long)(tot / s_rpFrames));
        for (int i = 0; i < RP_N; ++i)
            Serial.printf(" %s %lu", RP_NAME[i], (unsigned long)(s_rp[i] / s_rpFrames));
        Serial.printf(" | lvgl %lu us/frame\n",
                      (unsigned long)((display_lvgl_us() - s_rpLvgl) / s_rpFrames));
    }
    s_rpAt = now; s_rpFrames = 0;
    for (int i = 0; i < RP_N; ++i) s_rp[i] = 0;
    s_rpLvgl = display_lvgl_us();
}
#else
void radar_impl::radar_phase_report(void) {}
#endif

static void grid_draw_cb(lv_event_t *e) {
    RADAR_PHASE(RP_GRID);
    lv_draw_ctx_t *d = lv_event_get_draw_ctx(e);
    const lv_point_t c = { s_cx, s_cy };

    // A pushed design bakes its own rings/crosshair (and Orb's grid/"you are
    // here" triangle don't apply to a custom look at all) into the plate image
    // set up in init()/refreshCustomStyle() — this layer only still owes the
    // coastline/airport markers, which are the device's own native OSM data,
    // not something a design push carries.
    if (customStyled()) {
        // The theme's map, not the firmware's. Roads used to be drawn here in one fixed
        // grey whatever the design was doing, which a dark or a sepia dial had no way to
        // argue with. Colour, strength and whether they draw at all now travel with the
        // theme; the defaults in theme_style.h are the exact constants used before, so a
        // theme that says nothing about the map is unchanged.
        const theme_style::Radar &rs = theme_style::radar();
        // Clamped rather than trusted: this is a stroke width handed straight to LVGL, and a
        // zero draws nothing while a large one is mostly a way to fill the dial with roads.
        if (rs.mapRoadsOn) {
            const lv_coord_t rw = (lv_coord_t)(rs.mapRoadWidth < 1 ? 1 : (rs.mapRoadWidth > 8 ? 8 : rs.mapRoadWidth));
            roads_sd::draw(d, lv_color_hex(rs.mapRoadColor), (lv_opa_t)rs.mapRoadOpacity, rw);
        }
        // Coastline/waterways deliberately not drawn under a custom design. Inland it is
        // canals and washes rather than a recognisable shoreline, and on a 466 px dial it
        // read as clutter competing with the roads. The data still ships and the stock
        // scopes below still draw it; only the themed path opts out.
        //
        // Airports still answer to the device's own setting as well: it is a preference
        // about what the owner wants to see, not only about how a theme looks.
        if (s_airportsEnabled && rs.mapAirportsOn) airports_draw(d, lv_color_hex(rs.mapAirportColor), 150);
        return;
    }

    if (orb()) {
        lv_draw_line_dsc_t gl;
        lv_draw_line_dsc_init(&gl);
        gl.color = ORB_GRID;
        gl.width = 1;
        gl.opa = 120;
        const int step = 38;
        for (int x = s_cx % step; x < SCREEN_W; x += step) {
            lv_point_t p1 = { (lv_coord_t)x, 0 }, p2 = { (lv_coord_t)x, SCREEN_H - 1 };
            lv_draw_line(d, &gl, &p1, &p2);
        }
        for (int y = s_cy % step; y < SCREEN_H; y += step) {
            lv_point_t p1 = { 0, (lv_coord_t)y }, p2 = { SCREEN_W - 1, (lv_coord_t)y };
            lv_draw_line(d, &gl, &p1, &p2);
        }
        // center "you are here" triangle (orange, pointing up)
        lv_point_t tri[3] = { rot_pt(0, -11, 0, s_cx, s_cy),
                              rot_pt(10, 8, 0, s_cx, s_cy),
                              rot_pt(-10, 8, 0, s_cx, s_cy) };
        lv_draw_rect_dsc_t td;
        lv_draw_rect_dsc_init(&td);
        td.bg_color = ORB_ACCENT;
        td.bg_opa = LV_OPA_COVER;
        td.border_color = lv_color_hex(0x8A4A00);
        td.border_width = 1;
        td.border_opa = 160;
        roads_sd::draw(d, road_color(), 130, 1);
        coastline_draw(d, coast_color(), 170, 2);    // landmass outline under the triangle
        if (s_airportsEnabled) airports_draw(d, airport_color(), 150);
        lv_draw_polygon(d, &td, tri, 3);
        return;
    }

    // roads + coastline first, so the rings/crosshair sit cleanly on top.
    // Steel blue (sepia in Aviator) + 2 px so the coastline reads as a map
    // outline, distinct from the altitude-trail palette; roads are a thinner,
    // more muted neutral so they don't compete with it.
    roads_sd::draw(d, road_color(), 150, 1);
    coastline_draw(d, coast_color(), 165, 2);
    if (s_airportsEnabled) airports_draw(d, airport_color(), 150);

    // phosphor: concentric rings + crosshair
    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.color = s_cRing;
    ad.width = 2;
    const lv_coord_t rr[4] = { 50, 104, 160, RADAR_R_OUTER_PX };
    const lv_opa_t   ro[4] = { 66, 66, 66, 87 };
    for (int i = 0; i < 4; ++i) { ad.opa = ro[i]; lv_draw_arc(d, &ad, &c, rr[i], 0, 360); }

    lv_draw_line_dsc_t ll;
    lv_draw_line_dsc_init(&ll);
    ll.color = s_cRing;
    ll.width = 2;
    ll.opa = 41;
    lv_point_t h1 = { (lv_coord_t)(s_cx - 211), s_cy }, h2 = { (lv_coord_t)(s_cx + 211), s_cy };
    lv_point_t v1 = { s_cx, (lv_coord_t)(s_cy - 211) }, v2 = { s_cx, (lv_coord_t)(s_cy + 211) };
    lv_draw_line(d, &ll, &h1, &h2);
    lv_draw_line(d, &ll, &v1, &v2);
}

// =============================== helpers =====================================
static lv_obj_t *make_label(lv_obj_t *parent, const char *txt, const lv_font_t *font,
                            lv_color_t color, lv_align_t align, lv_coord_t dx, lv_coord_t dy) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_align(l, align, dx, dy);
    return l;
}

static lv_obj_t *make_layer(lv_obj_t *parent, lv_event_cb_t draw_cb) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, SCREEN_W, SCREEN_H);
    lv_obj_center(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    if (draw_cb) lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

static void pulse_anim_cb(void *obj, int32_t v) {
    lv_obj_t *o = (lv_obj_t *)obj;
    const lv_coord_t dia = 10 + (lv_coord_t)((v * 44) / 100);
    lv_obj_set_size(o, dia, dia);
    lv_obj_center(o);
    lv_obj_set_style_border_opa(o, (lv_opa_t)(220 - v * 220 / 100), 0);
}

// Leave selection mode: clear the selection and hand the knob back to the shell
// so a turn opens the app switcher again. File scope so the sweep timer's idle
// check (above, outside the namespace) and knobPress()/knobExit() can all call it.
void radar_impl::radar_exit_select() {
    radar::select(-1);
    s_selectMode = false;
    app_shell::setCaptured(false);
}

// A pushed design can reorder its six movable layers — sweep, aircraft
// (blips/selection/off-range/center, all drawn by ac_draw_cb), the
// selection-banner text canvas, the two plain static image overlays, and the
// plain color-wash overlay — e.g. so the sweep sits above the text instead
// of below it, or so the color wash covers everything but the topmost
// static. The order lists them back-to-front (0=sweep, 1=aircraft, 2=text,
// 3=static1, 4=static2, 5=overlay), the same convention as the clock's
// CUSTOM_HAND_ORDER. Unlike the hands (sprites redrawn in order inside one
// callback), these are separate LVGL objects, so re-stacking means actually
// moving them; s_overlayImg (CRT+glass, NOT the same thing as the color-wash
// s_dimLayer above) is reasserted last so it always stays the true top layer
// regardless of where the other six land.

// Where that order comes from. A theme installed as files alone states it
// in radar_style.json; anything older says nothing and keeps whatever
// CUSTOM_RADAR_LAYER_ORDER the last firmware push welded in.
int radar_impl::radarLayerOrder(const int **out) {
    static const int welded[] = CUSTOM_RADAR_LAYER_ORDER;
    if (customStyled() && theme_style::radar().orderN > 0) {
        *out = theme_style::radar().order;
        return theme_style::radar().orderN;
    }
    *out = welded;
    return CUSTOM_RADAR_LAYER_ORDER_N;
}

// The readout's slot in the six-kind order (0=sweep, 1=aircraft, 2=text, 3/4=statics,
// 5=wash), named because it now stands for a group rather than a single object.
static constexpr int KIND_TEXT = 2;

static void applyRadarLayerOrder() {
    const int *order = nullptr;
    const int orderN = radarLayerOrder(&order);
    // 0=sweep, 1=aircraft, 2=text, 3=static1, 4=static2, 5=overlay (color
    // wash). Whichever sweep object is actually active (vector wedge or
    // rotating image) takes the "sweep" slot — only one of them is ever
    // visible at a time.
    const bool sweepImgActive = customStyled() && theme_style::radar().sweepTypeImage;
    lv_obj_t *byKind[6] = { sweepImgActive ? s_sweepImg : s_sweep, s_acLayer, s_textCanvas, s_staticImg[0], s_staticImg[1], s_dimLayer };
    for (int i = 0; i < orderN; ++i) {
        const int k = order[i];
        if (k < 0 || k >= 6) continue;
        // Kind 2 is not one object, it is THREE: the card's art, the drawn plate behind the
        // words, and the words themselves. Only the text was ever in this table, so lifting
        // the aircraft (kind 1) raised them above the card plate while the text kept rising
        // above everything — the card sat UNDER the aircraft on the device while the theme tool
        // showed it near the top of the stack. They are one thing to the person designing
        // it, so they move as one, plate first and words last.
        if (k == KIND_TEXT) {
            if (s_cardImg) lv_obj_move_foreground(s_cardImg);
            if (s_cardObj) lv_obj_move_foreground(s_cardObj);
            if (s_textCanvas) lv_obj_move_foreground(s_textCanvas);
            continue;
        }
        if (byKind[k]) lv_obj_move_foreground(byKind[k]);
    }
    if (s_overlayImg) lv_obj_move_foreground(s_overlayImg);
    if (s_simBadge)   lv_obj_move_foreground(s_simBadge);   // outranks even the glass

    // What the stack ACTUALLY is, straight from LVGL, rather than what the order array was
    // supposed to achieve. lv_obj_get_index is the real z-position among siblings, so this
    // is the answer to "does the device draw the layers the way the theme tool shows them?"
    // measured instead of argued. It is how the info card was caught sitting under the
    // aircraft: its plate and its text were in two different places in this list.
    {
        struct { const char *name; lv_obj_t *o; } zs[] = {
            { "background", s_plateImg },  { "map",     s_gridLayer },
            { "rings",      s_ringsImg },  { "sweep",   sweepImgActive ? s_sweepImg : s_sweep },
            { "aircraft",   s_acLayer },   { "cardArt", s_cardImg },
            { "cardPlate",  s_cardObj },   { "readout", s_textCanvas },
            { "wash",       s_dimLayer },  { "glass",   s_overlayImg },
        };
        char line[240]; int n = 0;
        n += snprintf(line + n, sizeof(line) - n, "[zorder]");
        for (auto &z : zs)
            if (z.o && n < (int)sizeof(line) - 24)
                n += snprintf(line + n, sizeof(line) - n, " %s=%d", z.name, (int)lv_obj_get_index(z.o));
        Serial.println(line);
    }
}

namespace radar {


// Diagnostic: hide a single layer so its cost shows up as a frame-rate delta.
// Deliberately blunt and deliberately not persisted — it exists to answer "which
// layer is expensive" with a measurement rather than an argument.
void debugHideLayer(int kind, bool hide) {
    lv_obj_t *o = nullptr;
    switch (kind) {
        case 0: o = (customStyled() && theme_style::radar().sweepTypeImage) ? s_sweepImg : s_sweep; break;
        case 1: o = s_acLayer;      break;
        case 2: o = s_textCanvas;   break;
        case 3: o = s_staticImg[0]; break;
        case 4: o = s_staticImg[1]; break;
        case 5: o = s_dimLayer;     break;
        case 6: o = s_plateImg;     break;
        case 7: o = s_gridLayer;    break;   // map: roads + coastline + airports, re-vectored per draw
        case 8: o = s_overlayImg;   break;   // glass + CRT: a full-screen alpha blend, composited
                                             // over whatever region the sweep invalidates, every frame
        default: return;
    }
    // "Show" for the map layer means "whatever the bake decided", not blindly visible:
    // un-hiding a map that is baked into the background would turn per-frame
    // re-vectoring back on, which is exactly what the probe did to tonight's baseline.
    if (kind == 7 && !hide) { apply_grid_visibility(); return; }
    if (o) show(o, !hide);
    Serial.printf("[radar] debug: layer %d %s\n", kind, hide ? "hidden" : "shown");
}



void setTheme(int t) {
    s_theme = ((t % THEME_COUNT) + THEME_COUNT) % THEME_COUNT;
    const bool drg = orb();

    switch (s_theme) {                          // pick the scope chrome palette
        case THEME_MILITARY:
            s_cRing = lv_color_hex(0x49C46B); s_cLead = lv_color_hex(0x76E08C);
            s_cInk  = lv_color_hex(0xE0FFE6); s_cSoft = lv_color_hex(0x9FD7A8); break;
        case THEME_AVIATOR:
            s_cRing = AVI_RING; s_cLead = AVI_LEAD; s_cInk = AVI_INK; s_cSoft = AVI_SOFT; break;
        default:                                // orb (uses its own colors elsewhere) / any invalid value
            s_cRing = COL_GREEN; s_cLead = COL_LEAD; s_cInk = COL_INK; s_cSoft = COL_SOFT; break;
    }

    if (s_parent) {
        if (drg) {
            lv_obj_set_style_bg_color(s_parent, ORB_BG_TOP, 0);
            lv_obj_set_style_bg_grad_color(s_parent, ORB_BG_BOT, 0);
            lv_obj_set_style_bg_grad_dir(s_parent, LV_GRAD_DIR_VER, 0);
        } else {
            lv_obj_set_style_bg_color(s_parent, aviator() ? AVI_BG : lv_color_black(), 0);
            lv_obj_set_style_bg_grad_dir(s_parent, LV_GRAD_DIR_NONE, 0);
        }
        lv_obj_set_style_bg_opa(s_parent, LV_OPA_COVER, 0);
    }
    // A custom design has no compass letters, range readout, or pulse ring in
    // its own preview — hide all of the native chrome so the device matches it.
    const bool styled = customStyled();
    for (int i = 0; i < 4; ++i) show(s_rose[i], !drg && !styled);   // hide compass in Orb
    show(s_rangeLbl, !drg && s_rangeLblVisible && !styled);
    show(s_centerDot, !drg && !styled);                   // orb draws an orange triangle instead; custom style draws its own center marker in ac_draw_cb
    show(s_pulse, !drg && !styled);

    // retint the persistent chrome objects for the active palette
    if (s_rose[0]) lv_obj_set_style_text_color(s_rose[0], s_cInk, 0);
    for (int i = 1; i < 4; ++i) if (s_rose[i]) lv_obj_set_style_text_color(s_rose[i], s_cSoft, 0);
    if (s_centerDot) lv_obj_set_style_bg_color(s_centerDot, s_cInk, 0);
    if (s_pulse)     lv_obj_set_style_border_color(s_pulse, s_cInk, 0);
    if (s_rangeLbl)  lv_obj_set_style_text_color(s_rangeLbl, s_cRing, 0);

    flow_redraw_all();
    if (s_parent) lv_obj_invalidate(s_parent);
    show_theme_label(THEME_NAMES[s_theme]);
    if (s_themeCb) s_themeCb(s_theme);
}

int  theme() { return s_theme; }
const char *themeName(int t) {
    return (t >= 0 && t < THEME_COUNT) ? THEME_NAMES[t] : "";
}
void cycleTheme() { setTheme(s_theme + 1); }
void flashThemeName() { show_theme_label(THEME_NAMES[s_theme]); }   // touch reveal (no theme change)
void setThemeChangedCb(void (*cb)(int)) { s_themeCb = cb; }
// Only meaningful while the loading notice is up: once aircraft arrive the notice is gone
// and there is nothing to relabel, and a scope with traffic on it does not need telling
// that the feed is fine.
void setFeedNote(const char *note) {
    if (!s_loading || !s_loadingPending) return;
    lv_label_set_text(s_loading, note ? note : "Loading aircraft\nand location data");
}

// Forced on for as long as the active theme's radar.simulate is true, regardless of what
// else the theme asks for. There is no theme-side field that can hide this: the whole
// defect it fixes is a theme-tool control whose own hint framed it as a preview convenience
// when it is not, so nothing short of "the device itself refuses to stay quiet about it"
// closes the gap. Called from main.cpp wherever the theme's settings are applied, so it
// tracks the SAME flag that decides whether main.cpp fabricates aircraft, not a copy of it.
void setSimulatedBadge(bool on) {
    if (s_simBadge) show(s_simBadge, on);
}

void setRangeLabelVisible(bool v) { s_rangeLblVisible = v; if (s_rangeLbl) show(s_rangeLbl, v && !orb() && !customStyled()); }

void setSweepEnabled(bool on) {
    s_sweepEnabled = on;
    const bool sweepImgActive = customStyled() && theme_style::radar().sweepTypeImage;
    if (s_sweep) {
        show(s_sweep, on && !sweepImgActive);
        if (!on) lv_obj_invalidate(s_sweep);   // clear any wedge currently painted
    }
    if (s_sweepImg) show(s_sweepImg, on && sweepImgActive);
}
bool sweepEnabled() { return s_sweepEnabled; }

// Defined with the flatten pass below; setAirportsEnabled sits above it in the file.

void setAirportsEnabled(bool on) {
    s_airportsEnabled = on;
    if (s_gridLayer) lv_obj_invalidate(s_gridLayer);   // repaint the chrome with/without markers
    if (customStyled()) {          // the etched copy includes the markers; re-etch without them
        take_map_snapshot();
        rebuild_flat_background();
        apply_grid_visibility();
    }
}
bool airportsEnabled() { return s_airportsEnabled; }

// 0 = off, 1 = short, 2 = medium (default), 3 = long. Controls both the per-aircraft
// trail and the persistent flow layer (the long-lived "where everything has been" tracks).
void setTrailLength(int level) {
    switch (level) {
        case 0: s_trailMax = 0;  s_flowMax = 0;    s_flowGenMax = 0;  break;
        // Segment counts halved from (150/1500/700). A repaint costs roughly 300 us per
        // segment on this hardware — that is LVGL canvas draw-call overhead, not line
        // length — so 700 segments is a 210 ms repaint and 240 is a 70 ms one. Even
        // batched, a repaint should fit inside about one frame rather than three.
        case 1: s_trailMax = 3;  s_flowMax = 80;   s_flowGenMax = 8;  break;   // ~16 s
        case 3: s_trailMax = 12; s_flowMax = 700;  s_flowGenMax = 30; break;   // ~60 s
        default: s_trailMax = 7; s_flowMax = 240;  s_flowGenMax = 14; break;   // ~28 s
    }
    if (s_flowMax == 0) { s_flow.clear(); s_trails.clear(); }
    else while ((int)s_flow.size() > s_flowMax) s_flow.pop_front();
    flow_redraw_all();                              // repaint the flow canvas at the new length
    if (s_acLayer) lv_obj_invalidate(s_acLayer);
}

void setMaxOnScreen(int n) {
    s_maxOnScreen = (n < 1) ? 1 : (n > ADSB_MAX_AIRCRAFT ? ADSB_MAX_AIRCRAFT : n);  // never more than the feed pulls
    if (s_acLayer) lv_obj_invalidate(s_acLayer);
}

void setLargeText(bool on) {
    s_bigText = on;
    if (s_acLayer) lv_obj_invalidate(s_acLayer);
}


// Section ledger for radar::init(), measured taking 3.7 MB — the single largest consumer
// on the device, allocated at boot whether or not Flight Tracker is ever opened.
static void rmark(const char *what) {
#ifdef ARDUINO
    static uint32_t prev = 0;
    const uint32_t now = (uint32_t)ESP.getFreePsram();
    Serial.printf("[psram/radar] %-24s free %6u KB", what, (unsigned)(now / 1024));
    if (prev && prev >= now) Serial.printf("   (-%u KB)", (unsigned)((prev - now) / 1024));
    prev = now;
    Serial.println();
#else
    (void)what;
#endif
}

void init(void *lv_parent) {
    lv_obj_t *parent = (lv_obj_t *)lv_parent;
    s_parent = parent;
    s_cx = SCREEN_CX;
    s_cy = SCREEN_CY;
    s_acs.clear();
    s_trails.clear();
    s_flow.clear();
    s_selHex.clear();
    s_flowRedrawCtr = 0;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Baked plate (background+rings+crosshair): the bottom-most layer, created
    // first so everything else (coastline, sweep, aircraft) draws over it.
    rmark("radar::init start");
    s_plateImg = lv_img_create(parent);
    rmark("after plate img");
    lv_obj_clear_flag(s_plateImg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_plateImg);
    lv_obj_add_flag(s_plateImg, LV_OBJ_FLAG_HIDDEN);

    // Canvas OBJECT only; its 636 KB buffer arrives via canvas_acquire() the first time
    // a trail segment actually needs drawing (see flow_redraw_all/update), and leaves
    // when trails are cleared. Hidden while unbuffered so composition skips it.
    rmark("after flow buffer");
    s_flowCanvas = lv_canvas_create(parent);
    lv_obj_clear_flag(s_flowCanvas, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_flowCanvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(s_flowCanvas);

    s_gridLayer = make_layer(parent, grid_draw_cb);

    // Rings and crosshair, created straight after the map so LVGL's own creation order
    // puts the etching over the roads. Everything movable is lifted above this by
    // applyRadarLayerOrder(), so it can never end up over an aircraft.
    s_ringsImg = lv_img_create(parent);
    lv_obj_clear_flag(s_ringsImg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_ringsImg);
    lv_obj_add_flag(s_ringsImg, LV_OBJ_FLAG_HIDDEN);

    s_sweep     = make_layer(parent, sweep_draw_cb);
    s_acLayer   = make_layer(parent, ac_draw_cb);

    // The sweep's "image" type: a real lv_img, rotated live (see
    // sweep_timer_cb), shown instead of s_sweep's vector wedge when active.
    rmark("after flow canvas");
    s_sweepImg = lv_img_create(parent);
    rmark("after sweep img");
    // Antialias the sweep's rotation. This was previously off, on the reasoning that the
    // blade's edges "are a soft glow to begin with" so filtering bought nothing. That was
    // true of the sweep it was written for and is false of a themed one: Steam Punk's is
    // hard-edged brass with gear teeth and a thin shaft, and nearest-neighbour rotation
    // makes that fine detail crawl and snap from frame to frame. Read as jitter on device.
    //
    // The cost argument does not survive measurement either. The sprite is 73x227, about
    // 16k pixels; the ~880 ms/s this screen spends in LVGL goes on recompositing the
    // near-full-screen area the rotation dirties, not on the transform itself. Filtering
    // it is close to free at this size.
    // Smooth filtering, still on, and deliberately left that way until somebody measures it.
    //
    // Turning it off looked like an obvious win by analogy with the wind crank, where the
    // same call was the difference between a handle that kept up and one that lagged. It was
    // tried here and the A/B said nothing: the owner's Modern design draws a VECTOR sweep, so this
    // object is not even on screen, and the apparent improvement in the first run was the
    // scope settling rather than the change. ?orb sweepaa flips it live on a design that does
    // use an image sweep, which is where the question can actually be answered.
    lv_img_set_antialias(s_sweepImg, true);
    lv_obj_clear_flag(s_sweepImg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_sweepImg, LV_OBJ_FLAG_HIDDEN);

    s_rose[0] = make_label(parent, "N", &lv_font_montserrat_28, COL_INK,  LV_ALIGN_TOP_MID,    0, 12);
    s_rose[1] = make_label(parent, "S", &lv_font_montserrat_16, COL_SOFT, LV_ALIGN_BOTTOM_MID, 0, -12);
    s_rose[2] = make_label(parent, "E", &lv_font_montserrat_16, COL_SOFT, LV_ALIGN_RIGHT_MID, -12, 0);
    s_rose[3] = make_label(parent, "W", &lv_font_montserrat_16, COL_SOFT, LV_ALIGN_LEFT_MID,   12, 0);

    char rng[16];
    snprintf(rng, sizeof(rng), "%.0f km", (double)RANGE_KM_DEFAULT);
    s_rangeLbl = make_label(parent, rng, &lv_font_montserrat_14, COL_GREEN, LV_ALIGN_CENTER, 92, -8);
    lv_obj_set_style_text_opa(s_rangeLbl, 128, 0);

    // theme-name banner: flashed briefly on a theme change or a screen tap (see
    // show_theme_label), sits below the HUD status row (y ~50-70) so it never overlaps
    // it. A solid black plaque behind white text keeps it readable over any theme/scene.
    // Deliberately plain and unthemeable, same reasoning as the update overlay: it is a
    // system message about the device's state, and a theme that styled it into
    // invisibility would defeat the one job it has.
    s_loading = make_label(parent, "Loading aircraft\nand location data", &lv_font_montserrat_20,
                           lv_color_white(), LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_loading, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_loading, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_loading, 12, 0);
    lv_obj_set_style_pad_all(s_loading, 18, 0);
    lv_obj_set_style_text_align(s_loading, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_loading, 6, 0);

    // A live number is the whole fix: frozen text and text that is still true both LOOK
    // identical after the first render, and "is it stuck" was a real question asked about
    // this exact screen. A number that visibly counts up answers it without narrating
    // stages the poll loop does not actually have (it is one repeating step: ask, wait,
    // maybe get an answer, not a multi-part pipeline worth pretending to show).
    s_loadTicker = make_label(parent, "", &lv_font_montserrat_14,
                              lv_color_hex(0xAAB2C0), LV_ALIGN_CENTER, 0, 58);
    show(s_loadTicker, false);
    show(s_loading, false);

    // A small, honest banner near the bottom of the dial for when the aircraft feed is not
    // answering. Deliberately NOT the big centred "Loading" box: by the time this shows,
    // there is usually a scope full of last-known traffic worth still seeing, and covering
    // it would be its own kind of lie. Amber rather than red because nothing is broken.
    s_feedWarn = make_label(parent, "", &lv_font_montserrat_14,
                            lv_color_hex(0xFFB23C), LV_ALIGN_BOTTOM_MID, 0, -46);
    lv_obj_set_style_bg_color(s_feedWarn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_feedWarn, LV_OPA_70, 0);
    lv_obj_set_style_radius(s_feedWarn, 8, 0);
    lv_obj_set_style_pad_all(s_feedWarn, 8, 0);
    lv_obj_set_style_text_align(s_feedWarn, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_feedWarn, 3, 0);
    show(s_feedWarn, false);

    s_themeLabel = make_label(parent, "", &lv_font_montserrat_20, lv_color_white(), LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_bg_color(s_themeLabel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_themeLabel, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_themeLabel, 8, 0);
    lv_obj_set_style_pad_hor(s_themeLabel, 14, 0);
    lv_obj_set_style_pad_ver(s_themeLabel, 4, 0);
    show(s_themeLabel, false);

    s_pulse = lv_obj_create(parent);
    lv_obj_remove_style_all(s_pulse);
    lv_obj_set_size(s_pulse, 12, 12);
    lv_obj_center(s_pulse);
    lv_obj_set_style_radius(s_pulse, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_pulse, COL_INK, 0);
    lv_obj_set_style_border_width(s_pulse, 2, 0);
    lv_obj_clear_flag(s_pulse, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_pulse);
    lv_anim_set_exec_cb(&a, pulse_anim_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 2600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    s_centerDot = lv_obj_create(parent);
    lv_obj_remove_style_all(s_centerDot);
    lv_obj_set_size(s_centerDot, 7, 7);
    lv_obj_center(s_centerDot);
    lv_obj_set_style_radius(s_centerDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_centerDot, COL_INK, 0);
    lv_obj_set_style_bg_opa(s_centerDot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_centerDot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Selection text banners (callsign/stats/route), a theme push only — a
    // dedicated transparent canvas (not LVGL labels), so a banner can curve along
    // an arc and glow, redrawn by refresh_custom_text() whenever the custom
    // design is active and something is selected. Created after the aircraft
    // layer so banners sit above the scope, rings, and blips.
    // Created BEFORE the text canvas so they sit under it in LVGL's own z-order, which is
    // creation order among siblings — the card is a backdrop for the words, never over them.
    s_cardImg = lv_img_create(parent);
    lv_obj_clear_flag(s_cardImg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_cardImg, LV_OBJ_FLAG_HIDDEN);
    s_cardObj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_cardObj);
    lv_obj_clear_flag(s_cardObj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_cardObj, LV_OBJ_FLAG_HIDDEN);

#if CUSTOM_HAS_RADAR
    {
        // Canvas OBJECT only; the 636 KB buffer is acquired by refresh_custom_text()
        // while banners are actually visible (a selection exists, or an always-on
        // RTEXT4 range banner is compiled in) and released when they are not.
        rmark("after text buffer");
        s_textCanvas = lv_canvas_create(parent);
        lv_obj_clear_flag(s_textCanvas, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_textCanvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(s_textCanvas);
    }
#endif

    // Two plain decorative overlays — no rotation, just position+opacity —
    // insertable anywhere in the layer order via applyRadarLayerOrder().
    for (int i = 0; i < 2; ++i) {
        s_staticImg[i] = lv_img_create(parent);
        lv_obj_clear_flag(s_staticImg[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_staticImg[i], LV_OBJ_FLAG_HIDDEN);
    }

    // The "Overlay" card: a plain full-scope color wash, no image — just a
    // flat fill+opacity rect sized to the whole screen (the round display's
    // own hardware/LVGL clipping crops it to the circle, same as everything
    // else here, so no separate mask is needed). Insertable anywhere in the
    // layer order like the two statics above.
    s_dimLayer = lv_obj_create(parent);
    lv_obj_remove_style_all(s_dimLayer);
    lv_obj_set_size(s_dimLayer, SCREEN_W, SCREEN_H);
    lv_obj_center(s_dimLayer);
    lv_obj_clear_flag(s_dimLayer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_dimLayer, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_dimLayer, LV_OBJ_FLAG_HIDDEN);

    // Baked overlay (CRT+glass): the top-most layer, created last so it sits
    // over the sweep/aircraft/selection-banner layers too, matching the
    // editor's own draw order (CRT/glass are painted after everything else).
    rmark("after statics");
    s_overlayImg = lv_img_create(parent);
    rmark("after overlay img");
    lv_obj_clear_flag(s_overlayImg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_overlayImg);
    lv_obj_add_flag(s_overlayImg, LV_OBJ_FLAG_HIDDEN);

    // "This traffic is made up." Above the glass overlay, above everything: the one label
    // on this screen no theme JSON can hide, resize, recolor or move, because the whole
    // point is that it survives an author who forgot they turned Test traffic on, and
    // an owner who never knew. See setSimulatedBadge(), driven by theme_style::radar().simulate.
    s_simBadge = make_label(parent, "TEST DATA", &lv_font_montserrat_14,
                            lv_color_white(), LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(s_simBadge, lv_color_hex(0xC62E2E), 0);
    lv_obj_set_style_bg_opa(s_simBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_simBadge, 4, 0);
    lv_obj_set_style_pad_hor(s_simBadge, 7, 0);
    lv_obj_set_style_pad_ver(s_simBadge, 3, 0);
    lv_obj_add_flag(s_simBadge, LV_OBJ_FLAG_HIDDEN);

    s_sweepDeg = 0.0f;
    s_prevSweepDeg = 0.0f;
    if (!s_timer) s_timer = lv_timer_create(sweep_timer_cb, SWEEP_FRAME_MS, nullptr);

    rmark("before refreshCustomStyle");
    // NOT decoding the artwork here any more. This call pulled the plate, sweep, blip
    // and both static layers off the SD card at boot — measured at 2431 KB — for a
    // screen the user may never open. Flight Tracker already has the right lifecycle:
    // knobEnter() calls refreshCustomStyle() when the app is actually shown, and
    // knobExit() calls radar_sprite_release() when it is left. init() was simply doing
    // it eagerly as well, so the memory was claimed from boot and never handed back.
    //
    // Anything that boots straight into Flight Tracker (CUSTOM_BOOT_TARGET == 2) still
    // gets its artwork, because that path goes through the app shell's onEnter.
    rmark("after refreshCustomStyle");
    setTheme(s_theme);
    rmark("after setTheme (init done)");
}

// (Re-)attach the plate/overlay image sources, decoding lazily if needed.
// Call once at init(), and again from Flight Tracker's onEnter after an
// onExit released the decoded PSRAM (radar_sprite_release()) — an image
// object's src has to be re-set after that, the same way the clock's
// draw_custom() re-fetches custom_plate()/custom_overlay() every redraw.
void refreshCustomStyle() {
    s_pacingStale = true;
    if (s_plateImg) {
        const lv_img_dsc_t *plate = radar_custom_plate();
        if (plate) { lv_img_set_src(s_plateImg, plate); show(s_plateImg, true); }
        else show(s_plateImg, false);
    }
    if (s_ringsImg) {
        const lv_img_dsc_t *rg = radar_custom_rings();
        if (rg) { lv_img_set_src(s_ringsImg, rg); show(s_ringsImg, true); }
        else show(s_ringsImg, false);
    }
    if (s_overlayImg) {
        const lv_img_dsc_t *ov = radar_custom_overlay();
        if (ov) { lv_img_set_src(s_overlayImg, ov); show(s_overlayImg, true); }
        else show(s_overlayImg, false);
    }
    // Two plain decorative overlays: position/opacity come from theme_style
    // (per-theme, see radar_style.json), the pixels from radar_sprite.cpp —
    // same SD-first-then-flash decode as the plate/overlay, just centered at
    // (x,y) instead of always filling the whole screen.
    const theme_style::RadarStatic *rs[2] = { &theme_style::radar().static1, &theme_style::radar().static2 };
    for (int i = 0; i < 2; ++i) {
        if (!s_staticImg[i]) continue;
        const lv_img_dsc_t *img = rs[i]->show ? radar_custom_static(i) : nullptr;
        if (img) {
            lv_img_set_src(s_staticImg[i], img);
            // Zoom (256 = 100%) scales around the image's own pivot, which
            // defaults to its center — so positioning by unscaled w/h below
            // still lands the visual center at (x,y) at any scale.
            lv_img_set_zoom(s_staticImg[i], (uint16_t)lroundf(rs[i]->scale * 256.0f));
            lv_obj_set_pos(s_staticImg[i], (lv_coord_t)(rs[i]->x - (int)img->header.w / 2), (lv_coord_t)(rs[i]->y - (int)img->header.h / 2));
            lv_obj_set_style_img_opa(s_staticImg[i], (lv_opa_t)rs[i]->opacity, 0);
            show(s_staticImg[i], true);
        } else {
            show(s_staticImg[i], false);
        }
    }
    // The sweep's "image" type: pivot/center are compile-time (coupled to
    // whichever sprite is actually baked in, same reasoning as the blip
    // icon's pivot) — angle is live, driven by sweep_timer_cb via s_sweepDeg.
    if (s_sweepImg) {
        const bool useImage = customStyled() && theme_style::radar().sweepTypeImage;
        const lv_img_dsc_t *sweepSrc = useImage ? radar_custom_sweep() : nullptr;
        if (sweepSrc) {
            lv_img_set_src(s_sweepImg, sweepSrc);
            // Theme data first, welded macros as the fallback (see theme_style::Radar).
            const theme_style::Radar &rsw = theme_style::radar();
            const int spx = rsw.sweepPivotX  >= 0 ? rsw.sweepPivotX  : CUSTOM_SWEEP_IMAGE_PIVOT_X;
            const int spy = rsw.sweepPivotY  >= 0 ? rsw.sweepPivotY  : CUSTOM_SWEEP_IMAGE_PIVOT_Y;
            const int scx = rsw.sweepCenterX >= 0 ? rsw.sweepCenterX : CUSTOM_SWEEP_IMAGE_CENTER_X;
            const int scy = rsw.sweepCenterY >= 0 ? rsw.sweepCenterY : CUSTOM_SWEEP_IMAGE_CENTER_Y;
            lv_img_set_pivot(s_sweepImg, spx, spy);
            lv_obj_set_pos(s_sweepImg, scx - spx, scy - spy);
            lv_img_set_angle(s_sweepImg, (int16_t)lroundf(s_sweepDeg * 10.0f));
            show(s_sweepImg, true);
#if !defined(ARDUINO)
            // SIM_SWEEP_DEG=90 pins the hand at a known angle for a capture. LVGL skips the
            // transform entirely at angle 0, so an unpinned screenshot is taken at the one
            // angle where a wrong pivot cannot show — which is how a rotation about the
            // wrong point survives every screenshot anyone thinks to take.
            if (const char *forced = getenv("SIM_SWEEP_DEG")) {
                s_sweepDeg = (float)atof(forced);
                lv_img_set_angle(s_sweepImg, (int16_t)lroundf(s_sweepDeg * 10.0f));
            }
#endif
            // Report what LVGL actually ended up holding, not what we asked it for. A sweep
            // sprite whose pivot is its own centre hides every possible mistake here, because
            // that is also LVGL's default after lv_img_set_src — so a pivot that never landed
            // looks perfect until the day a sprite is trimmed and its pivot moves off centre.
            {
                lv_obj_update_layout(s_sweepImg);   // position is deferred; reading it first lies
                lv_point_t got; lv_img_get_pivot(s_sweepImg, &got);
                Serial.printf("[sweepimg] src %dx%d  asked pivot %d,%d  lvgl holds %d,%d  obj pos %d,%d size %dx%d  turns about %d,%d (want %d,%d)\n",
                              (int)sweepSrc->header.w, (int)sweepSrc->header.h, spx, spy,
                              (int)got.x, (int)got.y,
                              (int)lv_obj_get_x(s_sweepImg), (int)lv_obj_get_y(s_sweepImg),
                              (int)lv_obj_get_width(s_sweepImg), (int)lv_obj_get_height(s_sweepImg),
                              (int)(lv_obj_get_x(s_sweepImg) + got.x), (int)(lv_obj_get_y(s_sweepImg) + got.y),
                              scx, scy);
            }
        } else {
            show(s_sweepImg, false);
        }
    }
    // The "Overlay" card: plain color+opacity, no image — see s_dimLayer above.
    if (s_dimLayer) {
        const theme_style::Radar &rs2 = theme_style::radar();
        if (rs2.overlayEnabled) {
            lv_obj_set_style_bg_color(s_dimLayer, lv_color_hex(rs2.overlayColor), 0);
            lv_obj_set_style_bg_opa(s_dimLayer, (lv_opa_t)rs2.overlayOpacity, 0);
            show(s_dimLayer, true);
        } else {
            show(s_dimLayer, false);
        }
    }
    // Merge the unchanging lower layers now that every one of them has been given
    // its current source, position, scale and opacity. Runs last on purpose: it
    // reads the finished state rather than trying to predict it, and it hides only
    // what it has actually absorbed, so a failure here degrades to the live stack
    // rather than to a missing layer.
    rebuild_flat_background();
    apply_grid_visibility();
    applyRadarLayerOrder();
}

void update(const std::vector<Aircraft> &aircraft, const RadarSettings &s) {
    std::vector<AcDraw> out;
    out.reserve(aircraft.size());
    std::set<std::string> present;
    const float R = (float)RADAR_R_OUTER_PX;
    ++s_flowGen;                                  // one tick per poll; flow segments age in these units
    s_lastRangeKm = s.rangeKm;                    // kept current for the range banner (radar_range_fmt)

    // Reproject the coastline only when the scope geometry actually changes (home
    // moved or range zoomed) — never per frame. Then repaint the static chrome layer.
    static double s_coLat = 1e9, s_coLon = 1e9; static float s_coRange = -1.0f;
    if (s.homeLat != s_coLat || s.homeLon != s_coLon || s.rangeKm != s_coRange) {
        const bool firstFix = (s_coRange < 0.0f);
        s_coLat = s.homeLat; s_coLon = s.homeLon; s_coRange = s.rangeKm;
        coastline_project(s.homeLat, s.homeLon, s.rangeKm, s_cx, s_cy, R);
        airports_project(s.homeLat, s.homeLon, s.rangeKm, s_cx, s_cy, R);
        roads_sd::project(s.homeLat, s.homeLon, s.rangeKm, s_cx, s_cy, R);
        if (s_gridLayer) lv_obj_invalidate(s_gridLayer);
        // The projection is new, so the etched copy is stale: render it once at the new
        // geometry and fold it back into the flattened background. Costs one frame's
        // worth of work on an event a person triggers rarely (move home, zoom range),
        // and buys back the per-frame re-vectoring the rest of the time.
        if (customStyled()) {
            take_map_snapshot();
            rebuild_flat_background();
            apply_grid_visibility();
        }
        if (!firstFix) {
            // Scope scale/center changed: old trails were plotted at the previous
            // projection and would be wrong now — drop them and clear the flow layer.
            s_trails.clear();
            s_flow.clear();
            flow_redraw_all();
        }
    }

    std::map<std::string, lv_point_t> prevPos;        // smooth-motion: glide starts here
    for (const AcDraw &a : s_acs) prevPos[a.hex] = a.pos;

    for (const Aircraft &ac : aircraft) {
        const double distKm = geo::haversineKm(s.homeLat, s.homeLon, ac.lat, ac.lon);
        const double brg = geo::bearingDeg(s.homeLat, s.homeLon, ac.lat, ac.lon);
        const geo::Point p = geo::projectToScreen(distKm, brg, s.rangeKm, s_cx, s_cy, R, s.rotationDeg);

        AcDraw d;
        lv_point_t target;
        target.x = (lv_coord_t)lroundf(p.x);
        target.y = (lv_coord_t)lroundf(p.y);
        d.to = target;
        {
            auto pit = prevPos.find(std::string(ac.hex.c_str()));
            if (pit != prevPos.end()) {
                const long dx = (long)target.x - pit->second.x;
                const long dy = (long)target.y - pit->second.y;
                d.from = (dx * dx + dy * dy > 120L * 120L) ? target : pit->second;  // snap if it jumped
            } else d.from = target;                                                  // new contact: appear in place
        }
#if MOTION_INTERP
        // Custom designs GLIDE too, as of 2.9.5, and the comment that used to sit here was
        // wrong. It argued that a custom design's PSRAM-backed alpha-composited layers make
        // each interpolation step's invalidation far more expensive, so aircraft should snap
        // to each polled position instead of gliding through it.
        //
        // Measured on the owner's Steam Punk face, A/B over 40 seconds each, with a counter
        // watching what the glyphs actually did:
        //
        //   snapping:  49 steps per 5 s,  0 glyph moves,  0 px    9 fps
        //   gliding:   49 steps per 5 s, 29 glyph moves, 30 px    9 fps
        //
        // Identical frame rate, no stalls either way, and the sweep's own spread figure
        // showed no difference outside its noise. The claim assumed roughly 22 expensive
        // recomposites between polls. What actually happens is about 29 moves of ONE PIXEL
        // spread over five seconds, because an aircraft only crosses about 30 px of a 30 km
        // scope in a whole poll. Tiny invalidation boxes, and the cost went with them.
        //
        // The line that follows this comment is why nobody had ever measured it: it disabled
        // the code path the claim was about, so there was nothing running to be expensive.
        // Overridable over the cable (?orb glide 1) so the claim above can be TESTED on a
        // real custom theme rather than trusted. It is a performance claim about a code path
        // that this same branch then disables, which means nothing has ever measured it on
        // the hardware it describes. Worse, an attempt to measure it on 2026-09-04 by
        // sweeping ?orb interpms found "no cost at all" on a Steam Punk face, which was true
        // and meaningless: with from and to equal there was no glide to cost anything. The
        // instrument was reading a code path the theme never enters.
        const bool snap = (s_forceGlide < 0) ? false : (s_forceGlide == 0);
        if (snap) { d.pos = target; d.from = target; }
        else       d.pos = d.from;   // begin the glide at the previous position
#else
        d.pos = target;
        d.from = target;
#endif
        d.inRange = p.inRange;
        d.track = ac.track;
        d.color = alt_color(ac.altBaro, ac.onGround);
        d.emergency = acIsEmergency(ac.squawk);
        snprintf(d.hex,  sizeof(d.hex),  "%s", ac.hex.c_str());
        snprintf(d.call, sizeof(d.call), "%s", ac.flight.c_str());
        snprintf(d.type, sizeof(d.type), "%s", ac.type.c_str());
        d.altFt = ac.altBaro;
        d.onGround = ac.onGround;
        d.vsFpm = ac.baroRate;
        d.gsKt = ac.gs;
        d.distKm = (float)distKm;
        d.bearingDeg = (float)brg;
        d.squawk = ac.squawk;
        // millis() and lv_tick_get() are the same clock on-device (lv_conf.h wires LVGL's
        // tick straight to millis()), so this needs no unit conversion. aging::age() reads a
        // contact newer than "now" (the poll's stamp is momentarily ahead of this render) as
        // age 0, so it is freshness 1.0, not a negative age wrapping into "ancient."
        d.lastUpdateMs = ac.lastUpdateMs;
        d.freshness = ac_freshness(aging::age(lv_tick_get(), ac.lastUpdateMs));
        if (ac.onGround) snprintf(d.altTxt, sizeof(d.altTxt), "GND");
        else             snprintf(d.altTxt, sizeof(d.altTxt), "%.0f ft", (double)ac.altBaro);

        const std::string key = ac.hex.c_str();
        present.insert(key);
        if (d.inRange) {
            std::vector<lv_point_t> &hist = s_trails[key];
            const bool moved = hist.empty() ||
                               abs((int)hist.back().x - (int)target.x) > 0 ||
                               abs((int)hist.back().y - (int)target.y) > 0;
            if (moved) {
                // Flow segments persist on their canvas once painted, so a segment laid
                // down inside a zone would sit on the artwork for its entire lifetime.
                // Tested here, at creation, rather than at draw time.
                if (s_flowMax > 0 && !hist.empty() &&
                    !in_excluded_zone(hist.back().x, hist.back().y) &&
                    !in_excluded_zone(target.x, target.y)) {
                    FlowSeg seg = { hist.back(), target, s_flowGen };
                    s_flow.push_back(seg);
                    while ((int)s_flow.size() > s_flowMax) s_flow.pop_front();
                    flow_draw_seg(seg);
                }
                if (s_trailMax > 0) {
                    hist.push_back(target);
                    while ((int)hist.size() > s_trailMax) hist.erase(hist.begin());
                } else {
                    hist.clear();
                }
            }
            // d.trail is filled in below, after the cap: copying it here paid a vector allocation
            // for every in-range contact each poll, including the ones the cap then drops.
        }
        out.push_back(std::move(d));
    }

    for (auto it = s_trails.begin(); it != s_trails.end();) {
        if (present.find(it->first) == present.end()) it = s_trails.erase(it);
        else ++it;
    }
    if (!s_selHex.empty() && present.find(s_selHex) == present.end()) s_selHex.clear();

    // Fade the flow layer by AGE, not just count: drop segments older than s_flowGenMax
    // polls so old tracks self-clear even in busy airspace (a 5 nm view doesn't stay caked
    // in green). If any were dropped, repaint the flow canvas so they actually disappear.
    if (s_flowGenMax > 0 && !s_flow.empty()) {
        // Expire old segments, but do NOT repaint on every prune.
        //
        // The flow canvas is additive, so removing a segment means clearing and redrawing
        // every remaining one. That made one or two segments ageing out cost a full
        // repaint of up to 700, measured at 210-330 ms ON THE RENDER THREAD — three-plus
        // dropped frames, every poll, which is precisely the periodic stutter in the
        // sweep. The work was wildly disproportionate to the change: repaint everything
        // to remove two.
        //
        // So expiry is batched. Segments linger a little past their age limit until
        // enough have accumulated to be worth one repaint. A trail tail fading a beat
        // late is invisible; the sweep hitching is not, and the owner's stated priority is
        // explicit that even motion wins.
        size_t expired = 0;
        while (expired < s_flow.size() &&
               (uint16_t)(s_flowGen - s_flow[expired].gen) > (uint16_t)s_flowGenMax) {
            ++expired;
        }
        const size_t batch = s_flow.size() / 6 > 12 ? s_flow.size() / 6 : 12;
        // Repaint when a worthwhile batch has expired, or when everything has (the tail
        // of a fade-out, where waiting for a batch that will never arrive would strand
        // the last few segments on screen).
        if (expired >= batch || (expired > 0 && expired == s_flow.size())) {
            s_flow.erase(s_flow.begin(), s_flow.begin() + expired);
            flow_redraw_all();
        }
    }

    // Which aircraft the scope follows, and it is deliberately STICKY: see track_select.h for
    // the rules and tests/track_select_test.cpp for the proof they did not change when they
    // moved there. The instrument follows aircraft instead of re-deciding what is interesting
    // twice a second.
    std::sort(out.begin(), out.end(),
              [](const AcDraw &a, const AcDraw &b) { return a.distKm < b.distKm; });
    // Counted BEFORE the cap below trims `out`, which is the whole point: the interesting
    // number is how many candidates existed, not how many survived.
    int dbgInRange = 0, dbgFlying = 0;
    for (const AcDraw &a : out) if (a.inRange) { ++dbgInRange; if (!a.onGround) ++dbgFlying; }
    if ((int)out.size() > s_maxOnScreen) {
        std::vector<track_select::Cand> cands;
        cands.reserve(out.size());
        for (const AcDraw &a : out) {
            track_select::Cand c;
            c.trackable = a.inRange && !a.onGround;         // ground traffic is never worth a slot
            c.current   = a.freshness >= 1.0f;              // confirmed within AC_DIM_START_MS
            c.incumbent = s_tracked.find(std::string(a.hex)) != s_tracked.end();
            cands.push_back(c);
        }
        const std::vector<int> pick = track_select::pick(cands, s_maxOnScreen);
        // Only if nothing qualified: better to show distant or grounded contacts than an
        // empty scope, which would look broken rather than quiet.
        if (pick.empty()) {
            out.resize(s_maxOnScreen);
        } else {
            std::vector<AcDraw> kept;
            kept.reserve(pick.size());
            for (int i : pick) kept.push_back(std::move(out[i]));   // moved, not copied
            out.swap(kept);
        }
    }
    s_tracked.clear();
    for (const AcDraw &a : out) s_tracked.insert(std::string(a.hex));

    // Trails for what is actually drawn, and only that. s_trails still records every in-range
    // contact so a contact that later earns a slot arrives with its history intact.
    for (AcDraw &a : out) {
        if (!a.inRange) continue;
        auto it = s_trails.find(std::string(a.hex));
        if (it != s_trails.end()) a.trail = it->second;
    }

    // Why the dial shows what it shows. Added 2026-08-22: the theme asked for 14 aircraft
    // and five appeared, and every explanation for that gap was a guess. These are the four
    // numbers that actually decide it — what the feed sent, how many fell inside the ring,
    // how many of those were flying, and how many survived the cap — so the answer is read
    // rather than reasoned about. Throttled to one line every ~10 s.
    {
        // lv_tick_get(), not millis(): this file also builds for the desktop simulator,
        // where millis() does not exist. LVGL's tick is available in both.
        static uint32_t s_acDbgAt = 0;
        if (lv_tick_get() - s_acDbgAt > 10000) {
            s_acDbgAt = lv_tick_get();
            // "table" is aircraft.size(): the persistent count, fresh entries plus anything
            // still aging out from an earlier poll. "dimming" is how many of THOSE are below
            // full freshness right now — the direct, printable proof that a contact is
            // being retained and faded rather than dropped the instant one poll misses it.
            int dimming = 0;
            for (const AcDraw &a : out) if (a.freshness < 0.999f) ++dimming;
            Serial.printf("[acdbg] table=%u inRange=%d flying=%d drawn=%u dimming=%d cap=%d rangeKm=%.0f\n",
                          (unsigned)aircraft.size(), dbgInRange, dbgFlying,
                          (unsigned)out.size(), dimming, s_maxOnScreen, (double)s.rangeKm);
        }
    }

    if (++s_flowRedrawCtr >= FLOW_REDRAW_EVERY) {
        s_flowRedrawCtr = 0;
        flow_redraw_all();
    }

    if (s_rangeLbl) {                                 // keep the range label in sync with settings
        char r[16];
        snprintf(r, sizeof(r), "%.0f km", (double)s.rangeKm);
        lv_label_set_text(s_rangeLbl, r);
    }

    const uint32_t now = lv_tick_get();              // measure actual cadence for the glide clock
    s_pollMs = (s_lastUpdateMs && now > s_lastUpdateMs) ? (now - s_lastUpdateMs) : (uint32_t)POLL_INTERVAL_MS;
    // The CEILING WAS BELOW THE POLL INTERVAL, so the normal case always hit it.
    //
    // Polls land about every 10.4 s against a nominal POLL_INTERVAL_MS of 10000, and the
    // glide clock was clipped to 8000. So every aircraft finished its whole journey two and
    // a half seconds before the next position arrived and then sat perfectly still waiting
    // for it. Combined with the ease-out below, which had them 94% of the way there by
    // t=0.75, the visible result was a lurch followed by four or five seconds of nothing:
    // "they jump every about 10 or 11 seconds", which is the poll interval exactly.
    //
    // A ceiling is still right, because a missed poll should not turn into a half-minute
    // crawl. It just has to sit ABOVE the interval it is bounding rather than below it. This
    // is the same fault as the sweep's stall clamp feeding its own statistics: a guard set
    // to protect something quietly became the thing damaging it.
    if (s_pollMs < 400) s_pollMs = 400;
    const uint32_t glideMax = (uint32_t)POLL_INTERVAL_MS + (uint32_t)POLL_INTERVAL_MS / 2;
    if (s_pollMs > glideMax) s_pollMs = glideMax;
    s_lastUpdateMs = now;
    s_animStartMs  = now;

    s_acs = std::move(out);
    // The scope has real content now: the projection and etch above are done and this
    // snapshot is live. Dismissing here rather than on a timer means the notice lasts
    // exactly as long as the wait actually lasts.
    if (s_loadingPending) {
        s_loadingPending = false;
        if (s_loading)    show(s_loading, false);
        if (s_loadTicker) show(s_loadTicker, false);
        // Mirrors refreshCustomStyle's condition for the image sweep: it is visible only
        // when a custom design asks for the image type AND actually ships the sprite.
        if (s_sweepImg && customStyled() && theme_style::radar().sweepTypeImage && radar_custom_sweep())
            show(s_sweepImg, true);
        if (s_sweep) lv_obj_invalidate(s_sweep);
    }
    if (s_acLayer) lv_obj_invalidate(s_acLayer);
    refresh_custom_text();   // live values (alt/spd/dist/...) for the current selection, if any, just changed
}

void tickSweep() { /* sweep self-animates via lv_timer */ }

// Late detail landed on a card that is still up: give the reader a full window from now,
// rather than whatever was left of the one that started when they turned the knob.
// Say WHICH thing is unwell, because from the desk a blank scope looks identical whether
// the WiFi dropped, the firmware wedged, or somebody else's server is having a bad night —
// and the last of those is by far the likeliest. The device knows which it is, so it should
// say so rather than leave a person guessing at their own hardware.
//
// Only after a real gap: aircraft arrive every ten seconds and a single missed poll is
// normal, so warning at the first hiccup would train people to ignore this.
void setFeedStatus(bool wifiUp, uint32_t staleSec, bool locationKnown) {
    if (!s_feedWarn) return;
    // No location is not a thing that waiting fixes, so it DISMISSES the loading notice
    // instead of queueing behind it.
    //
    // Without this the two states deadlock, and it is the deadlock that matters: the notice
    // is raised by knobEnter() whenever the scope is empty, and cleared only by update(),
    // which runs when a poll delivers a snapshot. The poll is gated on having a location.
    // So an Orb that does not know where it is can never clear the notice, and the early
    // return below meant the one message that explains why was suppressed by it. The screen
    // said "Loading aircraft and location data" with a counter ticking upward, for ever,
    // which is exactly the open-ended wait the charter forbids under S1 — and it was worse
    // than the plain bug, because the label is deliberately theme-proof so nothing could
    // style it away either.
    //
    // Clearing the flag also releases the sweep (sweep_timer_cb returns early while it is
    // set), so the dial turns over an empty scope with the banner on it. That is the right
    // picture: the instrument is alive, the sky is not the problem, and the words say so.
    if (!locationKnown && s_loadingPending) {
        s_loadingPending = false;
        if (s_loading)    show(s_loading, false);
        if (s_loadTicker) show(s_loadTicker, false);
    }
    // While the big "Loading" box is still up it is already saying this, in more words.
    if (s_loadingPending) { show(s_feedWarn, false); return; }
    // STATE ONLY, NEVER A CAUSE.
    //
    // This used to say "WiFi is fine, the service is not answering". Measured 2026-08-23 while
    // that exact sentence was on the dial: the service answered a laptop in 1.3 s with 88
    // aircraft, and the Orb itself was unreachable over WiFi. Both halves were wrong.
    //
    // The bug was claiming a diagnosis from WiFi.status(), which only reports that the radio
    // is ASSOCIATED with an access point. It says nothing about whether the device can
    // actually use the network — and when internal memory is exhausted it cannot, while
    // still reporting WL_CONNECTED. So the banner asserted the one thing it had no way to
    // know, and asserted it confidently.
    //
    // An indicator that names the wrong culprit is worse than none: it sends a person to
    // check their router while the fault is somewhere else entirely, and once it has done
    // that twice nothing it says is believed again. So it now reports only what is directly
    // observable — no fresh aircraft — and leaves the diagnosis to the logs, which can be
    // checked rather than trusted.
    const char *msg = nullptr;
    // Same clock the contacts themselves age on (AC_DIM_START_MS, config.h), not a second
    // number chosen independently. They used to disagree — 45s here, 60s for the first
    // visible dimming — so for 15 real seconds the feed could be exactly stale enough to
    // trip this banner while every aircraft on the dial was still drawn at full brightness,
    // which read as the instrument flatly contradicting itself: "no data" over a screen
    // full of normal-looking traffic. This is the fix the owner found live, on the device,
    // 2026-08-24. One clock, so the banner and the first dimmed pixel can never disagree
    // about whether anything is stale.
    //
    // Location outranks staleness, and is checked first for that reason. With no centre
    // there is nothing to query and therefore never any traffic, so "No aircraft data"
    // would be perfectly true and completely useless — it describes the symptom of a
    // device that does not know where it is, and sends the reader to look at the feed.
    // This still obeys the rule above: whether a location has ever been established is a
    // fact read straight out of NVS, not a diagnosis of anything.
    //
    // THREE STATES, NOT ONE, and that is UX-066: a dead connection, a dead feed and a
    // device that does not know where it is are three different problems with three
    // different things to do about them, and until now the last two both said "No aircraft
    // data". wifiUp has been a parameter of this function the whole time and was never once
    // read — the caller measured it, passed it in, and the message ignored it.
    //
    // Naming adsb.lol does NOT break the rule above, and the distinction is worth being
    // exact about. What was wrong before was asserting a CAUSE that had not been observed:
    // "WiFi is fine, the service is not answering" claimed two things the device had no way
    // to know, and both were false at the time. "adsb.lol is not answering" claims one
    // thing the device did observe directly — it asked that host, repeatedly, and has had
    // nothing back for as long as the contacts have been ageing. It does not say why, and
    // it does not say whose fault it is.
    //
    // UX-039's test is whether the owner can tell a dead internet connection from one dead
    // feed by reading the screen. With one message for both, they could not.
    if (!wifiUp)             msg = "No WiFi\nYour Orb is fine";
    else if (!locationKnown) msg = "Location not set\nSettings " LV_SYMBOL_RIGHT " Location";
    else if (staleSec >= ADSB_NO_DATA_MS / 1000) msg = "No aircraft data\n" ADSB_SOURCE_NAME " is not answering";
    // Log only on change: this is called every status tick, and a line per tick would bury
    // the feed diagnostics underneath it.
    static const char *s_shown = nullptr;
    if (!msg) {
        if (s_shown) { Serial.println("[feedwarn] cleared"); s_shown = nullptr; }
        show(s_feedWarn, false);
        return;
    }
    if (s_shown != msg) {
        s_shown = msg;
        Serial.printf("[feedwarn] showing: %s (stale %lus)\n", msg, (unsigned long)staleSec);
    }
    lv_label_set_text(s_feedWarn, msg);
    show(s_feedWarn, true);
    lv_obj_move_foreground(s_feedWarn);
}

void setSweepFrameMs(uint32_t ms) {
    const uint32_t use = ms ? ms : (uint32_t)SWEEP_FRAME_MS;
    if (s_timer) lv_timer_set_period(s_timer, (uint32_t)use);
    // Reseed the pacing state so the first step after a change is measured from now rather
    // than from a gap that belongs to the old period.
    s_lastSweepMs = 0;
    s_emaDtMs = 0.0f;
    Serial.printf("[sweep] frame period -> %lu ms\n", (unsigned long)use);
}

// How often aircraft glyphs are allowed to move, live, for measuring what a faster glide
// costs. Zero restores AC_INTERP_MS. An instrument, not a setting: nothing persists it, so a
// reboot puts the product decision back.
void setAcInterpMs(uint32_t ms) {
    s_acInterpMs = ms;
    Serial.printf("[radar] glide cadence -> %lu ms%s\n",
                  (unsigned long)(ms ? ms : (uint32_t)AC_INTERP_MS), ms ? "" : " (default)");
}

// Force the glide on or off regardless of the theme, for measuring what it costs on a
// custom design. -1 restores the compiled behaviour.
// Smooth filtering on the rotated sweep image, on or off, live. For proving whether it is
// the cost rather than assuming it: the same trend can appear in two runs for reasons that
// have nothing to do with the change, and a single before-and-after cannot tell them apart.
void setTrailSteps(int n) {
    s_forceTrailSteps = n;
    Serial.printf("[radar] trail lines -> %s%d\n", n > 0 ? "" : "the design's own, currently ", 
                  n > 0 ? n : theme_style::radar().sweepTrailSteps);
}

void setSweepAA(int on) {
    if (s_sweepImg) lv_img_set_antialias(s_sweepImg, on != 0);
    Serial.printf("[radar] sweep antialias -> %s (image sweep %s)\n",
                  on ? "on" : "off",
                  (customStyled() && theme_style::radar().sweepTypeImage) ? "in use" : "NOT in use");
}

void setGlide(int mode) {
    s_forceGlide = mode;
    Serial.printf("[radar] glide -> %s\n",
                  mode < 0 ? "auto (custom designs snap)" : (mode ? "forced on" : "forced off"));
}

void noteSelectionDetailArrived() {
    if (s_selectMode) s_selActivityMs = lv_tick_get();
}

// The SAME sweep, on another tile. See the header for why this is not a second one.
//
// The scope is tile 0 of a tileview and the weather map is tile 1, so the sweep built here
// simply is not on the weather tile: it had no sweep at all. Moving the object costs a
// re-parent and nothing else, because the timer that turns it is created once in init() and
// never paused, and it advances by real elapsed time rather than by ticks. It therefore
// keeps turning at the same rate through the move, and arrives at the right angle rather
// than starting again from zero.
//
// Ordering: foreground within its new parent, so it sits over the precipitation image the
// way it sits over the scope's rings. Callers that put anything above it re-assert that
// afterwards, the same as everywhere else on this device.
// Build the weather map its OWN sweep, on its own tile, from its own settings.
//
// This started out as "move the scope's sweep across", which worked and was wrong: the
// weather map then wore the Flight Tracker's brass, because it was literally the Flight
// Tracker's object. They are separate apps that happen to be built the same way.
//
// What is shared is the timer and s_sweepDeg, which is the only part smoothness ever
// depended on. Nothing else crosses between them: not the colour, not the artwork, not the
// speed, not the on/off switch.
void buildWeatherSweep(void *lv_parent) {
    lv_obj_t *parent = (lv_obj_t *)lv_parent;
    if (!parent || s_wxSweep) return;
    s_wxSweep = make_layer(parent, wx_sweep_draw_cb);
    lv_obj_move_foreground(s_wxSweep);
}

void setWeatherSweepVisible(bool on) {
    if (!s_wxSweep) return;
    if (on) lv_obj_clear_flag(s_wxSweep, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(s_wxSweep, LV_OBJ_FLAG_HIDDEN);
}

} // namespace radar

#pragma once
// radar_view.cpp's shared internals: the palette, the tunables, the file-level state, and the small helpers every part
// of the Flight Tracker uses. radar_view.cpp was one 3300-line file; it is split along its own section markers, and
// this is what the pieces share. Private to src/app/radar: nothing outside includes it.
//
// Everything is in namespace radar_impl so that names such as show() or s_theme cannot collide with another
// file's at link time. The using-directive at the bottom lets the radar_*.cpp files use them unqualified.
#include "radar_view.h"
#include "display.h"   // display_lvgl_us(): see the frame profiler below
#include "curved_text.h"
#include "app_shell.h"       // knob capture: default view releases it, selection mode grabs it
#include "config.h"
#include "aircraft_aging.h"
#include "track_select.h"
#include "geo.h"
#include "coastline.h"
#include "roads_sd.h"
#include "airports.h"
#include "text_tokens.h"   // shared {token} expansion, see radar_fmt()
#include "route.h"           // route_request()/route_get() — {from}/{to} tokens in a custom text banner
#include "custom_radar.h"    // CUSTOM_HAS_RADAR / CUSTOM_RTEXT{1,2,3}_* / CUSTOM_HAS_RADAR_STYLE / CUSTOM_SWEEP_*, CUSTOM_BLIP_*, CUSTOM_SEL_*, CUSTOM_OFFRANGE_*, CUSTOM_CENTER_* — a theme push's selection banners + visual styling
#include "radar_sprite.h"    // radar_custom_plate()/radar_custom_overlay()/radar_custom_blip_icon() — the editor's baked background+rings+crosshair / CRT+glass / aircraft-icon layers
#include "custom_radar_blip.h"   // CUSTOM_HAS_RADAR_BLIP_IMAGE / CUSTOM_RADAR_BLIP_PIVOT_X/Y
#include "custom_radar_sweep.h"  // CUSTOM_SWEEP_IMAGE_PIVOT_X/Y / CUSTOM_SWEEP_IMAGE_CENTER_X/Y — compile-time, coupled to whichever sweep sprite is baked in
#include "theme_style.h"
#include "theme_font.h"   // per-theme fonts, with the compiled font as fallback     // per-theme sweep/blip/selection/off-range/center/RTEXT values — see theme_style.h for what's covered vs. stays compile-time
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include <algorithm>
#include <stdlib.h>
#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#else
// Desktop simulator: no ESP heap caps and no Serial. The flatten/etch code below is
// shared (the sim benefits from the same architecture), so shim the two device-isms
// rather than fork the logic. Same pattern wx_radar_client.cpp already uses.
#include <cstdarg>
static inline void *heap_caps_malloc(size_t sz, int) { return malloc(sz); }
static inline void heap_caps_free(void *p) { free(p); }
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } void println(const char *s) const { puts(s); } } Serial;
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace radar_impl {

// ---- phosphor palette (mockup) ----
#define COL_GREEN  lv_color_hex(0x1DFF86)
#define COL_LEAD   lv_color_hex(0x3DFF9A)
#define COL_INK    lv_color_hex(0xEAFFF3)
#define COL_SOFT   lv_color_hex(0x9AFFC8)
#define COL_EMERG  lv_color_hex(0xFF5A3C)
// coastline outline — steel blue, deliberately off the red/amber/lime/green/cyan
// altitude-trail palette so land never reads as an aircraft track. Aviator theme
// swaps in a sepia/brass equivalent so it reads as an aged chart, not a scope.
#define COAST_COLOR lv_color_hex(0x4E86C6)
#define COAST_COLOR_AVI lv_color_hex(0x6B5638)
// roads (from the SD card, see roads_sd.cpp) — a muted neutral grey, distinct from
// both the coastline's blue and the airport markers' grey-blue so all three read
// as separate layers rather than blurring together.
#define ROAD_COLOR lv_color_hex(0x707868)
#define ROAD_COLOR_AVI lv_color_hex(0x8A7F63)
// airport markers — a neutral muted grey-blue so they sit quietly under the traffic.
#define AIRPORT_COLOR lv_color_hex(0x8A93A6)
#define AIRPORT_COLOR_AVI lv_color_hex(0x9C8F73)
// ---- aviator palette (WWII scope: brass rings, ivory sweep/ink, warm chrome) ----
#define AVI_RING lv_color_hex(0x6B5A3A)
#define AVI_LEAD lv_color_hex(0xDACFA6)
#define AVI_INK  lv_color_hex(0xEDE3CC)
#define AVI_SOFT lv_color_hex(0x9C8F73)
#define AVI_BG   lv_color_hex(0x14100A)
// ---- orb palette (Orb) ----
#define ORB_BLIP   lv_color_hex(0xFFE11A)
#define ORB_EMERG  lv_color_hex(0xFF4D2E)
#define ORB_ACCENT lv_color_hex(0xFF8A1E)
#define ORB_GRID   lv_color_hex(0x3F8B30)
#define ORB_BG_TOP lv_color_hex(0x18540F)
#define ORB_BG_BOT lv_color_hex(0x09250A)
#define ORB_FLOW   lv_color_hex(0xFFC24D)

// ---- sweep config ----
#define SWEEP_PERIOD_MS   8000
// Sweep redraw cadence. Every tick invalidates the sweep's rotated bounding box, and
// LVGL must then re-blend every layer intersecting it — with this theme that is seven
// layers, two of them full-screen with alpha. Measured on device: ~250 ms of compositing
// per frame, i.e. ~4 fps, while this timer was asking for a redraw every 30 ms. Asking
// eight times faster than the hardware can deliver does not make it faster, it just
// queues more invalidation work behind an already-late frame.
//
// Now that the sweep advances by REAL elapsed time (see sweep_timer_cb), a slower tick
// does not slow the rotation down — it just takes bigger angular steps per redraw. So
// this is chosen to be achievable rather than aspirational.
// Ask for frames at a rate the hardware can actually meet. This theme composites for
// ~166 ms per frame (measured: 6 fps, 88% of every second inside LVGL), and this timer was
// asking every 66 ms, two and a half times faster. The surplus requests do not produce
// surplus frames; they just land whenever the renderer gets to them, so the gaps between
// redraws are irregular. Since the sweep advances by real elapsed time, irregular gaps
// become irregular angular steps, which is the jitter the owner could see.
//
// Slower and regular beats faster and ragged here: a steady sweep is what makes this read
// as an instrument, and that was the owner's explicit priority over everything else on screen.
// Paced to what this device can ACTUALLY render, not to what looks good on paper.
//
// This was 66 ms (15 fps requested). Measured on hardware, a Flight Tracker frame takes
// 66-160 ms, averaging 81: the minimum is exactly this timer period, and everything above
// it is the renderer failing to keep up. So the timer was asking for frames faster than
// they could be drawn, and the overshoot landed as uneven arrival — 66 ms then 160 ms then
// 70 — which is precisely the stutter the eye picks up. A frame rate you cannot hit is not
// a frame rate, it is a source of jitter.
//
// The owner's priority is explicit and this follows it: perfectly even motion beats a higher
// number. At 100 ms the timer, not the renderer, decides when frames happen almost all of
// the time, so they arrive evenly. Costs ~2 fps and buys consistency.
// Measured, not guessed. Three values tried on the hardware, steady-state frame-time
// spread (the thing the eye actually reads as stutter):
//     66 ms  -> avg 81 ms,  spread 94-99 ms    (the old value: asking for frames it cannot draw)
//    100 ms  -> avg 103 ms, spread 55-60 ms    <- best
//    120 ms  -> avg 123 ms, spread 122-157 ms  (worse: heavy frames land as bigger multiples)
#define SWEEP_FRAME_MS    100
#define SWEEP_TRAIL_DEG   38.0f
#define SWEEP_TRAIL_STEPS 20
#define SWEEP_TRAIL_OPA   72

// ---- aircraft / flow / orb config ----
// How often aircraft glyphs are allowed to move. Deliberately coarse, and it is a product
// decision rather than a performance accident: a steady sweep is what makes this read as an
// instrument, while an aircraft's position being two seconds stale is invisible. The owner chose
// that trade explicitly.
//
// Each step invalidates one box per aircraft that moved, and with ~28 contacts on screen
// every one of those boxes forces LVGL to re-blend all the layers it touches. That was the
// variable cost per frame, and variable cost is exactly what the sweep cannot tolerate:
// because the sweep advances by real elapsed time, an unusually slow frame makes it take an
// unusually big angular jump. Correct speed, uneven motion. Measured before this change:
// 88% of every second inside LVGL, frame rate wandering 5-7 fps.
//
// Time-gated rather than counted in frames, so the cadence stays 2 s whatever the frame
// rate is doing. A frame counter would have made this drift with the very thing it is
// meant to stabilise.
// Matched to how far a glyph actually travels, which is the only thing that decides how
// many steps are worth taking. An aircraft crosses about 30 px of a 30 km scope between
// polls ten seconds apart, so 250 ms gives about forty steps for thirty pixels of travel:
// slightly more than one step per pixel, and anything faster is work that cannot change a
// single pixel on the glass.
//
// It was 2000, which is five steps across a whole poll, and that was chosen to protect a
// frame budget. The A/B above says the budget was never at risk: 80 ms and 2000 ms both
// held 9 fps, because the steps are one pixel each whatever their cadence.
#define AC_INTERP_MS      250
// Overridable over the cable (?orb interpms), so the cost of a faster glide can be measured
// by sweeping the value on a running Orb rather than reflashing once per trial. Zero means
// use AC_INTERP_MS. Deliberately not persisted: it is an instrument, not a setting.
extern uint32_t s_acInterpMs;
// -1 auto (custom designs snap, built-ins glide), 0 force snap, 1 force glide. Never
// persisted: an instrument, not a setting.
extern int s_forceGlide;
// 0 = the design's own count. An instrument, never persisted.
extern int s_forceTrailSteps;
// Set when the style changes under a running screen, so the pacing is measured fresh.
extern bool s_pacingStale;
#define TRAIL_MAX         7
#define TAP_RADIUS_PX     40    // generous finger-tap catch radius (picks the nearest glyph within it)
#define FLOW_MAX          240   // see setTrailLength: repaint cost is ~300 us per segment
#define FLOW_REDRAW_EVERY 80
#define FLOW_OPA          55
#define ORB_BLIPS      7
#define ORB_ARROWS     8
#define BALL_R            9
#define WAVE_EXPAND       28.0f

extern int        s_theme;
extern void      (*s_themeCb)(int);
// scope "chrome" palette (rings/sweep/crosshair/labels) — retinted per theme
extern lv_color_t s_cRing, s_cLead, s_cInk, s_cSoft;
static const char *THEME_NAMES[THEME_COUNT] = { "ORB", "MILITARY", "AVIATOR" };
// First-entry loading notice. Opening the Flight Tracker from cold does real work
// before there is anything to show: the coastline, airports and roads are all projected
// for this location, the map is etched, and the first aircraft snapshot has to arrive.
// The sweep starts turning and then visibly stops for a few seconds, which reads as a
// crash rather than as loading. So say what is happening.
extern lv_obj_t   *s_loading;
extern bool        s_loadingPending;
extern uint32_t    s_loadStartMs;
extern int         s_loadShownSec;
extern lv_obj_t   *s_themeLabel;
extern lv_timer_t *s_themeLabelTimer;
extern lv_obj_t  *s_parent;
extern lv_obj_t  *s_gridLayer;
extern lv_obj_t  *s_sweep;
extern lv_obj_t  *s_wxSweep;
extern lv_obj_t  *s_sweepImg;
extern lv_obj_t  *s_acLayer;
extern lv_obj_t  *s_flowCanvas;
extern lv_color_t *s_flowBuf;
extern lv_obj_t  *s_rose[4];
extern lv_obj_t  *s_centerDot;
extern lv_obj_t  *s_pulse;
extern lv_obj_t  *s_rangeLbl;
extern bool       s_rangeLblVisible;
extern bool       s_sweepEnabled;
extern bool       s_airportsEnabled;
extern int        s_maxOnScreen;
extern bool       s_bigText;
extern int        s_trailMax;
extern int        s_flowMax;
extern int        s_flowGenMax;
extern lv_timer_t *s_timer;
extern float       s_sweepDeg;
// The weather map's own angle, advanced in the SAME callback off the SAME smoothed frame
// time, just at its own rate. The two used to share s_sweepDeg outright, which is why the
// weather theme's sweepSpeed was a slider in the theme tool that moved nothing: there was only
// one speed and it belonged to the Flight Tracker. Sharing the TIMER is what keeps the
// motion even; sharing the ANGLE was never the part that mattered.
extern float       s_wxSweepDeg;
// Sweep pacing state, at file scope so the loading gate can reset it. A multi-second
// stall during first-entry projection would otherwise poison the smoothed frame time and
// make the sweep lurch on its first few steps after the wait.
extern uint32_t    s_lastSweepMs;
extern float       s_emaDtMs;
extern float       s_prevSweepDeg;
extern float       s_wxPrevSweepDeg;
extern float       s_wavePhase;
extern uint32_t    s_lastUpdateMs;
extern uint32_t    s_animStartMs;
extern uint32_t    s_pollMs;
extern int         s_frameCtr;
extern lv_coord_t  s_cx, s_cy;
extern std::string s_selHex;
// Flight Tracker knob state machine (shared by device + simulator so they can't
// drift). Two modes: DEFAULT VIEW — nothing selected, knob released, a turn opens
// the app switcher and a push enters selection. SELECTION MODE — an aircraft
// selected, knob captured, a turn cycles aircraft; a push, or SELECT_IDLE_MS of no
// input, drops back to the default view. See knobPress()/knobTurn()/knobEnter().
extern bool        s_selectMode;
extern uint32_t    s_selActivityMs;
// How long a selected aircraft stays selected without input.
//
// Was 5000, which was never long enough to read the card even when it worked. The route
// line ("PHX -> SFO") is fetched on demand from a second service, and that lookup queues
// behind the ADS-B poll and the weather pumps on the same task — so the card could easily
// time out before its own text arrived. That is now much likelier to succeed at all (the
// lookup used TLS this board cannot do, see route_client.cpp), but it is still a network
// round trip, and five seconds was a window you had to race.
static constexpr uint32_t SELECT_IDLE_MS = 5000;
void radar_exit_select();             // -> default view (deselect + release knob); defined below
// Called when late-arriving detail (the route) reaches a card that is already up. Restarts
// the idle countdown, because the thing worth reading only just appeared: without this the
// route could land with a second left on the clock and vanish as you registered it.
void noteSelectionDetailArrived();
extern float       s_lastRangeKm;
extern lv_obj_t   *s_feedWarn;
extern lv_obj_t   *s_simBadge;
extern lv_obj_t   *s_loadTicker;
extern lv_obj_t   *s_textCanvas;
// The selection card: a plate under those banners, parked on the far side of the scope
// from whatever is selected. Two objects rather than one drawn shape, so LVGL does the
// rounded corners, the border and the compositing itself — the text canvas above stays
// purely text, and a glyph's glow lands over the card correctly instead of losing to it
// under the canvas's own "higher opacity wins" rule.
extern lv_obj_t   *s_cardObj;
extern lv_obj_t   *s_cardImg;
extern lv_color_t *s_textBuf;
extern lv_obj_t   *s_plateImg;
extern lv_obj_t   *s_ringsImg;
extern lv_obj_t   *s_overlayImg;
extern lv_obj_t   *s_staticImg[2];
extern lv_obj_t   *s_dimLayer;

struct FlowSeg { lv_point_t a, b; uint16_t gen; };   // gen = the poll it was laid down on
extern std::deque<FlowSeg> s_flow;
extern int s_flowRedrawCtr;
extern uint16_t s_flowGen;

struct AcDraw {
    lv_point_t pos;            // current (animated) screen position — what gets drawn
    lv_point_t from, to;       // smooth-motion glide endpoints (M4 interpolation)
    float      track;
    lv_color_t color;
    bool       emergency;
    bool       inRange;
    char       hex[8];
    char       call[12];
    char       type[8];
    char       altTxt[12];
    float      altFt;
    bool       onGround;
    float      vsFpm, gsKt, distKm, bearingDeg;
    int        squawk;
    float      freshness;      // 1.0 = seen this poll, fading to a floor as it ages, see ac_freshness()
    uint32_t   lastUpdateMs;   // when main.cpp last heard this contact; age_step() re-derives freshness from it
    std::vector<lv_point_t> trail;
};
extern std::vector<AcDraw> s_acs;
// Hexes the scope is currently following. See the sticky-selection block in update() for
// why the set persists between polls instead of being recomputed from distance each time.
extern std::set<std::string> s_tracked;
extern std::map<std::string, std::vector<lv_point_t>> s_trails;

static const float GX[4] = { 0.0f,  7.0f, 0.0f, -7.0f };
static const float GY[4] = { -11.0f, 5.0f, 8.0f, 5.0f };

// Aviator theme only: a narrow kite for everyday traffic, a wide kite for recognized
// large/heavy types — same 4-point convex "kite" family as GX/GY above (just resized),
// not a literal notched silhouette. LVGL's software polygon fill ONLY supports convex
// polygons (see lv_draw_sw_polygon.c: "Only convex polygons are supported") — an
// earlier version of this used a notched (concave) shape and hard-locked the device,
// because a concave input can spin its scanline fill loop forever. Verify convexity
// (all four cross-products of consecutive edges same sign) before changing these.
static const float FIGHTER_X[4] = {  0.0f,  4.0f,  0.0f,  -4.0f };
static const float FIGHTER_Y[4] = { -9.0f,  3.0f,  5.0f,   3.0f };
static const float BOMBER_X[4]  = {  0.0f, 12.0f,  0.0f, -12.0f };
static const float BOMBER_Y[4]  = {-14.0f,  4.0f,  9.0f,   4.0f };

// Recognized large/heavy ICAO type-designator prefixes -> draw the bomber silhouette.
// Everything else (GA, regional, and anything the feed didn't identify) reads as a fighter.
bool is_big_type(const char *t);

// The scope's own skin. Orb's neon grid and Aviator's sepia dial are selected by s_theme; Military and anything else
// falls back to the plain ring/crosshair scope.
static inline bool orb() { return s_theme == THEME_ORB; }
static inline bool aviator() { return s_theme == THEME_AVIATOR; }
static inline lv_color_t coast_color()   { return aviator() ? COAST_COLOR_AVI   : COAST_COLOR; }
static inline lv_color_t airport_color() { return aviator() ? AIRPORT_COLOR_AVI : AIRPORT_COLOR; }
static inline lv_color_t road_color()    { return aviator() ? ROAD_COLOR_AVI    : ROAD_COLOR; }

// A theme push with full visual styling (background/rings/crosshair baked
// into a plate image, sweep/blip/selection/off-range/center as live parameters,
// CRT/glass baked into an overlay image) — replaces the built-in Orb/Military/
// Aviator scope look entirely, the same way a custom design already overrides
// the clock's faces. CUSTOM_HAS_RADAR_STYLE is always defined (0 in the
// committed stub) by custom_radar.h, so this is cheap and safe to call anywhere.
static inline bool customStyled() { return (bool)CUSTOM_HAS_RADAR_STYLE; }

void show(lv_obj_t *o, bool v);

void hide_theme_label_cb(lv_timer_t * /*t*/);

// Flash the theme name at the top of the radar for ~2s, so cycling themes (knob
// push, touch long-press, a screen tap, or a fresh boot) always shows which one
// you're on. White text on a solid black plaque so it stays readable over any theme.
void show_theme_label(const char *name);

lv_color_t alt_color(float altFt, bool onGround);

// Full brightness for the first 25 s, which is two and a half polls, then fading linearly
// to AC_DIM_FLOOR_OPA by 75 s and held there until the table drops the entry entirely at
// AC_HARD_EXPIRE_MS (main.cpp's applyPolledAircraft — this never sees one older). Never
// reaches zero before removal: the point is to SAY a contact has gone quiet, not to make it
// disappear piecemeal ahead of actually being gone.
// The banner may never arrive before anything on the dial looks stale. That ordering IS the
// 2026-08-24 fix: "No aircraft data" over full-brightness traffic is the instrument
// contradicting itself. Fading first and announcing later is a graduated warning and is
// fine. A comment cannot fail; this can.
static_assert(ADSB_NO_DATA_MS >= AC_DIM_START_MS,
              "the no-data banner must not appear before the contacts start dimming");

// The curve itself lives in aircraft_aging.h, shared with main.cpp's pruning and covered by
// tests/aircraft_aging_test.cpp.
float ac_freshness(uint32_t ageMs);

static inline lv_opa_t scale_opa(lv_opa_t base, float mul) {
    return (lv_opa_t)lroundf((float)base * (mul < 0.0f ? 0.0f : (mul > 1.0f ? 1.0f : mul)));
}

static inline lv_point_t rim_point(float bearingDeg, float r) {
    const float a = bearingDeg * (float)M_PI / 180.0f;
    lv_point_t p;
    p.x = (lv_coord_t)lroundf((float)s_cx + r * sinf(a));
    p.y = (lv_coord_t)lroundf((float)s_cy - r * cosf(a));
    return p;
}

// rotate the local point (px,py) by `deg` (clockwise, screen coords) and offset to (ox,oy)
static inline lv_point_t rot_pt(float px, float py, float deg, lv_coord_t ox, lv_coord_t oy) {
    const float a = deg * (float)M_PI / 180.0f;
    const float c = cosf(a), s = sinf(a);
    lv_point_t p;
    p.x = (lv_coord_t)(ox + (lv_coord_t)lroundf(px * c - py * s));
    p.y = (lv_coord_t)(oy + (lv_coord_t)lroundf(px * s + py * c));
    return p;
}


// ---- defined in radar_sweep.cpp / radar_aircraft.cpp / radar_view.cpp ----
void wx_sweep_draw_cb(lv_event_t *e);
void sweep_draw_cb(lv_event_t *e);
void sweep_timer_cb(lv_timer_t *t);
void draw_glow(lv_draw_ctx_t *d, lv_point_t pos, float baseR, float glowPx, lv_color_t color);
void ac_draw_cb(lv_event_t *e);

// Is this point inside a theme's exclusion zone? See theme_style::Radar::zones for why
// these exist: they let decoration sit in the free baked background instead of in an
// expensive layer above the aircraft. Cheap enough to call per point per frame — at most
// six squared-distance comparisons, no square roots.
static inline bool in_excluded_zone(lv_coord_t x, lv_coord_t y) {
    if (!customStyled()) return false;
    const theme_style::Radar &rs = theme_style::radar();
    for (int i = 0; i < rs.zoneCount; ++i) {
        const theme_style::Radar::Zone &z = rs.zones[i];
        bool inside;
        if (z.rect) {
            const long dx = (long)x - z.x, dy = (long)y - z.y;
            inside = (dx >= -(z.w / 2) && dx <= z.w / 2 && dy >= -(z.h / 2) && dy <= z.h / 2);
        } else {
            const long dx = (long)x - z.x, dy = (long)y - z.y;
            inside = (dx * dx + dy * dy <= (long)z.r * z.r);
        }
        // Inverted zones hide everything OUTSIDE the shape, which is how one big circle
        // becomes a containment ring keeping aircraft off the dial's border.
        if (inside != z.invert) return true;
    }
    return false;
}
static inline bool ac_masked(const AcDraw &ac) { return in_excluded_zone(ac.pos.x, ac.pos.y); }

#ifdef ARDUINO
enum { RP_GRID = 0, RP_SWEEP, RP_AC, RP_WX, RP_N };
inline uint32_t s_rp[RP_N] = { 0 };      // inline variable: one array shared by every radar_*.cpp
struct RadarPhase {
    int slot; uint32_t t0;
    explicit RadarPhase(int s) : slot(s), t0(micros()) {}
    ~RadarPhase() { s_rp[slot] += micros() - t0; }
};
#define RADAR_PHASE(x) RadarPhase _rp(x)
#else
#define RADAR_PHASE(x) do { } while (0)
#endif
void radar_phase_report(void);   // radar_view.cpp: once a frame, prints one line every ten seconds (device only)

bool canvas_acquire(lv_obj_t *canvas, lv_color_t *&buf, const char *tag);
void canvas_release(lv_obj_t *canvas, lv_color_t *&buf);
int radarLayerOrder(const int **out);
}  // namespace radar_impl

using namespace radar_impl;

// ---- defined in radar_flatbg.cpp / radar_select.cpp (namespace radar, as they were in the one file) ----
namespace radar {
void take_map_snapshot();
void apply_grid_visibility();
void rebuild_flat_background();
void refresh_custom_text();
}

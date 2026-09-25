#pragma once
// Per-theme visual style (colors/positions/formats/geometry) — the runtime half of
// the multi-theme SD system. Art (plate/overlay/hand/blip PNGs) already travels per
// theme via /themes/<slug>/*.png (see theme_sd.h + each screen's own decode_sd_first
// pattern). Until this module existed, STYLE (everything a theme push bakes as a
// CUSTOM_* #define into custom_clock.h/custom_radar.h and the like)
// was compile-time only — one shared firmware binary, so switching the active SD theme
// via Settings > Design swapped the art but not the color/format/layout, which stayed
// stuck on whichever theme's screen was pushed last (see git history/PRs referencing
// "theme bleeding"). This reads a small per-theme JSON file alongside the art
// (/themes/<slug>/{clock,radar,settings,menu}_style.json, written by Launch Kit on
// every push — see server.js's writeSimSdAsset("*_style.json", ...) call sites) and
// overrides the compiled CUSTOM_* defaults with it, per field, per theme.
//
// NOT covered (known, deliberate limitations — same class of "compile-time only" gap
// that already existed for these before this module, unchanged by it):
//   - Fonts (CUSTOM_*_FONT) — real compiled LVGL glyph bitmaps, not simple values;
//     making these travel per-theme needs LVGL's binary font/lv_fs runtime-loading
//     path, a separate, much larger undertaking. Whichever theme's screen was pushed
//     last still wins the font family/size/weight.
//
// FIXED since this comment was written (these now DO travel per theme):
//   - Clock hand art, pivot, center, blend, draw order, and the per-hand show/hide
//     gates. Art comes from /themes/<slug>/clock_hand_{hour,minute,second}.png and
//     clock_static{1,2}.png; geometry from the "hands" block in clock_style.json.
//   - Which apps appear in the knob menu, from /themes/<slug>/theme.json.
//   - Radar sweep/blip rotation pivots, and the radar layer order.
//   - Radar's operational params: range, max aircraft, hide-ground, min-altitude and
//     the centre dead zone all travel in radar_style.json now. The compile-time
//     CUSTOM_HAS_RADAR_{RANGE,MAXAC,HIDEGROUND,MINALT,DEADZONE} overrides that used to
//     sit alongside them in main.cpp are gone, not merely outranked: two of them
//     (hide-ground, min-altitude) ran AFTER applyThemeSettings() and silently undid a
//     theme that had stated both, which is the failure this whole module exists to stop.
//   - Home lat/lon deliberately does NOT travel. It is the owner's, not the theme's: a
//     design that moved somebody's Orb to the designer's city on install would be the
//     device reconfiguring itself over a choice its owner had already made. It comes
//     from Settings, the setup page, or GPS, and nothing on the card can outrank that.
//     CUSTOM_HAS_RADAR_HOME, which pinned every unit flashed from one push to that
//     push's coordinates and quietly made the setup page's lat/lon box a no-op, is
//     deleted rather than migrated.
//   - Most of the remaining CUSTOM_HAS_* show/hide gates have been demoted to seeding a
//     runtime default: the SD art path runs whatever the macro says, and the draw sites
//     test a `show` field this module fills in. What is still a real compile-time gate,
//     and therefore still decided by whichever theme was pushed last, is the four master
//     switches (CUSTOM_HAS_RADAR,
//     CUSTOM_HAS_RADAR_STYLE) and the menu's three per-slot gates. Those are the next
//     migration, not a standing limitation.
#include <lvgl.h>
#include "theme_font_resolve.h"
#include "theme_roles.h"

namespace theme_style {

// What this firmware understands of a theme, as one number a designer's tool can ask for: the capability level.
// FW_VERSION cannot serve, because it tracks releases and says nothing about what a theme may contain.
//
// Every new theme capability bumps this and adds an entry to the ledger in docs/theme-caps.md, which also says what an
// Orb below each level does with a theme that uses it (draws the compiled default, ignores the key, or refuses the
// design). tests/test_theme_caps_ledger.py fails when the two disagree.
constexpr int THEME_CAPS = 54;

struct ClockText {
    bool     show   = false;
    int      x      = 233;
    int      y      = 233;
    uint32_t color  = 0xF2F5F9;
    // 0..255. Costs nothing: both glyph blitters already multiply every pixel by an
    // opacity and the value was simply pinned at full, and LVGL labels alpha-blend for
    // anti-aliasing regardless. See THEME_CAPS 18.
    int      opa    = 255;
    int      glow   = 0;
    uint32_t glowColor = 0xF2F5F9;
    char     fmt[32] = "";
    // The plate behind the words. THEME_CAPS 33: every text control in the theme tool now offers
    // the same two choices, a background or a curve, because offering a different set of
    // controls on each card taught nobody anything except that the cards were written on
    // different days. bgOpa defaults to 0, so a theme that never asked for one is unchanged.
    uint32_t bg     = 0x000000;
    int      bgOpa  = 0;     // 0..255, 0 = no plate at all
    int      radius = 4;     // corner rounding, px
    bool     curved = false;
    int      curveR = 0;
    float    arcDeg = 0.0f;
    int      align  = 0;   // 0 left, 1 center, 2 right
};

// One rotating (or static) clock-face image layer. `show` is the runtime replacement for
// the CUSTOM_HAS_{HOUR,MINUTE,SECOND,STATIC1,STATIC2} compile-time gates: a theme that
// has no second hand sets show=false and no second hand is drawn, whatever the last
// flashed theme happened to compile in. Geometry travels with the art because a pivot is
// only meaningful against its own image's pixel dimensions.
struct Hand {
    bool show    = false;
    int  pivotX  = 0,   pivotY  = 0;
    int  centerX = 233, centerY = 233;
    int  blend   = 0;
};

struct Clock {
    uint32_t  bg = 0x000000;
    // Rotate the whole background plate in lockstep with a hand: 0 none, 1 hour,
    // 2 minute, 3 second. Launch Kit's "Rotate with" control on the background.
    //
    // The exported plate PNG is baked with only its static rotation applied (the editor
    // explicitly leaves the follow angle out), so the device adds the live hand angle on
    // top. Without this the border sat at one fixed angle while the hand moved, lining up
    // once an hour by coincidence.
    int       plateFollow = 0;
    // THEME_CAPS 43. Which side of the hands the two text banners fall on. False, the way it
    // has always drawn, puts the hands over the words: a watch sweeps its hands across
    // whatever is printed on the dial. True lifts the words on top, which is what a date
    // window or a signature laid across the face wants.
    bool      textOverHands = false;
    ClockText text1;
    ClockText text2;
    Hand      hand[5];                        // 0=hour 1=minute 2=second 3=static1 4=static2
    int       order[5] = { 3, 4, 0, 1, 2 };   // back-to-front draw order, kind indices
    int       orderN   = 5;
    // Shadows cast by ONE light that does not move with the hands.
    //
    // The hand's own sprite cannot carry this: a shadow painted into it turns with the
    // hand, which reads as a lamp orbiting the dial. So each hand gets a second sprite,
    // clock_shadow_{hour,minute,second}.png, holding its silhouette already blurred and
    // already in the shadow's colour and opacity. That art is drawn at the hand's angle
    // about the hand's own pivot, but centred dx/dy away in SCREEN space, which is what
    // keeps the shadow pointing the same way all the way round the dial.
    //
    // Only the offset lives here. Softness, colour and strength are baked, so they cost
    // the device nothing at all and the runtime work is one ordinary rotate-and-blend.
    bool      shadowOn = false;
    int       shadowDX = 0, shadowDY = 0;     // px, screen space, applied to every hand
    // A second hand that sweeps instead of ticking. THEME_CAPS 47.
    //
    // Off by default, because a tick is what most clocks do and what every theme written
    // before this expects. The owner asked for the choice per design: "most of the time i'll want
    // the second hand to only move every second, but there might be some clocks, like the
    // modern theme where i'd love it to move perfectly smooth, like some clocks do."
    //
    // It is not free and it is not always possible; see clock_view's sweep_possible(). A
    // design that draws anything above its second hand other than the glass falls back to
    // ticking rather than drawing the layers in the wrong order.
    bool      secondSweep = false;
    // THEME_CAPS 38. A virtual mainspring: the clock runs down and has to be wound with the
    // knob. See clock_wind.h for why it exists and what it refuses to do. Off unless a
    // design asks, because a stopped clock reads as a broken one to anybody who did not
    // switch it on themselves.
    bool      windOn     = false;
    int       windSecs   = 86400;  // how long one full wind lasts, in seconds
    int       windTurns  = 5;      // how many turns of the knob a full wind takes
    bool      windSound  = true;   // a click per detent while winding
    bool      windNotice = true;   // the full-screen "please wind" panel when it stops
    // THEME_CAPS 41. The wind screen is a DESIGN, not a fixed panel. It was white on black in
    // the built-in face because it started life as a system notice like the no-SD screen; it
    // is not one. It appears during ordinary use, on a themed clock, and a Steam Punk Orb
    // asking to be wound in the same grey sans as a factory error message is the seam showing.
    //
    // Sizes come from the compiled ladder and nothing between its rungs: see font_for_px().
    // The colour behind the picture, and nothing else. There was an opacity beside it, back
    // when this screen veiled the clock rather than covering it; it is gone with the veil,
    // because a field nothing reads is a control the theme tool can offer and the Orb will ignore.
    // THEME_CAPS 44. A picture behind the wind screen, and switches for everything drawn
    // over it. The owner asked for the picture after asking for no picture, and both were right at
    // the time: a scrim over a running clock wants transparency, a designed screen of its own
    // wants art. The colour and its opacity still sit under the image, so a design can have
    // either, or a photograph with a wash over it.
    bool      windImageOn   = false;   // wind_bg.png
    bool      windRingShow  = true;
    bool      windTitleShow = true;
    bool      windAskShow   = true;
    // A crank that turns with the knob. One revolution of the crank per revolution of the
    // knob, because anything else is a gear ratio nobody asked for and the point is that the
    // thing on screen moves the way your hand does.
    bool      windCrankOn   = false;   // wind_crank.png
    // Where the crank's own turning point sits on the dial, and where that point is INSIDE
    // the artwork. Exactly the pair a hand carries, and for the same reason: the picture is
    // trimmed to its ink, so the pivot has to travel with the crop or the crank wobbles
    // instead of turning.
    int       windCrankX    = 233;   // on the dial
    int       windCrankY    = 233;
    int       windCrankPX   = 0;     // in the artwork's own pixels
    int       windCrankPY   = 0;
    // Where the crank sits at rest, in degrees clockwise from the artwork's own orientation.
    //
    // A crank's picture points wherever it was drawn pointing, and that is rarely where it
    // should sit when the screen opens. The owner's brass key reads best coming in from the top
    // left, which is not how the photograph was cropped. Rotating the file is the wrong
    // answer: the pivot is marked in the artwork's pixels, so turning the image moves the
    // point it turns about.
    //
    // 0 keeps every theme written before this exactly as it was.
    int       windCrankRest = 0;
    // A shadow under the crank, from the same ONE light the hands use (see shadowOn above).
    //
    // Its own sprite, wind_crank_shadow.png, baked to the SAME size and pivot as the crank
    // so the two turn as one. Softness, colour and strength are baked, so the device does
    // one more rotate-and-blend and nothing else; only the offset lives here, in SCREEN
    // pixels, which is what stops the shadow orbiting with the crank and reading as a lamp
    // going round the dial.
    //
    // Its own switch rather than the dial's, because the wind screen is a design in its own
    // right: its background, typefaces and margins are all separate from the face's, and a
    // crank can want a shadow on a dial whose hands do not.
    bool      windCrankShadowOn = false;
    int       windCrankShadowDX = 0, windCrankShadowDY = 0;
    uint32_t  windBg        = 0x000000;
    uint32_t  windRingTrack = 0x22282F;
    uint32_t  windRingFill  = 0xD8B56A;
    int       windRingWidth = 8;
    int       windRingR     = 212;   // px from the middle to the ring's centre line
    // Left and right margins per line, the band it may use, exactly as the News screen sets
    // its headlines. They decide where the words WRAP, so a line with no room breaks earlier
    // rather than running off a round screen, and an uneven pair shifts the block sideways.
    // THEME_CAPS 42 with the typefaces, because a face and the width it wraps at are the same
    // decision made twice if they arrive separately.
    int       windTitleML = 60, windTitleMR = 60;
    int       windAskML   = 83, windAskMR   = 83;
    int       windTurnsML = 60, windTurnsMR = 60;
    char      windTitle[64] = "The clock has\nwound down";
    int       windTitleSize = 28;
    uint32_t  windTitleCol  = 0xFFFFFF;
    int       windTitleY    = -84;   // px from the middle, negative is up
    // How solid each wind line is, 0-255. THEME_CAPS 48.
    //
    // Every other themed text on this device has carried an opacity since August, and these
    // three shipped without one. That is exactly the mistake
    // adding-a-screen.md exists to prevent: the Headlines screen shipped missing five
    // standard controls and was repaired one complaint at a time.
    //
    // Fully opaque by default, which is what these lines have always drawn at, so no theme
    // written before this changes.
    int       windTitleOpa = 255, windAskOpa = 255, windTurnsOpa = 255;
    char      windAsk[96]   = "Please wind the clock using the knob";
    int       windAskSize   = 20;
    uint32_t  windAskCol    = 0x9AA4B0;
    int       windAskY      = 8;
    bool      windTurnsShow = true;
    int       windTurnsSize = 16;
    uint32_t  windTurnsCol  = 0x5A636E;
    int       windTurnsY    = 122;
};

// Which apps this theme puts in the knob menu. Was custom_apps.h, compiled in, so it was
// one roster for the whole device rather than one per theme. Settings is deliberately
// absent: it is a system screen, not an app, and a theme that could switch it off would
// strand the user with no way back to WiFi, brightness, or theme selection.
struct Apps {
    bool clock        = true;
    bool flight       = true;
    bool weather      = true;
    bool surveillance = true;
    bool headlines    = true;   // world headlines, fetched through the gateway
    bool ticker       = true;   // stock ticker, through the same gateway
};

// ---- the Stock Ticker -------------------------------------------------------
//
// One symbol at a time, large enough to read across a room, with the rest of the watchlist
// running past underneath. The knob moves between them.
//
// The watchlist lives HERE, in the theme, for the same reason the news topics do: it is the
// thing a person picks, it wants to be editable in the theme tool rather than over a serial
// console, and a design that is about markets should be able to arrive with its own.
constexpr int TICKER_MAX_SYMBOLS = 8;
constexpr int TICKER_SYM_BYTES   = 13;   // 12 characters and a NUL, matching the gateway

struct Ticker {
    uint32_t bg           = 0x05070A;
    // Comma separated, as typed. Split on the device rather than stored pre-split: it is
    // one string in the JSON, one field in the theme tool, and one thing to get wrong.
    char     symbols[TICKER_MAX_SYMBOLS * TICKER_SYM_BYTES] = "^GSPC,^DJI,^IXIC,AAPL";
    int      pollSeconds  = 60;    // a minute. Clamped 15..900 on the way in.

    // Up and down. Not fixed green and red: the convention is inverted in Japan, and a
    // design in brass and cream should not be made to wear traffic-light colours.
    uint32_t upColor      = 0x35D07F;
    uint32_t downColor    = 0xE5484D;
    uint32_t flatColor    = 0x8A94A6;

    // The focused quote: name, price, change. Three text elements, each with the same
    // controls every other text element on this device has.
    uint32_t nameColor    = 0x8A94A6;
    int      nameSize     = 16;
    int      nameY        = 186;
    bool     nameShow     = true;

    int      priceSize    = 40;
    int      priceY       = 222;
    bool     priceShow    = true;
    // Off means the price is drawn in the up/down colour with everything else, which is the
    // livelier look; on lets a design hold the price steady and let only the change move.
    bool     priceColorOn = false;
    uint32_t priceColor   = 0xE8ECF1;

    int      changeSize   = 22;
    int      changeY      = 286;
    bool     changeShow   = true;
    bool     changePct    = true;   // show the percentage as well as the absolute move

    // The strip. STRIP_CURVED bends it around the bezel with curved_text::draw_arc, which is
    // the same code the clock's numerals and the scope's readouts use.
    enum StripPlace : uint8_t { STRIP_BOTTOM = 0, STRIP_TOP = 1, STRIP_CURVED = 2 };
    bool     stripShow    = true;
    uint8_t  stripPlace   = STRIP_CURVED;
    int      stripSize    = 16;
    uint32_t stripColor   = 0xC8D0DA;
    int      stripOpa     = 235;
    int      stripSpeed   = 26;    // px/sec, or degrees/sec when curved
    int      stripY       = 392;   // flat placements only
    int      stripRadius  = 196;   // curved only
    int      stripAngle   = 0;     // curved only: where the middle of the window sits, 0 = top
    bool     stripUpDown  = true;  // colour each entry by its own direction
};

// Display names, kept strictly separate from the identifiers they label.
//
// The identifiers here — the theme's folder slug, and the app keys "clock", "flight" and
// so on — are permanent. They are paths on the card, NVS values, and struct fields, so
// renaming one orphans data on every card already in the wild. The names below are just
// labels: a theme may call Flight Tracker whatever suits it, and change its mind, without
// anything underneath moving.
//
// Not having this distinction cost real time: the theme displayed as "Modern" lives in a
// folder called `the-office` (its former name), so "push Modern to the Orb" and
// "/themes/the-office/" looked like unrelated things.
struct Names {
    // 48, because the theme tool writes up to 40 characters into theme.json's "name" and this
    // was 32: a name of 32 to 40 characters was cut silently on the splash and in ?orb
    // hello, and nothing on either side said so. Found by cross-checking the two limits.
    char theme[48]        = "";              // the theme's own label, e.g. "Modern"
    // Who made it, from theme.json's "author". UX-047 says the name travels inside the
    // theme, and UX-028 says the splash shows it; this is the one place the device keeps
    // it. Empty when the file makes no claim, and the splash then prints the name alone.
    char author[64]       = "";
    char clock[20]        = "Clock";
    char flight[20]       = "Flight Tracker";
    char weather[20]      = "Weather";
    char surveillance[20] = "Surveillance";
    char ticker[20]       = "Stock Ticker";
    // The KEY stays `headlines`, the LABEL is "News", and the source file is still called
    // intel_view.cpp. Three names for one screen, none of which can be made to agree.
    //
    // There used to be a second app whose key was `intel`: a city/temperature/pressure
    // screen carrying a wiki fun fact captioned "INTEL". It is gone, this screen took the
    // word for a while, and the word was never what the screen does. It shows the news, so
    // it is called News.
    //
    // The key cannot follow, because it is a field in every theme.json already written to a
    // card and renaming it would silently switch this app off on every one of them. The
    // filenames do not follow either, for the same reason a rename is cosmetic by design:
    // a theme may call this screen anything it likes, so no filename could track it anyway.
    //
    // A theme that stores its own name for this app keeps it. Only themes with no opinion
    // (the empty string, which is what the theme tool saves unless someone types a name) pick this
    // up, which is why changing it here changes every stock theme and overrides nobody.
    char headlines[20]    = "News";         // key `headlines` -> intel_view.cpp
    char settings[20]     = "Settings";      // renameable, but never hideable
};

// One themeable line of text, wherever a screen puts one.
//
// Named RadarText when the Flight Tracker was the only screen with live text banners. The
// Weather map now carries four of its own (THEME_CAPS 28) and needs exactly these fields,
// so the struct was renamed rather than copied: a second struct with the same members is
// how two screens start drifting on what "curved" means. `RadarText` stays as an alias
// because it is spelled that way across radar_view.cpp and in every theme already written.
//
// `onCard` is the one Flight Tracker specific: it rides the aircraft selection card, and no
// other screen has one. A screen without a card simply leaves it false.
struct TextSlot {
    bool     show   = false;
    int      x      = 233;
    int      y      = 233;
    uint32_t color  = 0xFFFFFF;
    // 0..255. Costs nothing: both glyph blitters already multiply every pixel by an
    // opacity and the value was simply pinned at full, and LVGL labels alpha-blend for
    // anti-aliasing regardless. See THEME_CAPS 18.
    int      opa    = 255;
    int      glow   = 0;
    uint32_t glowColor = 0xFFFFFF;
    char     fmt[80] = "";
    // The plate behind the words. THEME_CAPS 33: every text control in the theme tool now offers
    // the same two choices, a background or a curve, because offering a different set of
    // controls on each card taught nobody anything except that the cards were written on
    // different days. bgOpa defaults to 0, so a theme that never asked for one is unchanged.
    uint32_t bg     = 0x000000;
    int      bgOpa  = 0;     // 0..255, 0 = no plate at all
    int      radius = 4;     // corner rounding, px
    bool     curved = false;
    int      curveR = 0;
    float    arcDeg = 0.0f;
    int      align  = 0;
    // Ride the selection card instead of sitting at a fixed spot on the scope. When set,
    // x/y stop being screen coordinates and become an offset from the card's own centre —
    // so the line travels with the card as the card chases the far side of the dial.
    bool     onCard = false;
};
using RadarText = TextSlot;

// The selection card: a little plate that appears when an aircraft is picked, always on
// the OPPOSITE side of the scope from that aircraft, so the thing you just selected is
// never sitting under the words describing it. Vector (a rounded rect) or an image.
struct RadarCard {
    bool     enabled    = false;
    bool     typeImage  = false;
    int      radius     = 120;   // px from the scope's centre to the card's centre
    int      w          = 150;   // vector card size; an image card uses its own pixels
    int      h          = 60;
    int      corner     = 10;    // vector corner rounding
    uint32_t color      = 0x101418;
    int      opacity    = 220;   // 0..255
    uint32_t borderColor = 0x39FF8A;
    int      borderWidth = 1;
};

// A plain decorative overlay (Static 1/2, see custom_radar_static.h) — no
// rotation/pivot, just a position and opacity. New with this struct itself
// (no legacy compile-time macro carries a default), so a missing/older
// radar_style.json just leaves it hidden (show=false), not mis-positioned.
struct RadarStatic {
    bool  show = false;
    int   x = 233;
    int   y = 233;
    int   opacity = 255;
    float scale = 1.0f;
};

// A keep-out shape, in 466x466 screen coordinates.
//
// These exist so decorative artwork can live in the BAKED BACKGROUND instead of in a layer
// above the moving parts. Measured 2026-08-17: Steam Punk's brass bezel, as a layer above
// the movers, cost 24% of the Flight Tracker's frame rate; the same art baked into the
// background costs nothing at all. A zone gets the same visual result, the moving parts
// never cross the decoration, for a handful of comparisons per frame.
//
// A zone is a circle or an axis-aligned rectangle, and it can be inverted. Inverted means
// "hide OUTSIDE this shape", which turns one zone into a containment ring: put a big
// inverted circle just inside the dial's border and nothing can encroach on it, replacing a
// border overlay, which is another layer above the movers and another 24%.
static constexpr int MAX_ZONES = 6;
struct Zone {
    int  x = 233, y = 233;
    int  r = 0;                 // circle radius, when rect is false
    int  w = 0, h = 0;          // full width/height centred on x,y, when rect is true
    bool rect   = false;
    bool invert = false;        // true = hide outside the shape instead of inside it
};

// The weather map's OWN look. Its own background, its own sweep, its own colours.
//
// It is a separate app that happens to be built the same way, and it was briefly sharing
// the Flight Tracker's sweep object outright, which meant it also wore the Flight Tracker's
// brass. Reuse the ENGINEERING, not the assets: the sweep here is a second object driven by
// the SAME timer, because the smoothness never came from sharing the object. It came from
// one timer that advances by real elapsed time and is never paused, and two objects can
// hang off that as easily as one when only ever one of them is visible.
//
// Defaults are deliberately NOT the Flight Tracker's green: a weather map that arrives
// looking like the aircraft scope is the cross-contamination this struct exists to end.
struct Weather {
    uint32_t bg              = 0x000000;
    bool     sweepEnabled    = true;
    bool     sweepTypeImage  = false;   // a theme's own sweep_wx.png, when it ships one
    uint32_t sweepColor      = 0x7FB2D9;   // a cool grey-blue, not the scope's phosphor
    uint32_t sweepLeadColor  = 0xDCEBF7;
    int      sweepTrailDeg   = 38;
    int      sweepOpacity    = 55;      // 0..100
    int      sweepLength     = 233;
    int      sweepSpeed      = 45;      // deg/sec
    int      sweepTrailWidth = 5;
    int      sweepLeadWidth  = 2;
    int      sweepTrailSteps = 20;
    // The rings and the road overlay, which the map draws for itself rather than borrowing.
    // ringColorOn is off by default and that is deliberate. These rings were drawn in
    // UI_GREEN, which is not a fixed colour at all: it is the built-in palette's accent, so
    // it differs between ORB, MILITARY and AVIATOR. There is no hex that could be this
    // field's default without silently restyling somebody's weather map the first time the
    // firmware actually started reading it, which it never had. Off means "carry on using
    // the palette accent"; a design that wants the weather map to stop borrowing the Flight
    // Tracker's phosphor turns it on and picks. Same shape as Intel's briefColorOn.
    uint32_t ringColor       = 0x1E3A2E;
    bool     ringColorOn     = false;
    bool     ringsEnabled    = true;
    // The same set the Flight Tracker's rings have, because they are the same idea on the
    // same dial and there was no reason for one to be adjustable and the other not. Defaults
    // reproduce the three fixed circles this screen drew before they were controls.
    int      ringCount       = 3;      // 1..5, spread evenly out to the rim
    int      ringWidth       = 1;      // px
    int      ringOpacity     = 180;    // 0..255
    bool     crosshair       = false;  // off by default: this screen has never had one
    uint32_t roadColor       = 0x4A4A4A;
    bool     roadsEnabled    = true;
    // The coastline, on its own switch and its own colour. Roads are worldwide now, but a
    // shoreline is what makes a place recognisable on a map this small: on the Florida
    // peninsula the roads alone read as scribble until the coast puts them somewhere.
    uint32_t coastColor      = 0x2B4A63;
    bool     coastEnabled    = true;

    // Four themeable lines, the same four the Flight Tracker has and the same struct, so a
    // control means the same thing on both screens. Until THEME_CAPS 28 this screen drew a
    // hard-coded temperature, a hard-coded age stamp and a hard-coded loading line, none of
    // which a design could move, colour, or switch off. Tokens are listed in weather_view.cpp
    // next to the table that fills them.
    TextSlot text[4];

    // The data source credit: where it sits and what it looks like, but never whether it
    // exists.
    //
    // UX-030 says a theme may restyle a credit and may not remove it. The first pass read
    // that as "leave it entirely alone", which is stricter than the rule and left a white
    // chip on a black pill sitting on top of every design that did not want one. Position,
    // colour, typeface and the pill behind it are all a theme's business. Being there is not.
    //
    // There is no `show`, on purpose: a switch that can be set wrong eventually is. Opacity
    // has a floor for the same reason, since "invisible" is how a removal would be spelled
    // if the field allowed it. bgOpa has NO floor, because a credit with no pill behind it
    // is still a credit.
    struct Credit {
        int      x       = 233;
        int      y       = 382;
        uint32_t color   = 0x9AA0A6;
        int      opa     = 255;   // clamped to CREDIT_MIN_OPA on the way in
        uint32_t bg      = 0x000000;
        // 0, the same as every other text control since THEME_CAPS 33: a text background
        // that starts clear. This was 170, which made the credit the one line on the device
        // that arrived wearing something nobody had asked for.
        int      bgOpa   = 0;
        int      radius  = 4;
        int      align   = 1;     // 0 left, 1 centre, 2 right
        // Curved, like every other text element on this device. ONLY WITHOUT THE PILL, and
        // that is a real constraint rather than a missing feature: the pill is a rounded
        // rectangle an LVGL label draws for itself, curved text is glyphs blitted onto a
        // canvas, and a rounded rectangle is not a shape that exists once the line bends.
        // The theme tool only offers the switch once the pill is off, and the firmware treats
        // curved as the winner if a hand-written theme asks for both.
        bool     curved  = false;
        int      curveR  = 180;   // px from the centre it orbits
        int      arcDeg  = 180;   // where round the dial it sits, 0 = twelve o'clock
    };
    static constexpr int CREDIT_MIN_OPA = 128;   // half. Below this it stops being a credit.
    Credit   credit;

    // Keep-out shapes for the map layers, exactly as the Flight Tracker has for aircraft.
    // Without these the rain and the coastline paint straight over whatever the background
    // art was doing, which is the whole reason the Flight Tracker got them first.
    Zone     zones[MAX_ZONES];
    int      zoneCount       = 0;
};

struct Radar {
    bool     sweepEnabled    = true;
    bool     sweepTypeImage  = false;  // new with this field — see RadarStatic above for why there's no compiled-macro fallback
    uint32_t sweepColor      = 0x39FF14;
    uint32_t sweepLeadColor  = 0xC8FFB0;
    int      sweepTrailDeg   = 38;
    int      sweepOpacity    = 60;    // 0..100
    int      sweepLength     = 233;
    int      sweepSpeed      = 45;
    // The trail's own line work, hard-coded until now (a 5 px trail of 20 steps behind a
    // 2 px leading edge). Defaults below are exactly those numbers, so a theme that does
    // not mention them looks the same as it always did.
    int      sweepTrailWidth = 5;     // px, thickness of each trail line
    int      sweepLeadWidth  = 2;     // px, thickness of the solid leading edge
    int      sweepTrailSteps = 20;    // how many lines the fading wedge is made of

    bool     blipEnabled     = true;  // new with this field — see RadarStatic above for why there's no compiled-macro fallback
    bool     blipTypeImage   = false;
    bool     blipRotate      = true;  // new with this field — see RadarStatic above for why there's no compiled-macro fallback
    bool     blipKiteShape   = false;
    int      blipSize        = 9;
    int      blipKiteT       = 0;     // 0..100
    bool     blipFixedColorMode = false;
    uint32_t blipFixedColor  = 0x39FF14;
    uint32_t blipAltGround   = 0x888888;
    uint32_t blipAltLow      = 0xFF5A3C;
    uint32_t blipAltMid      = 0xFFB23C;
    uint32_t blipAltHigh     = 0xC8FF3C;
    uint32_t blipAltCruise   = 0x39FF14;
    uint32_t blipAltJet      = 0x3CE0FF;
    int      blipGlow        = 0;
    uint32_t blipGlowColor   = 0xFFFFFF;
    bool     blipImageTint   = true;

    bool     selEnabled      = true;
    int      selStyle        = 0;  // 0=ring, 1=glow the aircraft, 2=recolor the aircraft — new with this field, see RadarStatic above for why there's no compiled-macro fallback
    uint32_t selColor        = 0xFF9D3C;
    int      selWidth        = 2;
    int      selDiameter     = 30;
    int      selGlow         = 0;
    uint32_t selGlowColor    = 0xFF9D3C;

    bool     offRangeEnabled = true;
    uint32_t offRangeColor   = 0xFF9D3C;
    int      offRangeSize    = 5;

    bool     centerEnabled     = true;  // new with this field — see RadarStatic above for why there's no compiled-macro fallback
    int      centerRadius      = 6;
    uint32_t centerColor       = 0xFF9D3C;
    int      centerInnerRadius = 2;
    uint32_t centerInnerColor  = 0x0B1F0F;

    RadarText rtext[4];
    RadarCard card;
    // The map the Orb carries: real OSM roads around wherever it is, drawn under the
    // scope's chrome. Always on and always the same grey until now, which a dark themed
    // dial had no way to argue with. Defaults are the colours it has always used.
    bool     mapRoadsOn      = true;
    uint32_t mapRoadColor    = 0x707868;
    int      mapRoadOpacity  = 150;   // 0..255
    bool     mapAirportsOn   = true;
    uint32_t mapAirportColor = 0x8A93A6;
    RadarStatic static1, static2;

    bool     overlayEnabled  = false;  // new with this field — see RadarStatic above for why there's no compiled-macro fallback
    uint32_t overlayColor    = 0x000000;
    int      overlayOpacity  = 0;      // 0..255, same convention as RadarStatic::opacity
    // Scope behaviour, not appearance — but theme data all the same, because these were
    // compile-time macros (CUSTOM_RADAR_MAXAC / MINALT / HIDEGROUND) baked in by a Launch
    // Kit firmware push. A theme installed as data alone, which is what the theme tool makes,
    // had no way to express them. -1 means "not specified": keep whatever the welded
    // default or the user's saved setting already chose.
    int      maxAircraft     = -1;     // how many contacts the scope follows at once
    int      minAltFt        = -1;     // ignore anything below this altitude
    // How far the rim is, in km. -1 means the theme has no opinion and the Orb keeps its
    // own zoom, the same sentinel minAltFt above uses. Launch Kit could set this only by
    // recompiling (CUSTOM_RADAR_RANGE_KM), so a files-only theme had no way to say it.
    float    rangeKm         = -1.0f;
    int      hideGround      = -1;     // 1 = never show aircraft on the ground, 0 = show, -1 = unset
    // A blind circle at the middle of the dial, in PIXELS, inside which no aircraft is
    // drawn. Sized in pixels rather than km because what it exists to clear is the
    // design's own centre artwork — a hub, a compass rose, a logo — and that artwork is a
    // fixed size on the glass whatever the range happens to be. Converted against the live
    // range at use (see deadZoneKm() in main.cpp), so zooming keeps it covering the same
    // ink. -1 is "no opinion", the same sentinel the four above use.
    //
    // Was CUSTOM_RADAR_DEADZONE_PX, and it was the one operational value with no
    // device-side control at all, so a files-only theme could neither state it nor work
    // around not being able to.
    int      deadZonePx      = -1;
    // Synthesised traffic instead of the live feed. For judging a design without waiting
    // on whatever happens to be overhead, and for watching masking behave against motion
    // that is predictable rather than whatever the sky is doing.
    bool     simulate        = false;

    // Rotation pivots for image-type sweeps and blips, in their own image's pixels.
    //
    // These were CUSTOM_SWEEP_IMAGE_PIVOT_* / CUSTOM_RADAR_BLIP_PIVOT_*, compiled in by
    // whichever theme firmware push ran last. A theme installed as files alone could
    // therefore ship a sweep sprite and have it spun around a point measured for somebody
    // else's artwork — which is not a subtle fault: a hand pivoting 40 px off its hub
    // wobbles instead of turning. -1 keeps the welded value, so an older theme is
    // unaffected.
    int      sweepPivotX     = -1;
    int      sweepPivotY     = -1;
    int      sweepCenterX    = -1;   // where on the dial that pivot sits
    int      sweepCenterY    = -1;
    int      blipPivotX      = -1;
    int      blipPivotY      = -1;

    // Exclusion zones: circles on the 466x466 dial where aircraft are not drawn.
    //
    // These exist so decorative artwork can live in the BAKED BACKGROUND instead of in a
    // layer above the aircraft. Measured 2026-08-17: Steam Punk's brass bezel, as a layer
    // above the movers, cost 24% of the radar's frame rate; the same art baked into the
    // background costs nothing at all. A zone gets the same visual result — aircraft never
    // cross the decoration — for a handful of comparisons per poll.
    //
    // Aircraft inside a zone vanish and reappear on the far side. They are hidden, not
    // dropped: a tracked contact keeps its slot while masked, or the scope would discard
    // it on entering and adopt a replacement, which is the churn sticky tracking removes.
    //
    // A zone is a circle or an axis-aligned rectangle, and it can be inverted. Inverted
    // means "hide OUTSIDE this shape", which turns one zone into a containment ring: put
    // a big inverted circle just inside the dial's border and aircraft can never encroach
    // on it, replacing a border overlay — another layer above the movers, another 24%.
    // The shape itself now lives at namespace scope (see Zone above the Weather struct), so
    // the Weather map can hide its own layers with the same geometry rather than a second
    // copy of it. `Radar::Zone` still resolves, because it is spelled that way in
    // radar_view.cpp and there is no reason to make that file move for this.
    using Zone = theme_style::Zone;
    static constexpr int MAX_ZONES = theme_style::MAX_ZONES;
    Zone     zones[MAX_ZONES];
    int      zoneCount       = 0;

    // Back-to-front draw order for the movable layers, same six kinds and the same
    // convention as CUSTOM_RADAR_LAYER_ORDER: 0=sweep, 1=aircraft, 2=text, 3=static1,
    // 4=static2, 5=colour wash.
    //
    // This was the last item still listed as compile-time only at the top of this file.
    // It mattered once a theme could be installed as files alone: a theme that
    // wants its sweep hand passing OVER the aircraft rather than under them had no way
    // to say so, and inherited whatever order the last firmware push happened to weld
    // in. orderN == 0 means "not specified", which keeps exactly that welded order, so
    // every theme made before this field is unaffected.
    int      order[6]        = { 0, 1, 2, 3, 4, 5 };
    int      orderN          = 0;

    // This theme ships radar_rings.png and its background plate therefore has no rings
    // baked in. Purely a declaration to check against THEME_CAPS: the
    // firmware draws whatever asset it finds either way, but an Orb that cannot draw it
    // must refuse the theme rather than show a dial with no grid on it.
    bool     ringsPlate      = false;

    // How heavy the roads are, in pixels. Welded at 1 until THEME_CAPS 7.
    int      mapRoadWidth    = 1;

    // The sweep's hub: the disc at the point the hand turns about. Drawn as part of the
    // sweep layer, so it travels with the hand through the stack rather than sitting at a
    // fixed depth. Off by default, which is what every theme made before this looked like.
    bool     sweepHubOn      = false;
    uint32_t sweepHubColor   = 0x39FF8A;
    int      sweepHubRadius  = 6;
    int      sweepHubGlow    = 0;      // px of halo beyond the disc, 0 = none
    uint32_t sweepHubGlowColor = 0x39FF8A;
};

// One line of text on the splash / About screen.
//
// Deliberately NOT ClockText: that struct leads with `show`, and these three lines do not
// have one. See Splash below for why. It also carries no size, because the clock's text is
// drawn at a size the theme's baked font already fixed, and these are drawn from the
// compiled ladder instead.
struct SplashText {
    int      x         = 233;
    int      y         = 233;
    // From the compiled ladder in lv_conf.h only. LVGL fonts are glyph bitmaps, not
    // outlines, so a size the binary was not built with cannot be drawn at any quality.
    int      size      = 14;
    uint32_t color     = 0xFFFFFF;
    int      opa    = 255;   // 0..255, see THEME_CAPS 18
    int      glow      = 0;
    uint32_t glowColor = 0xFFFFFF;
    int      align     = 1;      // 0 left, 1 center, 2 right
    // The plate behind the words. THEME_CAPS 33: every text control in the theme tool now offers
    // the same two choices, a background or a curve, because offering a different set of
    // controls on each card taught nobody anything except that the cards were written on
    // different days. bgOpa defaults to 0, so a theme that never asked for one is unchanged.
    uint32_t bg     = 0x000000;
    int      bgOpa  = 0;     // 0..255, 0 = no plate at all
    int      radius = 4;     // corner rounding, px
    bool     curved    = false;
    int      curveR    = 0;
    float    arcDeg    = 0.0f;
    // THEME_CAPS 36. Read for the config address and the theme line only; the version and
    // the credits never parse it (see the items table in theme_style.cpp), so a theme
    // cannot switch those off, which UX-028 and UX-030 require. Defaults on.
    bool     show      = true;
};

// The splash, which is also the About page.
//
// Three lines here are not the theme's to delete. The firmware version has to stay readable
// off the device, so that a tool can ask which build you are on before it writes a
// design. The map credit is not courtesy either: the roads and coastlines baked into this
// firmware are OpenStreetMap data under ODbL, and that licence requires the attribution to
// appear. So a theme gets to say where these sit, how big they are, what colour, whether
// they curve. It does not get a switch that turns them off, because a switch that quietly
// breaks a licence is not a feature.
//
// `styled` is the migration flag, and it means one specific thing: this theme shipped a
// splash_style.json, therefore its splash.png was built by a tool that knows to leave the
// glass OUT of the bake. Older themes have no such file, their splash.png already has the
// glass painted in, and drawing the overlay over them again would show it twice. Absent
// means "behave exactly as before", which is the same promise hasAsset() makes.
struct Splash {
    bool styled = false;
    // These reproduce, exactly, the offsets and colours settings_view.cpp used to hardcode:
    // CENTER +120 / +152 / +186 on a 466 px screen is y 353 / 385 / 419, in the Settings
    // palette's ink and soft. A theme that says nothing about the splash has to look
    // identical to the one that shipped before this existed, or the level is a redesign
    // wearing a feature's clothes.
    // Designated rather than positional. These were positional, and adding `opa` to
    // SplashText silently slid every value one place along: arcDeg's 0.0f landed on an int
    // and only a narrowing warning caught it. The next field added would not have been so
    // lucky, and a colour quietly becoming an alignment is not a compiler's problem.
    SplashText version{ .x = 233, .y = 353, .size = 14, .color = 0xFFFFFF, .opa = 255 };
    SplashText network{ .x = 233, .y = 385, .size = 14, .color = 0x6A7078, .opa = 255 };
    SplashText credits{ .x = 233, .y = 419, .size = 12, .color = 0x6A7078, .opa = 255 };
    // The theme's name and who made it, THEME_CAPS 35. UX-028 lists this beside the version
    // and the credits as what the splash must carry, and until now it carried neither. One
    // rung above the version in the same 32 px rhythm the other three keep, in the credits'
    // ink. Where it lands on a given theme's art is the designer's to settle, like the rest.
    SplashText theme{ .x = 233, .y = 321, .size = 12, .color = 0x6A7078, .opa = 255 };
};

// The Headlines screen. The background can be a colour or a picture, and the picture
// arrives the same way every other screen's does: intel_plate.png, decoded by
// plate_sprite.cpp, tried in flash before the card. This was colour-only until THEME_CAPS
// 14 for exactly the reason the charter's P6 gives — the decode path did not exist, and a
// picker in the theme tool that installed a setting the device ignored would have been worse than
// no picker. The pipeline exists now, so the control does too.
struct Intel {
    uint32_t bg          = 0x000000;
    uint32_t titleColor  = 0x7E8794;
    int      titleOpa    = 255;   // 0..255, see THEME_CAPS 18
    uint32_t textColor   = 0xE8ECF1;
    int      textOpa     = 255;
    uint32_t sourceColor = 0x5F6874;
    int      sourceOpa   = 255;
    uint32_t staleColor  = 0xC8922E;
    // ---- browsing: which headline the knob is on -------------------------------------
    //
    // None of this shows on a resting screen, which is the promise every capability level
    // here keeps: turning the knob is what makes a selection appear, and six seconds of
    // stillness takes it away again. So a theme built before this existed looks exactly as
    // it did until somebody reaches for the knob.
    //
    // The default marks the selection by DIMMING the others rather than by colouring the
    // one, because that is the only choice that works without knowing the theme's palette.
    // A hardcoded white selection is invisible on a pale design and a hardcoded bar colour
    // is wrong on half of them; fading what you are not reading is right on all of them.
    // 150 rather than the 110 this started at: a neighbour should read as quieter, not as
    // nearly gone. Checked on a screenshot rather than guessed, which is how 110 was caught.
    int      selDim      = 150;   // 0..255: what unselected headlines fade to while browsing
    bool     selColorOn  = false; // give the selected headline a colour of its own
    uint32_t selColor    = 0xFFFFFF;
    // The bar is ON by default, because a highlight band is what a scrolling list looks like
    // everywhere else including this Orb's own Settings wheel, and it is what was asked for.
    // Quiet enough at 26/255 to sit under a dark theme's text as well as a light one's: it is
    // the theme's own text colour, so it always contrasts with the background it is on.
    bool     selBarOn    = true;
    uint32_t selBarColor = 0xE8ECF1;
    int      selBarOpa   = 26;
    int      selBarRadius = 12;
    int      selBarPadX  = 10;
    int      selBarPadY  = 6;
    // ---- the briefing: press a headline to read the story's own summary ---------------
    //
    // No font slots of its own, on purpose. The heading draws in the headline face and the
    // body in the source-credit face, both of which are already installed and both of which
    // are shipped with their full glyph range (only the title is subsetted, because only
    // the title's words are known in advance). A brief is arbitrary feed text, so a
    // subsetted face would draw holes in it, and a fourth face would be another ~30 KB of
    // install for a screen most designs will never restyle.
    bool     briefColorOn = false;  // false: follow the headline colour
    uint32_t briefColor   = 0xE8ECF1;
    int      briefOpa     = 255;
    int      briefGap     = 18;     // between the heading and the body
    // THEME_CAPS 49. The body's own size from the compiled ladder, read only when the theme
    // shipped no font_intel_brief.bin; 0 means the credit's face and size, which is what
    // every theme before this level gets.
    int      briefSize    = 0;
    // THEME_CAPS 51. Take the title down while a story is open, so the band can hold just
    // the headline and its story. Its own key rather than a rule, because a title that
    // names the feed ("BBC News") is worth keeping on some designs and clutter on others.
    bool     briefHideTitle = false;
    // THEME_CAPS 50. Where the marks go. Each pair is read only when its *Place is true;
    // false is the worked-out spot: the list's chevron under the last row shown, the Back
    // button at the foot of the band, the story's chevron between the story and the button.
    // Screen coordinates, the centre of the thing, like every other placed element here.
    bool     morePlace      = false;
    int      moreX          = 233;
    int      moreY          = 400;
    bool     backPlace      = false;
    int      backX          = 233;
    int      backY          = 370;
    bool     briefMorePlace = false;
    int      briefMoreX     = 233;
    int      briefMoreY     = 340;
    // How many headlines to FETCH, 1..INTEL_MAX_ITEMS (20). Not the same question as how
    // many are on screen: the surplus is what the knob scrolls through.
    int      count       = 3;
    // How many to SHOW at once. 0 means "as many as fit", which is what this screen did
    // before the two numbers were separable and is still the right answer for most
    // designs. Above 0 it is a ceiling, not a promise: a count that cannot fit the dial
    // at the chosen size is still reduced to what actually fits, because the alternative
    // is drawing text off the edge of the glass.
    int      onScreen    = 0;
    // Both plain lowercase words the gateway itself defines (see INTEL_FEEDS,
    // buildtheorb/app/src/server.ts) — general/world/sports/science/tech/business/space,
    // and bbc/guardian. A topic with no feed for the requested source (space has no BBC or
    // Guardian feed) is not an error: the gateway substitutes what that topic actually has
    // and says so in its own log, same graceful-fallback shape as every other data source
    // this Orb reads.
    char     topic[16]   = "general";
    char     source[16]  = "bbc";
    // The wrap boundary each headline row is laid out inside. false (square) is the
    // original behaviour: every row gets the same fixed width regardless of how close it
    // sits to the top or bottom of the dial. true (curved) computes each row's width as
    // the chord of curveRadius at that row's height, so rows nearer the middle stay wide
    // and rows nearer the edge narrow to match the glass actually under them.
    bool     curvedBounds = false;
    // The radius, in px, curved mode measures its chord against. 233 is the screen's own
    // true radius, which makes the text boundary hug the real bezel. Smaller pulls the
    // margins in tighter than the bezel actually requires, for a more dramatic taper;
    // larger relaxes it, approaching square mode's straight sides as it grows. Clamped to
    // [140, 400] on the way in: below 140 an outer row's chord can hit zero or go
    // imaginary, and above 400 the curve is imperceptible within the rows' actual height
    // range, so it stops being worth the field.
    int      curveRadius  = 233;

    // THEME_CAPS 11 below here. Every default reproduces the fixed layout this screen
    // shipped with, byte for byte, so a theme that never touches these looks identical.
    //
    // The title as its own text element. ASCII only (the theme tool strips the rest on export):
    // Montserrat's compiled glyph set is the same one that already forces the gateway to
    // send ASCII headlines. Coordinates are absolute screen px (0..466), the convention
    // every themed text field uses; the view subtracts the centre itself.
    char     title[24]    = "INTEL";
    bool     titleShow    = true;
    int      titleSize    = 14;    // one of the compiled Montserrat sizes: 12/14/16/18/20/28
    int      titleX       = 233;
    int      titleY       = 65;    // 233 - 168, the fixed layout's exact spot
    // Headline type size. 0 means automatic, which is the original behaviour: 16 px for
    // three or fewer, 14 px for four or five. Any other value must be a compiled size
    // (see FONT_SIZES in theme_style.cpp, which mirrors lv_conf.h exactly); the parser
    // snaps unknown values back to 0 rather than handing LVGL a font that was never
    // linked in. Sizes big enough to overflow the dial are what scrolling is for.
    int      textSize     = 0;
    // The text block's own margins, px in from each edge. 68 each side is exactly the
    // fixed layout's 330 px column. Asymmetric margins move the block as well as size it,
    // which is the point: "where I want to put it" and "how much room it takes up" are
    // the same two numbers. The curved boundary, when on, intersects with this box.
    int      marginLeft   = 68;
    int      marginRight  = 68;
    // The band the headline block is allowed to occupy, px in from the top and bottom of
    // the dial. 0 means "work it out", which keeps the old behaviour: the block is bounded
    // by the title above and the age line below. Above 0 these win, so a design can hold
    // the headlines clear of artwork the automatic bounds know nothing about.
    int      marginTop    = 0;
    int      marginBottom = 0;
    // Minutes between fetches. 10 is what INTEL_POLL_MS welded in before this field.
    // Clamped to [5, 120]: the feeds themselves refresh on the order of minutes, so
    // anything faster than 5 is pure gateway traffic for identical bytes.
    int      pollMinutes  = 10;
    // Move the whole headline block up or down, px, without touching the title or the age
    // line. An offset rather than an absolute Y on purpose: the block's own position is
    // computed (centred between the title and the age line, or filled from the top when it
    // overflows), and an absolute coordinate would throw that arithmetic away and have to
    // be re-tuned every time the count or the type size changed. 0 is dead centre of
    // whatever the layout worked out, so an untouched theme is unmoved. Clamped [-160, 160].
    int      blockOffsetY = 0;
    // Extra leading between the two lines of one headline, px. LVGL's own line height is
    // tight by design (16 px of face gets 18 px of line), which is right for a paragraph
    // and cramped for two lines read across a room. 0 keeps exactly what shipped before.
    // Clamped [0, 24].
    int      lineGap      = 0;
    // Space between a headline and the source credit under it, px, on top of the fonts'
    // own metrics. Was a hardcoded 2 in three places in intel_view.cpp, which is fine for a
    // compact sans and far too tight for a display serif: at 24 px Playfair Display the
    // descenders of g and y ran straight into the credit line. Whether 2 px is enough is a
    // question about the TYPEFACE, so it cannot be a constant. 2 is the default, so a theme
    // that says nothing is laid out exactly as before. Clamped [0, 24].
    int      sourceGap    = 2;
    // THEME_CAPS 20. Which edge a headline and its credit line up on: 0 left, 1 centre,
    // 2 right. Both default to centre, which is what this screen drew from the beginning.
    // They are separate settings because a credit set flush right under a left-aligned
    // headline is an ordinary thing to want and tying them together would forbid it. The
    // JSON carries "left" | "center" | "right"; ALIGN_* below is the parsed form, and the
    // values line up with nothing in LVGL on purpose, since the view maps them itself.
    static constexpr int ALIGN_LEFT   = 0;
    static constexpr int ALIGN_CENTER = 1;
    static constexpr int ALIGN_RIGHT  = 2;
    int      textAlign    = ALIGN_CENTER;
    int      sourceAlign  = ALIGN_CENTER;
    // The angle the whole headline block lies at, in whole degrees, positive clockwise.
    // 0 is square to the screen and is what this screen has always drawn. It exists for
    // background art with an edge in it: a drawn sheet of paper at 7 degrees wants its type
    // at 7 degrees, and every other way of getting there means baking the words into the
    // picture, which cannot be done with text that arrives from a feed.
    //
    // The headlines, their credits and the selection bar rotate together, as one block.
    // The title and the updated line do not: they have positions of their own, the same
    // way blockOffsetY moves the headlines and leaves them alone.
    //
    // Costs nothing at 0. LVGL only builds a transform layer for an object whose angle is
    // non-zero, so every theme that never touches this draws exactly as it did. A rotated
    // one spends roughly 430 KB of PSRAM per redraw on that layer, which this board has
    // (the menu overlay holds about twice it) and which is why the range is clamped rather
    // than free: [-90, 90]. Past 90 the block reads upside down, and paying for a layer to
    // draw unreadable text is not a trade worth offering.
    int      blockAngle   = 0;
    // The "just now" age line, a full text field like every other one in the theme tool: its own
    // colour, compiled type size, position and glow. ageColor's default is the exact grey
    // the line borrowed from sourceColor before it had a colour of its own, so an untouched
    // theme is unchanged. It still switches to staleColor when the headlines go stale —
    // that flip is the line's whole reason to exist and no colour choice removes it.
    bool     ageShow      = true;
    int      ageX         = 233;
    int      ageY         = 409;   // 233 + 176, the fixed layout's exact spot
    // What the age line SAYS, with {t} standing in for the phrase the Orb works out
    // ("just now", "5 min ago", "2 hr ago"). The default is the bare token, which is
    // exactly what this line has always printed. A theme can write "Last updated: {t}" or
    // "{t}, last checked" and the words are its own. Any text outside the token is
    // reproduced verbatim; a format with no token at all is a fixed caption, which is
    // allowed on the grounds that someone may genuinely want one.
    // The credit under each headline ("BBC", "NASA"). It had no size of its own and was
    // welded to 12 px, which is fine beside 16 px text and invisible beside 40 px.
    int      sourceSize   = 12;
    char     ageFmt[40]   = "{t}";
    uint32_t ageColor     = 0x5F6874;
    int      ageOpa       = 255;   // 0..255, see THEME_CAPS 18
    int      ageSize      = 12;    // one of the compiled Montserrat sizes
    int      ageGlow      = 0;     // px of halo, 0 = none, clamped to [0, 20]
    uint32_t ageGlowColor = 0x5F6874;
    // Bend the line along an arc, the way the clock's banners and the scope's readouts can.
    // When set, ageX/ageY stop being read: an arc is placed by its radius and the clock
    // angle it is centred on, not by a corner. 176/180 puts it exactly where the straight
    // line sat by default, at the bottom of the dial, so switching it on moves nothing
    // until the radius is changed.
    bool     ageCurved    = false;
    // The plate behind it, THEME_CAPS 33, on the same control every other line of text has.
    uint32_t ageBg     = 0x000000;
    int      ageBgOpa  = 0;    // 0..255, 0 = none
    int      ageRadius = 4;
    int      ageCurveR    = 176;    // px from the dial centre
    float    ageArcDeg    = 180.0f; // clock angle the text is centred on; 180 = six o'clock
};

// Reads /themes/<slug>/{clock,radar,settings,menu,intel}_style.json (theme_select::activeSlug())
// and populates the runtime structs below, field by field — any file that's missing, or
// any field a file doesn't set, keeps the CUSTOM_* compile-time default (so a theme
// exported before this module existed, or a stock/no-design build, behaves exactly as
// before). Call once at boot, right after theme_select::init() resolves the active slug
// (theme_select::set() always triggers a real reboot/re-exec, so init() — and this —
// naturally reruns on every theme switch too; no live-reload path needed).
void load();

const Clock    &clock();
const Radar     &radar();
const Weather   &weather();
const Ticker    &ticker();
const Intel     &intel();
const Splash    &splash();
const Apps      &apps();      // from /themes/<slug>/theme.json
const Names     &names();     // display labels; see the Names comment on why these are not ids

// The theme's display name, falling back to its slug when it has none. Use this anywhere
// a person reads it (Settings > Design, /health), never the raw slug.
const char *themeLabel();
const char *themeAuthor();    // theme.json "author", or "" when the file makes no claim

// The display name for any installed theme, not just the active one — Settings > Design
// lists them all. Falls back to the slug when a theme declares no name. Reads that
// theme's theme.json, so call it when a page opens, not per frame.
void labelFor(const char *slug, char *out, size_t cap);

// Does the active theme actually contain this asset, e.g. "menu_plate.png"?
//
// theme.json carries an "assets" list of every image the theme ships. Launch Kit rebuilds
// it from what is genuinely on disk at push time, so a layer it decided not to ship (a
// fully transparent overlay, say) is absent from the list as well as from the folder.
//
// This exists because pushing a theme never deletes anything from the card: files from
// older pushes just sit there, and the firmware kept finding them, decoding them, baking
// them into flash and drawing them. Measured on the Steam Punk card: two empty overlay
// layers nobody had shipped in months, costing 1.3 MB of flash to draw nothing.
//
// A theme whose theme.json has no "assets" list answers true for everything, so older
// themes already on a card behave exactly as before.
bool hasAsset(const char *name);

// The theme's font map: which file each text slot loads (theme.json "fonts":
// {"radar2": "font_body16.bin", ...}), THEME_CAPS 52. load() fills it when the card has the
// theme and empties it first; theme_font fills it from the flash blob when the card does not.
// One table, so nothing holds a second copy.
theme_font::FontMap &fontMap();

// Colour roles, THEME_CAPS 53. Legacy: the theme has no palette, so every colour option is its compiled default;
// palette() is this built-in palette, the one app_theme::palette() reads when no theme (or a theme with no
// palette) is active. Theme: theme.json has a palette. BuiltIn: no theme is active. In the last two, a "$role"
// string in a style file reads as that colour.
enum class PaletteMode : uint8_t { Legacy, Theme, BuiltIn };
PaletteMode paletteMode();
inline bool paletteOn() { return paletteMode() != PaletteMode::Legacy; }
bool roleDefaults();                      // false when theme.json says "roleDefaults": false
const theme_roles::Palette &palette();

// A hash of the declared asset list, or 0 when the theme declares none. theme_art stores
// this alongside a bake and re-bakes whenever it changes, so editing a theme's layers is
// picked up on the next boot without anyone remembering to invalidate a cache.
uint32_t assetsFingerprint();

} // namespace theme_style

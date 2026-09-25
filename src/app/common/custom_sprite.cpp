#include "custom_sprite.h"
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <chrono>
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } } Serial;
static uint32_t millis() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
#endif
#include "pixel_slot.h"      // one pointer, size and "tried" flag per asset, and the rule that a release resets them all
#include "plate_sprite.h"    // the loader: pre-baked flash first, then the card, then nothing

// The theme's clock layers and the wind screen's pictures, as raw pixel buffers the clock compositor blits and
// rotates. There is no compiled fallback: a theme that ships no file for a layer draws without that layer (the
// built-in look draws the missing pieces from the palette instead).
namespace {

// 0=hour,1=minute,2=second,3=static1,4=static2 - the two statics share this exact same slot machinery, just
// always drawn at angle 0 (see clock_view.cpp). Slots 0-4 are the hand and static layers the draw order names.
// Slots 5-7 are the three hands' SHADOW silhouettes, which share every bit of it but are deliberately not
// addressable from the order list: a shadow is not a layer anyone arranges, it is drawn with the hand it belongs to.
constexpr int SLOTS = 8;

PixelSlot g_plate, g_overlay, g_splashOv;
PixelSlot g_windBg, g_windCrank, g_windCrankSh;
PixelSlot g_hand[SLOTS];

const uint8_t *fetch(PixelSlot &s, const char *asset, bool alpha, const char *tag) {
    return pixel_slot_get(s, alpha ? 3 : 2, [&](int &w, int &h) {
        return plate_sprite::load_pixels(asset, alpha, w, h, tag);
    });
}

}  // namespace

const uint16_t *custom_plate() {
    // Pre-baked in flash? Then there is nothing to do at all: no 226 ms card read, no 205 ms decode, no 424 KB of
    // PSRAM. See theme_art.h.
    return (const uint16_t *)fetch(g_plate, "clock_plate.png", false, "custom_sprite plate");
}

const uint8_t *custom_overlay() {
    return fetch(g_overlay, "clock_overlay.png", true, "custom_sprite overlay");
}

// The splash's own glass, and the reason it is not just custom_overlay().
//
// clock_overlay.png is the one overlay the theme tool bakes the HUB into - the pivot cap the
// hands appear to turn on. The splash has no hands, so borrowing that file put a white dot in
// the middle of the startup screen and of Settings > About, on every theme that ships
// splash_style.json. The other screens' overlays (menu_overlay.png, settings_overlay.png and
// radar_overlay.png) are the same glass and CRT without the hub, for the screens that have no
// hands. The splash was simply never given one, and the firmware reached for the clock's
// instead.
//
// So: splash_overlay.png, or nothing. NO fallback to clock_overlay.png, because falling
// back to it is the entire bug. A theme baked before the theme tool exports this file gets no glass
// on its splash, which is the right way round - a missing layer is a plainer screen, a
// wrong layer is a dot nobody can explain.
const uint8_t *splash_overlay() {
    return fetch(g_splashOv, "splash_overlay.png", true, "custom_sprite splash overlay");
}

// The wind screen's picture and its crank. Same shape as every other asset here: flash first
// where the bake put one, then the card, then nothing at all. Nothing is a plainer screen,
// which is always the right way to be missing a layer.
//
// OPAQUE, not alpha. The wind screen covers the clock completely, so this is a straight blit
// rather than a per-pixel blend, and it is the difference between redrawing a region and
// compositing one. Decoded without alpha for the same reason the clock's plate is.
const uint16_t *wind_background(int &w, int &h) {
    const uint8_t *p = fetch(g_windBg, "wind_bg.png", false, "custom_sprite wind background");
    w = g_windBg.w; h = g_windBg.h;
    return (const uint16_t *)p;
}

CustomSprite wind_crank() {
    const uint8_t *p = fetch(g_windCrank, "wind_crank.png", true, "custom_sprite wind crank");
    return { p, g_windCrank.w, g_windCrank.h };
}

// Baked to the crank's own size and pivot, so both are placed by the same arithmetic and
// turned by the same angle. Only the screen-space offset separates them.
CustomSprite wind_crank_shadow() {
    const uint8_t *p = fetch(g_windCrankSh, "wind_crank_shadow.png", true, "custom_sprite wind crank shadow");
    return { p, g_windCrankSh.w, g_windCrankSh.h };
}

CustomSprite custom_hand(int kind) {
    if (kind < 0 || kind >= SLOTS) return { nullptr, 0, 0 };
    // From the card, same contract as plate/overlay. Hands were the last visual element that could not travel per
    // theme, which is why a theme switch used to leave the previous theme's hands on the new clock face.
    static const char *sdName[SLOTS] = {
        "clock_hand_hour.png",   "clock_hand_minute.png", "clock_hand_second.png",
        "clock_static1.png",     "clock_static2.png",
        // Theme-only: there is no compiled-in fallback for a shadow, and there should not be. A firmware that
        // shipped its own would put a shadow under a theme that never asked for one.
        "clock_shadow_hour.png", "clock_shadow_minute.png", "clock_shadow_second.png",
    };
    const uint8_t *p = fetch(g_hand[kind], sdName[kind], true, "custom_sprite hand");
    return { p, g_hand[kind].w, g_hand[kind].h };
}

// The pre-blurred, pre-coloured silhouette a hand casts, or {nullptr,0,0} if the theme ships none.
CustomSprite custom_shadow(int hand) {
    if (hand < 0 || hand > 2) return { nullptr, 0, 0 };
    return custom_hand(hand + 5);
}

// Drop every decoded PSRAM buffer and forget that it was asked for, so the next call to custom_plate(),
// custom_overlay(), custom_hand() and the rest loads again. Flash-resident pixels are memory-mapped, not
// allocated: their reference is dropped rather than freed. Called when the custom clock face is no longer the
// app on screen, so a design's ~1 MB of decoded pixels isn't held resident while some other app is in front.
//
// EVERY slot is reset, the three hand shadows included. Only the first five were, and a shadow that had been
// released was then never loaded again until a reboot (see pixel_slot.h).
void custom_sprite_release() {
    const uint32_t t0 = millis();
    size_t freed = 0;
    PixelSlot *layers[] = { &g_plate, &g_overlay, &g_splashOv, &g_windBg, &g_windCrank, &g_windCrankSh };
    for (PixelSlot *s : layers) freed += pixel_slot_drop(*s, plate_sprite::release_pixels);
    for (PixelSlot &s : g_hand)  freed += pixel_slot_drop(s, plate_sprite::release_pixels);
    if (freed) Serial.printf("[custom_sprite] released ~%u KB of decoded PSRAM in %u ms\n", (unsigned)(freed / 1024), (unsigned)(millis() - t0));
}

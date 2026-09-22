#include "custom_sprite.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <chrono>
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } } Serial;
static void heap_caps_free(void *p) { free(p); }
static uint32_t millis() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#endif
#include "theme_style.h"   // hasAsset() — ignore files the theme does not declare
#include "png_decode.h"
#include <string.h>
#include "config.h"   // SCREEN_W / SCREEN_H — the fixed plate/overlay canvas size
#include "theme_sd.h"   // theme_sd::read_whole/free — SD-hosted plate/overlay, one rung above flash
#include "theme_select.h"   // theme_select::activeSlug() — which /themes/<slug>/ folder to read from
#include "theme_art.h"      // pre-baked RGB565 in flash — tried before the card, costs nothing

namespace {

// PNG bytes -> a fresh PSRAM buffer, in the shared decoder (png_decode.h). `out` is left
// null on failure, so a bad PNG can never be mistaken for a loaded one.
bool decode(const uint8_t *png, uint32_t len, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    out = png_decode::to_buffer(png, len, alpha ? png_decode::FMT_RGB565_ALPHA : png_decode::FMT_RGB565, w, h, "custom_sprite", tag);
    return out != nullptr;
}

// The active theme's asset, read from the card and decoded into PSRAM. There is no compiled fallback: a theme that ships
// no file for a layer draws without that layer (the built-in look draws the missing pieces from the palette instead).
constexpr size_t SD_ASSET_MAX_BYTES = 2 * 1024 * 1024;   // a 466x466 plate/overlay PNG is never remotely this big
bool decode_from_sd(const char *assetName, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    const char *slug = theme_select::activeSlug();
    // Only read what the theme says it ships. A push never deletes from the card, so
    // files from older pushes linger; trusting them meant decoding and drawing layers
    // the theme had already dropped. See theme_style::hasAsset().
    if (slug[0] && theme_style::hasAsset(assetName)) {
        char path[64];
        snprintf(path, sizeof(path), "/themes/%s/%s", slug, assetName);
        size_t sdLen = 0;
        uint8_t *sdBuf = theme_sd::read_whole(path, sdLen, SD_ASSET_MAX_BYTES);
        if (sdBuf) {
            const bool ok = decode(sdBuf, (uint32_t)sdLen, alpha, out, w, h, tag);
            theme_sd::free(sdBuf);
            if (ok) { Serial.printf("[custom_sprite] %s: source SD %s\n", tag, path); return true; }
            Serial.printf("[custom_sprite] %s: SD file %s read (%u B) but DECODE FAILED\n",
                          tag, path, (unsigned)sdLen);
        } else {
            Serial.printf("[custom_sprite] %s: no SD file at %s\n", tag, path);
        }
    } else if (!slug[0]) {
        Serial.printf("[custom_sprite] %s: no active theme slug, skipping SD\n", tag);
    } else {
        // Two causes, two messages. This branch used to say "no active theme slug" for
        // both, and the wrong one sent an hour down the wrong path on 2026-09-01: the slug
        // was fine, the theme's asset list simply did not name the file.
        Serial.printf("[custom_sprite] %s: theme does not declare %s, skipping SD\n", tag, assetName);
    }
    return false;
}

uint16_t *s_plate = nullptr;   bool s_plateTried = false;
uint8_t  *s_overlay = nullptr; bool s_overlayTried = false;
uint8_t  *s_splashOv = nullptr; bool s_splashOvTried = false;
// 0=hour,1=minute,2=second,3=static1,4=static2 — the two statics share this exact
// same slot/decode machinery, just always drawn at angle 0 (see clock_view.cpp).
// Slots 0-4 are the hand and static layers the draw order names. Slots 5-7 are the three
// hands' SHADOW silhouettes, which share every bit of this decode/flash-resident machinery
// but are deliberately not addressable from the order list: a shadow is not a layer anyone
// arranges, it is drawn with the hand it belongs to.
constexpr int SLOTS = 8;
uint8_t  *s_hand[SLOTS] = { nullptr };
int       s_handW[SLOTS] = { 0 }, s_handH[SLOTS] = { 0 };
bool      s_handTried[SLOTS] = { false };

} // namespace

const uint16_t *custom_plate() {
    if (!s_plate && !s_plateTried) {
        s_plateTried = true;
        int w = 0, h = 0;
        // Pre-baked in flash? Then there is nothing to do at all: no 226 ms card read,
        // no 205 ms decode, no 424 KB of PSRAM. See theme_art.h.
        if (const uint8_t *p = theme_art::find_active("clock_plate.png", theme_art::FMT_RGB565, w, h)) {
            s_plate = (uint16_t *)p;
            Serial.printf("[custom_sprite] plate: flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", w, h);
            return s_plate;
        }
        uint8_t *o = nullptr;
        if (decode_from_sd("clock_plate.png", false, o, w, h, "plate")) s_plate = (uint16_t *)o;
    }
    return s_plate;
}

const uint8_t *custom_overlay() {
    if (!s_overlay && !s_overlayTried) {
        s_overlayTried = true;
        int w = 0, h = 0;
        if (const uint8_t *p = theme_art::find_active("clock_overlay.png", theme_art::FMT_RGB565_ALPHA, w, h)) {
            s_overlay = (uint8_t *)p;
            Serial.printf("[custom_sprite] overlay: flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", w, h);
            return s_overlay;
        }
        uint8_t *o = nullptr;
        if (decode_from_sd("clock_overlay.png", true, o, w, h, "overlay")) s_overlay = o;
    }
    return s_overlay;
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
    if (!s_splashOv && !s_splashOvTried) {
        s_splashOvTried = true;
        int w = 0, h = 0;
        if (const uint8_t *p = theme_art::find_active("splash_overlay.png", theme_art::FMT_RGB565_ALPHA, w, h)) {
            s_splashOv = (uint8_t *)p;
            Serial.printf("[custom_sprite] splash overlay: flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", w, h);
            return s_splashOv;
        }
        uint8_t *o = nullptr;
        if (decode_from_sd("splash_overlay.png", true, o, w, h, "splash overlay")) s_splashOv = o;
    }
    return s_splashOv;
}

// The wind screen's picture and its crank. Same shape as every other asset here: flash first
// where the bake put one, then the card, then nothing at all. Nothing is a plainer screen,
// which is always the right way to be missing a layer.
namespace {
uint16_t *s_windBg = nullptr;   int s_windBgW = 0,    s_windBgH = 0;    bool s_windBgTried = false;
uint8_t *s_windCrank = nullptr; int s_windCrankW = 0, s_windCrankH = 0; bool s_windCrankTried = false;
uint8_t *s_windCrankSh = nullptr; int s_windCrankShW = 0, s_windCrankShH = 0; bool s_windCrankShTried = false;

CustomSprite load_wind(const char *name, uint8_t *&buf, int &w, int &h, bool &tried, const char *tag) {
    if (!buf && !tried) {
        tried = true;
        int fw = 0, fh = 0;
        if (const uint8_t *p = theme_art::find_active(name, theme_art::FMT_RGB565_ALPHA, fw, fh)) {
            buf = (uint8_t *)p; w = fw; h = fh;
        } else {
            uint8_t *o = nullptr;
            if (decode_from_sd(name, true, o, fw, fh, tag)) { buf = o; w = fw; h = fh; }
        }
    }
    return { buf, w, h };
}
}  // namespace

// OPAQUE, not alpha. The wind screen covers the clock completely, so this is a straight blit
// rather than a per-pixel blend, and it is the difference between redrawing a region and
// compositing one. Decoded without alpha for the same reason the clock's plate is.
const uint16_t *wind_background(int &w, int &h) {
    if (!s_windBg && !s_windBgTried) {
        s_windBgTried = true;
        int fw = 0, fh = 0;
        if (const uint8_t *p = theme_art::find_active("wind_bg.png", theme_art::FMT_RGB565, fw, fh)) {
            s_windBg = (uint16_t *)p; s_windBgW = fw; s_windBgH = fh;
        } else {
            uint8_t *o = nullptr;
            if (decode_from_sd("wind_bg.png", false, o, fw, fh, "wind background")) {
                s_windBg = (uint16_t *)o; s_windBgW = fw; s_windBgH = fh;
            }
        }
    }
    w = s_windBgW; h = s_windBgH;
    return s_windBg;
}

CustomSprite wind_crank() {
    return load_wind("wind_crank.png", s_windCrank, s_windCrankW, s_windCrankH, s_windCrankTried, "wind crank");
}

// Baked to the crank's own size and pivot, so both are placed by the same arithmetic and
// turned by the same angle. Only the screen-space offset separates them.
CustomSprite wind_crank_shadow() {
    return load_wind("wind_crank_shadow.png", s_windCrankSh, s_windCrankShW, s_windCrankShH,
                     s_windCrankShTried, "wind crank shadow");
}

CustomSprite custom_hand(int kind) {
    if (kind < 0 || kind >= SLOTS) return { nullptr, 0, 0 };
    if (!s_handTried[kind]) {
        s_handTried[kind] = true;
        // From the card, same contract as plate/overlay. Hands were the
        // last visual element that could not travel per theme, which is why a theme
        // switch used to leave the previous theme's hands on the new clock face.
        static const char *sdName[SLOTS] = {
            "clock_hand_hour.png",   "clock_hand_minute.png", "clock_hand_second.png",
            "clock_static1.png",     "clock_static2.png",
            // Theme-only: there is no compiled-in fallback for a shadow, and there should
            // not be. A firmware that shipped its own would put a shadow under a theme
            // that never asked for one.
            "clock_shadow_hour.png", "clock_shadow_minute.png", "clock_shadow_second.png",
        };
        int w = 0, h = 0;
        if (const uint8_t *p = theme_art::find_active(sdName[kind], theme_art::FMT_RGB565_ALPHA, w, h)) {
            s_hand[kind] = (uint8_t *)p; s_handW[kind] = w; s_handH[kind] = h;
            return { s_hand[kind], s_handW[kind], s_handH[kind] };
        }
        uint8_t *o = nullptr;
        if (decode_from_sd(sdName[kind], true, o, w, h, "hand")) {
            s_hand[kind] = o; s_handW[kind] = w; s_handH[kind] = h;
        }
    }
    return { s_hand[kind], s_handW[kind], s_handH[kind] };
}

// Drop every decoded PSRAM buffer and reset the "tried" flags so the next call to
CustomSprite custom_shadow(int hand) {
    if (hand < 0 || hand > 2) return { nullptr, 0, 0 };
    return custom_hand(hand + 5);
}

// custom_plate()/custom_overlay()/custom_hand() re-decodes from the flash-resident
// PNG bytes (which are never freed — they're .rodata, not a runtime allocation).
// Called when the custom clock face is no longer the app on screen, so a design's
// ~1 MB of decoded pixels isn't held resident while some other app is in front.
void custom_sprite_release() {
    if (s_windBg    && !theme_art::owns((uint8_t *)s_windBg)) heap_caps_free(s_windBg);
    if (s_windCrank && !theme_art::owns(s_windCrank)) heap_caps_free(s_windCrank);
    if (s_windCrankSh && !theme_art::owns(s_windCrankSh)) heap_caps_free(s_windCrankSh);
    s_windBg = nullptr;
    s_windCrank = nullptr;
    s_windCrankSh = nullptr;
    s_windBgTried = s_windCrankTried = s_windCrankShTried = false;
    const uint32_t t0 = millis();
    size_t freed = 0;
    // theme_art::owns() means the pixels live in memory-mapped flash: nothing was
    // allocated, so the reference is dropped rather than freed.
    if (s_plate)   { if (!theme_art::owns(s_plate))   { freed += (size_t)SCREEN_W * SCREEN_H * 2; heap_caps_free(s_plate); }   s_plate = nullptr; }
    if (s_overlay) { if (!theme_art::owns(s_overlay)) { freed += (size_t)SCREEN_W * SCREEN_H * 3; heap_caps_free(s_overlay); } s_overlay = nullptr; }
    if (s_splashOv) { if (!theme_art::owns(s_splashOv)) { freed += (size_t)SCREEN_W * SCREEN_H * 3; heap_caps_free(s_splashOv); } s_splashOv = nullptr; }
    for (int i = 0; i < SLOTS; ++i) if (s_hand[i]) {
        if (!theme_art::owns(s_hand[i])) {
            freed += (size_t)s_handW[i] * s_handH[i] * 3;
            heap_caps_free(s_hand[i]);
        }
        s_hand[i] = nullptr; s_handW[i] = 0; s_handH[i] = 0;
    }
    s_plateTried = s_overlayTried = s_splashOvTried = false;
    for (int i = 0; i < 5; ++i) s_handTried[i] = false;
    if (freed) Serial.printf("[custom_sprite] released ~%u KB of decoded PSRAM in %u ms\n", (unsigned)(freed / 1024), (unsigned)(millis() - t0));
}

#include "settings_sprite.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } } Serial;
static void heap_caps_free(void *p) { free(p); }
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#endif
#include "png_decode.h"
#include <string.h>
#include "custom_settings_plate.h"
#include "custom_settings_overlay.h"
#include "theme_sd.h"   // theme_sd::read_whole/free — SD-hosted plate/overlay, one rung above flash
#include "theme_select.h"   // theme_select::activeSlug() — which /themes/<slug>/ folder to read from
#include "theme_style.h"   // hasAsset() — ignore files the theme does not declare
#include "theme_art.h"      // pre-baked RGB565 in flash — tried before the card, costs nothing

namespace {

// PNG bytes -> a fresh PSRAM buffer, in the shared decoder (png_decode.h). `out` is left
// null on failure, so a bad PNG can never be mistaken for a loaded one.
bool decode(const uint8_t *png, uint32_t len, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    out = png_decode::to_buffer(png, len, alpha ? png_decode::FMT_RGB565_ALPHA : png_decode::FMT_RGB565, w, h, "settings_sprite", tag);
    return out != nullptr;
}

// SD-hosted plate/overlay for the active theme (theme_select::activeSlug()),
// tried before the flash-baked PNG. No slug selected, or any SD failure
// (missing file, bad PNG), falls straight through to whatever
// CUSTOM_HAS_SETTINGS_PLATE/OVERLAY already resolves to.
constexpr size_t SD_ASSET_MAX_BYTES = 2 * 1024 * 1024;
bool decode_sd_first(const char *assetName, const uint8_t *flashPng, uint32_t flashLen, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
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
            if (ok) { Serial.printf("[settings_sprite] %s: source SD %s\n", tag, path); return true; }
        }
    }
    if (!flashPng) return false;
    return decode(flashPng, flashLen, alpha, out, w, h, tag);
}

// Flash first, card second. Deliberately the same signature as decode_sd_first() above,
// so every call site below is just a rename: a hit costs nothing at all (no SD read, no
// PNG decode, no PSRAM), and a miss falls through to exactly the previous behaviour.
bool load_asset(const char *assetName, const uint8_t *flashPng, uint32_t flashLen,
                bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    if (const uint8_t *p = theme_art::find_active(
            assetName, alpha ? theme_art::FMT_RGB565_ALPHA : theme_art::FMT_RGB565, w, h)) {
        out = (uint8_t *)p;
        Serial.printf("[settings_sprite] %s: flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", tag, w, h);
        return true;
    }
    return decode_sd_first(assetName, flashPng, flashLen, alpha, out, w, h, tag);
}

uint8_t     *s_plateBuf = nullptr;   lv_img_dsc_t s_plateDsc;   bool s_plateTried = false;
uint8_t     *s_overlayBuf = nullptr; lv_img_dsc_t s_overlayDsc; bool s_overlayTried = false;

} // namespace

const lv_img_dsc_t *settings_custom_plate() {
    if (!s_plateTried) {
        s_plateTried = true;
        int w = 0, h = 0;
#if CUSTOM_HAS_SETTINGS_PLATE
        const bool ok = load_asset("settings_plate.png", CUSTOM_SETTINGS_PLATE_PNG, CUSTOM_SETTINGS_PLATE_PNG_LEN, false, s_plateBuf, w, h, "plate");
#else
        const bool ok = load_asset("settings_plate.png", nullptr, 0, false, s_plateBuf, w, h, "plate");
#endif
        if (ok) {
            s_plateDsc.header.always_zero = 0;
            s_plateDsc.header.w = w;
            s_plateDsc.header.h = h;
            s_plateDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
            s_plateDsc.data_size = (uint32_t)w * h * 2;
            s_plateDsc.data = s_plateBuf;
        }
    }
    if (s_plateBuf) return &s_plateDsc;
    return nullptr;
}

const lv_img_dsc_t *settings_custom_overlay() {
    if (!s_overlayTried) {
        s_overlayTried = true;
        int w = 0, h = 0;
#if CUSTOM_HAS_SETTINGS_OVERLAY
        const bool ok = load_asset("settings_overlay.png", CUSTOM_SETTINGS_OVERLAY_PNG, CUSTOM_SETTINGS_OVERLAY_PNG_LEN, true, s_overlayBuf, w, h, "overlay");
#else
        const bool ok = load_asset("settings_overlay.png", nullptr, 0, true, s_overlayBuf, w, h, "overlay");
#endif
        if (ok) {
            s_overlayDsc.header.always_zero = 0;
            s_overlayDsc.header.w = w;
            s_overlayDsc.header.h = h;
            s_overlayDsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
            s_overlayDsc.data_size = (uint32_t)w * h * 3;
            s_overlayDsc.data = s_overlayBuf;
        }
    }
    if (s_overlayBuf) return &s_overlayDsc;
    return nullptr;
}

void settings_sprite_release() {
    // theme_art::owns() means the pixels are memory-mapped flash, never an allocation:
    // freeing that would be a wild pointer into the partition.
    if (s_plateBuf)   { if (!theme_art::owns(s_plateBuf))   heap_caps_free(s_plateBuf);   s_plateBuf = nullptr; }
    if (s_overlayBuf) { if (!theme_art::owns(s_overlayBuf)) heap_caps_free(s_overlayBuf); s_overlayBuf = nullptr; }
    s_plateTried = s_overlayTried = false;
}

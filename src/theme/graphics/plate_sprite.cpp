#include "plate_sprite.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <SD.h>
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
#include "theme_sd.h"
#include "sdcard.h"
#include "theme_select.h"
#include "theme_style.h"
#include "theme_art.h"

namespace {

constexpr size_t SD_ASSET_MAX_BYTES = 2 * 1024 * 1024;

// PNG bytes -> a fresh PSRAM RGB565 buffer, in the shared decoder (png_decode.h). `out` is
// left null on failure, so a bad PNG can never be mistaken for a loaded one.
bool decode(const uint8_t *png, uint32_t len, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    out = png_decode::to_buffer(png, len, alpha ? png_decode::FMT_RGB565_ALPHA : png_decode::FMT_RGB565,
                                w, h, tag, "png");
    return out != nullptr;
}

// Flash first, then the card, then nothing. hasAsset() is what stops a file left behind by
// an older push of the same theme being drawn after the design dropped it.
bool load_asset(const char *assetName, bool alpha, uint8_t *&out, int &w, int &h, const char *tag) {
    const theme_art::Format want = alpha ? theme_art::FMT_RGB565_ALPHA : theme_art::FMT_RGB565;
    const uint8_t *p = nullptr;
    theme_art::Format fmt = want;
    if (theme_art::lookup(theme_select::activeSlug(), assetName, p, w, h, fmt)) {
        if (fmt == want) {
            out = (uint8_t *)p;
            Serial.printf("[%s] flash-resident %dx%d (0 ms, 0 KB PSRAM)\n", tag, w, h);
            return true;
        }
    }
    const char *slug = theme_select::activeSlug();
    if (!slug[0]) return false;
    char path[64];
    snprintf(path, sizeof(path), "/themes/%s/%s", slug, assetName);
    if (!theme_style::hasAsset(assetName)) {
        // SAY SO. The theme's asset list is what stops a file left behind by an abandoned
        // install being drawn after the design dropped it, and refusing to read it is
        // correct. But refusing SILENTLY is indistinguishable from the feature being broken:
        // the file is right there on the card, the design says it wants a picture, and the
        // screen shows none. That exact combination cost an evening.
        //
        // The check only runs when the answer was already no, so it costs nothing in the
        // normal case, and it names both halves of the disagreement.
#ifdef ARDUINO
        if (sdcard::Guard guard; SD.exists(path))
#endif
            Serial.printf("[%s] %s is ON THE CARD but not in this theme's asset list, so it is "
                          "being ignored. Re-install the theme.\n", tag, assetName);
        return false;
    }
    size_t sdLen = 0;
    uint8_t *sdBuf = theme_sd::read_whole(path, sdLen, SD_ASSET_MAX_BYTES);
    if (!sdBuf) return false;
    const bool ok = decode(sdBuf, (uint32_t)sdLen, alpha, out, w, h, tag);
    theme_sd::free(sdBuf);
    if (ok) Serial.printf("[%s] source SD %s\n", tag, path);
    return ok;
}

}  // namespace

const lv_img_dsc_t *plate_sprite::get(Plate &p) {
    if (!p.tried) {
        p.tried = true;
        int w = 0, h = 0;
        if (load_asset(p.asset, p.alpha, p.buf, w, h, p.tag)) {
            p.dsc.header.always_zero = 0;
            p.dsc.header.w  = w;
            p.dsc.header.h  = h;
            p.dsc.header.cf = p.alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
            p.dsc.data_size = (uint32_t)w * h * (p.alpha ? 3 : 2);
            p.dsc.data      = p.buf;
        }
    }
    return p.buf ? &p.dsc : nullptr;
}

void plate_sprite::release(Plate &p) {
    // theme_art::owns() means the pixels are memory-mapped flash rather than an allocation:
    // freeing that would be a wild pointer into the partition.
    if (p.buf) {
        if (!theme_art::owns(p.buf)) heap_caps_free(p.buf);
        p.buf = nullptr;
    }
    p.tried = false;
}

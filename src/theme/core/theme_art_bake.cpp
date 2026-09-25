// One-time PNG -> raw RGB565 conversion, writing the result into the `themeart` flash
// partition so nothing has to be read or unpacked while the device is in use.
//
// Runs at boot, only when the active theme has no baked assets yet, which in practice
// means "the first boot after a theme push" — the reboot the user is already waiting
// through. Everything here is best-effort: an asset that fails to read, fails to decode,
// or does not fit simply stays on the SD path, which still works exactly as before.
#include "theme_art.h"
#include <stdio.h>   // snprintf: glibc/libstdc++ do not pull it in for us
#include "theme_font.h"   // distinct_files(), map_text(): the fonts this theme loads
#include "theme_bake_policy.h"   // should_bake(): never erase the flash cache when the card is out

#ifdef ARDUINO
#include <Arduino.h>
#include "theme_style.h"   // hasAsset() — never bake a file the theme does not declare
#include "png_decode.h"
#include <esp_heap_caps.h>
#include "theme_sd.h"
#include "theme_select.h"
#include "sdcard.h"   // mounted(): with no card nothing can be read, so nothing may be erased

namespace theme_art {
namespace {

// Priority order, most-frequently-shown first. The partition holds roughly 3.4 MB and a
// full-screen asset is 424 KB opaque / 636 KB with alpha, so a rich theme will not fit
// entirely. Baking in this order means the cheap wins land first and whatever spills over
// is the artwork shown least often. install_asset() logs each skip.
struct Asset { const char *name; bool alpha; };

// Fonts, stored byte-for-byte rather than decoded. They are lv_font_conv binaries that
// LVGL parses itself (see theme_font.cpp), so there is nothing to convert here — the
// point is only that they live in the same fast, memory-mapped cache as the artwork.
// Baked first so typography never loses the space race to a background.
//
// The list is theme_font's own slot table, not a copy. A copy is what sat here from the
// day fonts were baked at all: eleven names, written when there were eleven slots, and
// never touched as the Headlines, Ticker, Weather and wind screens each gained slots of
// their own. theme_font loads ONLY from this bake, so those fifteen fonts were shipped,
// declared, written to the card and then never read, and every one of those screens drew
// the compiled face whatever typeface the design chose. theme_art's cache VERSION went up
// with this so an already-baked theme is baked again, fonts included.
const Asset ASSETS[] = {
    { "menu_plate.png",        false },   // every menu open — the whole reason for this
    { "settings_plate.png",    false },
    { "radar_plate.png",       false },
    { "clock_plate.png",       false },
    { "clock_overlay.png",     true  },
    { "clock_hand_hour.png",   true  },
    { "clock_hand_minute.png", true  },
    { "clock_hand_second.png", true  },
    { "radar_sweep.png",       true  },
    { "radar_blip.png",        true  },
    { "radar_static1.png",     true  },
    { "radar_static2.png",     true  },
    { "clock_static1.png",     true  },
    { "clock_static2.png",     true  },
    { "menu_overlay.png",      true  },
    { "settings_overlay.png",  true  },
    { "splash.png",            false },   // boot only, so it loses the space race by design
};
constexpr size_t ASSET_N = sizeof(ASSETS) / sizeof(ASSETS[0]);
constexpr size_t SD_ASSET_MAX_BYTES = 2 * 1024 * 1024;

void (*s_progress)(const char *, int, int) = nullptr;

// Decodes through the same png_decode::to_buffer() the sprite loaders use. That has to be
// true and not merely arranged: the baked bytes are handed to LVGL in place of that
// function's output, so any difference in the colour conversion would show up as wrong
// colours with no other symptom. Caller frees.
bool decode_png(const uint8_t *png, size_t len, bool alpha,
                uint8_t *&out, int &w, int &h) {
    out = png_decode::to_buffer(png, (uint32_t)len,
                                alpha ? png_decode::FMT_RGB565_ALPHA : png_decode::FMT_RGB565,
                                w, h, "theme_art", "bake");
    return out != nullptr;
}

} // namespace

void set_progress(void (*cb)(const char *, int, int)) { s_progress = cb; }

bool bake_active_theme() {
    const char *slug = theme_select::activeSlug();
    if (!slug || !slug[0]) return false;
    if (!space_total()) return false;              // no partition on this layout
    // Re-bake when the theme's declared asset list has moved, not only when nothing is
    // baked at all. Without this, adding a layer to a theme would never reach flash and
    // removing one would leave the old pixels cached: the cache would quietly drift from
    // the theme and only a firmware VERSION bump would ever resync it.
    const uint32_t want = theme_style::assetsFingerprint();
    const bool card = sdcard::mounted();
    if (!should_bake(card, slug_baked(slug), baked_manifest(slug), want)) {
        // Told apart in the log: an unchanged theme is the normal case; a missing card is the one where
        // erasing the index (install_begin) would destroy the cache and then find nothing to rewrite.
        Serial.printf(card ? "[theme_art] '%s' already baked and unchanged — nothing to do\n"
                           : "[theme_art] '%s': no card, leaving the flash cache alone\n", slug);
        return false;
    }
    if (slug_baked(slug))
        Serial.printf("[theme_art] '%s' asset list changed (%08x -> %08x) — re-baking\n",
                      slug, (unsigned)baked_manifest(slug), (unsigned)want);

    Serial.printf("[theme_art] baking '%s' into flash (one time, this boot only)\n", slug);
    const uint32_t t0 = millis();
    if (!install_begin(slug, want)) { Serial.println("[theme_art] install_begin failed — staying on SD"); return false; }

    int baked = 0;
    // Count what will actually be attempted so the on-screen progress has a real total.
    int totalPlanned = 0;
    size_t fontN = 0;
    const char *const *FONT_ASSETS = theme_font::distinct_files(fontN);
    for (size_t i = 0; i < fontN; ++i) if (theme_style::hasAsset(FONT_ASSETS[i])) ++totalPlanned;
    for (size_t i = 0; i < ASSET_N; ++i) if (theme_style::hasAsset(ASSETS[i].name)) ++totalPlanned;
    if (s_progress) s_progress(nullptr, 0, totalPlanned);
    int attempted = 0;

    // Fonts first: they are small (tens of KB) next to a 636 KB layer, and a theme that
    // spilled its font would silently fall back to the previous theme's typography, which
    // is the exact confusion this whole change exists to remove.
    for (size_t i = 0; i < fontN; ++i) {
        if (!theme_style::hasAsset(FONT_ASSETS[i])) continue;
        if (s_progress) s_progress(FONT_ASSETS[i], ++attempted, totalPlanned);
        char path[80];
        snprintf(path, sizeof(path), "/themes/%s/%s", slug, FONT_ASSETS[i]);
        size_t len = 0;
        uint8_t *buf = theme_sd::read_whole(path, len, SD_ASSET_MAX_BYTES);
        if (!buf) continue;
        if (install_asset(slug, FONT_ASSETS[i], 0, 0, FMT_RAW, buf, len)) {
            ++baked;
            Serial.printf("[theme_art] baked %-22s %u KB (font)\n",
                          FONT_ASSETS[i], (unsigned)(len / 1024));
        }
        theme_sd::free(buf);
    }

    {   // The font map, so an Orb with no card still knows which face each slot loads. It is
        // not a file on the card, so it is written from memory, and only when the theme maps.
        static char mapText[1200];
        const size_t n = theme_font::map_text(mapText, sizeof(mapText));
        if (n && install_asset(slug, "fonts.map", 0, 0, FMT_RAW, (const uint8_t *)mapText, n)) {
            ++baked;
            Serial.printf("[theme_art] stored fonts.map (%u bytes)\n", (unsigned)n);
        }
    }

    for (size_t i = 0; i < ASSET_N; ++i) {
        // Never bake something the theme does not declare. Stale files from older pushes
        // sit on the card forever, and baking them wastes flash on artwork that is not
        // part of this theme: two undeclared empty overlays cost 1.3 MB before this
        // check existed. See theme_style::hasAsset().
        if (!theme_style::hasAsset(ASSETS[i].name)) continue;
        if (s_progress) s_progress(ASSETS[i].name, ++attempted, totalPlanned);
        char path[80];
        snprintf(path, sizeof(path), "/themes/%s/%s", slug, ASSETS[i].name);
        size_t pngLen = 0;
        uint8_t *pngBuf = theme_sd::read_whole(path, pngLen, SD_ASSET_MAX_BYTES);
        if (!pngBuf) continue;                     // asset not part of this theme: normal

        uint8_t *raw = nullptr;
        int w = 0, h = 0;
        const bool ok = decode_png(pngBuf, pngLen, ASSETS[i].alpha, raw, w, h);
        theme_sd::free(pngBuf);
        if (!ok) { Serial.printf("[theme_art] %s: decode failed, leaving on SD\n", ASSETS[i].name); continue; }

        const size_t bytes = (size_t)w * h * (ASSETS[i].alpha ? 3 : 2);
        if (install_asset(slug, ASSETS[i].name, w, h,
                          ASSETS[i].alpha ? FMT_RGB565_ALPHA : FMT_RGB565, raw, bytes)) {
            ++baked;
            Serial.printf("[theme_art] baked %-22s %dx%d %u KB\n",
                          ASSETS[i].name, w, h, (unsigned)(bytes / 1024));
        }
        heap_caps_free(raw);
    }

    if (!baked) { Serial.println("[theme_art] nothing baked"); return false; }
    if (!install_commit()) { Serial.println("[theme_art] commit failed — staying on SD"); return false; }
    Serial.printf("[theme_art] baked %d asset(s) in %u ms — subsequent shows are free\n",
                  baked, (unsigned)(millis() - t0));
    return true;
}

} // namespace theme_art

#else

namespace theme_art {
bool bake_active_theme() { return false; }
void set_progress(void (*)(const char *, int, int)) {}
} // namespace theme_art

#endif

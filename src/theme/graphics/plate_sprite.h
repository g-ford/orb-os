#pragma once
#include <lvgl.h>

// A background plate, or a glass overlay, for a screen that has no compiled-in fallback.
//
// Every screen used to carry its own copy of these four functions, because each had its own compiled-in
// fallback symbols, its own log tag and its own asset names. The compiled fallbacks are gone, so what is left
// of the difference is an asset name and a word in a log line, and both of those are parameters. This is the
// one loader: Settings, the app picker, News, Weather and the Ticker all use it.
//
// Same three rungs as everywhere else: pre-baked flash first (free, no PSRAM), then the SD
// PNG (~430 ms and ~424 KB), then nothing at all, which is the flat background colour the
// screen had before it could carry a picture.
namespace plate_sprite {

// A handle per screen, so two screens can hold their own plates without knowing about each
// other. Declare one at file scope in the view that owns it.
struct Plate {
    const char   *asset;      // e.g. "weather_plate.png"
    const char   *tag;        // what its log lines say
    uint8_t      *buf = nullptr;
    lv_img_dsc_t  dsc {};
    bool          tried = false;
    bool          alpha = false;   // a glass or CRT overlay: RGB565 plus an alpha byte, drawn over the screen
};

// Decoded on the first ask and remembered until release(). Returns nullptr when the theme
// ships no such file, which is not an error: it is a design that chose a colour. An `alpha` plate is
// decoded with its alpha channel and described as TRUE_COLOR_ALPHA.
const lv_img_dsc_t *get(Plate &p);

// Give the pixels back. Called on the way out of a screen, so one app's artwork is not held
// while another is on the dial.
void release(Plate &p);

// The same loader for a caller that wants raw pixels rather than an LVGL image (the clock's hands, the wind
// screen). Pre-baked flash first: the pixels come back in place and nothing is allocated. Otherwise decoded from
// the card into PSRAM. nullptr when the theme ships no such file. RGB565, with an alpha byte after each pixel when
// `alpha`. `tag` starts the log lines.
const uint8_t *load_pixels(const char *assetName, bool alpha, int &w, int &h, const char *tag);

// Give pixels from load_pixels() back. Flash-resident pixels are only forgotten. True if PSRAM was freed.
bool release_pixels(const uint8_t *p);

}  // namespace plate_sprite

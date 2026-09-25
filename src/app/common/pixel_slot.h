#pragma once
// A lazily loaded, releasable pixel buffer: the bookkeeping the clock's sprites and the wind screen each carried
// as a trio of variables (pointer, size, "tried" flag) per asset.
//
// It is a header of its own, with the loader and the release passed in, so the one rule that matters can be
// tested on the desktop: after a release, EVERY slot loads again. custom_sprite.cpp used to reset the "tried"
// flag for the first five of its eight slots and free all eight, so the three hand shadows, once released when
// the clock left the screen, were never loaded again until a reboot.
#include <stddef.h>
#include <stdint.h>

struct PixelSlot {
    const uint8_t *px = nullptr;
    int            w = 0, h = 0, bpp = 0;   // bytes per pixel: 2 for opaque RGB565, 3 with an alpha byte
    bool           tried = false;           // asked for once already, whether or not it arrived
};

// The pixels, loading them on the first ask. `load(w, h)` returns them (or nullptr for "this theme has none") and
// fills the size. A missing asset is not asked for again until the slot is dropped.
template <class Load>
const uint8_t *pixel_slot_get(PixelSlot &s, int bpp, Load load) {
    if (!s.px && !s.tried) {
        s.tried = true;
        s.bpp = bpp;
        s.px = load(s.w, s.h);
    }
    return s.px;
}

// Forget the pixels so the next get loads again. `release(px)` frees decoded PSRAM and returns true, or does
// nothing and returns false for a buffer that lives in flash. Returns the bytes freed.
template <class Release>
size_t pixel_slot_drop(PixelSlot &s, Release release) {
    size_t freed = 0;
    if (s.px && release(s.px)) freed = (size_t)s.w * (size_t)s.h * (size_t)s.bpp;
    s.px = nullptr;
    s.w = s.h = 0;
    s.tried = false;
    return freed;
}

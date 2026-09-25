// Host test for src/app/common/pixel_slot.h.   tests/run_pixel_slot_test.sh
#include "pixel_slot.h"

#include <assert.h>
#include <stdio.h>

static int g_loads = 0;
static uint8_t g_pixels[16];

// A loader standing in for plate_sprite::load_pixels: counts calls, reports a 4x2 image.
static const uint8_t *loader(int &w, int &h) { ++g_loads; w = 4; h = 2; return g_pixels; }
static const uint8_t *missing(int &w, int &h) { ++g_loads; w = h = 0; return nullptr; }
static bool frees(const uint8_t *) { return true; }
static bool flash_resident(const uint8_t *) { return false; }   // owned by flash: nothing to free

static void a_slot_loads_once() {
    PixelSlot s;
    g_loads = 0;
    assert(pixel_slot_get(s, 3, loader) == g_pixels);
    assert(pixel_slot_get(s, 3, loader) == g_pixels);
    assert(g_loads == 1 && s.w == 4 && s.h == 2);
}

static void a_missing_asset_is_not_retried_until_the_slot_is_dropped() {
    PixelSlot s;
    g_loads = 0;
    assert(pixel_slot_get(s, 3, missing) == nullptr);
    assert(pixel_slot_get(s, 3, missing) == nullptr);
    assert(g_loads == 1);                                    // a theme with no such file is asked once
    pixel_slot_drop(s, frees);
    assert(pixel_slot_get(s, 3, loader) == g_pixels && g_loads == 2);   // and asked again after a release
}

static void dropping_reports_what_was_freed_and_nothing_for_flash() {
    PixelSlot a, b;
    assert(pixel_slot_get(a, 3, loader) && pixel_slot_get(b, 2, loader));
    assert(pixel_slot_drop(a, frees) == (size_t)4 * 2 * 3);
    assert(pixel_slot_drop(b, flash_resident) == 0);         // a flash-resident buffer is forgotten, not freed
    assert(a.px == nullptr && b.px == nullptr);
}

// The bug this header exists to prevent: the clock keeps eight slots (five layers and three hand shadows), and
// releasing them reset the "already tried" flag for only the first five. A shadow that was released was never loaded
// again until a reboot. Every slot, however many there are, must be loadable again after a release.
static void every_slot_loads_again_after_a_release() {
    constexpr int N = 8;
    PixelSlot slots[N];
    g_loads = 0;
    for (auto &s : slots) assert(pixel_slot_get(s, 3, loader));
    assert(g_loads == N);
    for (auto &s : slots) pixel_slot_drop(s, frees);
    for (auto &s : slots) assert(pixel_slot_get(s, 3, loader) == g_pixels);
    assert(g_loads == 2 * N);
}

int main() {
    a_slot_loads_once();
    a_missing_asset_is_not_retried_until_the_slot_is_dropped();
    dropping_reports_what_was_freed_and_nothing_for_flash();
    every_slot_loads_again_after_a_release();
    printf("pixel_slot: all tests passed\n");
    return 0;
}

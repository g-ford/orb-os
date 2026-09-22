#pragma once
// Decodes the boot-splash artwork into a PSRAM RGB565 buffer at the moment it's needed.
// Tries, in order: a pre-baked flash copy of the active theme's splash.png, then that file
// on the microSD card. A theme with no splash of its own decodes nothing: ui_splash_show()
// then draws its background and the text lines.
#include <lvgl.h>

// Decodes into an internal PSRAM buffer (reused across calls — the result is only valid
// until the next call) and fills `out` to point at it. Returns false on decode failure.
bool splash_art_decode(lv_img_dsc_t *out);

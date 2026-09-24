#pragma once
// The wheel: the one knob-driven list renderer, used by the app picker and by every list in Settings.
//
// It knows nothing about themes. Everything a theme decides arrives in a Look from the caller
// (src/app/common/wheel_look does that), so the platform layer takes on no theme dependency and the shape of
// the wheel is the same wherever it appears. Geometry is in wheel_layout.h and is not configurable.
//
// LVGL labels have no glow, so the rows are drawn onto one transparent canvas the size of the panel (~868 KB of
// PSRAM). It is allocated when a wheel screen appears and given back when it goes, because held permanently it
// competed with decoded theme art and lost.
#include <lvgl.h>
#include <stddef.h>

namespace wheel {

constexpr int SELECTED_GLOW = 8;   // px of halo behind the selected row, when a Look asks for it

struct Look {
    lv_color_t       selColor;
    lv_color_t       itemColor;
    lv_color_t       glowColor;
    int              selGlow;     // 0 = none
    const lv_font_t *selFont;
    const lv_font_t *itemFont;
};

// Create the canvas as the topmost child of `parent`. Safe to call repeatedly. On failure (PSRAM pressure)
// available() stays false and the caller must keep a plain-label fallback visible: a list you cannot read is a
// list you cannot leave.
void acquire(lv_obj_t *parent);
void release();
bool available();

// Blank the canvas without drawing a wheel, for a page that has none.
void clear();

// Draw `count` rows with row `sel` selected: layout, ellipsis, glow, and invalidate only the rectangle that
// changed (this frame's plus the previous frame's, so the old text is erased). No-op without a canvas.
void draw(const char *const *rows, int count, int sel, const Look &look);

// Cut `text` to fit `maxW` in `font`, ending in "...", into `out`. Returns true if it was cut. The same rule
// draw() applies, exposed for callers that lay out plain labels when there is no canvas.
bool fit(const lv_font_t *font, const char *text, float maxW, char *out, size_t cap);

} // namespace wheel

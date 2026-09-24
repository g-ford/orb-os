#pragma once
// The wheel's geometry and nothing else: no LVGL, no theme, no fonts. That is what lets
// tests/wheel_layout_test.cpp hold the shape still on the desktop, and what stops a theme from reaching
// it: these are the values Settings' wheel had stock, now the same for every wheel and no theme's to change.
#include <math.h>
#include <stddef.h>

namespace wheel_layout {

constexpr float RADIUS     = 170.0f;   // px from the centre to a row a quarter turn away
constexpr float BOW        = 18.0f;    // px a row leans sideways at a quarter turn
constexpr float STEP_DEG   = 22.0f;    // degrees of dial between neighbouring rows
constexpr float CENTRE_DY  = 0.0f;     // where the selected row sits, relative to the panel centre
constexpr float FADE       = 2.0f;     // row opacity is cos(angle) raised to 2 * FADE
constexpr float PANEL_C    = 233.0f;   // centre of the 466 px panel, and the radius of its circle
constexpr float ROW_MARGIN = 26.0f;    // room kept clear at each end of a row for the bezel
constexpr float MIN_ROW_W  = 80.0f;    // a row is never given less than this

struct Row {
    float         sx, sy;    // offset from the panel centre, unrounded
    unsigned char opa;       // 0..255 distance fade
    float         maxW;      // the widest this row may draw on the round panel
    bool          offDial;   // a quarter turn or more away: draw nothing
};

// Row `index` when row `sel` is selected.
//
// Past a quarter turn there is no dial left to put anything on, so the angle is clamped for the position
// and the row is flagged offDial. The falloff is floored at zero: cosf(pi/2) is a tiny NEGATIVE number in
// floating point, and powf(negative, fractional) is NaN, which used to become an opacity nobody chose.
inline Row row(int index, int sel) {
    const int   d        = index - sel;
    const float angleDeg = fabsf((float)d) * STEP_DEG;
    const bool  off      = angleDeg >= 90.0f;
    const float rad      = fminf(angleDeg, 90.0f) * 3.14159265f / 180.0f;
    Row r;
    r.sy  = CENTRE_DY + (d < 0 ? -1.0f : 1.0f) * RADIUS * sinf(rad);
    r.sx  = BOW * (1.0f - cosf(rad));
    const float fall = fmaxf(0.0f, cosf(rad));
    r.opa = off ? 0 : (unsigned char)lroundf(255.0f * powf(fall, 2.0f * FADE));
    // The panel is round, so a row's room depends on its height: the chord of the circle at sy, less the lean
    // and a margin at each end.
    const float chord = 2.0f * sqrtf(fmaxf(0.0f, PANEL_C * PANEL_C - r.sy * r.sy));
    r.maxW    = fmaxf(MIN_ROW_W, chord - 2.0f * fabsf(r.sx) - 2.0f * ROW_MARGIN);
    r.offDial = off;
    return r;
}

// How many leading characters of `text` to keep so that they plus "..." fit in maxW. `adv[i]` is the advance
// of text[i], `dot` the advance of '.'. Returns n when the whole text fits (or maxW <= 0). Drops letters from
// the end, never ends on a space, and keeps at least one character, so a row is never just dots.
inline size_t ellipsis_keep(const float *adv, const char *text, size_t n, float dot, float maxW) {
    float total = 0.0f;
    for (size_t i = 0; i < n; ++i) total += adv[i];
    if (maxW <= 0.0f || total <= maxW) return n;
    size_t keep = n;
    float  kept = total;
    while (keep > 1 && kept + 3.0f * dot > maxW) { --keep; kept -= adv[keep]; }
    while (keep > 1 && text[keep - 1] == ' ')    { --keep; kept -= adv[keep]; }
    return keep;
}

} // namespace wheel_layout

#pragma once
// Colour roles: the palette a theme picks, and the one place the rest is derived from it.
//
// A theme picks four colours (bg, primary, secondary, text); the other seven are mixes of those unless the theme
// states them. Every screen's colour options default to a role, so a theme that is only a palette still looks
// designed. Pure logic, no LVGL: tests/theme_roles_test.cpp runs it on the desktop, and tools/build_theme.py
// reads THEME_ROLE_LIST below for the names it accepts, so the builder cannot drift from the firmware.
//
// The mixing lives here and nowhere else. The builder does no colour arithmetic, on purpose: two languages
// computing "dim" would disagree in the last bit and nobody would know which was right.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// The order matters: the four a theme picks come first. Keep the X(...) entries one per name; the builder
// finds them with a regular expression.
#define THEME_ROLE_LIST(X) \
    X(bg) X(primary) X(secondary) X(text) \
    X(muted) X(dim) X(hairline) X(panel) X(highlight) X(onPrimary) X(alert)

namespace theme_roles {

enum Role : uint8_t {
#define X(n) R_##n,
    THEME_ROLE_LIST(X)
#undef X
    ROLE_COUNT
};

constexpr int BASE_ROLES = 4;   // bg, primary, secondary, text: what a theme picks

const char *const NAMES[ROLE_COUNT] = {
#define X(n) #n,
    THEME_ROLE_LIST(X)
#undef X
};

struct Palette { uint32_t v[ROLE_COUNT]; };
struct Input   { uint32_t v[ROLE_COUNT]; bool set[ROLE_COUNT]; };   // what a theme states

// The role with this exact name, or -1. Case matters: the names are JSON keys.
inline int find_role(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < ROLE_COUNT; ++i)
        if (strcmp(NAMES[i], name) == 0) return i;
    return -1;
}

// `pct` percent of the way from a to b, per channel, rounded to nearest. pct is clamped to 0..100 so a bad value
// can never push a channel past either end.
inline uint32_t mix(uint32_t a, uint32_t b, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint32_t out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        const int ca = (int)((a >> sh) & 0xFF), cb = (int)((b >> sh) & 0xFF);
        out |= (uint32_t)((ca * (100 - pct) + cb * pct + 50) / 100) << sh;
    }
    return out;
}

// Perceived brightness, 0..255 (Rec. 601 weights, no gamma: this only has to rank two colours).
inline int luma(uint32_t c) {
    return (int)((((c >> 16) & 0xFF) * 299 + ((c >> 8) & 0xFF) * 587 + (c & 0xFF) * 114) / 1000);
}

// Whichever of a and b contrasts more with `fill`; a tie goes to a.
inline uint32_t on_colour(uint32_t fill, uint32_t a, uint32_t b) {
    const int d = luma(fill);
    return abs(luma(a) - d) >= abs(luma(b) - d) ? a : b;
}

inline void input_clear(Input &in) {
    memset(in.v, 0, sizeof(in.v));
    memset(in.set, 0, sizeof(in.set));
}

// What an Orb shows when no theme is selected, and what a base role a theme leaves out falls back to. All eleven
// written out: the night-vision green set every screen has always drawn (formerly the compiled APP_THEME_DEFAULT
// palette, retired in spec step 4; app_theme::palette() now derives from this), so a theme with no palette of its
// own is unchanged. src/theme_assets/default/theme.yaml lists the
// same eleven and a test keeps the two equal.
inline constexpr Palette BUILT_IN = {{
    0x000000,   // bg
    0x1DFF86,   // primary
    0x9AFFC8,   // secondary
    0xEAFFF3,   // text
    0x818C86,   // muted
    0x5F7A6C,   // dim
    0x1C2620,   // hairline
    0x0C160F,   // panel
    0x232A36,   // highlight
    0x05100A,   // onPrimary
    0xE5484D,   // alert
}};

// The palette a theme resolves to. The four base roles are taken from `in`, or from BUILT_IN when left out; the
// other seven derive from them unless `in` states them. Starting ratios, tuned by eye with --themeshot.
inline Palette resolve(const Input &in) {
    Palette p = BUILT_IN;
    for (int r = 0; r < BASE_ROLES; ++r)
        if (in.set[r]) p.v[r] = in.v[r] & 0xFFFFFF;
    const uint32_t bg = p.v[R_bg], pr = p.v[R_primary], tx = p.v[R_text];
    p.v[R_muted]     = mix(tx, bg, 45);
    p.v[R_dim]       = mix(pr, bg, 55);
    p.v[R_hairline]  = mix(pr, bg, 80);
    p.v[R_panel]     = mix(bg, tx, 6);
    p.v[R_highlight] = mix(bg, pr, 25);
    p.v[R_onPrimary] = on_colour(pr, bg, tx);
    p.v[R_alert]     = 0xE5484D;
    for (int r = BASE_ROLES; r < ROLE_COUNT; ++r)
        if (in.set[r]) p.v[r] = in.v[r] & 0xFFFFFF;
    return p;
}

} // namespace theme_roles

// Host test for src/theme/core/theme_roles.h.   tests/run_theme_roles_test.sh
#include "theme_roles.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace theme_roles;

static Input pick(uint32_t bg, uint32_t primary, uint32_t secondary, uint32_t text) {
    Input in;
    input_clear(in);
    in.v[R_bg] = bg;               in.set[R_bg] = true;
    in.v[R_primary] = primary;     in.set[R_primary] = true;
    in.v[R_secondary] = secondary; in.set[R_secondary] = true;
    in.v[R_text] = text;           in.set[R_text] = true;
    return in;
}

static void the_roles_are_the_eleven_the_builder_reads() {
    assert(ROLE_COUNT == 11);
    assert(strcmp(NAMES[R_bg], "bg") == 0);
    assert(strcmp(NAMES[R_onPrimary], "onPrimary") == 0);
    assert(strcmp(NAMES[R_alert], "alert") == 0);
    assert(BASE_ROLES == 4 && R_text == 3 && R_muted == 4);          // the four a theme picks come first
    assert(find_role("primary") == R_primary);
    assert(find_role("onPrimary") == R_onPrimary);
    assert(find_role("nope") == -1);
    assert(find_role("") == -1);
    assert(find_role("Primary") == -1);                              // case matters: it is a JSON key
}

static void mix_goes_from_a_to_b_per_channel() {
    assert(mix(0x102030, 0xA0B0C0, 0) == 0x102030);
    assert(mix(0x102030, 0xA0B0C0, 100) == 0xA0B0C0);
    assert(mix(0x000000, 0xFFFFFF, 50) == 0x808080);                 // (255*50 + 50) / 100 = 128
    assert(mix(0x102030, 0xA0B0C0, -5) == 0x102030);                 // clamped, never negative
    assert(mix(0x102030, 0xA0B0C0, 150) == 0xA0B0C0);                // clamped, never past b
    assert(mix(0xFFFFFF, 0xFFFFFF, 37) == 0xFFFFFF);                 // no channel can overflow
}

// Portal's four colours; the expected numbers come from an independent calculation.
static void a_dark_palette_derives_the_rest() {
    const Palette p = resolve(pick(0x0B0E11, 0xFF9A1F, 0x82CEFF, 0xFFFFFF));
    assert(p.v[R_bg] == 0x0B0E11 && p.v[R_primary] == 0xFF9A1F);
    assert(p.v[R_secondary] == 0x82CEFF && p.v[R_text] == 0xFFFFFF);
    assert(p.v[R_muted] == 0x919394);
    assert(p.v[R_dim] == 0x794D17);
    assert(p.v[R_hairline] == 0x3C2A14);
    assert(p.v[R_panel] == 0x1A1C1F);
    assert(p.v[R_highlight] == 0x483115);
    assert(p.v[R_onPrimary] == 0x0B0E11);                            // dark on orange
    assert(p.v[R_alert] == 0xE5484D);
}

// Review focus 5: a light theme. Derivation mixes toward bg, so it stays inside the palette.
static void a_light_palette_stays_readable() {
    const Palette p = resolve(pick(0xF4F5F7, 0x3B5BFF, 0x6E6E73, 0x1C1C1E));
    assert(p.v[R_muted] == 0x7D7E80);
    assert(p.v[R_dim] == 0xA1B0FB);
    assert(p.v[R_hairline] == 0xCFD6F9);
    assert(p.v[R_panel] == 0xE7E8EA);
    assert(p.v[R_highlight] == 0xC6CFF9);
    assert(p.v[R_onPrimary] == 0xF4F5F7);                            // light text on a mid-blue primary
}

static void on_colour_picks_the_one_that_contrasts_more() {
    assert(on_colour(0xFFFF00, 0x000000, 0xFFFFFF) == 0x000000);     // yellow: dark text
    assert(on_colour(0x101010, 0x000000, 0xFFFFFF) == 0xFFFFFF);     // near black: light text
    assert(on_colour(0x808080, 0x000000, 0xFFFFFF) == 0x000000);     // dead centre: a tie goes to the first
    assert(luma(0x808080) == 128 && luma(0x000000) == 0 && luma(0xFFFFFF) == 255);
}

static void a_stated_derived_role_beats_the_derivation() {
    Input in = pick(0x0B0E11, 0xFF9A1F, 0x82CEFF, 0xFFFFFF);
    in.v[R_muted] = 0x123456; in.set[R_muted] = true;
    in.v[R_alert] = 0xFF0000; in.set[R_alert] = true;
    const Palette p = resolve(in);
    assert(p.v[R_muted] == 0x123456 && p.v[R_alert] == 0xFF0000);
    assert(p.v[R_dim] == 0x794D17);                                  // the others still derive
}

static void a_base_role_left_out_falls_back_to_the_built_in_one() {
    Input in;
    input_clear(in);
    in.v[R_primary] = 0xFF9A1F; in.set[R_primary] = true;
    const Palette p = resolve(in);
    assert(p.v[R_primary] == 0xFF9A1F);
    assert(p.v[R_bg] == BUILT_IN.v[R_bg] && p.v[R_text] == BUILT_IN.v[R_text]);
    assert(p.v[R_secondary] == BUILT_IN.v[R_secondary]);
}

// The built-in palette is written out in full and equals today's night-vision AppPalette (the palette app_theme.cpp used to hold), so a
// theme with no palette of its own keeps the colours every screen has always had.
static void the_built_in_palette_is_todays_default() {
    assert(BUILT_IN.v[R_bg] == 0x000000 && BUILT_IN.v[R_primary] == 0x1DFF86);
    assert(BUILT_IN.v[R_secondary] == 0x9AFFC8 && BUILT_IN.v[R_text] == 0xEAFFF3);
    assert(BUILT_IN.v[R_muted] == 0x818C86 && BUILT_IN.v[R_dim] == 0x5F7A6C);
    assert(BUILT_IN.v[R_hairline] == 0x1C2620 && BUILT_IN.v[R_panel] == 0x0C160F);
    assert(BUILT_IN.v[R_highlight] == 0x232A36 && BUILT_IN.v[R_onPrimary] == 0x05100A);
    assert(BUILT_IN.v[R_alert] == 0xE5484D);
    for (int r = 0; r < ROLE_COUNT; ++r) assert(BUILT_IN.v[r] <= 0xFFFFFF);
}

int main() {
    the_roles_are_the_eleven_the_builder_reads();
    mix_goes_from_a_to_b_per_channel();
    a_dark_palette_derives_the_rest();
    a_light_palette_stays_readable();
    on_colour_picks_the_one_that_contrasts_more();
    a_stated_derived_role_beats_the_derivation();
    a_base_role_left_out_falls_back_to_the_built_in_one();
    the_built_in_palette_is_todays_default();
    printf("theme_roles: all tests passed\n");
    return 0;
}

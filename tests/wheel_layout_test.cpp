// Host test for src/platform/wheel/wheel_layout.h.   tests/run_wheel_layout_test.sh
//
// The expected numbers were computed from the arithmetic in the pre-refactor wheel_layout() in
// settings_view.cpp with its stock values (radius 170, bow 18, step 22 degrees, fade 2.0), before this file
// existed. If a number here changes, the wheel moved, and that needs a reason.
#include "wheel_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

using namespace wheel_layout;

static bool near(float a, float b, float tol = 0.05f) { return fabsf(a - b) <= tol; }

static void the_selected_row_sits_dead_centre_at_full_strength() {
    const Row r = row(3, 3);
    assert(near(r.sx, 0.0f) && near(r.sy, 0.0f));
    assert(r.opa == 255);
    assert(!r.offDial);
    assert(near(r.maxW, 414.0f));
}

static void rows_step_away_along_the_dial() {
    static const struct { int d; float sy, sx; int opa; float maxW; } W[] = {
        { 1,  63.683f,  1.311f, 188, 393.64f },
        { 2, 118.092f,  5.052f,  68, 339.61f },
        { 3, 155.303f, 10.679f,   7, 274.03f },
        { 4, 169.896f, 17.372f,   0, 232.16f },
    };
    for (const auto &w : W) {
        const Row below = row(5 + w.d, 5);
        assert(near(below.sy,  w.sy) && near(below.sx, w.sx));
        assert(below.opa == w.opa && near(below.maxW, w.maxW));
        assert(!below.offDial);
        const Row above = row(5 - w.d, 5);          // the same distance above: mirrored in y, same lean
        assert(near(above.sy, -w.sy) && near(above.sx, w.sx));
        assert(above.opa == w.opa && near(above.maxW, w.maxW));
    }
}

static void a_quarter_turn_or_more_away_is_off_the_dial() {
    assert(!row(4, 0).offDial);                     // 88 degrees
    assert(row(5, 0).offDial && row(5, 0).opa == 0);// 110 degrees
    assert(row(0, 5).offDial);
    const Row far = row(1000, 0);                   // the clamp: no NaN however far
    assert(far.offDial && far.opa == 0);
    assert(!isnan(far.sx) && !isnan(far.sy) && near(far.sy, 170.0f));
}

static void text_that_fits_is_left_alone() {
    const float adv[] = { 10, 10, 10, 10, 10 };
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 50.0f) == 5);    // fits exactly
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 500.0f) == 5);
    assert(ellipsis_keep(adv, "ABCDE", 5, 4.0f, 0.0f) == 5);     // no limit given
    assert(ellipsis_keep(adv, "", 0, 4.0f, 10.0f) == 0);         // nothing to cut
}

static void a_cut_leaves_room_for_three_dots() {
    float adv[10];
    for (float &a : adv) a = 10.0f;                              // 100 wide, dots are 4 each
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 99.0f) == 8);   // 80 + 12 = 92 <= 99
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 92.0f) == 8);   // exactly fits with the dots
    assert(ellipsis_keep(adv, "ABCDEFGHIJ", 10, 4.0f, 91.0f) == 7);   // one pixel less loses a letter
}

static void a_cut_never_ends_on_a_space() {
    const float adv[] = { 10, 10, 10, 10, 10 };                  // "AB CD"
    assert(ellipsis_keep(adv, "AB CD", 5, 4.0f, 42.0f) == 2);    // would keep "AB ", drops the space
}

static void one_glyph_is_always_kept() {
    const float adv[] = { 200, 200 };
    assert(ellipsis_keep(adv, "ab", 2, 4.0f, 50.0f) == 1);       // never "..." on its own
}

int main() {
    the_selected_row_sits_dead_centre_at_full_strength();
    rows_step_away_along_the_dial();
    a_quarter_turn_or_more_away_is_off_the_dial();
    text_that_fits_is_left_alone();
    a_cut_leaves_room_for_three_dots();
    a_cut_never_ends_on_a_space();
    one_glyph_is_always_kept();
    printf("wheel_layout: all tests passed\n");
    return 0;
}

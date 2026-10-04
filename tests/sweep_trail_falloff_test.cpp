// Host test for src/app/radar/sweep_trail_falloff.h.   tests/run_sweep_trail_falloff_test.sh
#include "sweep_trail_falloff.h"

#include <assert.h>
#include <stdio.h>

using namespace radar_impl;

static void full_opacity_at_the_lead_edge() {
    assert(trail_alpha_frac(0.0f, 38.0f) == 1.0f);
    assert(trail_alpha_frac(-5.0f, 38.0f) == 1.0f);   // ahead of the lead edge: still full
}

static void zero_at_and_beyond_the_tail() {
    assert(trail_alpha_frac(38.0f, 38.0f) == 0.0f);
    assert(trail_alpha_frac(50.0f, 38.0f) == 0.0f);
}

static void quadratic_ease_matches_the_original_discrete_curve() {
    // The old stepped code used frac = 1 - i/steps, opa = frac*frac*max. Halfway back
    // from the lead edge is frac=0.5, so the curve must still land on 0.25 there.
    const float got = trail_alpha_frac(19.0f, 38.0f);
    assert(got > 0.249f && got < 0.251f);
}

static void falls_off_smoothly_with_no_flat_steps() {
    // The whole point of this curve: strictly decreasing across the trail, so a bake
    // sampling it pixel-by-pixel never reproduces the old banding.
    float prev = trail_alpha_frac(0.0f, 38.0f);
    for (float behind = 0.5f; behind <= 38.0f; behind += 0.5f) {
        const float v = trail_alpha_frac(behind, 38.0f);
        assert(v < prev);
        prev = v;
    }
}

static void degenerate_trail_deg_is_safe() {
    assert(trail_alpha_frac(1.0f, 0.0f) == 0.0f);
    assert(trail_alpha_frac(1.0f, -5.0f) == 0.0f);
}

int main() {
    full_opacity_at_the_lead_edge();
    zero_at_and_beyond_the_tail();
    quadratic_ease_matches_the_original_discrete_curve();
    falls_off_smoothly_with_no_flat_steps();
    degenerate_trail_deg_is_safe();
    printf("sweep_trail_falloff: all tests passed\n");
    return 0;
}

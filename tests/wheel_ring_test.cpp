// Host test for src/platform/wheel/wheel_ring.h.   tests/run_wheel_ring_test.sh
#include "wheel_ring.h"

#include <assert.h>
#include <stdio.h>

using namespace wheel_layout;

static void five_apps_put_the_current_one_in_the_middle_at_every_position() {
    for (int pos = 0; pos < 5; ++pos) {
        int idx[8];
        const int n = ring_rows(5, pos, idx);
        assert(n == 5);
        assert(idx[2] == pos);                       // selected sits at visible / 2
        assert(idx[1] == (pos + 4) % 5 && idx[3] == (pos + 1) % 5);
        assert(idx[0] == (pos + 3) % 5 && idx[4] == (pos + 2) % 5);
        bool seen[5] = { false, false, false, false, false };
        for (int i = 0; i < n; ++i) { assert(!seen[idx[i]]); seen[idx[i]] = true; }   // each app once
    }
}

static void four_apps_still_show_each_once_with_the_current_one_at_visible_over_two() {
    int idx[8];
    const int n = ring_rows(4, 3, idx);
    assert(n == 4 && idx[2] == 3);
    bool seen[4] = { false, false, false, false };
    for (int i = 0; i < n; ++i) seen[idx[i]] = true;
    assert(seen[0] && seen[1] && seen[2] && seen[3]);
}

static void one_and_two_apps_and_none() {
    int idx[8];
    assert(ring_rows(1, 0, idx) == 1 && idx[0] == 0);
    assert(ring_rows(2, 1, idx) == 2 && idx[1] == 1);
    assert(ring_rows(0, 0, idx) == 0);
}

int main() {
    five_apps_put_the_current_one_in_the_middle_at_every_position();
    four_apps_still_show_each_once_with_the_current_one_at_visible_over_two();
    one_and_two_apps_and_none();
    printf("wheel_ring: all tests passed\n");
    return 0;
}

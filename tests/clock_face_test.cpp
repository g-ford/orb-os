// Host test for src/app/clock/clock_face.h.   tests/run_clock_face_test.sh
#include "clock_face.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

using namespace clock_face;

static bool near(float a, float b, float eps = 0.01f) { return fabsf(a - b) < eps; }
static float length(const Segment &s) { return hypotf(s.x1 - s.x0, s.y1 - s.y0); }

static void twelve_ticks_are_major() {
    int major = 0;
    for (int i = 0; i < 60; ++i) if (is_major(i)) ++major;
    assert(major == 12);
    assert(is_major(0) && is_major(15) && is_major(55) && !is_major(1) && !is_major(59));
}

static void ticks_sit_at_the_right_angles_and_lengths() {
    const Layout &L = DEFAULT_LAYOUT;
    const Segment top = tick(L, 0);                         // 12 o'clock: straight up from the centre
    assert(near(top.x0, L.cx) && near(top.y0, L.cy - 222.0f));
    assert(near(length(top), L.majorLen));
    const Segment right = tick(L, 15);                      // 3 o'clock
    assert(near(right.x0, L.cx + 222.0f) && near(right.y0, L.cy));
    const Segment down = tick(L, 30);
    assert(near(down.x0, L.cx) && near(down.y0, L.cy + 222.0f));
    const Segment left = tick(L, 45);
    assert(near(left.x0, L.cx - 222.0f) && near(left.y0, L.cy));
    assert(near(length(tick(L, 1)), L.minorLen));           // a minor tick is the short one
}

static void every_tick_stays_inside_the_ring() {
    const Layout &L = DEFAULT_LAYOUT;
    for (int i = 0; i < 60; ++i) {
        const Segment s = tick(L, i);
        assert(hypotf(s.x0 - L.cx, s.y0 - L.cy) < L.ringR);
        assert(hypotf(s.x1 - L.cx, s.y1 - L.cy) < hypotf(s.x0 - L.cx, s.y0 - L.cy));      // runs inward
        assert(s.x0 >= 0 && s.x0 <= 466 && s.y0 >= 0 && s.y0 <= 466);                     // and on the 466 px screen
    }
}

static void a_hand_at_twelve_points_straight_up() {
    const Blade b = blade(100, 100, 0, 50, 10, 4);
    assert(near(b.x[0], 100) && near(b.y[0], 50));           // the tip
    assert(near(b.x[1], 104) && near(b.y[1], 92));           // the shoulder, 16% of the way out, to the right
    assert(near(b.x[2], 100) && near(b.y[2], 110));          // the tail, behind the pivot
    assert(near(b.x[3], 96) && near(b.y[3], 92));            // the shoulder on the left
}

static void a_hand_at_three_points_right() {
    const Blade b = blade(100, 100, 90, 50, 10, 4);
    assert(near(b.x[0], 150) && near(b.y[0], 100));
    assert(near(b.x[2], 90) && near(b.y[2], 100));
}

static void the_hour_hand_creeps_and_the_minute_hand_follows_the_seconds() {
    Angles a = angles(3, 0, 0);
    assert(near(a.hour, 90) && near(a.minute, 0) && near(a.second, 0));
    a = angles(12, 30, 0);
    assert(near(a.hour, 15) && near(a.minute, 180));         // half past twelve: the hour hand is half way to one
    a = angles(15, 45, 30);                                   // 24-hour input wraps to 3:45:30
    assert(near(a.hour, 112.75f) && near(a.minute, 273.0f) && near(a.second, 180));
    a = angles(0, 0, 0);
    assert(near(a.hour, 0));
}

int main() {
    twelve_ticks_are_major();
    ticks_sit_at_the_right_angles_and_lengths();
    every_tick_stays_inside_the_ring();
    a_hand_at_twelve_points_straight_up();
    a_hand_at_three_points_right();
    the_hour_hand_creeps_and_the_minute_hand_follows_the_seconds();
    printf("clock_face: all tests passed\n");
    return 0;
}

// Host test for src/core/swipe.{h,cpp}.   tests/run_swipe_test.sh
#include "swipe.h"

#include <assert.h>
#include <stdio.h>

using swipe::Dir;

// The tests own their limits, so retuning config.h on the board cannot break them.
static const swipe::Limits LIM = { 60, 2.0f, 700 };

// One finger: down at (x0,y0), a few samples on the way, lifted after `ms`. The lift is fed
// position (0,0) on purpose: the detector must ignore where a lift claims to be.
static Dir drag(swipe::Detector &d, int x0, int y0, int x1, int y1, uint32_t ms, uint16_t rot = 0) {
    const int STEPS = 5;
    for (int i = 0; i <= STEPS; ++i)
        d.feed(x0 + (x1 - x0) * i / STEPS, y0 + (y1 - y0) * i / STEPS, true, ms * i / (STEPS + 1), rot);
    return d.feed(0, 0, false, ms, rot);
}

static void the_four_directions() {
    swipe::Detector d(LIM);
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);
    assert(drag(d, 180, 233, 300, 233, 200) == Dir::Right);
    assert(drag(d, 233, 320, 233, 200, 200) == Dir::Up);      // the finger moves UP the screen
    assert(drag(d, 233, 200, 233, 320, 200) == Dir::Down);
}

static void short_slow_and_still_are_not_swipes() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 233, 193, 233, 200) == Dir::None);    // 40 px
    assert(drag(d, 300, 233, 180, 233, 900) == Dir::None);    // 900 ms
    assert(drag(d, 233, 233, 233, 233, 80) == Dir::None);     // a tap
    assert(drag(d, 233, 233, 233, 233, 2000) == Dir::None);   // a long press
}

static void the_limits_are_inclusive() {
    swipe::Detector d(LIM);
    assert(drag(d, 200, 233, 140, 233, 200) == Dir::Left);    // exactly 60 px
    assert(drag(d, 200, 233, 141, 233, 200) == Dir::None);    // 59 px
    assert(drag(d, 100, 100, 200, 150, 200) == Dir::Right);   // exactly 2:1
    assert(drag(d, 100, 100, 200, 151, 200) == Dir::None);    // just under 2:1
    assert(drag(d, 300, 233, 180, 233, 700) == Dir::Left);    // exactly 700 ms
    assert(drag(d, 300, 233, 180, 233, 701) == Dir::None);    // one over
}

static void a_diagonal_is_ignored_not_guessed() {
    swipe::Detector d(LIM);
    assert(drag(d, 150, 150, 250, 250, 200) == Dir::None);
    assert(drag(d, 150, 150, 250, 190, 200) == Dir::Right);   // 100 x 40 is plainly sideways
}

// A display rotated clockwise by R shows logical Right as a finger moving in the direction
// R(Right). The detector turns the physical swipe back, so the logical direction comes out.
static void rotation_90() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 150, 233, 250, 200, 90) == Dir::Right);   // physical down
    assert(drag(d, 150, 233, 250, 233, 200, 90) == Dir::Up);      // physical right
    assert(drag(d, 233, 250, 233, 150, 200, 90) == Dir::Left);    // physical up
    assert(drag(d, 250, 233, 150, 233, 200, 90) == Dir::Down);    // physical left
}

static void rotation_180_and_270() {
    swipe::Detector d(LIM);
    assert(drag(d, 250, 233, 150, 233, 200, 180) == Dir::Right);  // physical left
    assert(drag(d, 233, 150, 233, 250, 200, 180) == Dir::Up);     // physical down
    assert(drag(d, 233, 250, 233, 150, 200, 270) == Dir::Right);  // physical up
    assert(drag(d, 250, 233, 150, 233, 200, 270) == Dir::Up);     // physical left
}

static void rotation_wraps_past_360() {
    swipe::Detector d(LIM);
    assert(drag(d, 233, 150, 233, 250, 200, 450) == Dir::Right);  // 450 is 90
}

static void an_odd_mounting_angle_still_reads_a_clear_swipe() {
    swipe::Detector d(LIM);
    // Logical Right at 30 degrees clockwise is about (87, 50) on the panel.
    assert(drag(d, 150, 150, 237, 200, 200, 30) == Dir::Right);
}

static void forty_five_degrees_is_ambiguous_so_it_is_nothing() {
    swipe::Detector d(LIM);
    assert(drag(d, 150, 233, 250, 233, 200, 45) == Dir::None);
}

static void cancel_swallows_only_the_gesture_in_progress() {
    swipe::Detector d(LIM);
    d.feed(300, 233, true, 0, 0);
    d.feed(240, 233, true, 100, 0);
    d.cancel();
    assert(d.feed(180, 233, false, 200, 0) == Dir::None);         // the wake touch: nothing happens
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);        // the next one is a fresh gesture
    d.cancel();                                                    // nothing in progress: a no-op...
    assert(drag(d, 300, 233, 180, 233, 200) == Dir::Left);        // ...that must not swallow the NEXT one
}

static void a_lift_with_no_touch_is_nothing() {
    swipe::Detector d(LIM);
    assert(d.feed(0, 0, false, 10, 0) == Dir::None);
    assert(d.feed(0, 0, false, 30, 0) == Dir::None);
}

static void the_answer_arrives_once_and_only_on_lift() {
    swipe::Detector d(LIM);
    for (int i = 0; i < 5; ++i)
        assert(d.feed(300 - 30 * i, 233, true, i * 40, 0) == Dir::None);   // still down: no answer yet
    assert(d.feed(0, 0, false, 200, 0) == Dir::Left);
    assert(d.feed(0, 0, false, 220, 0) == Dir::None);             // a second lift is not a second swipe
}

static void the_ring_skips_and_wraps() {
    // Clock, Flight, Weather, Intel, Settings (captured, so skipped)
    const bool skip[5] = { false, false, false, false, true };
    assert(swipe::ringNeighbour(skip, 5, 3, +1) == 0);   // Intel -> Clock, Settings skipped
    assert(swipe::ringNeighbour(skip, 5, 0, -1) == 3);   // Clock back -> Intel
    assert(swipe::ringNeighbour(skip, 5, 0, +1) == 1);
    assert(swipe::ringNeighbour(skip, 5, 1, -1) == 0);
    assert(swipe::ringNeighbour(skip, 5, 4, +1) == 0);   // from a skipped app: still lands somewhere sane
}

static void a_ring_of_one_or_none_goes_nowhere() {
    const bool onlyClock[3] = { false, true, true };
    assert(swipe::ringNeighbour(onlyClock, 3, 0, +1) == -1);
    assert(swipe::ringNeighbour(onlyClock, 3, 0, -1) == -1);
    const bool none[2] = { true, true };
    assert(swipe::ringNeighbour(none, 2, 0, +1) == -1);
    assert(swipe::ringNeighbour(none, 0, 0, +1) == -1);           // no roster registered yet
    const bool one[1] = { false };
    assert(swipe::ringNeighbour(one, 1, 0, +1) == -1);
    assert(swipe::ringNeighbour(onlyClock, 3, 0, 0) == -1);       // no direction, no move
}

// Review finding: app_shell::transitioning() compared (int32_t)(until - millis()) > 0 and never
// cleared it, so 2^31 ms (24.9 days) after a slide ended the window read as OPEN again and every
// swipe was dropped for the next 24.9 days. withinMs() is the wrap-safe form both windows now use.
static void a_window_is_wrap_safe_and_never_reopens() {
    assert(swipe::withinMs(1000, 1100, 250));
    assert(!swipe::withinMs(1000, 1250, 250));                    // exactly the length: closed
    assert(!swipe::withinMs(1000, 1000 + 0x80000010u, 250));      // 24.9 days later: still closed
    assert(!swipe::withinMs(1000, 1000 + 0xFFFFFF00u, 250));      // and much later
    assert(swipe::withinMs(0xFFFFFF00u, 0x00000040u, 500));       // a window straddling the wrap is open
    assert(!swipe::withinMs(1000, 999, 250));                     // a clock reading before the start is not "inside"
}

// Review finding: a touch that WAKES a dimmed screen must not navigate, but the IMU's motion wake
// (and the knob) can clear the dimmed flag a poll before the touch is first seen, so "was it
// dimmed" alone misses it. A touch soon after the screen woke, by any route, is a wake touch.
static void a_touch_just_after_the_screen_woke_is_a_wake_touch() {
    assert(swipe::wakeTouch(true, 5000, 0, 400));                 // still dimmed
    assert(swipe::wakeTouch(false, 5100, 5000, 400));             // the IMU undimmed it 100 ms ago
    assert(!swipe::wakeTouch(false, 5400, 5000, 400));            // 400 ms on it is an ordinary touch
    assert(!swipe::wakeTouch(false, 900000, 5000, 400));          // long awake
    assert(swipe::wakeTouch(false, 200, 0, 400));                 // boot: a swipe in the first moments is cancelled, harmlessly
}

int main() {
    a_window_is_wrap_safe_and_never_reopens();
    a_touch_just_after_the_screen_woke_is_a_wake_touch();
    the_four_directions();
    short_slow_and_still_are_not_swipes();
    the_limits_are_inclusive();
    a_diagonal_is_ignored_not_guessed();
    rotation_90();
    rotation_180_and_270();
    rotation_wraps_past_360();
    an_odd_mounting_angle_still_reads_a_clear_swipe();
    forty_five_degrees_is_ambiguous_so_it_is_nothing();
    cancel_swallows_only_the_gesture_in_progress();
    a_lift_with_no_touch_is_nothing();
    the_answer_arrives_once_and_only_on_lift();
    the_ring_skips_and_wraps();
    a_ring_of_one_or_none_goes_nowhere();
    printf("swipe: all ok\n");
    return 0;
}

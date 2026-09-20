#pragma once
// Touch swipes, as pure logic: no Arduino, no LVGL, no hardware, so it runs in the host tests.
//
// The device reads the CST9217 and the simulator reads the mouse, and both feed the same
// Detector, which is what makes "it behaved right in the sim" mean "it behaves right on the
// Orb". What a swipe DOES is decided in input_router::onSwipe(), never here.
#include <stdint.h>

namespace swipe {

enum class Dir { None, Left, Right, Up, Down };

// The three tests a touch has to pass to be a swipe. Passed in rather than read from config.h so
// the tests own their limits: retuning the defaults on the board must not break them.
struct Limits {
    int      minPx;       // travel along the main axis
    float    axisRatio;   // the main axis must be at least this many times the other
    uint32_t maxMs;       // finger down to finger up
};

class Detector {
public:
    explicit Detector(const Limits &limits) : lim_(limits) {}

    // Call once per poll with whether a finger is down and where. Returns a direction only on
    // the poll where the finger lifts; x and y are ignored on a lift. `rotationDeg` is
    // display::rotation(): the swipe is turned back through it so Left means the logical left
    // however the Orb is mounted. A swipe within about 26 degrees of an axis (after rotation)
    // is a swipe; near 45 degrees it is nothing rather than a guess.
    Dir  feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg);

    // The gesture in progress will report None when it lifts. A no-op when no finger is down,
    // so it can never swallow the NEXT gesture. Used for the touch that wakes a dimmed screen.
    void cancel();

private:
    Limits   lim_;
    bool     active_ = false, cancelled_ = false;
    int      x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
    uint32_t t0_ = 0;
};

// Which app a swipe lands on. `skip[i]` is true for an app the ring must not visit (hidden, or
// one that holds the knob, like Settings). `dir` is +1 or -1; the ring wraps. Returns -1 when
// there is nowhere to go: no roster, a ring of one, or everything else skipped. `from` may
// itself be skipped.
int ringNeighbour(const bool *skip, int count, int from, int dir);

// Is `nowMs` still inside a window that opened at `startMs` and lasts `lenMs`? Wrap-safe, and
// unlike a signed "until - now > 0" test it CANNOT read as open again 2^31 ms (24.9 days) after
// the window closed, which is what an always-on Orb that was swiped once would have hit.
inline bool withinMs(uint32_t startMs, uint32_t nowMs, uint32_t lenMs) {
    return (uint32_t)(nowMs - startMs) < lenMs;
}

// Is a touch that starts now the one that WOKE a dimmed screen (and so must not also navigate)?
// True while the screen is still dimmed, and for `graceMs` after it woke by ANY route: the IMU's
// motion wake and the knob clear the dimmed flag independently of the touch poll, so asking only
// "was it dimmed" misses a touch that lands a poll after the screen came up.
inline bool wakeTouch(bool dimmedNow, uint32_t nowMs, uint32_t undimAtMs, uint32_t graceMs) {
    return dimmedNow || withinMs(undimAtMs, nowMs, graceMs);
}

}  // namespace swipe

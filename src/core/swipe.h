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

}  // namespace swipe

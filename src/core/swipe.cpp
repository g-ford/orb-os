#include "swipe.h"
#include <math.h>

namespace swipe {

namespace {

// The physical vector, turned back through the display's clockwise rotation into the logical
// frame. Screen coordinates have y pointing down, and a clockwise turn by R takes (x,y) to
// (x cos R - y sin R, x sin R + y cos R), so the way back is the transpose. Cardinal angles use
// exact values: cosf(M_PI/2) is not quite zero, and a swipe test should not depend on that.
void rotate_back(int px, int py, uint16_t deg, float &lx, float &ly) {
    deg %= 360;
    float c, s;
    switch (deg) {
        case 0:   c = 1;  s = 0;  break;
        case 90:  c = 0;  s = 1;  break;
        case 180: c = -1; s = 0;  break;
        case 270: c = 0;  s = -1; break;
        default: {
            const float r = (float)deg * 3.14159265f / 180.0f;
            c = cosf(r);
            s = sinf(r);
        }
    }
    lx = px * c + py * s;
    ly = -px * s + py * c;
}

Dir classify(float dx, float dy, const Limits &lim) {
    const float ax = fabsf(dx), ay = fabsf(dy);
    if (ax >= ay) {
        if (ax < (float)lim.minPx || ax < lim.axisRatio * ay) return Dir::None;
        return dx < 0 ? Dir::Left : Dir::Right;
    }
    if (ay < (float)lim.minPx || ay < lim.axisRatio * ax) return Dir::None;
    return dy < 0 ? Dir::Up : Dir::Down;   // y grows downward: a negative dy is a finger moving up
}

}  // namespace

Dir Detector::feed(int x, int y, bool down, uint32_t nowMs, uint16_t rotationDeg) {
    if (down) {
        if (!active_) { active_ = true; cancelled_ = false; x0_ = x; y0_ = y; t0_ = nowMs; }
        x1_ = x;
        y1_ = y;
        return Dir::None;
    }
    if (!active_) return Dir::None;          // a lift with no touch before it
    active_ = false;
    if (cancelled_) return Dir::None;
    if ((uint32_t)(nowMs - t0_) > lim_.maxMs) return Dir::None;
    float lx, ly;
    rotate_back(x1_ - x0_, y1_ - y0_, rotationDeg, lx, ly);
    return classify(lx, ly, lim_);
}

void Detector::cancel() {
    if (active_) cancelled_ = true;
}

int ringNeighbour(const bool *skip, int count, int from, int dir) {
    if (count <= 0 || dir == 0) return -1;
    const int step = dir > 0 ? 1 : -1;
    for (int n = 1; n < count; ++n) {        // count-1 candidates: never `from` itself
        const int idx = ((from + step * n) % count + count) % count;
        if (!skip[idx]) return idx;
    }
    return -1;
}

}  // namespace swipe

#pragma once
// The geometry of the drawn clock face. Pure math with no LVGL, so the layout is tested on the desktop and the
// drawing in clock_view.cpp only has to put pixels where this says.
#include <math.h>

namespace clock_face {

constexpr float DEG2RAD = 0.017453292519943295f;

struct Layout {
    float cx, cy;              // dial centre
    float ringR;               // radius of the ring's centre line
    float ringW;               // ring stroke
    float majorLen, minorLen;  // tick lengths, measured inward from the ring
    float majorW, minorW;      // tick stroke
};

// The 466x466 round screen: a ring hugging the bezel, twelve long ticks and forty-eight short ones.
constexpr Layout DEFAULT_LAYOUT = { 233.0f, 233.0f, 226.0f, 4.0f, 24.0f, 10.0f, 5.0f, 2.0f };

struct Segment { float x0, y0, x1, y1; };

inline bool is_major(int i) { return i % 5 == 0; }

// Tick i (0..59, 0 at 12 o'clock, clockwise), running inward from just inside the ring.
inline Segment tick(const Layout &L, int i) {
    const float a = (float)i * 6.0f * DEG2RAD;
    const float sx = sinf(a), sy = -cosf(a);
    const float outer = L.ringR - L.ringW * 0.5f - 2.0f;
    const float inner = outer - (is_major(i) ? L.majorLen : L.minorLen);
    return { L.cx + outer * sx, L.cy + outer * sy, L.cx + inner * sx, L.cy + inner * sy };
}

struct Blade { float x[4], y[4]; };

// A tapered hand pivoting at (cx, cy): the tip `len` out, the tail `tail` behind the pivot, and `hw` half-width at
// the shoulder, which is 16% of the way out. Four corners, clockwise from the tip. The same shape clock_view.cpp's
// draw_hand_at() has always drawn for the compiled faces.
inline Blade blade(float cx, float cy, float angDeg, float len, float tail, float hw) {
    const float a = angDeg * DEG2RAD;
    const float dx = sinf(a), dy = -cosf(a);      // along the hand
    const float qx = cosf(a), qy = sinf(a);       // across it
    const float sx = cx + len * 0.16f * dx, sy = cy + len * 0.16f * dy;
    return { { cx + len * dx, sx + hw * qx, cx - tail * dx, sx - hw * qx },
             { cy + len * dy, sy + hw * qy, cy - tail * dy, sy - hw * qy } };
}

struct Angles { float hour, minute, second; };   // degrees clockwise from 12

// The hour hand creeps with the minutes and the minute hand with the seconds, as on a real watch.
inline Angles angles(int hour, int minute, float second) {
    const float mins = (float)minute + second / 60.0f;
    const float hrs = (float)(hour % 12) + mins / 60.0f;
    return { hrs * 30.0f, mins * 6.0f, second * 6.0f };
}

} // namespace clock_face

#pragma once
// Synthesised traffic, for a theme that asks for it (theme_style::radar().simulate).
//
// Eight aircraft on straight courses at fixed speeds, seeded once so the SAME aircraft persist
// and actually travel rather than teleporting each poll. That is what makes trails, sticky
// tracking and zone masking observable without waiting on the sky. Positions advance by real
// elapsed time, so it runs at the same pace whatever the poll interval is.
//
// Was inline in adsb_task, with its state as function-local statics.
#include <stdint.h>
#include <vector>
#include "aircraft.h"

namespace sim_traffic {

constexpr int COUNT = 8;

// Replace `out` with the synthesised contacts as of `nowMs`. The FIRST call anchors both the
// clock and the courses: they are spread around (homeLat, homeLon) as it was then, at varied
// radii and headings, so some cross the middle, some skirt the rim, and some pass through
// whatever keep-out areas a design has drawn. Later calls keep that anchor and only advance
// time. (The longitude scaling uses the home latitude passed each time, as it always did.)
void generate(std::vector<Aircraft> &out, double homeLat, double homeLon, uint32_t nowMs);

// Forget the anchor, so the next generate() re-seeds. Only tests need this.
void reset();

}  // namespace sim_traffic

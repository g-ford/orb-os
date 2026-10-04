#pragma once
// The sweep trail's opacity curve, pulled out of the per-pixel gradient bake (radar_sweep.cpp)
// so the shape of the fade can be proven on the host before it ever touches a pixel. Both the
// Flight Tracker's and the weather map's trail use this same curve — it's the thing that used
// to come out stepped, one lv_draw_line per theme's sweepTrailSteps; baking it into a gradient
// texture instead means this is now the ONLY place the fade shape lives.

namespace radar_impl {

// behindDeg: how many degrees back from the lead edge a point sits. 0 is the lead edge
// itself (full opacity); trailDeg is the tail, where the trail has faded out completely.
// Matches the old discrete code's frac = 1 - i/steps, opa = frac*frac*max, just continuous
// instead of sampled at `steps` fixed points.
inline float trail_alpha_frac(float behindDeg, float trailDeg) {
    if (trailDeg <= 0.0f) return 0.0f;
    if (behindDeg <= 0.0f) return 1.0f;
    if (behindDeg >= trailDeg) return 0.0f;
    const float frac = 1.0f - behindDeg / trailDeg;
    return frac * frac;
}

} // namespace radar_impl

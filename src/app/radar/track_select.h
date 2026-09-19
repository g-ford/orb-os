#pragma once
// Which contacts the scope follows once there are more of them than it has slots, as a pure
// function of three flags per contact, so it can be tested on the host without LVGL.
//
// Deliberately STICKY. This used to be "sort by distance, keep the nearest N", recomputed every
// poll. With a cap of five over a busy city that set churns constantly: two aircraft trade places
// by a kilometre and the scope drops one and adopts another on the far side of the dial. It reads
// as the instrument losing its mind rather than tracking anything. So a contact keeps its slot
// for as long as it stays trackable, and a slot only opens when the aircraft in it leaves the
// ring or lands; free slots go to the nearest untracked contact.
#include <stddef.h>
#include <vector>

namespace track_select {

struct Cand {
    bool trackable;   // on the scope and flying. Ground traffic never earns a slot.
    bool current;     // confirmed within AC_DIM_START_MS, i.e. not yet dimming
    bool incumbent;   // held a slot on the previous poll
};

// `c` must be sorted nearest-first. Returns the indices to keep, in this order, at most `cap`:
//   1. current incumbents, nearest first
//   2. then new, current arrivals, nearest first
//   3. only THEN aging contacts. One that has gone quiet must never hold a slot a genuinely
//      current one needs; the dimming display is what keeps it visible while there IS room,
//      not a claim on room when there is not.
// Empty means nothing qualified; the caller decides what a scope with no candidates shows.
inline std::vector<int> pick(const std::vector<Cand> &c, int cap) {
    std::vector<int> kept;
    if (cap <= 0) return kept;
    kept.reserve((size_t)cap);
    const int n = (int)c.size();
    for (int i = 0; i < n && (int)kept.size() < cap; ++i)
        if (c[i].trackable && c[i].current && c[i].incumbent) kept.push_back(i);
    for (int i = 0; i < n && (int)kept.size() < cap; ++i)
        if (c[i].trackable && c[i].current && !c[i].incumbent) kept.push_back(i);
    for (int i = 0; i < n && (int)kept.size() < cap; ++i)
        if (c[i].trackable && !c[i].current) kept.push_back(i);
    return kept;
}

}  // namespace track_select

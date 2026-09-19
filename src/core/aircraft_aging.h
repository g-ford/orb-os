#pragma once
// How a contact ages. Pure functions of two clock readings, no state, no LVGL, no FreeRTOS,
// so the same code runs on the device, in the simulator and in tests/aircraft_aging_test.cpp.
//
// Two consumers, one clock. main.cpp prunes the contact table with prune() and radar_view.cpp
// dims what is left with freshness(); both read a contact's age through age(), so "dimming
// has started" and "it is about to be dropped" cannot disagree about how old something is.
#include <stddef.h>
#include <stdint.h>
#include "config.h"

namespace aging {

// Milliseconds since `stampMs`. Signed difference, so it stays right across millis()
// rolling over at 49 days, where a plain `now >= stamp` comparison reads every contact from
// before the wrap as brand new until the clock catches up again. A stamp slightly AHEAD of
// `nowMs` (a poll that landed between two clock reads) is age 0, not a wrapped-round ~49 days.
inline uint32_t age(uint32_t nowMs, uint32_t stampMs) {
    const int32_t d = (int32_t)(nowMs - stampMs);
    return d < 0 ? 0u : (uint32_t)d;
}

// Full brightness for AC_DIM_START_MS (two and a half polls, so ordinary jitter and one
// dropped poll stay invisible), then a linear fade to AC_DIM_FLOOR_OPA by AC_DIM_FLOOR_MS,
// held there until prune() drops the contact at AC_HARD_EXPIRE_MS. Never zero: the point is to
// SAY a contact has gone quiet, not to make it vanish ahead of actually being gone.
inline float freshness(uint32_t ageMs) {
    if (ageMs <= AC_DIM_START_MS) return 1.0f;
    if (ageMs >= AC_DIM_FLOOR_MS) return AC_DIM_FLOOR_OPA;
    const float span = (float)(AC_DIM_FLOOR_MS - AC_DIM_START_MS);
    return 1.0f - (1.0f - AC_DIM_FLOOR_OPA) * (float)(ageMs - AC_DIM_START_MS) / span;
}

// Erase every entry not heard from within AC_HARD_EXPIRE_MS. Returns how many went. `Table`
// is anything with begin/end/erase whose values have a `lastUpdateMs`, which is g_acTable.
template <class Table>
size_t prune(Table &table, uint32_t nowMs) {
    size_t gone = 0;
    for (auto it = table.begin(); it != table.end();) {
        if (age(nowMs, it->second.lastUpdateMs) > (uint32_t)AC_HARD_EXPIRE_MS) { it = table.erase(it); ++gone; }
        else ++it;
    }
    return gone;
}

}  // namespace aging

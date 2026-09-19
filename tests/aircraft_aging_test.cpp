// Host test for src/core/aircraft_aging.h.   tests/run_aircraft_aging_test.sh
//
// The bug this guards: contacts were aged and dropped only inside a SUCCESSFUL poll, so a
// feed outage left the last snapshot on the dial at full brightness, forever. The shared
// helpers are what both the pruning tick (main.cpp) and the dimming (radar_view.cpp) now use.
#include "aircraft_aging.h"

#include <assert.h>
#include <map>
#include <stdio.h>
#include <string>

struct Contact { uint32_t lastUpdateMs = 0; };
using Table = std::map<std::string, Contact>;

static const uint32_t EXPIRE = AC_HARD_EXPIRE_MS;

static void age_is_clock_safe() {
    assert(aging::age(5000, 1000) == 4000);
    assert(aging::age(1000, 1000) == 0);
    assert(aging::age(1000, 1500) == 0);                 // stamp a hair ahead of "now": age 0, not ~49 days
    // millis() rolled over between the stamp and now. A `now >= stamp` test would call this 0.
    assert(aging::age(500, UINT32_MAX - 999) == 1500);
}

static void freshness_curve() {
    assert(aging::freshness(0) == 1.0f);
    assert(aging::freshness(AC_DIM_START_MS) == 1.0f);   // ordinary jitter and one dropped poll stay invisible
    assert(aging::freshness(AC_DIM_START_MS + 1) < 1.0f);
    assert(aging::freshness(AC_DIM_FLOOR_MS) == AC_DIM_FLOOR_OPA);
    assert(aging::freshness(EXPIRE) == AC_DIM_FLOOR_OPA);   // held at the floor until dropped, never zero
    assert(aging::freshness(UINT32_MAX) == AC_DIM_FLOOR_OPA);
    const uint32_t mid = (AC_DIM_START_MS + AC_DIM_FLOOR_MS) / 2;
    const float expectMid = (1.0f + AC_DIM_FLOOR_OPA) / 2.0f;
    assert(aging::freshness(mid) > expectMid - 0.001f && aging::freshness(mid) < expectMid + 0.001f);
    float prev = 1.0f;
    for (uint32_t a = 0; a <= EXPIRE; a += 250) {         // never brightens as a contact gets older
        const float f = aging::freshness(a);
        assert(f <= prev + 1e-6f && f >= AC_DIM_FLOOR_OPA);
        prev = f;
    }
}

static void prune_drops_only_the_expired() {
    Table t;
    const uint32_t now = 1000000;
    t["fresh"].lastUpdateMs = now;
    t["justUnder"].lastUpdateMs = now - EXPIRE;           // exactly at the limit: kept ("past" it is the rule)
    t["justOver"].lastUpdateMs = now - EXPIRE - 1;
    t["ancient"].lastUpdateMs = now - 10 * EXPIRE;
    t["ahead"].lastUpdateMs = now + 50;                   // stamped a moment after this clock read
    assert(aging::prune(t, now) == 2);
    assert(t.count("fresh") && t.count("justUnder") && t.count("ahead"));
    assert(!t.count("justOver") && !t.count("ancient"));
}

static void prune_survives_rollover() {
    Table t;
    t["beforeWrap"].lastUpdateMs = UINT32_MAX - 1000;     // heard 1.5 s before the wrap
    t["longBefore"].lastUpdateMs = UINT32_MAX - 2 * EXPIRE;
    assert(aging::prune(t, 500) == 1);                    // only the one that is genuinely old
    assert(t.count("beforeWrap") && !t.count("longBefore"));
}

// The scenario itself: three contacts last heard at t=10 s, then the feed goes silent. The only
// thing running is the tick, every AC_AGE_TICK_MS, exactly as adsb_task calls it.
static void a_silent_feed_still_ages_out() {
    Table t;
    const uint32_t heard = 10000;
    t["a"].lastUpdateMs = heard; t["b"].lastUpdateMs = heard; t["c"].lastUpdateMs = heard;

    uint32_t droppedAt = 0;
    for (uint32_t now = heard; now <= heard + 4 * EXPIRE; now += AC_AGE_TICK_MS) {
        const size_t gone = aging::prune(t, now);
        if (now - heard <= EXPIRE) assert(gone == 0 && t.size() == 3);     // kept until past the limit
        if (gone && !droppedAt) droppedAt = now;
        // Dimming keeps tracking the same clock the whole time.
        const float f = aging::freshness(aging::age(now, heard));
        if (now - heard <= AC_DIM_START_MS) assert(f == 1.0f); else assert(f < 1.0f);
    }
    assert(t.empty());
    assert(droppedAt > heard + EXPIRE && droppedAt <= heard + EXPIRE + AC_AGE_TICK_MS);   // within one tick of the deadline
    printf("silent feed: dropped %u ms after the last report (limit %u ms, tick %u ms)\n",
           (unsigned)(droppedAt - heard), (unsigned)EXPIRE, (unsigned)AC_AGE_TICK_MS);
}

int main() {
    age_is_clock_safe();
    freshness_curve();
    prune_drops_only_the_expired();
    prune_survives_rollover();
    a_silent_feed_still_ages_out();
    printf("aircraft_aging: all checks passed\n");
    return 0;
}

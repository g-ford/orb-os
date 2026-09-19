// Host test for src/app/radar/track_select.h.   tests/run_track_select_test.sh
//
// The selection used to be three loops inside radar::update() that copied whole AcDraw
// records (each carrying a trail vector) into a `kept` list. It is now a pure function over
// three flags per contact, so the copies could become moves. This proves that changed nothing:
// a verbatim copy of the old loops is the reference, and both are run over a few hundred
// thousand random scopes.
#include "track_select.h"

#include <assert.h>
#include <random>
#include <stdio.h>
#include <vector>

using track_select::Cand;

// The old algorithm, transcribed from radar_view.cpp as it was (the lambdas become fields).
static std::vector<int> reference(const std::vector<Cand> &out, int maxOnScreen) {
    std::vector<int> kept;
    for (int i = 0; i < (int)out.size(); ++i) {                 // current incumbents first, nearest first
        if ((int)kept.size() >= maxOnScreen) break;
        if (!out[i].trackable || !out[i].current) continue;
        if (out[i].incumbent) kept.push_back(i);
    }
    for (int i = 0; i < (int)out.size(); ++i) {                 // then backfill with new, current arrivals
        if ((int)kept.size() >= maxOnScreen) break;
        if (!out[i].trackable || !out[i].current) continue;
        if (!out[i].incumbent) kept.push_back(i);
    }
    for (int i = 0; i < (int)out.size(); ++i) {                 // only THEN spend a slot on an aging contact
        if ((int)kept.size() >= maxOnScreen) break;
        if (!out[i].trackable || out[i].current) continue;
        kept.push_back(i);
    }
    return kept;
}

static Cand c(bool trackable, bool current, bool incumbent) { return Cand{trackable, current, incumbent}; }

static void the_rules_by_example() {
    // nearest-first: 0 arrival, 1 incumbent, 2 aging incumbent, 3 grounded, 4 arrival
    const std::vector<Cand> v = { c(1,1,0), c(1,1,1), c(1,0,1), c(0,1,1), c(1,1,0) };
    assert((track_select::pick(v, 5) == std::vector<int>{1, 0, 4, 2}));   // incumbents, arrivals, then aging; never grounded
    assert((track_select::pick(v, 2) == std::vector<int>{1, 0}));         // a full scope keeps its incumbent and takes the nearest arrival
    assert((track_select::pick(v, 1) == std::vector<int>{1}));            // and an incumbent is not displaced by a nearer arrival
    // Two current incumbents, one nearer arrival, room for two: the incumbents win. Both are
    // "trackable and current", so this is the stickiness itself.
    const std::vector<Cand> s = { c(1,1,0), c(1,1,1), c(1,1,1) };
    assert((track_select::pick(s, 2) == std::vector<int>{1, 2}));
    // An aging contact never takes a slot a current one needs.
    const std::vector<Cand> q = { c(1,0,1), c(1,1,0) };
    assert((track_select::pick(q, 1) == std::vector<int>{1}));
}

static void nothing_qualifies() {
    assert(track_select::pick({}, 5).empty());
    assert(track_select::pick({ c(0,1,1), c(0,0,0) }, 5).empty());       // caller falls back to showing anything
    assert(track_select::pick({ c(1,1,1) }, 0).empty());
    assert(track_select::pick({ c(1,1,1) }, -3).empty());
}

static void same_as_the_old_loops() {
    std::mt19937 rng(20260920);
    long scopes = 0, nonEmpty = 0;
    for (int iter = 0; iter < 400000; ++iter) {
        const int n = (int)(rng() % 40);
        std::vector<Cand> v;
        for (int i = 0; i < n; ++i) v.push_back(c(rng() % 5 != 0, rng() % 3 != 0, rng() % 2 != 0));
        const int cap = (int)(rng() % 12);
        const std::vector<int> want = reference(v, cap);
        const std::vector<int> got = track_select::pick(v, cap);
        assert(got == want);
        assert((int)got.size() <= cap);
        ++scopes; nonEmpty += !got.empty();
    }
    printf("track_select: %ld random scopes match the old loops (%ld non-empty)\n", scopes, nonEmpty);
}

int main() {
    the_rules_by_example();
    nothing_qualifies();
    same_as_the_old_loops();
    printf("track_select: all checks passed\n");
    return 0;
}

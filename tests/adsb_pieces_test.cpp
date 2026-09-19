// Host tests for src/core/sim_traffic.{h,cpp} and src/core/feed_backoff.h.
//   tests/run_adsb_pieces_test.sh
//
// Both were inline in adsb_task. The simulator's reference below is that code, transcribed with
// its function-local statics as ordinary locals, so the extraction is checked against what it
// replaced rather than against itself.
#include "sim_traffic.h"
#include "feed_backoff.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string>

// ---- reference: the original inline block ---------------------------------------------
struct RefSim {
    struct SimAc { double lat0, lon0; float brgDeg, gsKt, altFt; };
    SimAc sim[8];
    bool  init = false;
    uint32_t t0 = 0;
    void run(std::vector<Aircraft> &fresh, double homeLat, double homeLon, uint32_t now) {
        if (!init) {
            init = true; t0 = now;
            for (int i = 0; i < 8; ++i) {
                const float a = (float)i * 45.0f;
                const float rKm = 8.0f + (float)(i % 4) * 9.0f;
                sim[i].lat0   = homeLat + (double)(rKm / 111.0f) * cos(a * (float)M_PI / 180.0f);
                sim[i].lon0   = homeLon + (double)(rKm / 111.0f) * sin(a * (float)M_PI / 180.0f)
                                / cos(homeLat * (double)M_PI / 180.0);
                sim[i].brgDeg = fmodf(a + 115.0f, 360.0f);
                sim[i].gsKt   = 180.0f + (float)(i % 5) * 55.0f;
                sim[i].altFt  = 3500.0f + (float)i * 2600.0f;
            }
        }
        const float hrs = (float)(now - t0) / 3600000.0f;
        fresh.clear();
        for (int i = 0; i < 8; ++i) {
            const float nm  = sim[i].gsKt * hrs;
            const float km  = nm * 1.852f;
            const float brg = sim[i].brgDeg * (float)M_PI / 180.0f;
            Aircraft a;
            char hexBuf[8]; snprintf(hexBuf, sizeof(hexBuf), "sim%03d", i);
            a.hex = hexBuf;
            char callBuf[10]; snprintf(callBuf, sizeof(callBuf), "SIM%03d", i);
            a.flight = callBuf;
            a.type = "SIM";
            a.lat = sim[i].lat0 + (double)(km / 111.0f) * cos(brg);
            a.lon = sim[i].lon0 + (double)(km / 111.0f) * sin(brg) / cos(homeLat * (double)M_PI / 180.0);
            a.altBaro = sim[i].altFt; a.onGround = false; a.track = sim[i].brgDeg; a.gs = sim[i].gsKt;
            a.baroRate = 0.0f; a.squawk = 1200; a.seenPos = 0; a.lastUpdateMs = now;
            fresh.push_back(a);
        }
    }
};

static void same_traffic_as_the_inline_code() {
    struct Home { double lat, lon; };
    const Home homes[] = { {33.4484, -112.0740}, {38.7223, -9.1393}, {-33.8688, 151.2093}, {64.1466, -21.9426} };
    for (const Home &h : homes) {
        sim_traffic::reset();
        RefSim ref;
        std::vector<Aircraft> a, b;
        const uint32_t base = 123456;                       // the first call's clock is the anchor
        for (uint32_t dt : {0u, 1u, 2500u, 10000u, 60000u, 3600000u, 86400000u}) {
            sim_traffic::generate(a, h.lat, h.lon, base + dt);
            ref.run(b, h.lat, h.lon, base + dt);
            assert(a.size() == 8 && b.size() == 8);
            for (size_t i = 0; i < 8; ++i) {
                assert(a[i].hex == b[i].hex && a[i].flight == b[i].flight && a[i].type == b[i].type);
                assert(a[i].lat == b[i].lat && a[i].lon == b[i].lon);
                assert(a[i].altBaro == b[i].altBaro && a[i].track == b[i].track && a[i].gs == b[i].gs);
                assert(a[i].squawk == 1200 && !a[i].onGround && a[i].lastUpdateMs == base + dt);
            }
        }
    }
}

static void the_anchor_is_the_first_call_not_the_latest_home() {
    sim_traffic::reset();
    std::vector<Aircraft> first, later;
    sim_traffic::generate(first, 33.0, -112.0, 1000);
    sim_traffic::generate(later, 33.0, -112.0, 1000);       // same instant: nothing has moved
    assert(first[0].lat == later[0].lat && first[0].lon == later[0].lon);
    // Home moves afterwards: the courses stay where they were seeded (only the longitude scaling
    // follows the new latitude), exactly as before.
    RefSim ref; std::vector<Aircraft> r;
    ref.run(r, 33.0, -112.0, 1000);
    ref.run(r, 40.0, -74.0, 61000);
    sim_traffic::generate(later, 40.0, -74.0, 61000);
    for (size_t i = 0; i < 8; ++i) assert(later[i].lat == r[i].lat && later[i].lon == r[i].lon);
}

static void they_actually_travel() {
    sim_traffic::reset();
    std::vector<Aircraft> t0, t1;
    sim_traffic::generate(t0, 33.0, -112.0, 0);
    sim_traffic::generate(t1, 33.0, -112.0, 3600000);       // one hour
    for (size_t i = 0; i < 8; ++i) {
        const double dlat = (t1[i].lat - t0[i].lat) * 111.0, dlon = (t1[i].lon - t0[i].lon) * 111.0 * cos(33.0 * M_PI / 180.0);
        const double km = sqrt(dlat * dlat + dlon * dlon), want = t0[i].gs * 1.852;
        assert(fabs(km - want) < want * 0.02);              // ground speed x one hour, to 2%
    }
}

// ---- backoff ---------------------------------------------------------------------------
static void failures_back_off_to_thirty_seconds_and_stay() {
    uint32_t b = 0;
    const uint32_t want[] = { 2000, 4000, 8000, 16000, 30000, 30000, 30000 };
    for (uint32_t w : want) { b = feed_backoff::after_failure(b); assert(b == w); }
}

static void refusals_back_off_to_five_minutes_and_never_past_it() {
    uint32_t b = 0;
    const uint32_t want[] = { 60000, 120000, 240000, 300000, 300000, 300000 };
    for (uint32_t w : want) { b = feed_backoff::after_refusal(b); assert(b == w); }
    // The old expression, for the record: 240000 doubled to 480000 (over its own limit) before
    // falling back to 300000. Whatever the starting point, the answer is now within the limit.
    for (uint32_t s : {0u, 1u, 59999u, 60000u, 150000u, 299999u, 300000u, 480000u, 4000000000u})
        assert(feed_backoff::after_refusal(s) <= 300000u && feed_backoff::after_refusal(s) >= 60000u);
}

int main() {
    same_traffic_as_the_inline_code();
    the_anchor_is_the_first_call_not_the_latest_home();
    they_actually_travel();
    failures_back_off_to_thirty_seconds_and_stay();
    refusals_back_off_to_five_minutes_and_never_past_it();
    printf("adsb pieces: all checks passed\n");
    return 0;
}

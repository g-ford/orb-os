#include "sim_traffic.h"
#include <math.h>
#include <stdio.h>

namespace {

struct SimAc { double lat0, lon0; float brgDeg, gsKt, altFt; };
SimAc    s_sim[sim_traffic::COUNT];
bool     s_init = false;
uint32_t s_t0 = 0;

}  // namespace

namespace sim_traffic {

void reset() { s_init = false; }

void generate(std::vector<Aircraft> &out, double homeLat, double homeLon, uint32_t nowMs) {
    if (!s_init) {
        s_init = true;
        s_t0 = nowMs;
        for (int i = 0; i < COUNT; ++i) {
            const float a = (float)i * 45.0f;
            const float rKm = 8.0f + (float)(i % 4) * 9.0f;
            s_sim[i].lat0   = homeLat + (double)(rKm / 111.0f) * cos(a * (float)M_PI / 180.0f);
            s_sim[i].lon0   = homeLon + (double)(rKm / 111.0f) * sin(a * (float)M_PI / 180.0f)
                              / cos(homeLat * (double)M_PI / 180.0);
            s_sim[i].brgDeg = fmodf(a + 115.0f, 360.0f);   // not radial: they cross the scope
            s_sim[i].gsKt   = 180.0f + (float)(i % 5) * 55.0f;
            s_sim[i].altFt  = 3500.0f + (float)i * 2600.0f;
        }
    }
    const float hrs = (float)(nowMs - s_t0) / 3600000.0f;
    out.clear();
    for (int i = 0; i < COUNT; ++i) {
        const float nm  = s_sim[i].gsKt * hrs;
        const float km  = nm * 1.852f;
        const float brg = s_sim[i].brgDeg * (float)M_PI / 180.0f;
        Aircraft a;
        char hexBuf[8]; snprintf(hexBuf, sizeof(hexBuf), "sim%03d", i);
        a.hex     = hexBuf;
        char callBuf[10]; snprintf(callBuf, sizeof(callBuf), "SIM%03d", i);
        a.flight  = callBuf;
        a.type    = "SIM";
        a.lat     = s_sim[i].lat0 + (double)(km / 111.0f) * cos(brg);
        a.lon     = s_sim[i].lon0 + (double)(km / 111.0f) * sin(brg)
                    / cos(homeLat * (double)M_PI / 180.0);
        a.altBaro = s_sim[i].altFt;
        a.onGround = false;
        a.track   = s_sim[i].brgDeg;
        a.gs      = s_sim[i].gsKt;
        a.baroRate = 0.0f;
        a.squawk  = 1200;
        a.seenPos = 0;
        a.lastUpdateMs = nowMs;
        out.push_back(a);
    }
}

}  // namespace sim_traffic

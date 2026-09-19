// Host test for src/core/ip_locate.{h,cpp}.   tests/run_ip_locate_test.sh
#include "ip_locate.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <thread>

using ip_locate::Fix;
using ip_locate::Mailbox;

static void a_real_response_parses() {
    Fix f;
    assert(ip_locate::parse(R"({"status":"success","city":"Phoenix","region":"Arizona","lat":33.4484,"lon":-112.074,"offset":-25200})", f));
    // ArduinoJson keeps the number as a float, so ~1e-6 degrees (15 cm) of it is not there; the
    // inline code this replaced read it the same way.
    assert(fabs(f.lat - 33.4484) < 1e-5 && fabs(f.lon - -112.074) < 1e-5);
    assert(strcmp(f.city, "Phoenix, Arizona") == 0);
    assert(f.hasOffset && f.offset == -25200);
}

static void optional_parts_may_be_missing() {
    Fix f;
    assert(ip_locate::parse(R"({"status":"success","lat":48.85,"lon":2.35})", f));
    assert(f.city[0] == '\0' && !f.hasOffset);                                  // no city, no timezone: still a position
    assert(ip_locate::parse(R"({"status":"success","city":"Reykjavik","lat":64.1,"lon":-21.9})", f));
    assert(strcmp(f.city, "Reykjavik") == 0);                                   // no region: no dangling ", "
    assert(ip_locate::parse(R"({"status":"success","lat":1,"lon":2,"offset":999999})", f));
    assert(!f.hasOffset);                                                       // an impossible UTC offset is ignored, not trusted
    assert(ip_locate::parse(R"({"status":"success","lat":1,"lon":2,"offset":50400})", f) && f.hasOffset);   // +14:00 is real
}

static void anything_unusable_is_refused_and_leaves_the_fix_alone() {
    Fix f; f.lat = 7; f.lon = 8;
    const char *bad[] = {
        "", "not json", "{}", R"({"status":"fail","message":"private range"})",
        R"({"status":"success"})",                                              // no coordinates
        R"({"status":"success","lat":91,"lon":0})", R"({"status":"success","lat":0,"lon":181})",
        R"({"status":"success","lat":-90.5,"lon":0})",
        R"({"lat":1,"lon":2})",                                                 // no status
    };
    for (const char *j : bad) { assert(!ip_locate::parse(j, f)); assert(f.lat == 7 && f.lon == 8); }
    assert(!ip_locate::parse(nullptr, f));
    Fix edge;                                                                   // the boundaries themselves are valid
    assert(ip_locate::parse(R"({"status":"success","lat":90,"lon":-180})", edge) && edge.lat == 90 && edge.lon == -180);
}

static void the_mailbox_walks_its_states() {
    Mailbox m; Fix out;
    assert(m.collect(out) == Mailbox::NONE);
    assert(!m.take_ask());                              // nothing asked yet
    assert(m.ask());
    assert(!m.ask());                                   // one in flight at a time
    assert(m.collect(out) == Mailbox::NONE);            // asked, not fetched
    assert(m.take_ask());
    assert(!m.take_ask());                              // the fetch is owned exactly once
    assert(!m.ask());
    assert(m.collect(out) == Mailbox::NONE);            // fetching
    Fix f; f.lat = 12.5; f.lon = -3.25; strcpy(f.city, "Somewhere"); f.hasOffset = true; f.offset = 3600;
    m.deliver(f);
    assert(!m.ask());                                   // delivered but not yet collected
    assert(m.collect(out) == Mailbox::OK);
    assert(out.lat == 12.5 && out.lon == -3.25 && strcmp(out.city, "Somewhere") == 0 && out.offset == 3600);
    assert(m.collect(out) == Mailbox::NONE);            // collected once only
    assert(m.ask());                                    // and the mailbox is free again
    assert(m.take_ask());
    m.fail();
    assert(m.collect(out) == Mailbox::FAILED);
    assert(m.collect(out) == Mailbox::NONE);
    assert(m.ask());
}

// The render core asking and collecting while the network core fetches and delivers, thousands
// of times. Every delivery must arrive whole (lat and lon were written together) and none may be
// lost or seen twice. Run under -fsanitize=thread in run_ip_locate_test.sh where available.
static void two_cores_never_see_a_torn_or_lost_result() {
    Mailbox m;
    const int ROUNDS = 20000;
    std::thread net([&] {
        int served = 0;
        while (served < ROUNDS) {
            if (!m.take_ask()) { std::this_thread::yield(); continue; }
            Fix f; f.lat = served; f.lon = -served; f.offset = served * 3; f.hasOffset = true;
            snprintf(f.city, sizeof(f.city), "city-%d", served);
            if (served % 7 == 3) m.fail(); else m.deliver(f);
            ++served;
        }
    });
    int oks = 0, fails = 0;
    for (int r = 0; r < ROUNDS; ++r) {
        while (!m.ask()) std::this_thread::yield();
        Fix out; Mailbox::Result res;
        while ((res = m.collect(out)) == Mailbox::NONE) std::this_thread::yield();
        if (r % 7 == 3) { assert(res == Mailbox::FAILED); ++fails; }
        else {
            assert(res == Mailbox::OK);
            char want[40]; snprintf(want, sizeof(want), "city-%d", r);
            assert(out.lat == r && out.lon == -r && out.offset == r * 3 && strcmp(out.city, want) == 0);
            ++oks;
        }
    }
    net.join();
    printf("two cores: %d deliveries + %d failures, none torn or lost\n", oks, fails);
}

int main() {
    a_real_response_parses();
    optional_parts_may_be_missing();
    anything_unusable_is_refused_and_leaves_the_fix_alone();
    the_mailbox_walks_its_states();
    two_cores_never_see_a_torn_or_lost_result();
    printf("ip_locate: all checks passed\n");
    return 0;
}

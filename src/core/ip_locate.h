#pragma once
// Where is this device, according to the network it is on (ip-api.com), split into the two
// halves that belong on different cores.
//
// The first-boot lookup used to run on the render core, from loop(): an HTTP GET with a 4 s
// connect and a 6 s read timeout, so a network that was slow or refused the request froze
// rendering, the knob and the 8 s long-press recovery for up to ten seconds, on the very first
// boot of a brand-new Orb. The fetch and parse are pure network and JSON, so they belong on
// core 0 with the other network work; APPLYING the answer writes the timezone string and
// repositions the scope, which are core 1's. The Mailbox is the hand-off between them.
#include <atomic>
#include <stddef.h>

namespace ip_locate {

struct Fix {
    double lat = 0, lon = 0;
    char   city[40] = "";        // "City, Region", empty when the service gave no city
    bool   hasOffset = false;    // UTC offset in seconds is present and sane
    long   offset = 0;
};

// Parse an ip-api.com response into a Fix. False for anything that is not a usable position:
// bad JSON, status != "success", coordinates out of range or absent.
bool parse(const char *json, Fix &out);

// One request in flight at a time. The render core asks and later collects; the network core
// takes the request, fetches, and delivers. Every method is safe to call from either core.
class Mailbox {
public:
    // Render core. False if a lookup is already in flight or waiting to be collected.
    bool ask();
    // Network core. True exactly once per ask(): this caller now owns the fetch.
    bool take_ask();
    // Network core. Finish the fetch, one way or the other.
    void deliver(const Fix &fix);
    void fail();

    enum Result { NONE, OK, FAILED };
    // Render core. NONE while nothing has finished. OK fills `out`; either way the mailbox is
    // free for the next ask().
    Result collect(Fix &out);

private:
    enum State { IDLE, ASKED, FETCHING, DONE, FAILED_ };
    std::atomic<int> m_state{IDLE};
    Fix              m_fix;      // written only while FETCHING, read only once DONE
};

}  // namespace ip_locate

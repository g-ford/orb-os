#pragma once
// How long adsb_task waits, on top of the poll interval, after the aircraft feed lets it down.
// Pure arithmetic, pulled out of the loop so the sequences can be read and tested.
#include <stdint.h>

namespace feed_backoff {

// A failed poll. This is usually the TLS handshake starving on a fragmented internal heap
// (SSL -32512), and hammering both hosts only fragments it further, so back off exponentially
// and let the heap coalesce: 2 s, 4 s, 8 s, 16 s, then 30 s. Reset to 0 on the first success.
inline uint32_t after_failure(uint32_t current) {
    return current == 0    ? 2000u
         : current < 15000 ? current * 2
                           : 30000u;
}

// The SERVER said no (a 4xx), which no amount of retrying changes: this is a rate limit, and
// the honest answer to being told we ask too often is to ask far less. In minutes, not
// seconds: 60 s, 2 min, 4 min, then 5 min and stay there.
//
// The 300 s ceiling is exact. The expression this replaced doubled anything under 300 s
// before capping, so a 240 s wait went to 480 s and only THEN fell back to 300, one wait that
// was longer than the limit it was written to enforce.
inline uint32_t after_refusal(uint32_t current) {
    const uint32_t doubled = current < 60000 ? 60000u : current * 2;
    return doubled > 300000u ? 300000u : doubled;
}

}  // namespace feed_backoff

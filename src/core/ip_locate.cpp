#include "ip_locate.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace ip_locate {

bool parse(const char *json, Fix &out) {
    if (!json) return false;
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    const char *status = doc["status"] | "";
    if (strcmp(status, "success") != 0) return false;
    const double la = doc["lat"] | 1000.0;
    const double lo = doc["lon"] | 1000.0;
    if (!(la >= -90 && la <= 90 && lo >= -180 && lo <= 180)) return false;

    Fix f;
    f.lat = la; f.lon = lo;
    const char *city   = doc["city"]   | "";
    const char *region = doc["region"] | "";
    if (city[0]) snprintf(f.city, sizeof(f.city), "%s%s%s", city, region[0] ? ", " : "", region);
    // The timezone follows the located region, not just the map centre. Anything outside the
    // real range of UTC offsets is treated as absent rather than trusted.
    const long off = doc["offset"] | 0x7FFFFFFFL;
    if (off != 0x7FFFFFFFL && off >= -50400 && off <= 50400) { f.hasOffset = true; f.offset = off; }
    out = f;
    return true;
}

bool Mailbox::ask() {
    int expected = IDLE;
    return m_state.compare_exchange_strong(expected, ASKED);
}

bool Mailbox::take_ask() {
    int expected = ASKED;
    return m_state.compare_exchange_strong(expected, FETCHING);
}

void Mailbox::deliver(const Fix &fix) {
    m_fix = fix;                                        // published by the release store below
    m_state.store(DONE, std::memory_order_release);
}

void Mailbox::fail() { m_state.store(FAILED_, std::memory_order_release); }

Mailbox::Result Mailbox::collect(Fix &out) {
    const int s = m_state.load(std::memory_order_acquire);
    if (s == DONE)    { out = m_fix; m_state.store(IDLE, std::memory_order_release); return OK; }
    if (s == FAILED_) {              m_state.store(IDLE, std::memory_order_release); return FAILED; }
    return NONE;
}

}  // namespace ip_locate

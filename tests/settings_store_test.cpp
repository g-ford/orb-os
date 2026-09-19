// Host test for src/platform/storage/settings_store.h.   tests/run_settings_store_test.sh
//
// BasicStore is templated on the preferences type, so an in-memory fake stands in for NVS.
#include "settings_store.h"

#include <assert.h>
#include <map>
#include <stdio.h>
#include <string>
#include <variant>

struct FakePrefs {
    static inline std::map<std::string, std::variant<int, bool, float, uint32_t, double, std::string>> disk;
    static inline std::string ns;
    static inline int opens = 0, closes = 0;
    static inline bool lastReadOnly = false;

    bool begin(const char *name, bool ro) { ns = name; lastReadOnly = ro; ++opens; return true; }
    void end() { ++closes; }
    bool isKey(const char *k) { return disk.count(k) != 0; }

    template <class T> T get(const char *k, T def) { auto it = disk.find(k); return it == disk.end() ? def : std::get<T>(it->second); }
    template <class T> size_t put(const char *k, T v) { disk[k] = v; return sizeof(T); }
    int getInt(const char *k, int d) { return get<int>(k, d); }
    bool getBool(const char *k, bool d) { return get<bool>(k, d); }
    float getFloat(const char *k, float d) { return get<float>(k, d); }
    uint32_t getUInt(const char *k, uint32_t d) { return get<uint32_t>(k, d); }
    double getDouble(const char *k, double d) { return get<double>(k, d); }
    std::string getString(const char *k, const char *d) { return get<std::string>(k, std::string(d)); }
    size_t putInt(const char *k, int v) { return put(k, v); }
    size_t putBool(const char *k, bool v) { return put(k, v); }
    size_t putFloat(const char *k, float v) { return put(k, v); }
    size_t putUInt(const char *k, uint32_t v) { return put(k, v); }
    size_t putDouble(const char *k, double v) { return put(k, v); }
    size_t putString(const char *k, const char *v) { return put(k, std::string(v)); }
};
using Store = settings::BasicStore<FakePrefs>;

static void the_namespace_is_the_one_every_deployed_orb_uses() {
    // If this string ever changes, every Orb already in the world loses its WiFi, location and
    // theme at the next update. It is asserted here so that takes a deliberate edit to a test.
    assert(std::string(settings::NAMESPACE) == "capsuleradar");
    { Store s; }
    assert(FakePrefs::ns == "capsuleradar");
}

static void missing_keys_read_their_declared_default() {
    FakePrefs::disk.clear();
    Store s(true);
    assert(s.get(settings::BRIGHT) == BRIGHTNESS_DEFAULT);
    assert(s.get(settings::VOL) == 60);
    assert(s.get(settings::SWEEP) == true);           // the ones that default ON
    assert(s.get(settings::AIRPORTS) == true);
    assert(s.get(settings::HIDE_GROUND) == false);
    assert(s.get(settings::RANGE_KM) == RANGE_KM_DEFAULT);
    assert(s.get(settings::IDLE_DIM_MS_) == (uint32_t)IDLE_DIM_MS);
    assert(s.get(settings::TZ) == std::string(TZ_STR));
    assert(s.get(settings::MAX_AC) == 12);
    assert(!s.has("homeLat"));
}

static void what_is_put_is_what_is_got_under_the_same_key() {
    FakePrefs::disk.clear();
    { Store w; w.put(settings::BRIGHT, 90); w.put(settings::MUTE, true); w.put(settings::PROX_KM, 12.5f);
      w.put(settings::HOME_LAT, 33.4484); w.put(settings::TZ, "PST8PDT,M3.2.0,M11.1.0"); w.put(settings::IDLE_DIM_MS_, 5000u); }
    Store r(true);
    assert(r.get(settings::BRIGHT) == 90);
    assert(r.get(settings::MUTE) == true);
    assert(r.get(settings::PROX_KM) == 12.5f);
    assert(r.get(settings::HOME_LAT) == 33.4484);
    assert(r.get(settings::TZ) == "PST8PDT,M3.2.0,M11.1.0");
    assert(r.get(settings::IDLE_DIM_MS_) == 5000u);
    assert(FakePrefs::disk.count("bright") && FakePrefs::disk.count("mute") && FakePrefs::disk.count("proxkm"));   // the on-disk names
}

static void reading_never_clamps_but_clamp_does() {
    FakePrefs::disk.clear();
    { Store w; w.put(settings::VOL, 250); }           // what an older build could have left behind
    Store r(true);
    assert(r.get(settings::VOL) == 250);              // comes back exactly as stored, as before
    assert(settings::VOL.clamp(250) == 100);
    assert(settings::VOL.clamp(-4) == 0);
    assert(settings::MAX_AC.clamp(0) == 1 && settings::MAX_AC.clamp(9999) == ADSB_MAX_AIRCRAFT);
    assert(settings::MIN_ALT_FT.clamp(70000) == 60000);
    assert(settings::ROT_DEG.clamp(400) == 359);
}

static void a_failed_write_is_visible_to_the_caller() {
    struct Failing : FakePrefs { size_t putDouble(const char *, double) { return 0; } };
    settings::BasicStore<Failing> s;
    assert(s.put(settings::HOME_LAT, 1.0) == 0);   // the setup page reports this as a failed save
}

static void end_is_idempotent_and_ok_reports_the_open() {
    FakePrefs::opens = FakePrefs::closes = 0;
    {
        Store s;
        assert(s.ok());
        s.end();                 // explicit early close, as the setup page does
        s.end();                 // and again: no double-close
    }                            // destructor: still no double-close
    assert(FakePrefs::opens == 1 && FakePrefs::closes == 1);
    struct Refuses : FakePrefs { bool begin(const char *, bool) { return false; } };
    settings::BasicStore<Refuses> bad;
    assert(!bad.ok());           // the setup page answers 500 on this
}

static void every_handle_is_closed() {
    FakePrefs::opens = FakePrefs::closes = 0;
    { Store a; Store b(true); (void)a; (void)b; }
    for (int i = 0; i < 5; ++i) { Store s; s.put(settings::UNITS, i % 3); }
    assert(FakePrefs::opens == FakePrefs::closes && FakePrefs::opens == 7);   // (the failing-write store above is counted before this resets)
}

int main() {
    the_namespace_is_the_one_every_deployed_orb_uses();
    missing_keys_read_their_declared_default();
    what_is_put_is_what_is_got_under_the_same_key();
    reading_never_clamps_but_clamp_does();
    a_failed_write_is_visible_to_the_caller();
    end_is_idempotent_and_ok_reports_the_open();
    every_handle_is_closed();
    printf("settings_store: all checks passed\n");
    return 0;
}

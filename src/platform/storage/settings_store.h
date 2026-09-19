#pragma once
// The one place the device's saved settings are named.
//
// Before this, the NVS namespace was a string literal at 48 call sites, each key was a string
// literal at every place it was read AND every place it was written, and each default and clamp
// range was restated beside every one of them: loadSettings(), the web handlers, and the
// Settings menu's host_set_*() each had their own copy. A key spelled differently at a read and
// a write saves a value that is never loaded, and nothing says so.
//
// Now each setting is declared once, below, with its key, default and allowed range, and every
// reader and writer goes through it. tests/test_settings_store.py fails on a "capsuleradar"
// literal anywhere else, and on a key that is duplicated or too long for NVS.
#include <stddef.h>
#include <stdint.h>
#include "config.h"

namespace settings {

// The NVS namespace. It is still "capsuleradar" and always will be: the product has been
// renamed twice (Plane Radar 2.0, Capsule Radar, The Orb OS) and this is the string every Orb
// already in the world has its WiFi credentials, location and theme saved under. Renaming it
// would factory-reset all of them at the next update. See the note in main.cpp.
constexpr const char *NAMESPACE = "capsuleradar";

// NVS refuses a key longer than 15 characters, and Preferences reports that as a failed write
// that nothing checks. tests/test_settings_store.py holds every key below to it.
constexpr int KEY_MAX = 15;

// A setting's identity, default and (for numbers) the range it may take. clamp() is what the
// web handlers and the Settings menu apply to a value BEFORE using and saving it; reading never
// clamps, so an old value already in NVS comes back exactly as it was stored.
struct Int    { const char *key; int      def; int lo, hi; int clamp(int v) const { return v < lo ? lo : v > hi ? hi : v; } };
struct Bool   { const char *key; bool     def; };
struct Float  { const char *key; float    def; };
struct UInt   { const char *key; uint32_t def; };
struct Double { const char *key; double   def; };
struct Str    { const char *key; const char *def; };

// ---- display and radar -------------------------------------------------------------
inline constexpr Int    BRIGHT       {"bright",     BRIGHTNESS_DEFAULT, 0, 255};   // the web page's range; the knob menu floors it at 8 (host_set_brightness)
inline constexpr Int    ROT_DEG      {"rotDeg",     0, 0, 359};
inline constexpr Int    THEME        {"theme",      4, 0, 0x7fffffff};   // range enforced by the theme code, not here
inline constexpr Bool   THEME_MIG_V2 {"themeMigV2", false};              // the one-time renumbering of the old themes
inline constexpr Float  RANGE_KM     {"rangeKm",    RANGE_KM_DEFAULT};
inline constexpr Int    MAX_AC       {"maxac",      12, 1, ADSB_MAX_AIRCRAFT};
inline constexpr Int    TRAIL_LEN    {"traillen",   2, 0, 3};
inline constexpr Int    MIN_ALT_FT   {"minalt",     0, 0, 60000};
inline constexpr Bool   SWEEP        {"sweep",      true};
inline constexpr Bool   AIRPORTS     {"airports",   true};
inline constexpr Bool   HIDE_GROUND  {"hideground", false};
inline constexpr Bool   MIL_ONLY     {"milonly",    false};
inline constexpr Bool   BIG_TEXT     {"bigtext",    false};
inline constexpr UInt   IDLE_DIM_MS_ {"idledim",    IDLE_DIM_MS};
inline constexpr Int    UNITS        {"units",      0, 0, 2};
inline constexpr Int    WX_UNITS     {"wxUnits",    0, 0, 2};
inline constexpr Int    WX_ZOOM      {"wxZoom2",    0, 0, 1};
inline constexpr Str    TZ           {"tz",         TZ_STR};

// ---- sound -------------------------------------------------------------------------
inline constexpr Int    VOL          {"vol",        60, 0, 100};
inline constexpr Bool   MUTE         {"mute",       false};
inline constexpr Bool   SND_RADAR    {"sndRadar",   false};
inline constexpr Bool   SND_CHIME    {"sndChime",   false};
inline constexpr Int    ALERT_MODE   {"alertmode",  2, 0, 2};
inline constexpr Float  PROX_KM      {"proxkm",     0.0f};
inline constexpr Int    CHIME_IDX    {"chimeIdx",   0, 0, 0x7fffffff};   // bounded by the chime list, not here

// ---- where it is standing ----------------------------------------------------------
inline constexpr Double HOME_LAT     {"homeLat",    HOME_LAT_DEFAULT};
inline constexpr Double HOME_LON     {"homeLon",    HOME_LON_DEFAULT};
inline constexpr Bool   LOC_SET      {"locSet",     false};              // its real default depends on homeLat; see loadSettings()
inline constexpr Bool   NEEDS_WIFI   {"needsWifiSetup", false};

// A handle on the namespace. Opens in the constructor and closes in the destructor, so no path
// can forget end() (a handle left open blocks the next writer). `Prefs` is a parameter only so
// the host test can drive this with an in-memory fake; on the device it is Preferences.
template <class Prefs>
class BasicStore {
public:
    explicit BasicStore(bool readOnly = false) : m_open(m_p.begin(NAMESPACE, readOnly)) {}
    ~BasicStore() { end(); }

    // False if the namespace would not open (NVS full or corrupt). Writes then fail and reads
    // return their defaults; callers that must tell the user, such as the setup page, ask.
    bool ok() const { return m_open; }
    // Close now instead of at the end of the scope. Idempotent, so it is safe alongside the
    // destructor; for the sites that close before doing something slow with the result.
    void end() { if (m_open) { m_p.end(); m_open = false; } }
    BasicStore(const BasicStore &) = delete;
    BasicStore &operator=(const BasicStore &) = delete;

    int      get(const Int &s)    { return m_p.getInt(s.key, s.def); }
    bool     get(const Bool &s)   { return m_p.getBool(s.key, s.def); }
    float    get(const Float &s)  { return m_p.getFloat(s.key, s.def); }
    uint32_t get(const UInt &s)   { return m_p.getUInt(s.key, s.def); }
    double   get(const Double &s) { return m_p.getDouble(s.key, s.def); }
    auto     get(const Str &s)    { return m_p.getString(s.key, s.def); }

    // Return what Preferences returns: the bytes written, and 0 when the write failed. The
    // setup page checks it, and a wrapper that swallowed it would turn a failed save into a
    // reported success.
    size_t put(const Int &s, int v)          { return m_p.putInt(s.key, v); }
    size_t put(const Bool &s, bool v)        { return m_p.putBool(s.key, v); }
    size_t put(const Float &s, float v)      { return m_p.putFloat(s.key, v); }
    size_t put(const UInt &s, uint32_t v)    { return m_p.putUInt(s.key, v); }
    size_t put(const Double &s, double v)    { return m_p.putDouble(s.key, v); }
    size_t put(const Str &s, const char *v)  { return m_p.putString(s.key, v); }

    bool has(const char *key) { return m_p.isKey(key); }
    // For the few keys that are not in the catalogue (WiFi backup, recents, firmware-seen).
    Prefs &raw() { return m_p; }

private:
    Prefs m_p;
    bool  m_open;
};

}  // namespace settings

#ifdef ARDUINO
#include <Preferences.h>
namespace settings { using Store = BasicStore<Preferences>; }
#endif

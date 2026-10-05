#pragma once
// A session-only, in-memory stand-in for NVS, for the native (desktop) sim build only -- there
// is no real NVS there and no reason to pretend settings persist across sim restarts. Device
// builds never see this file: ARDUINO is always defined there, and settings_descriptor.h's
// ARDUINO branch binds to the real settings::Store (Preferences) instead.
#include <map>
#include <stdint.h>
#include <string>
#include <variant>

namespace settings {

struct NativePrefs {
    static inline std::map<std::string, std::variant<int, bool, float, uint32_t, double, std::string>> values;

    bool begin(const char *, bool) { return true; }
    void end() {}
    bool isKey(const char *k) { return values.count(k) != 0; }

    template <class T> T get(const char *k, T def) { auto it = values.find(k); return it == values.end() ? def : std::get<T>(it->second); }
    template <class T> size_t put(const char *k, T v) { values[k] = v; return sizeof(T); }
    int         getInt(const char *k, int d)              { return get<int>(k, d); }
    bool        getBool(const char *k, bool d)            { return get<bool>(k, d); }
    float       getFloat(const char *k, float d)          { return get<float>(k, d); }
    uint32_t    getUInt(const char *k, uint32_t d)         { return get<uint32_t>(k, d); }
    double      getDouble(const char *k, double d)        { return get<double>(k, d); }
    std::string getString(const char *k, const char *d)   { return get<std::string>(k, std::string(d)); }
    size_t putInt(const char *k, int v)                   { return put(k, v); }
    size_t putBool(const char *k, bool v)                 { return put(k, v); }
    size_t putFloat(const char *k, float v)               { return put(k, v); }
    size_t putUInt(const char *k, uint32_t v)              { return put(k, v); }
    size_t putDouble(const char *k, double v)              { return put(k, v); }
    size_t putString(const char *k, const char *v)         { return put(k, std::string(v)); }
};

}  // namespace settings

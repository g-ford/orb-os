// tests/fake_prefs.h
// An in-memory stand-in for Arduino's Preferences, for host tests. settings::BasicStore is
// templated on the preferences type precisely so this can replace it with no NVS and no Arduino.
#pragma once
#include <map>
#include <stdint.h>
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

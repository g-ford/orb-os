#pragma once
// Presentation metadata for a setting, layered on top of settings_store.h's typed catalogue.
// A SettingDescriptor never owns storage: it points at one of the settings::Int/Bool/Float/Str
// constants that already live in settings_store.h, so settings_store stays the one place a
// setting's key, default and range are declared — tests/test_settings_store.py's guards keep
// protecting that, unchanged.
//
// get_int()/set_int() are the only functions that read or write through a descriptor; the
// on-device renderer and the web renderer both call them, never settings::Store() directly.
// They are templates on the Preferences type for the same reason settings::BasicStore<Prefs>
// is one: a host test can run them against an in-memory fake with no NVS and no Arduino.
//
// Float- and Str-backed settings have no Control mapping yet (no descriptor needs one): the
// switch below falls through to a no-op default rather than guessing at one.
#include "settings_store.h"

namespace settings {

enum class Control { Toggle, Slider, Enum, Text };
enum class StorageKind { Int, Bool, Float, Str };

struct StorageRef {
    StorageKind kind;
    union {
        const Int   *asInt;
        const Bool  *asBool;
        const Float *asFloat;
        const Str   *asStr;
    };
    // Member-init-list, not body assignment: a constexpr constructor for a union must
    // initialize its one active member in the init list. clang accepts the body-assignment
    // form too, as a C++20 extension (silently, under a warning) -- but the device build's
    // GCC (-std=gnu++17) rejects it outright, so this is the portable form for both.
    constexpr StorageRef(const Int *s)   : kind(StorageKind::Int),   asInt(s)   {}
    constexpr StorageRef(const Bool *s)  : kind(StorageKind::Bool),  asBool(s)  {}
    constexpr StorageRef(const Float *s) : kind(StorageKind::Float), asFloat(s) {}
    constexpr StorageRef(const Str *s)   : kind(StorageKind::Str),   asStr(s)   {}
};

struct SettingDescriptor {
    const char *label;
    Control control;
    StorageRef storage;
    const char *const *optionLabels = nullptr;   // Enum only: size == storage.asInt->hi - lo + 1
    const char *unitSuffix = nullptr;             // Slider only
    const char *note = nullptr;                   // optional short aside appended to the label on render,
                                                   // e.g. "(restarts the device)" -- both renderers append it
    void (*onChanged)(int value) = nullptr;       // optional: live-apply or other side effect
    int (*readLive)() = nullptr;                  // optional: the effective value, when it can
                                                   // differ from NVS (e.g. a theme override) --
                                                   // display_int() prefers this over get_int().
                                                   // Must stay within the descriptor's valid
                                                   // range (like get_int() does) -- display_int()
                                                   // does not clamp or bounds-check it, so an
                                                   // Enum's optionLabels[v - lo] lookup trusts it.
};

inline const char *key(const SettingDescriptor &d) {
    switch (d.storage.kind) {
        case StorageKind::Bool:  return d.storage.asBool->key;
        case StorageKind::Int:   return d.storage.asInt->key;
        case StorageKind::Float: return d.storage.asFloat->key;
        case StorageKind::Str:   return d.storage.asStr->key;
    }
    return "";
}

// Reads the current value as an int: Bool -> 0/1, Int -> itself. Never clamps, matching
// BasicStore::get()'s own contract — only clamp() (and so only set_int) clamps.
template <class Prefs>
int get_int(const SettingDescriptor &d) {
    BasicStore<Prefs> store(true);
    switch (d.storage.kind) {
        case StorageKind::Bool: return store.get(*d.storage.asBool) ? 1 : 0;
        case StorageKind::Int:  return store.get(*d.storage.asInt);
        default:                return 0;
    }
}

// Clamps (Int only), persists, then runs the descriptor's side effect — in that order, so
// onChanged always sees the value that was actually saved, never a rejected one.
template <class Prefs>
void set_int(const SettingDescriptor &d, int value) {
    int stored;
    switch (d.storage.kind) {
        case StorageKind::Bool:
            stored = value != 0;
            BasicStore<Prefs>(false).put(*d.storage.asBool, stored != 0);
            break;
        case StorageKind::Int:
            stored = d.storage.asInt->clamp(value);
            BasicStore<Prefs>(false).put(*d.storage.asInt, stored);
            break;
        default:
            return;
    }
    if (d.onChanged) d.onChanged(stored);
}

// The value to show, as opposed to the value to write. Prefers readLive() when the descriptor
// has one (a theme or other out-of-band source can make NVS not the effective value); falls
// back to get_int() otherwise. Writing always goes through set_int() regardless -- this is a
// display-only read.
template <class Prefs>
int display_int(const SettingDescriptor &d) {
    return d.readLive ? d.readLive() : get_int<Prefs>(d);
}

}  // namespace settings

// The non-template convenience form every call site outside a host test actually uses. Two
// bodies, never both compiled: `settings::Store` (BasicStore<Preferences>) only exists when
// ARDUINO is defined, and the native PlatformIO env never defines it -- `platform = native`,
// no `framework = arduino`, confirmed in platformio.ini. Nothing in this codebase has called
// settings::Store() from code that also builds natively before now, so native gets its own
// concrete type: a session-only in-memory stand-in with no claim to persist across sim
// restarts, same shape as tests/fake_prefs.h but living in production (platform/storage) since
// the real native sim binary links it, not just a host test.
#ifdef ARDUINO
#include <Preferences.h>
namespace settings {
inline int  get_int(const SettingDescriptor &d)            { return get_int<Preferences>(d); }
inline void set_int(const SettingDescriptor &d, int value) { set_int<Preferences>(d, value); }
inline int  display_int(const SettingDescriptor &d)        { return display_int<Preferences>(d); }
}
#else
#include "settings_native_prefs.h"
namespace settings {
inline int  get_int(const SettingDescriptor &d)            { return get_int<NativePrefs>(d); }
inline void set_int(const SettingDescriptor &d, int value) { set_int<NativePrefs>(d, value); }
inline int  display_int(const SettingDescriptor &d)        { return display_int<NativePrefs>(d); }
}
#endif

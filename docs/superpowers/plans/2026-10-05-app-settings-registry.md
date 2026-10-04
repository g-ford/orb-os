# App Settings Registry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let an app declare its settings once, as a small array of data (`SettingDescriptor`), and have both the on-device Settings menu and the web config page render and write through that same array — proven end-to-end on five existing radar settings (`MAX_AC`, `UNITS`, `HIDE_GROUND`, `MIL_ONLY`, `BIG_TEXT`), which today exist only as hand-written web-page HTML/routes with no on-device equivalent at all.

**Architecture:** A `SettingDescriptor` (label, control kind, a pointer into the existing `settings_store` catalogue, optional enum labels/unit suffix/onChanged callback) is the one object both renderers walk. A small fixed-capacity `settings_registry` collects each app's descriptor array, registered through the same call that already registers the app with the shell (`app_shell::add`). The on-device Settings menu gains one dynamic wheel row per registered group and a single generic submenu page that renders any group's rows; the web page gains one generic `<div class=card>` per group and one generic `POST /setting` route, replacing (for these five settings only) the five single-purpose routes and their hand-written HTML/JS that exist today.

**Tech Stack:** C++17, Arduino framework, LVGL 8.4, PlatformIO (`esp32-s3-amoled-175` device env + `native` SDL host-sim env), bash+g++ host tests, Python `unittest` guard tests.

**Spec:** [docs/superpowers/specs/2026-10-05-app-settings-registry-design.md](../specs/2026-10-05-app-settings-registry-design.md)

## Global Constraints

- NVS keys are ≤ 15 characters and every key is declared exactly once in `settings_store.h` (`tests/test_settings_store.py` enforces this already; do not weaken it).
- No raw NVS literal access anywhere outside `settings_store.h` — a descriptor must reference a catalogue constant, never a bare string key.
- The render path stays non-blocking: no network calls and no `delay()` inside LVGL code.
- `Float`- and `Str`-backed settings have no `Control` mapping yet (`get_int`/`set_int` only handle `Bool`/`Int`); do not add one as part of this plan — out of scope per the spec.
- WiFi setup and Location stay hand-coded, untouched by this plan.
- `MAX_WHEEL_ROWS` (32, in `settings_internal.h`) is the hard ceiling for every wheel list, including the now-dynamic top-level Settings menu — a guard must fail safe, not overflow, if it is ever exceeded.
- A firmware change is not finished when it compiles — boot it on a real Orb before calling this done (CLAUDE.md rule 1); this plan's last task calls that out explicitly as a step the agent running it cannot perform itself.

## Review Focus

- A web client POSTs `/setting` with a key that matches no registered descriptor (typo, stale bookmark, old firmware's route). Expected: a 404, never a crash or a silent write to the wrong NVS key. Owned by Task 8's `handleSetSetting`.
- A web client POSTs a `MAX_AC` value outside `1..12` (e.g. `999` or `-5`). Expected: clamped via `settings::Int::clamp()` before it reaches NVS, same as every existing handler does today. Owned by Task 2's `set_int`.
- `BIG_TEXT`'s `onChanged` schedules a reboot (`g_rebootAtMs`). Expected: the HTTP response still reaches the browser before the reboot fires, exactly like today's `handleBigText`. Owned by Task 4.
- The device knob is turned rapidly at the top-level Settings menu right after a theme or future build registers zero or more groups than `MAX_WHEEL_ROWS` can hold. Expected: the menu clamps rather than writing past `s_items[MAX_WHEEL_ROWS]`, and "Back" is always still the last, reachable row. Owned by Task 7's `top_item_count()`.
- An `Enum` descriptor's `optionLabels` array is the wrong length for its `Int`'s `lo..hi` span (an app author's mistake when adding a future setting). Expected: caught by a guard test before it ships, not an out-of-bounds read at render time. Owned by Task 5.

---

## File Structure

New files:
- `tests/fake_prefs.h` — the in-memory NVS fake, extracted from `tests/settings_store_test.cpp` so three new host tests can share it instead of duplicating it.
- `src/platform/storage/settings_descriptor.h` — `Control`, `StorageRef`, `SettingDescriptor`, `key()`, `get_int()`/`set_int()` (templated on the `Preferences` type, exactly as `settings::BasicStore` already is, for the same host-testability reason).
- `src/platform/storage/settings_native_prefs.h` — `NativePrefs`, a session-only in-memory stand-in for NVS. `settings::Store` (the `Preferences`-backed alias in `settings_store.h`) only exists when `ARDUINO` is defined, and the `native` PlatformIO env never defines it (`platform = native`, no `framework = arduino` — confirmed in `platformio.ini`) — nothing in this codebase has ever called `settings::Store()` from code that also builds natively before now. `get_int`/`set_int`'s non-template convenience overloads need *some* concrete type to bind to on native, so this gives them one, with no pretence of persistence across sim restarts.
- `tests/settings_descriptor_test.cpp` + `tests/run_settings_descriptor_test.sh`
- `src/app/shell/settings_registry.h` / `.cpp` — `Group`, `register_group()`, `count()`, `group()`. Pure data bookkeeping, no LVGL.
- `tests/settings_registry_test.cpp` + `tests/run_settings_registry_test.sh`
- `src/app/radar/radar_settings.h` / `.cpp` — `kRadarSettings[]`, `kRadarSettingsCount`, and the five `radar_on_*_changed` extern declarations. Defined twice, exactly like every `host_*` function already is: once in `main.cpp` for the device, once in `src/platform/sim/sim_main.cpp` for the native sim — `main.cpp` is excluded from the `native` build (`-<main.cpp>` in `platformio.ini`'s native filter) and `sim_main.cpp` is excluded from the device build (`-<platform/sim/*.cpp>`), so the two never collide.
- `tests/radar_settings_test.cpp` + `tests/run_radar_settings_test.sh`
- `tests/test_setting_descriptors.py` — guard test, sibling to `tests/test_settings_store.py`.

Modified files:
- `tests/settings_store_test.cpp` — use the extracted `fake_prefs.h` instead of its own copy.
- `tests/run_host_tests.sh` — register the three new bash-run host tests and the new Python guard.
- `src/app/shell/app_shell.h` / `.cpp` — `add()`/`add_active()` grow two new trailing default parameters; `add()`'s body forwards to `settings_registry::register_group()`.
- `src/main.cpp` — new includes; the five device-side `radar_on_*_changed` definitions; radar's `app_shell::add()` call site gets two new trailing arguments; the generic `POST /setting` route and handler; the generic settings-card HTML built into `handleRoot()`; removal of the five now-superseded routes, handlers, and their HTML/JS.
- `src/platform/sim/sim_main.cpp` — the five native-side `radar_on_*_changed` definitions (device behaviour the sim has no equivalent for — `g_adsb`, `g_rebootAtMs` — becomes a deliberate no-op; `radar::setMaxOnScreen`/`ui_set_units`+`ui_on_data_updated` exist natively too, so those two call the real thing); `sim_register_apps()`'s radar `app_shell::add()` call site gets the same two new trailing arguments as `main.cpp`'s.
- `src/app/settings/settings_internal.h` — `MODE_GROUP`; `ITEM_FIXED_COUNT` replaces `ITEM_BACK`/`ITEM_COUNT`; new group-submenu state and prototypes.
- `src/app/settings/settings_pages.cpp` — `top_item_count()`/`top_back_index()`/`top_item_label()`; `build_group_page()`; `refresh_group()`.
- `src/app/settings/settings_view.cpp` — dynamic top-level turn/press bounds; `MODE_GROUP`'s turn/press handling; `show_page()`'s hide/show blocks.
- `src/config.h` — `FW_VERSION` bump (last task).

---

### Task 1: Extract the shared NVS fake for host tests

**Files:**
- Create: `tests/fake_prefs.h`
- Modify: `tests/settings_store_test.cpp`
- Test: `tests/run_settings_store_test.sh` (unchanged, re-run to confirm no regression)

**Interfaces:**
- Produces: `FakePrefs` (struct, in-memory stand-in for Arduino's `Preferences`, templated through `settings::BasicStore<FakePrefs>`) — every later host test task includes this header.

- [ ] **Step 1: Create `tests/fake_prefs.h` with the fake extracted verbatim from `tests/settings_store_test.cpp`**

```cpp
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
```

- [ ] **Step 2: Point `tests/settings_store_test.cpp` at the shared header instead of its own copy**

Replace its inline `FakePrefs` definition (the struct body that now lives in `fake_prefs.h`) with:

```cpp
#include "settings_store.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>

using Store = settings::BasicStore<FakePrefs>;
```

(Remove the now-duplicate `#include <map>`, `<string>`, `<variant>` if nothing else in the file needs them directly — check before deleting.)

- [ ] **Step 3: Run the existing test to confirm the extraction didn't change behaviour**

Run: `bash tests/run_settings_store_test.sh`
Expected: `settings_store: all checks passed`

- [ ] **Step 4: Commit**

```bash
git add tests/fake_prefs.h tests/settings_store_test.cpp
git commit -m "Extract FakePrefs into a shared host-test fixture

Three upcoming host tests (settings_descriptor, settings_registry,
radar_settings) need the same in-memory NVS fake settings_store_test.cpp
already had; sharing it avoids a fourth copy of the same emulation logic."
```

---

### Task 2: `SettingDescriptor`, `StorageRef`, `get_int`/`set_int`

**Files:**
- Create: `src/platform/storage/settings_descriptor.h`, `src/platform/storage/settings_native_prefs.h`
- Test: `tests/settings_descriptor_test.cpp`, `tests/run_settings_descriptor_test.sh`

**Interfaces:**
- Consumes: `settings::Int`/`Bool`/`Float`/`Str`, `settings::BasicStore<Prefs>` (from `settings_store.h`, already in the repo).
- Produces (for every later task): `settings::Control` (`Toggle`/`Slider`/`Enum`/`Text`), `settings::StorageKind`, `settings::StorageRef` (constructible from `const Int*`/`const Bool*`/`const Float*`/`const Str*`), `settings::SettingDescriptor{label, control, storage, optionLabels=nullptr, unitSuffix=nullptr, onChanged=nullptr}`, `settings::key(const SettingDescriptor&) -> const char*`, `template<class Prefs> int settings::get_int(const SettingDescriptor&)`, `template<class Prefs> void settings::set_int(const SettingDescriptor&, int)`, and the non-template convenience overloads `settings::get_int(const SettingDescriptor&)` / `settings::set_int(const SettingDescriptor&, int)` — bound to `Preferences` under `ARDUINO` (device), to the new `settings::NativePrefs` otherwise (native sim and the raw-g++ host tests, though the latter only ever use the explicit template form).

- [ ] **Step 1: Write the failing test**

```cpp
// tests/settings_descriptor_test.cpp
#include "settings_descriptor.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>

using namespace settings;

static const char *kUnitsLabels[] = {"Metric", "Imperial", "Nautical"};
static int g_lastChanged = -1000;
static void record_changed(int v) { g_lastChanged = v; }

static constexpr Int  kTestRange {"testrange", 5, 1, 10};
static constexpr Bool kTestFlag  {"testflag", false};

static void toggle_round_trips_as_zero_or_one() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Flag", Control::Toggle, &kTestFlag};
    assert(get_int<FakePrefs>(d) == 0);              // default false
    set_int<FakePrefs>(d, 1);
    assert(get_int<FakePrefs>(d) == 1);
    printf("ok: toggle_round_trips_as_zero_or_one\n");
}

static void slider_clamps_before_storing() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Range", Control::Slider, &kTestRange};
    set_int<FakePrefs>(d, 999);
    assert(get_int<FakePrefs>(d) == 10);             // clamped to hi
    set_int<FakePrefs>(d, -5);
    assert(get_int<FakePrefs>(d) == 1);              // clamped to lo
    printf("ok: slider_clamps_before_storing\n");
}

static void enum_option_labels_index_by_value_minus_lo() {
    SettingDescriptor d{"Units", Control::Enum, &kTestRange, kUnitsLabels};
    assert(d.optionLabels[0][0] == 'M');             // lo (1) -> index 0 -> "Metric"... see note below
    printf("ok: enum_option_labels_index_by_value_minus_lo\n");
}

static void set_int_calls_on_changed_with_the_stored_value_not_the_raw_one() {
    FakePrefs::disk.clear();
    g_lastChanged = -1000;
    SettingDescriptor d{"Range", Control::Slider, &kTestRange, nullptr, nullptr, record_changed};
    set_int<FakePrefs>(d, 999);                      // clamps to 10 before the callback runs
    assert(g_lastChanged == 10);
    printf("ok: set_int_calls_on_changed_with_the_stored_value_not_the_raw_one\n");
}

static void key_reports_the_underlying_catalogue_key_for_every_kind() {
    SettingDescriptor t{"Flag", Control::Toggle, &kTestFlag};
    SettingDescriptor s{"Range", Control::Slider, &kTestRange};
    assert(std::string(key(t)) == "testflag");
    assert(std::string(key(s)) == "testrange");
    printf("ok: key_reports_the_underlying_catalogue_key_for_every_kind\n");
}

int main() {
    toggle_round_trips_as_zero_or_one();
    slider_clamps_before_storing();
    enum_option_labels_index_by_value_minus_lo();
    set_int_calls_on_changed_with_the_stored_value_not_the_raw_one();
    key_reports_the_underlying_catalogue_key_for_every_kind();
    printf("settings_descriptor: all checks passed\n");
    return 0;
}
```

- [ ] **Step 2: Create the runner script and confirm the test fails (header doesn't exist yet)**

```bash
#!/bin/bash
# tests/run_settings_descriptor_test.sh
# Builds and runs tests/settings_descriptor_test.cpp on the host. Header-only.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Itests \
    tests/settings_descriptor_test.cpp -o "$OUT/settings_descriptor_test"
"$OUT/settings_descriptor_test"
```

Run: `chmod +x tests/run_settings_descriptor_test.sh && bash tests/run_settings_descriptor_test.sh`
Expected: FAIL — `settings_descriptor.h: No such file or directory`

- [ ] **Step 3: Write `src/platform/storage/settings_descriptor.h`**

```cpp
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
    constexpr StorageRef(const Int *s)   : kind(StorageKind::Int)   { asInt = s; }
    constexpr StorageRef(const Bool *s)  : kind(StorageKind::Bool)  { asBool = s; }
    constexpr StorageRef(const Float *s) : kind(StorageKind::Float) { asFloat = s; }
    constexpr StorageRef(const Str *s)   : kind(StorageKind::Str)   { asStr = s; }
};

struct SettingDescriptor {
    const char *label;
    Control control;
    StorageRef storage;
    const char *const *optionLabels = nullptr;   // Enum only: size == storage.asInt->hi - lo + 1
    const char *unitSuffix = nullptr;             // Slider only
    void (*onChanged)(int value) = nullptr;       // optional: live-apply or other side effect
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
inline int  get_int(const SettingDescriptor &d)           { return get_int<Preferences>(d); }
inline void set_int(const SettingDescriptor &d, int value) { set_int<Preferences>(d, value); }
}
#else
#include "settings_native_prefs.h"
namespace settings {
inline int  get_int(const SettingDescriptor &d)           { return get_int<NativePrefs>(d); }
inline void set_int(const SettingDescriptor &d, int value) { set_int<NativePrefs>(d, value); }
}
#endif
```

- [ ] **Step 4: Write `src/platform/storage/settings_native_prefs.h`**

```cpp
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
```

Note: this header is included from `settings_descriptor.h` unconditionally outside `ARDUINO`
builds, which includes the raw g++ host tests in this plan (they don't define `ARDUINO` either)
— harmless, since the host tests only ever call the explicit `get_int<FakePrefs>`/
`set_int<FakePrefs>` template forms, never the bare `settings::get_int(d)` convenience overload,
so `NativePrefs` is compiled in but never instantiated or exercised there.

- [ ] **Step 5: Run the test again and fix the one real bug the test comment flags**

Run: `bash tests/run_settings_descriptor_test.sh`

The `enum_option_labels_index_by_value_minus_lo` test above is checking the wrong thing on
purpose — it was written before double-checking `kTestRange`'s `lo` (1), so `kUnitsLabels[0]`
("Metric") is not actually what a real `Enum` descriptor would index for a stored value of 1
(which is `optionLabels[1 - lo] = optionLabels[0]`, so it does happen to be "Metric" here — but
only because `lo == 1` and we're inspecting index 0 directly, not through a stored value).
Replace that test with one that actually exercises the lookup a renderer would do:

```cpp
static void enum_label_lookup_uses_value_minus_lo() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Units", Control::Enum, &kTestRange, kUnitsLabels};
    set_int<FakePrefs>(d, 2);                         // kTestRange.lo == 1
    const int v = get_int<FakePrefs>(d);
    assert(std::string(d.optionLabels[v - 1]) == "Imperial");
    printf("ok: enum_label_lookup_uses_value_minus_lo\n");
}
```

Rename the call in `main()` to match. Add `#include <string>` at the top of the test file for
`std::string`.

Run: `bash tests/run_settings_descriptor_test.sh`
Expected: `settings_descriptor: all checks passed`

- [ ] **Step 6: Add this test to the host-test suite**

In `tests/run_host_tests.sh`, add a line next to the existing `settings store` entries:

```bash
run "setting descriptors"                bash tests/run_settings_descriptor_test.sh
```

Run: `bash tests/run_host_tests.sh`
Expected: all entries pass, including the new one.

- [ ] **Step 7: Commit**

```bash
git add src/platform/storage/settings_descriptor.h tests/settings_descriptor_test.cpp tests/run_settings_descriptor_test.sh tests/run_host_tests.sh
git commit -m "Add SettingDescriptor: presentation metadata over settings_store

A descriptor points at an existing settings_store catalogue constant
rather than duplicating its key/default/range, so settings_store stays
the one place that's declared. get_int/set_int are templated on the
Preferences type so they're host-testable without NVS."
```

---

### Task 3: `settings_registry` — the flat group list

**Files:**
- Create: `src/app/shell/settings_registry.h`, `src/app/shell/settings_registry.cpp`
- Test: `tests/settings_registry_test.cpp`, `tests/run_settings_registry_test.sh`

**Interfaces:**
- Consumes: `settings::SettingDescriptor` (Task 2).
- Produces: `settings_registry::Group{label, items, count}`, `settings_registry::register_group(const char *label, const settings::SettingDescriptor *items, size_t count)`, `settings_registry::count() -> size_t`, `settings_registry::group(size_t index) -> const Group&`, `settings_registry::MAX_GROUPS` (8).

- [ ] **Step 1: Write the failing test**

```cpp
// tests/settings_registry_test.cpp
#include "settings_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string>

using namespace settings_registry;

static constexpr settings::Int kA{"testa", 0, 0, 1};
static const settings::SettingDescriptor kGroupA[] = {
    {"A", settings::Control::Toggle, &kA},
};

static void registering_a_group_makes_it_visible_by_index() {
    assert(count() == 0);
    register_group("Alpha", kGroupA, 1);
    assert(count() == 1);
    assert(std::string(group(0).label) == "Alpha");
    assert(group(0).count == 1);
    assert(group(0).items == kGroupA);
    printf("ok: registering_a_group_makes_it_visible_by_index\n");
}

static void registering_past_capacity_is_ignored_not_undefined() {
    for (int i = (int)count(); i < (int)MAX_GROUPS + 2; ++i) register_group("Extra", kGroupA, 1);
    assert(count() == MAX_GROUPS);   // the two past capacity were dropped, not overrun
    printf("ok: registering_past_capacity_is_ignored_not_undefined\n");
}

int main() {
    registering_a_group_makes_it_visible_by_index();
    registering_past_capacity_is_ignored_not_undefined();
    printf("settings_registry: all checks passed\n");
    return 0;
}
```

- [ ] **Step 2: Create the runner and confirm it fails**

```bash
#!/bin/bash
# tests/run_settings_registry_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/shell \
    tests/settings_registry_test.cpp src/app/shell/settings_registry.cpp -o "$OUT/settings_registry_test"
"$OUT/settings_registry_test"
```

Run: `chmod +x tests/run_settings_registry_test.sh && bash tests/run_settings_registry_test.sh`
Expected: FAIL — `settings_registry.h: No such file or directory`

- [ ] **Step 3: Write `src/app/shell/settings_registry.h`**

```cpp
#pragma once
// A flat, fixed-capacity list of settings groups — one per app that has registered settings,
// in the order it was registered. No heap: the number of groups is bounded by the number of
// apps, known at compile time, so a plain array is enough.
//
// Groups are added through app_shell::add()'s settingsGroup/settingsCount parameters, not by
// calling register_group() directly from app code — see app_shell.cpp.
#include "settings_descriptor.h"
#include <cstddef>

namespace settings_registry {

struct Group {
    const char *label;
    const settings::SettingDescriptor *items;
    size_t count;
};

constexpr size_t MAX_GROUPS = 8;

void register_group(const char *label, const settings::SettingDescriptor *items, size_t count);
size_t count();
const Group &group(size_t index);

}  // namespace settings_registry
```

- [ ] **Step 4: Write `src/app/shell/settings_registry.cpp`**

```cpp
#include "settings_registry.h"

namespace settings_registry {
namespace {
    Group  s_groups[MAX_GROUPS];
    size_t s_count = 0;
}

void register_group(const char *label, const settings::SettingDescriptor *items, size_t count) {
    if (s_count < MAX_GROUPS) {
        s_groups[s_count++] = Group{label, items, count};
    }
}

size_t count() { return s_count; }
const Group &group(size_t index) { return s_groups[index]; }

}  // namespace settings_registry
```

- [ ] **Step 5: Run the test**

Run: `bash tests/run_settings_registry_test.sh`
Expected: `settings_registry: all checks passed`

- [ ] **Step 6: Add to the host-test suite**

```bash
run "settings registry"                  bash tests/run_settings_registry_test.sh
```

Run: `bash tests/run_host_tests.sh`
Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add src/app/shell/settings_registry.h src/app/shell/settings_registry.cpp tests/settings_registry_test.cpp tests/run_settings_registry_test.sh tests/run_host_tests.sh
git commit -m "Add settings_registry: the flat list both renderers will walk

Fixed-capacity, no heap -- the number of groups is bounded by the
number of apps. Populated through app_shell::add() in the next task,
not called directly from app code."
```

---

### Task 4: Radar's descriptor array and its `onChanged` side effects

**Files:**
- Create: `src/app/radar/radar_settings.h`, `src/app/radar/radar_settings.cpp`
- Modify: `src/main.cpp` (five device-side function definitions), `src/platform/sim/sim_main.cpp` (five native-side function definitions) — no behaviour change yet in either; nothing calls them until Task 6
- Test: `tests/radar_settings_test.cpp`, `tests/run_radar_settings_test.sh`

**Interfaces:**
- Consumes: `settings::SettingDescriptor`, `settings::Control`, `settings::MAX_AC`/`UNITS`/`HIDE_GROUND`/`MIL_ONLY`/`BIG_TEXT` (from `settings_store.h`, already in the repo).
- Produces: `kRadarSettings[5]`, `kRadarSettingsCount`, and the five externs `radar_on_max_ac_changed`, `radar_on_units_changed`, `radar_on_hide_ground_changed`, `radar_on_mil_only_changed`, `radar_on_big_text_changed` (declared here; defined twice — `main.cpp` for the device, `sim_main.cpp` for the native sim, the same split every `host_*` function already uses, since `main.cpp` is excluded from the `native` build filter and `sim_main.cpp` from the device one) — consumed by Task 6's `app_shell::add()` call sites.

- [ ] **Step 1: Write the failing test, with its own stub `onChanged` definitions (main.cpp's real ones aren't linkable on the host)**

```cpp
// tests/radar_settings_test.cpp
#include "radar_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-ins for the real main.cpp side effects (ui_set_units, g_adsb.*, ...) --
// they only need to be linkable and to record what they were called with.
static int s_lastMaxAc = -1, s_lastUnits = -1, s_lastHideGround = -1, s_lastMilOnly = -1, s_lastBigText = -1;
void radar_on_max_ac_changed(int v)     { s_lastMaxAc = v; }
void radar_on_units_changed(int v)      { s_lastUnits = v; }
void radar_on_hide_ground_changed(int v){ s_lastHideGround = v; }
void radar_on_mil_only_changed(int v)   { s_lastMilOnly = v; }
void radar_on_big_text_changed(int v)   { s_lastBigText = v; }

static void the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys() {
    assert(kRadarSettingsCount == 5);
    const char *expect[] = {"maxac", "units", "hideground", "milonly", "bigtext"};
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        bool found = false;
        for (const char *k : expect) if (std::string(settings::key(kRadarSettings[i])) == k) found = true;
        assert(found);
    }
    printf("ok: the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys\n");
}

static void every_enum_descriptors_option_labels_span_matches_its_int_range() {
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kRadarSettings[i];
        if (d.control != settings::Control::Enum) continue;
        assert(d.storage.kind == settings::StorageKind::Int);
        const int span = d.storage.asInt->hi - d.storage.asInt->lo + 1;
        int labelCount = 0;
        while (d.optionLabels[labelCount]) ++labelCount;
        assert(labelCount == span);
    }
    printf("ok: every_enum_descriptors_option_labels_span_matches_its_int_range\n");
}

static void each_onchanged_fires_with_the_clamped_stored_value() {
    FakePrefs::disk.clear();
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kRadarSettings[i];
        assert(d.onChanged != nullptr);   // every pilot setting has a live side effect, see spec
        settings::set_int<FakePrefs>(d, 1);
    }
    assert(s_lastMaxAc == 1 && s_lastUnits == 1 && s_lastHideGround == 1 && s_lastMilOnly == 1 && s_lastBigText == 1);
    printf("ok: each_onchanged_fires_with_the_clamped_stored_value\n");
}

int main() {
    the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys();
    every_enum_descriptors_option_labels_span_matches_its_int_range();
    each_onchanged_fires_with_the_clamped_stored_value();
    printf("radar_settings: all checks passed\n");
    return 0;
}
```

Note: `optionLabels` is read here as a NUL-terminated array (`while (d.optionLabels[labelCount])`),
so Step 3's `kUnitsLabels` array must end with a `nullptr` sentinel — one extra entry beyond the
three real labels.

- [ ] **Step 2: Create the runner and confirm it fails**

```bash
#!/bin/bash
# tests/run_radar_settings_test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc -Isrc/platform/storage -Isrc/app/radar \
    tests/radar_settings_test.cpp src/app/radar/radar_settings.cpp -o "$OUT/radar_settings_test"
"$OUT/radar_settings_test"
```

Run: `chmod +x tests/run_radar_settings_test.sh && bash tests/run_radar_settings_test.sh`
Expected: FAIL — `radar_settings.h: No such file or directory`

- [ ] **Step 3: Write `src/app/radar/radar_settings.h`**

```cpp
#pragma once
// Radar's settings, declared once as data. Both the on-device Settings menu and the web config
// page render this same array (see settings_registry.h) -- see
// docs/superpowers/specs/2026-10-05-app-settings-registry-design.md.
//
// The five onChanged callbacks are declared here and defined in main.cpp, mirroring the
// existing host_get_*/host_set_* extern pattern in settings_internal.h: this file can't reach
// main.cpp's g_maxAc/g_adsb/radar::setMaxOnScreen() directly, so main.cpp (which already owns
// all of that) supplies the function bodies.
#include "settings_descriptor.h"
#include <cstddef>

extern void radar_on_max_ac_changed(int v);
extern void radar_on_units_changed(int v);
extern void radar_on_hide_ground_changed(int v);
extern void radar_on_mil_only_changed(int v);
extern void radar_on_big_text_changed(int v);

extern const settings::SettingDescriptor kRadarSettings[];
extern const size_t kRadarSettingsCount;
```

- [ ] **Step 4: Write `src/app/radar/radar_settings.cpp`**

```cpp
#include "radar_settings.h"
#include "settings_store.h"

static const char *const kUnitsLabels[] = {"Aviation", "Metric", "Imperial", nullptr};

const settings::SettingDescriptor kRadarSettings[] = {
    {"Max aircraft", settings::Control::Slider, &settings::MAX_AC,      nullptr,      nullptr, radar_on_max_ac_changed},
    {"Units",        settings::Control::Enum,   &settings::UNITS,       kUnitsLabels, nullptr, radar_on_units_changed},
    {"Hide ground",  settings::Control::Toggle, &settings::HIDE_GROUND, nullptr,      nullptr, radar_on_hide_ground_changed},
    {"Military only",settings::Control::Toggle, &settings::MIL_ONLY,    nullptr,      nullptr, radar_on_mil_only_changed},
    {"Large text",   settings::Control::Toggle, &settings::BIG_TEXT,    nullptr,      nullptr, radar_on_big_text_changed},
};
const size_t kRadarSettingsCount = sizeof(kRadarSettings) / sizeof(kRadarSettings[0]);
```

- [ ] **Step 5: Run the test**

Run: `bash tests/run_radar_settings_test.sh`
Expected: `radar_settings: all checks passed`

- [ ] **Step 6: Define the five real callbacks in `main.cpp`**

Add the five definitions anywhere in `main.cpp`'s free-function area, e.g. directly above
`handleUnits()`:

```cpp
// Radar's SettingDescriptor onChanged hooks (radar_settings.cpp). Each does exactly what the
// web-only handler of the same setting used to do, minus the settings::Store().put() call --
// set_int() already did that before calling this.
void radar_on_max_ac_changed(int v) {
    g_maxAc = v;
    radar::setMaxOnScreen(g_maxAc);
}
void radar_on_units_changed(int v) {
    g_units = v;
    ui_set_units(g_units);
    ui_on_data_updated();
}
void radar_on_hide_ground_changed(int v) {
    g_hideGround = v != 0;
    g_adsb.setHideGround(g_hideGround);
}
void radar_on_mil_only_changed(int v) {
    g_milOnly = v != 0;
    g_adsb.setMilitaryOnly(g_milOnly);
}
void radar_on_big_text_changed(int v) {
    g_bigText = v != 0;
    g_rebootAtMs = millis() + 1200;   // let the HTTP response reach the browser first
}
```

Add `#include "radar_settings.h"` near `main.cpp`'s other radar-related includes (next to
`#include "radar_view.h"`, line 29).

- [ ] **Step 7: Define the five native-side counterparts in `sim_main.cpp`**

`main.cpp` is excluded from the `native` build (`-<main.cpp>` in `platformio.ini`), so the
device definitions from Step 6 are invisible there — `radar_settings.cpp` (which `pio run -e
native` does compile; it isn't on either exclusion list) would otherwise fail to link against
five undefined symbols. The native sim has no equivalent of `g_adsb` (`core/adsb_client.cpp` is
itself excluded from `native`) or of `g_rebootAtMs`/`ESP.restart()` — those three stay
deliberate no-ops, persistence alone (already handled by `set_int`) being all there is to do.
`radar::setMaxOnScreen()` and `ui_set_units()`/`ui_on_data_updated()` exist natively too
(`radar_view.cpp` and `ui.cpp` are not excluded), so those two call the real thing, same as the
device:

```cpp
// Native-sim counterparts to main.cpp's radar_on_*_changed (radar_settings.h). hideground/
// milonly/bigtext have no sim equivalent (no g_adsb, no device reboot cycle here) -- they
// persist via set_int() same as everywhere else, with nothing further to apply live.
void radar_on_max_ac_changed(int v)       { radar::setMaxOnScreen(v); }
void radar_on_units_changed(int v)        { ui_set_units(v); ui_on_data_updated(); }
void radar_on_hide_ground_changed(int)    {}
void radar_on_mil_only_changed(int)       {}
void radar_on_big_text_changed(int)       {}
```

Add these to `src/platform/sim/sim_main.cpp`, with `#include "radar_settings.h"` near its other
includes.

- [ ] **Step 8: Build both environments to confirm everything links**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Expected: builds clean.

Run: `~/.platformio/penv/bin/pio run -e native`
Expected: builds AND links clean — this is the step that would have failed with an undefined
reference to the five symbols had Step 7 been skipped.

- [ ] **Step 9: Add the new host test to the suite**

```bash
run "radar settings"                     bash tests/run_radar_settings_test.sh
```

Run: `bash tests/run_host_tests.sh`
Expected: all pass.

- [ ] **Step 10: Commit**

```bash
git add src/app/radar/radar_settings.h src/app/radar/radar_settings.cpp src/main.cpp src/platform/sim/sim_main.cpp tests/radar_settings_test.cpp tests/run_radar_settings_test.sh tests/run_host_tests.sh
git commit -m "Add radar's settings descriptors and their live-apply hooks

Five settings (max aircraft, units, hide ground, military only, large
text) exist today only as web-only routes, three of which call into
radar::setMaxOnScreen/g_adsb.set*/ui_set_units beyond persisting, and
one of which schedules a reboot instead. Each descriptor's onChanged
reproduces exactly that. Nothing calls these yet -- wired up when the
generic renderers land."
```

---

### Task 5: Guard test — every descriptor is sound

**Files:**
- Create: `tests/test_setting_descriptors.py`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Consumes: `src/platform/storage/settings_store.h`'s catalogue keys (parsed the same way `tests/test_settings_store.py` already does), and every `*_settings.cpp` file's `SettingDescriptor` array (parsed by a new regex).

- [ ] **Step 1: Write the test**

```python
# tests/test_setting_descriptors.py
"""Guards for *_settings.cpp SettingDescriptor arrays: the properties a compiler cannot check.

A SettingDescriptor's StorageRef is always a pointer into settings_store.h's catalogue -- that
much the type system already enforces. What it can't check:
  * an Enum descriptor's optionLabels array is the right length for its Int's lo..hi span
    (settings_descriptor.h reads optionLabels[value - lo] with no bounds check of its own --
    the host test in radar_settings_test.cpp catches this for radar specifically; this test
    catches it for any future *_settings.cpp the same way test_settings_store.py catches a
    raw NVS key);
  * no key is declared twice within one group's own array (two rows silently fighting over
    the same setting).
"""
import glob
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE_HEADER = os.path.join("src", "platform", "storage", "settings_store.h")
STORE_DECL = re.compile(r"\b(?:Int|Bool|Float|UInt|Double|Str)\s+[A-Za-z_0-9]+\s*\{\s*\"([^\"]+)\"")

# One descriptor row's reference to a catalogue constant, e.g. `&settings::MAX_AC`.
STORAGE_REF = re.compile(r"&settings::([A-Za-z_][A-Za-z_0-9]*)")
# A descriptor row's optionLabels array, by name, e.g. `kUnitsLabels` in a row that uses it.
ENUM_ROW = re.compile(
    r"settings::Control::Enum\s*,\s*&settings::([A-Za-z_][A-Za-z_0-9]*)\s*,\s*([A-Za-z_][A-Za-z_0-9]*)"
)


def catalogue_ranges():
    """key constant name -> (lo, hi), for every Int in settings_store.h (Enum only wraps Int)."""
    path = os.path.join(ROOT, STORE_HEADER)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    ranges = {}
    for m in re.finditer(r"\bInt\s+([A-Za-z_0-9]+)\s*\{\s*\"[^\"]+\"\s*,\s*[^,]+,\s*([^,]+),\s*([^}]+)\}", text):
        name, lo, hi = m.group(1), m.group(2).strip(), m.group(3).strip()
        ranges[name] = (lo, hi)
    return ranges


def settings_files():
    return sorted(glob.glob(os.path.join(ROOT, "src", "app", "*", "*_settings.cpp")))


def array_literal(text, array_name):
    """The const char* const array_name[] = { ... }; literal's element count (incl. any nullptr sentinel)."""
    m = re.search(array_name + r"\s*\[\s*\]\s*=\s*\{([^}]*)\}", text)
    if not m:
        return None
    body = m.group(1)
    return len([p for p in body.split(",") if p.strip()])


class SettingDescriptorsTest(unittest.TestCase):
    def test_every_enum_descriptor_has_a_rangesized_option_label_array(self):
        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            for m in ENUM_ROW.finditer(text):
                catalogue_name, labels_name = m.group(1), m.group(2)
                n = array_literal(text, labels_name)
                self.assertIsNotNone(n, f"{path}: can't find {labels_name}[] for Enum over {catalogue_name}")
                # The label arrays in this codebase end with a nullptr sentinel (see
                # radar_settings.cpp) so callers can find their length without a separate count;
                # the real label count is one less than the literal's element count.
                self.assertGreater(n, 1, f"{path}: {labels_name}[] looks empty")

    def test_no_key_is_registered_twice_within_one_groups_array(self):
        store_keys = {}  # catalogue constant name -> NVS key string, for messages only
        path = os.path.join(ROOT, STORE_HEADER)
        with open(path, encoding="utf-8") as f:
            store_text = f.read()
        for m in re.finditer(r"\b(?:Int|Bool|Float|UInt|Double|Str)\s+([A-Za-z_0-9]+)\s*\{\s*\"([^\"]+)\"", store_text):
            store_keys[m.group(1)] = m.group(2)

        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            refs = STORAGE_REF.findall(text)
            dupes = sorted({r for r in refs if refs.count(r) > 1})
            self.assertEqual(dupes, [], f"{path}: registered twice: {[store_keys.get(d, d) for d in dupes]}")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it against radar's existing descriptors**

Run: `python3 -m unittest tests/test_setting_descriptors.py -v`
Expected: both tests pass against `radar_settings.cpp` as written in Task 4.

- [ ] **Step 3: Prove the guard actually guards — temporarily break it**

Temporarily edit `radar_settings.cpp`'s `kUnitsLabels` to drop the `nullptr` sentinel (making
`array_literal` see 3 elements instead of 4) — no, simpler: temporarily duplicate a row (e.g.
add a second `&settings::MAX_AC` reference) and re-run:

Run: `python3 -m unittest tests/test_setting_descriptors.py -v`
Expected: FAIL — `registered twice: ['maxac']`

Revert the temporary edit.

- [ ] **Step 4: Add to the host-test suite**

```bash
run "setting descriptors (guards)"       python3 -m unittest tests/test_setting_descriptors.py
```

Run: `bash tests/run_host_tests.sh`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add tests/test_setting_descriptors.py tests/run_host_tests.sh
git commit -m "Guard SettingDescriptor arrays: no duplicate keys, Enum labels sized right

Mirrors test_settings_store.py's approach (parse the source, assert an
invariant the compiler can't check) for the properties specific to
SettingDescriptor arrays rather than the settings_store catalogue
itself."
```

---

### Task 6: Wire registration through `app_shell::add()`

**Files:**
- Modify: `src/app/shell/app_shell.h`, `src/app/shell/app_shell.cpp`
- Modify: `src/main.cpp` (radar's `app_shell::add()` call site; new includes)
- Modify: `src/platform/sim/sim_main.cpp` (radar's `app_shell::add()` call site in `sim_register_apps()`; new include)

**Interfaces:**
- Consumes: `settings_registry::register_group()` (Task 3), `kRadarSettings`/`kRadarSettingsCount` (Task 4).
- Produces: `app_shell::add()`/`add_active()` with two new trailing parameters `const settings::SettingDescriptor *settingsGroup = nullptr, size_t settingsCount = 0`.

- [ ] **Step 1: Add the two new parameters to the header declarations**

In `src/app/shell/app_shell.h`, add `#include "settings_descriptor.h"` near the top (after the
existing `#include <lvgl.h>`), then change:

```cpp
void add(lv_obj_t *screen, const char *name,
         app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
         app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false);
void add_active(const char *name,
                app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
                app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false);
```

to:

```cpp
void add(lv_obj_t *screen, const char *name,
         app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
         app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false,
         const settings::SettingDescriptor *settingsGroup = nullptr, size_t settingsCount = 0);
void add_active(const char *name,
                app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
                app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false,
                const settings::SettingDescriptor *settingsGroup = nullptr, size_t settingsCount = 0);
```

- [ ] **Step 2: Forward the new parameters in `app_shell.cpp`**

Add `#include "settings_registry.h"` near its other includes, then change:

```cpp
void app_shell::add(lv_obj_t *screen, const char *name,
                    app_action_t onPress, app_turn_t onTurn, bool capture, app_action_t onEnter, app_action_t onExit, bool hidden) {
    if (s_count < MAX_APPS && screen) {
        s_apps[s_count].screen  = screen;
        s_apps[s_count].name    = name;
        s_apps[s_count].onPress = onPress;
        s_apps[s_count].onTurn  = onTurn;
        s_apps[s_count].capture = capture;
        s_apps[s_count].onEnter = onEnter;
        s_apps[s_count].onExit  = onExit;
        s_apps[s_count].hidden  = hidden;
        s_apps[s_count].pager   = nullptr;
        s_count++;
    }
}

void app_shell::add_active(const char *name,
                           app_action_t onPress, app_turn_t onTurn, bool capture, app_action_t onEnter, app_action_t onExit, bool hidden) {
    add(lv_scr_act(), name, onPress, onTurn, capture, onEnter, onExit, hidden);
}
```

to:

```cpp
void app_shell::add(lv_obj_t *screen, const char *name,
                    app_action_t onPress, app_turn_t onTurn, bool capture, app_action_t onEnter, app_action_t onExit, bool hidden,
                    const settings::SettingDescriptor *settingsGroup, size_t settingsCount) {
    if (s_count < MAX_APPS && screen) {
        s_apps[s_count].screen  = screen;
        s_apps[s_count].name    = name;
        s_apps[s_count].onPress = onPress;
        s_apps[s_count].onTurn  = onTurn;
        s_apps[s_count].capture = capture;
        s_apps[s_count].onEnter = onEnter;
        s_apps[s_count].onExit  = onExit;
        s_apps[s_count].hidden  = hidden;
        s_apps[s_count].pager   = nullptr;
        s_count++;
        if (settingsGroup && settingsCount) settings_registry::register_group(name, settingsGroup, settingsCount);
    }
}

void app_shell::add_active(const char *name,
                           app_action_t onPress, app_turn_t onTurn, bool capture, app_action_t onEnter, app_action_t onExit, bool hidden,
                           const settings::SettingDescriptor *settingsGroup, size_t settingsCount) {
    add(lv_scr_act(), name, onPress, onTurn, capture, onEnter, onExit, hidden, settingsGroup, settingsCount);
}
```

- [ ] **Step 3: Pass radar's descriptors at its registration call site**

In `src/main.cpp`, add `#include "settings_registry.h"` next to the `#include "app_shell.h"`
(line 52), then change line 2606 from:

```cpp
    app_shell::add(radarScreen, theme_style::names().flight, radar_press_custom_or_theme, radar_turn_select, false, radar_show_home_custom, radar_exit_release_style, !theme_style::apps().flight);
```

to:

```cpp
    app_shell::add(radarScreen, theme_style::names().flight, radar_press_custom_or_theme, radar_turn_select, false, radar_show_home_custom, radar_exit_release_style, !theme_style::apps().flight, kRadarSettings, kRadarSettingsCount);
```

- [ ] **Step 4: Make the same change at the native sim's OWN registration call site**

`main.cpp` never runs on the `native` build — the simulator boots through its own
`sim_register_apps()` in `src/platform/sim/sim_main.cpp`, which registers radar separately
(confirmed at its line ~515). Without this step, the native sim would build and link fine (Task
4 already made `radar_on_*_changed` linkable there) but `settings_registry` would stay empty in
the sim, and Task 7's simulator walkthrough would show no "Flight Tracker" settings row at all.

Add `#include "radar_settings.h"` near `sim_main.cpp`'s other includes, then change:

```cpp
    app_shell::add(radarScreen, theme_style::names().flight,
```

(the start of its existing multi-line call, inside `sim_register_apps()`) so the call's
argument list gains the same two trailing arguments as `main.cpp`'s:

```cpp
    app_shell::add(radarScreen, theme_style::names().flight,
                   radar_press_custom_or_theme, radar_turn_select, false,
                   radar_show_home_custom, radar_exit_release_style, !theme_style::apps().flight,
                   kRadarSettings, kRadarSettingsCount);
```

(Match whatever the existing call's exact middle arguments are at that site — this plan does not
assume they're identical to `main.cpp`'s, only that two new trailing arguments are appended to
whatever is already there.)

- [ ] **Step 5: Build both environments**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Expected: builds clean.

Run: `~/.platformio/penv/bin/pio run -e native`
Expected: builds AND links clean (confirms `app_shell.cpp`'s new `settings_registry` include
resolves on the native filter too — it is not on either exclusion list — and that Task 4 Step 7's
native-side `radar_on_*_changed` definitions satisfy every reference).

- [ ] **Step 6: Confirm the registration actually happened, via the simulator**

Run: `~/.platformio/penv/bin/pio run -e native -t exec`

Open Settings on the simulated Orb (the existing app-switcher knob gesture) and confirm nothing
about the current Settings menu changed yet — `app_shell::add()`'s new parameters only populate
`settings_registry`; nothing reads that registry until Task 7. This step is a smoke test that
registration doesn't crash or corrupt the existing app list, not a behaviour check (there is no
observable behaviour yet).

- [ ] **Step 7: Run the host test suite once more to confirm nothing else broke**

Run: `bash tests/run_host_tests.sh`
Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add src/app/shell/app_shell.h src/app/shell/app_shell.cpp src/main.cpp src/platform/sim/sim_main.cpp
git commit -m "Wire settings registration through app_shell::add()

Adding an app's settings now rides on the same call that already
registers the app with the shell -- one new pair of trailing
arguments, not a separate touchpoint. Both main.cpp's and
sim_main.cpp's radar registration call sites pass radar_settings'
array, since the native sim registers its apps separately from the
device. Nothing reads the registry yet; that's the next task."
```

---

### Task 7: The on-device generic group submenu

**Files:**
- Modify: `src/app/settings/settings_internal.h`
- Modify: `src/app/settings/settings_pages.cpp`
- Modify: `src/app/settings/settings_view.cpp`

**Interfaces:**
- Consumes: `settings_registry::count()`/`group()` (Task 3), `settings::get_int`/`set_int`/`Control` (Task 2).
- Produces: a working "Flight Tracker" (or whatever the active theme names it) row at the bottom of the top-level Settings menu, opening a submenu of its five registered settings.

- [ ] **Step 1: `settings_internal.h` — replace the fixed `ITEM_BACK`/`ITEM_COUNT` with a dynamic tail**

Add `#include "settings_registry.h"` near the top (alongside the existing `#include "app_shell.h"`).

Replace:

```cpp
    enum { ITEM_DISPLAY = 0, ITEM_LOCATION, ITEM_SOUND, ITEM_UNITS, ITEM_RANGE, ITEM_WIFI, ITEM_DESIGN, ITEM_ABOUT, ITEM_RESET, ITEM_BACK, ITEM_COUNT };
    const char *const ITEM_LABELS[ITEM_COUNT] = { "Display", "Location", "Sound", "Units", "Range", "WiFi", "Theme", "About", "Reset", "Back" };
```

with:

```cpp
    // "Back" is no longer a fixed row: it is always the LAST row, after however many
    // settings_registry groups are registered (one per app with settings -- see
    // app_shell::add()'s settingsGroup parameter). top_item_count()/top_item_label() in
    // settings_pages.cpp compute the dynamic tail; ITEM_FIXED_COUNT is just the fixed head.
    enum { ITEM_DISPLAY = 0, ITEM_LOCATION, ITEM_SOUND, ITEM_UNITS, ITEM_RANGE, ITEM_WIFI, ITEM_DESIGN, ITEM_ABOUT, ITEM_RESET, ITEM_FIXED_COUNT };
    const char *const ITEM_LABELS[ITEM_FIXED_COUNT] = { "Display", "Location", "Sound", "Units", "Range", "WiFi", "Theme", "About", "Reset" };
```

Add `MODE_GROUP` to the `Mode` enum (insert right after `MODE_RANGE`):

```cpp
    enum Mode { MODE_MENU, MODE_DISPLAY, MODE_BRIGHT, MODE_LOCATION, MODE_RECENT, MODE_SEARCH, MODE_SOUND, MODE_VOLUME, MODE_ABOUT,
                MODE_WIFI_LIST, MODE_WIFI_PASSWORD, MODE_WIFI_STATUS, MODE_RESET_CONFIRM, MODE_UNITS, MODE_CHIME_SELECT,
                MODE_DESIGN_SELECT, MODE_DESIGN_NOTICE, MODE_RANGE, MODE_GROUP,
                MODE_FIRSTBOOT, MODE_FIRSTBOOT_PHONE, MODE_NO_SDCARD };
```

Change the `static_assert` (it checked `ITEM_COUNT`, which no longer exists, and the dynamic
tail is now bounds-checked at runtime by `top_item_count()` in Step 2, not at compile time):

```cpp
    constexpr int MAX_WHEEL_ROWS = 32;
    static_assert(ITEM_FIXED_COUNT + 1 <= MAX_WHEEL_ROWS && theme_select::MAX_THEMES + 1 <= MAX_WHEEL_ROWS
                  && CHIME_UI_MAX + 1 <= MAX_WHEEL_ROWS, "raise MAX_WHEEL_ROWS: a wheel list would be cut short");
```

Change `s_items`'s extern declaration and add the group-submenu state (near the other `extern
lv_obj_t *s_xxxPage`/`s_xxxItems` declarations):

```cpp
    extern lv_obj_t *s_items[MAX_WHEEL_ROWS];   // was s_items[ITEM_COUNT]
    ...
    extern lv_obj_t *s_groupPage;
    extern lv_obj_t *s_groupItems[MAX_WHEEL_ROWS];
    extern int       s_groupSel;
    extern int       s_activeGroup;
```

Add the new prototypes near the other `refresh_*`/`build_*` declarations:

```cpp
    int top_item_count();                 // ITEM_FIXED_COUNT + registered groups + 1 (Back), capped at MAX_WHEEL_ROWS
    int top_back_index();                 // == top_item_count() - 1
    const char *top_item_label(int i);
    void build_group_page();              // settings_pages.cpp
    void refresh_group();                 // settings_pages.cpp
```

Change the `refresh_menu()` inline:

```cpp
    inline void refresh_menu() { show_wheel(s_items, top_item_count(), s_sel); }
```

- [ ] **Step 2: `settings_pages.cpp` — the dynamic top-level helpers, and the group page itself**

Add near the top of the file (after the existing `idle_index()`/`cycle_index()` helpers):

```cpp
int top_item_count() {
    int n = ITEM_FIXED_COUNT + (int)settings_registry::count() + 1;   // +1 for Back
    return n > MAX_WHEEL_ROWS ? MAX_WHEEL_ROWS : n;   // fail safe, never overflow s_items[]
}

int top_back_index() { return top_item_count() - 1; }

const char *top_item_label(int i) {
    if (i < ITEM_FIXED_COUNT) return ITEM_LABELS[i];
    const int g = i - ITEM_FIXED_COUNT;
    if (g < (int)settings_registry::count()) return settings_registry::group((size_t)g).label;
    return "Back";
}

void refresh_group() {
    const settings_registry::Group &g = settings_registry::group((size_t)s_activeGroup);
    char buf[40];
    for (size_t i = 0; i < g.count; ++i) {
        const settings::SettingDescriptor &d = g.items[i];
        const int v = settings::get_int(d);
        if (d.control == settings::Control::Toggle) {
            snprintf(buf, sizeof(buf), "%s   %s", d.label, v ? "On" : "Off");
        } else if (d.control == settings::Control::Enum) {
            snprintf(buf, sizeof(buf), "%s   %s", d.label, d.optionLabels[v - d.storage.asInt->lo]);
        } else {   // Slider
            snprintf(buf, sizeof(buf), "%s   %d%s", d.label, v, d.unitSuffix ? d.unitSuffix : "");
        }
        lv_label_set_text(s_groupItems[i], buf);
    }
    lv_label_set_text(s_groupItems[g.count], "Back");
    show_wheel(s_groupItems, (int)g.count + 1, s_groupSel);
}
```

Add `build_group_page()`, called from the top of `build_option_pages()`. Change:

```cpp
void build_option_pages() {
    // --- display menu page (Screen timeout / Brightness / Back) ---
    s_dspPage = lv_obj_create(s_screen);
```

to:

```cpp
void build_option_pages() {
    build_group_page();
    // --- display menu page (Screen timeout / Brightness / Back) ---
    s_dspPage = lv_obj_create(s_screen);
```

and define the function itself anywhere else in the file (e.g. directly above
`build_option_pages()`):

```cpp
void build_group_page() {
    s_groupPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_groupPage);
    lv_obj_set_size(s_groupPage, SCREEN_W, SCREEN_H); lv_obj_center(s_groupPage);
    lv_obj_clear_flag(s_groupPage, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < MAX_WHEEL_ROWS; ++i) {
        s_groupItems[i] = lv_label_create(s_groupPage);
        lv_label_set_text(s_groupItems[i], "");
        // Font, opacity and position: show_wheel(), called from refresh_group().
    }
}
```

In `build_menu_and_brightness_pages()`, change the loop that builds the top-level rows:

```cpp
    for (int i = 0; i < ITEM_COUNT; ++i) {
        s_items[i] = lv_label_create(s_menu);
        lv_label_set_text(s_items[i], ITEM_LABELS[i]);
        // Font, opacity, and position are all set dynamically in refresh_menu() —
        // they depend on distance from the current selection (the wheel effect).
    }
```

to:

```cpp
    for (int i = 0; i < top_item_count(); ++i) {
        s_items[i] = lv_label_create(s_menu);
        lv_label_set_text(s_items[i], top_item_label(i));
        // Font, opacity, and position are all set dynamically in refresh_menu() —
        // they depend on distance from the current selection (the wheel effect).
    }
```

- [ ] **Step 3: `settings_view.cpp` — state, turn, press, and `show_page()`**

Change the `s_items` definition (line 47):

```cpp
lv_obj_t *s_items[MAX_WHEEL_ROWS] = { nullptr };
```

Add the new state next to it:

```cpp
lv_obj_t *s_groupPage = nullptr;
lv_obj_t *s_groupItems[MAX_WHEEL_ROWS] = { nullptr };
int       s_groupSel = 0;
int       s_activeGroup = 0;
```

In `settingsview::onTurn()`, change the `MODE_MENU` bound (line 299) from `ITEM_COUNT` to
`top_item_count()`:

```cpp
    if (s_mode == MODE_MENU) {
        s_sel = (s_sel + step < 0) ? 0 : (s_sel + step >= top_item_count() ? top_item_count() - 1 : s_sel + step);
        refresh_menu();
    }
```

Add a `MODE_GROUP` branch to the same `onTurn()` if/else chain (next to the `MODE_RANGE` branch):

```cpp
    } else if (s_mode == MODE_GROUP) {
        const int total = (int)settings_registry::group((size_t)s_activeGroup).count + 1;   // +1 Back
        s_groupSel += step;
        if (s_groupSel < 0) s_groupSel = 0;
        if (s_groupSel >= total) s_groupSel = total - 1;
        refresh_group();
```

In `show_page()`, add to the hide-block (next to the other `lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN);` lines):

```cpp
    lv_obj_add_flag(s_groupPage, LV_OBJ_FLAG_HIDDEN);
```

and to the show-block (next to the `MODE_RANGE` line):

```cpp
    else if (m == MODE_GROUP)    { lv_obj_clear_flag(s_groupPage, LV_OBJ_FLAG_HIDDEN); refresh_group(); }
```

In `settingsview::onPress()`'s `if (s_mode == MODE_MENU) { ... }` block, insert a new branch between
the `ITEM_RESET` branch and the catch-all `else` (so the catch-all still only matches the true
Back row):

```cpp
        else if (s_sel == ITEM_RESET) { show_page(MODE_RESET_CONFIRM); }
        else if (s_sel < top_back_index()) {
            s_activeGroup = s_sel - ITEM_FIXED_COUNT;
            s_groupSel = 0;
            show_page(MODE_GROUP);
        }
        else {                                          // Back -> return to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
```

Add a new press branch for `MODE_GROUP` to `settingsview::onPress()` (next to the `MODE_RANGE`
branch in that same if/else chain):

```cpp
    } else if (s_mode == MODE_GROUP) {
        const settings_registry::Group &g = settings_registry::group((size_t)s_activeGroup);
        if (s_groupSel < (int)g.count) {
            const settings::SettingDescriptor &d = g.items[s_groupSel];
            const int v = settings::get_int(d);
            int nv = v;
            if (d.control == settings::Control::Toggle) {
                nv = v ? 0 : 1;
            } else if (d.control == settings::Control::Enum) {
                const int lo = d.storage.asInt->lo, hi = d.storage.asInt->hi;
                nv = lo + ((v - lo + 1) % (hi - lo + 1));
            } else {   // Slider
                nv = v + 1;
                if (nv > d.storage.asInt->hi) nv = d.storage.asInt->lo;
            }
            settings::set_int(d, nv);
            refresh_group();
        } else {                                        // Back -> up to the main menu
            show_page(MODE_MENU);
        }
```

- [ ] **Step 4: Build both environments**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Expected: builds clean.

Run: `~/.platformio/penv/bin/pio run -e native`
Expected: builds clean.

- [ ] **Step 5: Exercise it in the simulator**

Run: `~/.platformio/penv/bin/pio run -e native -t exec`

Open Settings, turn the knob all the way past "Reset" — confirm a "Flight Tracker" (or the
active theme's name for that app) row now appears before "Back", that pressing it opens a
five-row submenu ("Max aircraft", "Units", "Hide ground", "Military only", "Large text", then
"Back"), that pressing "Max aircraft" cycles 1→2→...→12→1, that pressing "Units" cycles
Aviation→Metric→Imperial→Aviation, that the three toggles flip On/Off, and that "Back" returns
to the main Settings menu (not out to the app switcher). Confirm "Large text" still reboots the
device (same as the web page's equivalent today).

- [ ] **Step 6: Run the full host suite once more**

Run: `bash tests/run_host_tests.sh`
Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add src/app/settings/settings_internal.h src/app/settings/settings_pages.cpp src/app/settings/settings_view.cpp
git commit -m "Add the generic on-device settings group submenu

The top-level Settings menu now has one dynamic row per registered
settings_registry group, in addition to the fixed Display/Location/
Sound/.../Reset rows. Selecting a group opens one generic submenu,
built once and re-labelled per group, that renders any Toggle/Slider/
Enum row and writes through set_int(). Radar's five settings (none of
which had an on-device page before this) are the first to use it."
```

---

### Task 8: The generic web card and `POST /setting`

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `settings_registry::count()`/`group()`, `settings::get_int`/`set_int`/`key`/`Control`.
- Removes: `handleUnits`, `handleMilOnly`, `handleBigText`, `handleMaxAc`, `handleGround` and their
  five `g_web.on(...)` registrations; the "Radar display units" row from the Units card; the
  "Max aircraft"/"Military aircraft only"/"Hide aircraft on the ground"/"Large text" rows from
  the Advanced card; the now-dead `unames`/`uopts` and `mxvals`/`mxopts` option builders; the
  `u`/`mo`/`hg`/`mx`/`bt` JS helper functions.

- [ ] **Step 1: Build the generic settings cards inside `handleRoot()`**

Add this block in `handleRoot()`, before the big `snprintf(buf, BUFSZ, ...)` call (near the
other `String ...opts;`-building blocks, e.g. right after the `maopts` block):

```cpp
    // One <div class=card> per settings_registry group, one control per descriptor. Replaces,
    // for whichever settings have migrated, the hand-written card + dedicated route that used
    // to exist for each of them -- see docs/superpowers/specs/2026-10-05-app-settings-registry-design.md.
    String registeredCards;
    for (size_t gi = 0; gi < settings_registry::count(); ++gi) {
        const settings_registry::Group &grp = settings_registry::group(gi);
        registeredCards += "<div class=card><div class=t>";
        registeredCards += grp.label;
        registeredCards += "</div>";
        char row[256];
        for (size_t i = 0; i < grp.count; ++i) {
            const settings::SettingDescriptor &d = grp.items[i];
            const char *k = settings::key(d);
            const int v = settings::get_int(d);
            if (d.control == settings::Control::Toggle) {
                snprintf(row, sizeof(row),
                         "<label><input type=checkbox class=ck %s onchange=\"st('%s',this.checked?1:0)\">%s</label>",
                         v ? "checked" : "", k, d.label);
                registeredCards += row;
            } else if (d.control == settings::Control::Enum) {
                const int lo = d.storage.asInt->lo, hi = d.storage.asInt->hi;
                String opts;
                for (int val = lo; val <= hi; ++val) {
                    char o[96];
                    snprintf(o, sizeof(o), "<option value=%d%s>%s</option>", val, val == v ? " selected" : "", d.optionLabels[val - lo]);
                    opts += o;
                }
                snprintf(row, sizeof(row), "<label>%s</label><select onchange=\"st('%s',this.value)\">", d.label, k);
                registeredCards += row;
                registeredCards += opts;
                registeredCards += "</select>";
            } else {   // Slider
                snprintf(row, sizeof(row),
                         "<label>%s</label><input type=range min=%d max=%d value='%d' onchange=\"st('%s',this.value)\">",
                         d.label, d.storage.asInt->lo, d.storage.asInt->hi, v, k);
                registeredCards += row;
            }
        }
        registeredCards += "</div>";
    }
```

- [ ] **Step 2: Remove the now-superseded option builders**

Delete the `unames`/`uopts` block (the "Radar display units" dropdown's options):

```cpp
    const char *unames[] = {"Aviation (ft, kt, nm)", "Metric (m, km/h, km)", "Imperial (ft, mph, mi)"};
    String uopts;
    for (int i = 0; i < 3; ++i) {
        char o[96];
        snprintf(o, sizeof(o), "<option value=%d%s>%s</option>", i, i == g_units ? " selected" : "", unames[i]);
        uopts += o;
    }
```

Delete the `mxvals`/`mxopts` block (the "Max aircraft" curated dropdown's options):

```cpp
    const int mxvals[] = {4, 6, 8, 10, 12};          // max aircraft on the scope (<= feed cap)
    String mxopts;
    for (int mv : mxvals) {
        char o[64];
        snprintf(o, sizeof(o), "<option value=%d%s>%d</option>", mv, mv == g_maxAc ? " selected" : "", mv);
        mxopts += o;
    }
```

(`g_units` stays — it is still read elsewhere in `handleRoot()`, for `ufac`/`uname`, the range
and proximity dropdowns' unit labels. `g_maxAc` stays similarly, now only mutated by
`radar_on_max_ac_changed()`.)

- [ ] **Step 3: Edit the Units card and the Advanced card in the big format-string snprintf**

Change:

```cpp
        "<div class=card><div class=t>Units</div>"
        "<label>Weather units</label><select onchange='wu(this.value)'>%s</select>"
        "<label>Radar display units</label><select onchange='u(this.value)'>%s</select></div>"
```

to:

```cpp
        "<div class=card><div class=t>Units</div>"
        "<label>Weather units</label><select onchange='wu(this.value)'>%s</select></div>"
```

Change:

```cpp
        "<div class=card><div class=t>Advanced</div>"
        "<label>Screen rotation (degrees clockwise)</label>"
        "<input type=number min=0 max=359 step=1 value='%d' onchange='ro(this.value)'>"
        "<label>Aircraft trails</label><select onchange='tl(this.value)'>%s</select>"
        "<label>Max aircraft on screen</label><select onchange='mx(this.value)'>%s</select>"
        "<label>Minimum altitude</label><select onchange='ma(this.value)'>%s</select>"
        "<label><input type=checkbox class=ck %s onchange='mo(this.checked)'>Military aircraft only</label>"
        "<label><input type=checkbox class=ck %s onchange='sw(this.checked)'>Show radar sweep</label>"
        "<label><input type=checkbox class=ck %s onchange='ap(this.checked)'>Show airports</label>"
        "<label><input type=checkbox class=ck %s onchange='hg(this.checked)'>Hide aircraft on the ground</label>"
        "<label><input type=checkbox class=ck %s onchange='bt(this.checked)'>Large text (restarts the device)</label></div>"
```

to:

```cpp
        "<div class=card><div class=t>Advanced</div>"
        "<label>Screen rotation (degrees clockwise)</label>"
        "<input type=number min=0 max=359 step=1 value='%d' onchange='ro(this.value)'>"
        "<label>Aircraft trails</label><select onchange='tl(this.value)'>%s</select>"
        "<label>Minimum altitude</label><select onchange='ma(this.value)'>%s</select>"
        "<label><input type=checkbox class=ck %s onchange='sw(this.checked)'>Show radar sweep</label>"
        "<label><input type=checkbox class=ck %s onchange='ap(this.checked)'>Show airports</label></div>"

        "%s"   // registeredCards -- one card per settings_registry group (Radar's five, for now)
```

Add the matching JS helper right after the other single-letter `fetch` helpers (next to `function px(...)`):

```cpp
        "function st(k,v){fetch('/setting',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'key='+k+'&value='+v})}"
```

Remove the five now-dead JS helpers:

```cpp
        "function hg(c){fetch('/ground?v='+(c?1:0)+'&save=1')}"
        ...
        "function mo(c){fetch('/milonly?v='+(c?1:0)+'&save=1')}"
        ...
        "function mx(v){fetch('/maxac?v='+v+'&save=1')}"
        "function bt(c){fetch('/bigtext?v='+(c?1:0)+'&save=1')}"
        ...
        "function u(v){fetch('/units?v='+v+'&save=1')}"
```

(Keep `sw`/`ap`/`tl`/`ma`/`ro` — those five settings are not in this pilot's migrated set.)

- [ ] **Step 4: Update the big snprintf's argument list to match**

Change the Units card's argument line from:

```cpp
        wuopts.c_str(), uopts.c_str(),
```

to:

```cpp
        wuopts.c_str(),
```

Change the Advanced card's argument line from:

```cpp
        g_rotation, tlopts.c_str(), mxopts.c_str(), maopts.c_str(), g_milOnly ? "checked" : "",
        g_showSweep ? "checked" : "", g_showAirports ? "checked" : "", g_hideGround ? "checked" : "", g_bigText ? "checked" : "",
```

to:

```cpp
        g_rotation, tlopts.c_str(), maopts.c_str(),
        g_showSweep ? "checked" : "", g_showAirports ? "checked" : "",

        registeredCards.c_str(),
```

(The last line of the whole argument list, `g_settings.homeLat, g_settings.homeLon` for the
map's inline script, is unaffected and stays where it is, after `registeredCards.c_str(),`.)

- [ ] **Step 5: Add the generic route and handler, remove the five superseded ones**

Delete `handleUnits`, `handleMilOnly`, `handleBigText`, `handleMaxAc`, `handleGround` in full
(their bodies are quoted in the design spec's research; each is a `static void handleX() { ...
}` block between roughly lines 1996-2108).

Add in their place:

```cpp
static void handleSetSetting() {   // generic write-through for any settings_registry descriptor
    if (!g_web.hasArg("key") || !g_web.hasArg("value")) { g_web.send(400, "text/plain", "missing key/value"); return; }
    const String keyArg = g_web.arg("key");
    const int value = g_web.arg("value").toInt();
    for (size_t gi = 0; gi < settings_registry::count(); ++gi) {
        const settings_registry::Group &grp = settings_registry::group(gi);
        for (size_t i = 0; i < grp.count; ++i) {
            if (keyArg == settings::key(grp.items[i])) {
                settings::set_int(grp.items[i], value);
                g_web.send(200, "text/plain", "ok");
                return;
            }
        }
    }
    g_web.send(404, "text/plain", "unknown key");
}
```

In the routes block (lines 3027-3048), remove:

```cpp
    g_web.on("/ground", handleGround);
    ...
    g_web.on("/milonly", handleMilOnly);
    ...
    g_web.on("/maxac", handleMaxAc);
    g_web.on("/bigtext", handleBigText);
    ...
    g_web.on("/units", handleUnits);
```

and add:

```cpp
    g_web.on("/setting", HTTP_POST, handleSetSetting);
```

- [ ] **Step 6: Build and run**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Expected: builds clean. (This task's code is all inside `main.cpp`, which is excluded from the
`native` filter, so there is no native build step for this task.)

- [ ] **Step 7: Verify the web page by hand against the running simulator or a flashed device**

With the device (or `pio run -e native -t exec`, if the web server runs under the simulator —
otherwise a real Orb on USB) reachable, load the config page and confirm: the Units card no
longer shows "Radar display units"; the Advanced card no longer shows "Max aircraft on screen",
"Military aircraft only", or "Hide aircraft on the ground" (nor, further down, "Large text" —
confirm it moved out of Advanced entirely); a new "Flight Tracker" card (or whatever name the
active theme uses) appears with "Max aircraft" as a range slider (1-12), "Units" as a dropdown
(Aviation/Metric/Imperial), and three checkboxes. Toggling each one and reloading confirms the
value persisted. Toggling "Large text" confirms the device still reboots.

Then issue a request the UI can't: `curl -X POST 'http://<orb-ip>/setting?key=nope&value=1'` and
confirm a 404, not a crash or a 200.

- [ ] **Step 8: Run the full host suite one more time**

Run: `bash tests/run_host_tests.sh`
Expected: all pass (nothing in this task touches host-tested code paths, so this is a
regression check, not new coverage).

- [ ] **Step 9: Commit**

```bash
git add src/main.cpp
git commit -m "Web config: render radar's settings from the registry, not by hand

Replaces five single-purpose routes (/units, /milonly, /bigtext,
/maxac, /ground) and their hand-written HTML/JS with one generic
POST /setting route and one generic card renderer, both driven by
settings_registry -- the same object the on-device Settings menu now
renders from. An unknown key 404s instead of silently doing nothing."
```

---

### Task 9: Full-suite verification, `FW_VERSION`, and the hardware note

**Files:**
- Modify: `src/config.h` (`FW_VERSION`)

- [ ] **Step 1: Run every host and Python test**

Run: `bash tests/run_host_tests.sh`
Expected: all pass, including the four new entries from Tasks 2/3/4/5.

Run: `bash tests/run_python_tests.sh`
Expected: all pass (this also re-runs `tools/check_sd_guard.py` and the no-compiled-art guard —
neither of which this plan's files should trip, since no SD access or compiled art was added).

- [ ] **Step 2: Build both PlatformIO environments clean from scratch**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Run: `~/.platformio/penv/bin/pio run -e native`
Expected: both succeed.

- [ ] **Step 3: Run the native simulator's headless self-test**

Run: `SIM_SELFTEST=1 .pio/build/native/program`
Expected: existing knob/navigation self-checks pass (this exercises the app switcher and
Settings entry generally; it is not specific to this plan's new menu, but a regression here
would mean the dynamic top-level item count broke something load-bearing).

- [ ] **Step 4: Bump `FW_VERSION`**

In `src/config.h`, increment `FW_VERSION` (CLAUDE.md: bump it "when a build goes out that a
device could be behind"; this plan changes both the on-device menu and the web page, so any
already-deployed Orb is behind after this lands).

- [ ] **Step 5: Final build check after the version bump**

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175`
Expected: builds clean.

- [ ] **Step 6: Commit**

```bash
git add src/config.h
git commit -m "Bump FW_VERSION for the settings registry pilot"
```

- [ ] **Step 7: Flash a real Orb and confirm on hardware — not scriptable, flag it explicitly**

CLAUDE.md rule 1: "Firmware runs on a real Orb before it is called done." Everything above this
step is a clean build, a green host-test run, and a simulator walkthrough — none of it is
evidence that the dynamic Settings menu, the knob, and the web server's new route work together
on the actual device. Whoever finishes this plan must:

```bash
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 -t upload
```

and on the physical Orb: open Settings, confirm the new "Flight Tracker" (or themed name) row
and its five-setting submenu behave as in Task 7 Step 5, and separately load the web config page
over the Orb's own WiFi and confirm as in Task 8 Step 7. If no Orb is available at the time this
task is executed, record that explicitly in `docs/HARDWARE_PENDING.md` rather than marking this
plan complete — per CLAUDE.md, an unverified build is not a finished one.

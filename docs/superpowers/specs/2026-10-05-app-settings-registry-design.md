# A shared settings registry for apps, Settings and the web page

Status: approved by the user 2026-10-05. Date: 2026-10-05.

## Intent

Adding an app's settings today means touching three places by hand: the on-device Settings
menu (a hand-built LVGL page in `settings_pages.cpp`, a `Mode` enum entry, a parallel label
array), the web config page (a hand-built HTML card plus a dedicated REST route per setting in
`main.cpp`), and `settings_store.h` for the NVS declaration itself. The first two are kept in
step by hand — commit `3f48a84` describes the web page as made to "mirror the on-device
Settings menu," which is a one-time act, not an invariant anything enforces.

The user's words: "I want to improve apps registering their settings and having it as part of
a 'setting object'. Then the setting app can render this on device and the web page can render
differently, but they are always using the same object. This way when adding a new app I don't
really need to update the settings and web page every time."

Decided in review:
- WiFi setup and Location (network scan, geocoding search) stay hand-coded, outside this system
  — they are interactions, not settings.
- The on-device Settings menu reorganizes to one submenu per app (Display, Sound, Radar,
  Weather, ... each app's settings together), replacing today's cross-cutting UI categories.
- Settings not owned by any one app (brightness, idle timeout, auto-cycle, theme, master
  volume) register as a "system" pseudo-group through the same mechanism real apps use.
- A descriptor references its `settings_store` catalogue entry directly (a small tagged union),
  not a get/set closure pair — this keeps `settings_store` the sole storage authority the
  existing guard test already protects, with no heap or `std::function` overhead.
- Registration rides on the shell's existing app-list entry (one field added, pointing at the
  app's descriptor array) rather than a new separate registry file.
- Scope is one pilot app end-to-end, not a one-pass migration of every existing setting.

## What is there today

`settings_store.h` (`src/platform/storage/settings_store.h`) is already a declarative, typed
catalogue — `inline constexpr Int BRIGHT{"bright", BRIGHTNESS_DEFAULT, 0, 255};` and five
sibling structs (`Bool`, `Float`, `UInt`, `Double`, `Str`) — and `tests/test_settings_store.py`
guards it: every key unique, every key ≤ 15 chars, the namespace literal lives only in the
header, and no `.getInt("literal")`-style call bypasses the catalogue constant anywhere under
`src/`. That part is sound and this design keeps it as-is.

What the catalogue does *not* carry is presentation metadata — a label, which screen it
belongs to, what control renders it, or (for something like units) the text for each option.
That metadata exists only informally, duplicated in two places:

- **On-device** (`src/app/settings/`): `settings_internal.h` declares one `Mode` enum value per
  screen and parallel label arrays (`ITEM_LABELS`, `IDLE_LABELS`, ...); `settings_pages.cpp`
  hand-builds each screen's LVGL rows in `build_option_pages()` and formats each row's text with
  `snprintf` in functions like `refresh_units()`/`refresh_range()`. All of it calls `host_get_*`
  / `host_set_*` externs that are implemented in `main.cpp`.
- **Web** (`src/main.cpp`): `handleRoot()` builds the whole page as one large hand-written
  HTML/CSS/JS literal, one `<div class=card>` per UI category with a bespoke control per
  setting; each setting gets its own REST route (`/bright`, `/vol`, `/units`, `/sweep`, ...),
  registered individually, each with its own handler calling `settings::Store().put(...)`.

No app folder (`radar`, `weather`, `intel`, `ticker`, ...) touches `settings_store` or NVS
directly today — `main.cpp` owns every persisted global, and settings are grouped by UI category
(Display, Sound, Units, Range, Location, WiFi, Theme), cutting across the apps that actually use
them. There is no guard today that notices when the device menu and the web page drift apart;
`3f48a84`'s mirroring was done by hand and could be undone by hand just as easily.

## Design

### The descriptor

```cpp
// src/platform/storage/settings_descriptor.h (new, platform-level, no LVGL)

enum class Control { Toggle, Slider, Enum, Text };

struct SettingDescriptor {
  const char *label;                    // "Range", "Hide ground traffic"
  Control control;
  StorageRef storage;                   // tagged union: Int*|Bool*|Float*|Str*, see below
  const char *const *optionLabels = nullptr; // Enum only, size == storage.asInt->hi - lo + 1
  const char *unitSuffix = nullptr;          // Slider only, e.g. "km", "%"
};
```

`StorageRef` is a small tagged union (a `Tag` enum plus a union of the four pointer types) over
the existing `settings::Int/Bool/Float/Str` catalogue constants — not a copy of their data, a
pointer to the `inline constexpr` instances that already live in `settings_store.h`. One pair of
functions, `get_int(const SettingDescriptor&)` / `set_int(const SettingDescriptor&, int)`,
switches on the tag and is the only place that calls `settings::Store().get/put`; both the
device renderer and the web renderer go through it, never around it. `Bool` and `Float` are read
and written as `int` at this layer (0/1 for `Bool`; `Float` is out of scope for v1 — none of the
pilot app's settings need it, see **Scope** below) so one dispatch pair covers both current
control kinds.

A `Toggle` descriptor wraps a `Bool`. A `Slider` wraps an `Int` (its `lo..hi` is the slider's
range, rendered with `unitSuffix`). An `Enum` also wraps an `Int`, but is rendered through
`optionLabels[value - lo]` instead of as a number — this is how `UNITS` (today's bare `Int{0,0,2}`
plus a separately-maintained label array) becomes self-describing. `Text` wraps a `Str`.

### Registration

Each app that owns settings declares its own array, colocated with the app, e.g.:

```cpp
// src/app/radar/radar_settings.cpp
inline constexpr const char *kUnitsLabels[] = {"Metric", "Imperial", "Nautical"};
inline constexpr SettingDescriptor kRadarSettings[] = {
  { "Max aircraft", Control::Slider, {&settings::MAX_AC} },
  { "Units",        Control::Enum,   {&settings::UNITS},      kUnitsLabels },
  { "Hide ground",  Control::Toggle, {&settings::HIDE_GROUND} },
  // ...
};
```

The registry itself (`src/app/shell/settings_registry.h/.cpp`, new) is a flat, fixed-capacity
list of `{label, const SettingDescriptor *items, size_t count}` groups — no heap, no dynamic
growth; the number of groups is bounded by the number of apps, known at compile time.

Two self-registering patterns were considered and rejected for this toolchain: a global object
whose constructor links itself into a list before `main()` runs is the usual zero-touch pattern,
but a translation unit whose only referenced symbol is an unused static global risks being
dropped by the linker as dead code on this build, silently losing an app's settings; a
`std::vector`-based dynamic registry adds heap churn for something fully known at boot. Instead,
registration piggybacks on the one place every app is already listed: the shell's app-list
entry (`src/app/shell/`) gains one field pointing at the app's descriptor array (`nullptr` for
apps with none). Adding an app's settings becomes: write the descriptor array in the app's own
folder, add one pointer in the app-list entry that already exists for launching it — no new
touchpoint.

The "system" pseudo-group (brightness, idle timeout, auto-cycle, theme, master volume) isn't
launchable, so it has no app-list entry to piggyback on; it registers with one direct call next
to where the app list itself is built, using the same descriptor array and the same registry —
still one call, just not routed through a picker entry.

### On-device renderer

The Settings menu's top level becomes one wheel row per registered group (replacing the current
flat category list), reusing `wheel::show_wheel` exactly as today. Selecting a group opens a
generic submenu built by walking that group's descriptor array — one row per descriptor,
formatted by `control` (`Toggle` → "On"/"Off", `Slider` → number + `unitSuffix`, `Enum` →
`optionLabels[value]`), reading and writing through `get_int`/`set_int`. This replaces, for
migrated groups, the per-screen hand coding in `settings_pages.cpp` (the `Mode` enum entries,
item enums, and label arrays for that group go away once it migrates). WiFi, Location, Display,
Sound, and Theme stay as their current hand-built pages until (and unless) a later pass migrates
them.

### Web renderer

A generic function walks the same registry and emits one `<div class=card>` per group, one
control per descriptor (`<input type=checkbox>` / `<input type=range>` / `<select>` /
`<input type=text>` chosen by `control`), and registers **one** new route,
`POST /setting` with a `{key, value}` body, that looks up the descriptor by key and dispatches
through `get_int`/`set_int` — replacing, for migrated settings, the one-dedicated-route-per-setting
pattern (`/bright`, `/sweep`, ...). Settings not yet migrated keep their existing dedicated
routes and hand-written HTML card untouched; the two systems coexist in `handleRoot()` during
migration, the generic cards simply appended alongside the hand-written ones.

### Scope: pilot app

**Radar** is the pilot. Of its existing settings, this design migrates the ones that are plain
scalar settings backed by `Int` or `Bool` (no `Float` mapping exists yet, see below): `MAX_AC`
(Slider), `UNITS` (Enum), `HIDE_GROUND`, `MIL_ONLY`, `BIG_TEXT` (Toggle). That set alone
exercises every `Control` kind. `RANGE_KM` (a `Float`), `TRAIL_LEN`, `MIN_ALT_FT`, `SWEEP`,
`AIRPORTS`, `ROT_DEG` stay on the old path for this pass — nothing about the design blocks
migrating them later (`TRAIL_LEN`/`MIN_ALT_FT`/`ROT_DEG` are plain `Int`s and would be trivial
`Slider` additions; `RANGE_KM` needs the `Float`-`Slider` mapping described below first) — there
is just no need to move everything at once to prove the mechanism end-to-end. Every other app
(weather, intel, ticker, clock), WiFi, Location, and the
"system" group's actual migration are explicitly follow-up work, not part of this design's
implementation. The "system" group's *registration path* (the direct-call mechanism) is
exercised by this design only to the extent needed to prove it compiles and dispatches; moving
its real settings over is also follow-up.

`Float`-backed settings have no `Control` mapping in v1 (no pilot setting needs one); adding
`Slider`-for-`Float` is a small, obvious extension when the first `Float` setting migrates, not
designed now.

### Testing and guards

- New host tests (`tests/run_host_tests.sh`) for `get_int`/`set_int`: clamping behaviour for
  `Slider`, 0/1 round-tripping for `Toggle`, in-range dispatch for `Enum`.
- A guard test, alongside `test_settings_store.py`'s existing checks: every `SettingDescriptor`'s
  `StorageRef` must point at a key that exists in the `settings_store` catalogue (no orphan
  descriptor — this falls out naturally since `StorageRef` only holds pointers to catalogue
  constants, but the test makes the invariant explicit and catches a future change that weakens
  the type); every `Enum` descriptor's `optionLabels` length must equal its `Int`'s `hi - lo + 1`;
  no duplicate key within a single group's descriptor array.
- No test is added to compare the device menu's rendered set against the web page's rendered
  set — by construction, both walk the same registry, so there is nothing left to drift. (This
  is the property today's "mirrored by hand" web page explicitly lacks.)

## Out of scope

- WiFi setup and Location screens: permanently hand-coded, never expressed as descriptors.
- Migrating any app other than radar's five listed settings, or the "system" group's actual
  settings, in this pass.
- A `Slider` mapping for `Float`-backed settings.
- Any textual/data-file schema format (YAML, JSON) for declaring settings — descriptors stay
  plain C++ structs, consistent with `settings_store` today, so the compiler and the existing
  guard tests keep doing the type-checking work.

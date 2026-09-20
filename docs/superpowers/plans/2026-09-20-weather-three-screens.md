# Weather: three screens on the knob — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put the Weather app back on the launch roster with three knob-selected screens — Now (temperature + outlook icon), Radar (rain over the geo map, unchanged), 7-Day — with global sources only, verified in the simulator at Brisbane.

**Architecture:** The data layer (`weather.{h,cpp}`) grows to seven days and gains a pure parser, URL builder, unit helpers and a fetch-status enum, all host-tested. Icon classification is a pure function (`wx_icon_kind`); icons are drawn by one custom-draw LVGL object each (`wx_icon`), so there is no baked art. The Now and 7-Day screens live in a new `wx_screens` module built onto the existing weather panel in `ui.cpp`, which gains a three-way mode stepped by the app's knob-turn handler. The radar mode is untouched.

**Tech Stack:** C++17, LVGL 8.4, ArduinoJson 7, PlatformIO (`~/.platformio/penv/bin/pio`; envs `native` and `esp32-s3-amoled-175`), plain host `c++` for unit tests, Python `unittest` for the default theme.

**Spec:** [docs/superpowers/specs/2026-09-20-weather-three-screens-design.md](../specs/2026-09-20-weather-three-screens-design.md)

## Global Constraints

- Sources are Open-Meteo (forecast) and RainViewer (radar), both plain HTTP. Nothing in the weather path may reference `INTEL_GATEWAY_HOST` or `workers.dev`.
- No new theme keys: no `THEME_CAPS` bump, no Orb Studio work in this plan.
- Knob grammar: a turn belongs to the app on screen; the rock opens the switcher. Push is left unassigned on Weather (the shell shows its standard hint).
- Entering the Weather app always lands on Now. Turns wrap Now → Radar → 7-Day in both directions.
- Personal permissions never go in `.claude/settings.json`.
- `FW_VERSION` is bumped (Task 7). `tools/publish-firmware.sh` and the wrangler deploy are **never** run: CLAUDE.md rule 1 needs a hardware boot first.
- Commit hygiene: work on `feat/weather-three-screens`; stage explicit paths only (never `git add -A`); every commit message ends with `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- Render code uses named constants, not magic numbers. Text that can come from outside is ASCII (Open-Meteo values here are numbers and ISO dates; condition words are compiled in).
- `pio` is not on PATH: use `~/.platformio/penv/bin/pio`.

## Review Focus

Inputs the spec implies but a naive implementation gets wrong, most likely first. Each has a test in the task named.

1. **Open-Meteo answers 200 with `null` for the current temperature** (seen live with an unsupported model). It must be an empty state, never "0 C". → Task 1 (`null_current_temperature_is_not_a_reading`).
2. **A missing or unknown weather code (`-1`).** `weather_condition(-1)` currently returns "Partly cloudy" because `-1 <= 2`. It must read "Unknown" and draw the cloud icon. → Task 2 (`unknown_codes_are_unknown`).
3. **The weather path must never touch the gateway** — the point of the whole exercise. → Task 1 (`url_is_plain_http_and_never_the_gateway`).
4. **Knob wrap and entry state**: stepping past the last screen wraps, both directions, and a stale or out-of-range current screen (including the dead `CLOUDS` mode) starts from Now. → Task 1 (`screen_step_wraps_both_ways`), Task 6 (sim wrap check).
5. **Short or ragged forecasts**: fewer than seven days, or a `null` high/low mid-array, must give fewer rows, not "0 / 0" rows. → Task 1 (`ragged_daily_arrays_truncate`, `more_days_than_we_hold_are_clipped`).

Also noted, not testable on the host: large-text accessibility mode does not enlarge the new screens (fixed fonts), and the radar buffers' PSRAM behaviour beside the new screens is a first-hardware check.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/app/weather/weather.h` / `weather.cpp` (modify) | Snapshot (7 days, `isDay`), `weather_url`, `weather_parse`, unit helpers, `weather_status_*`, `weather_screen_step`, fixed `weather_condition` |
| `src/app/weather/weather_client.cpp` (modify) | Fetch only: HTTP GET on device, libcurl on native; hands the body to `weather_parse` |
| `src/app/weather/wx_icon_kind.h` / `.cpp` (create) | Pure WMO-code → icon kind mapping, no LVGL |
| `src/app/weather/wx_icon.h` / `.cpp` (create) | One custom-draw LVGL object per icon, drawn from a kind |
| `src/app/weather/wx_screens.h` / `.cpp` (create) | The Now and 7-Day screens: build, refresh, show |
| `src/app/ui/ui.{h,cpp}` (modify) | Three-way mode, `ui_weather_step/screen/reset`, hook `wx_screens` into `build_weather` |
| `src/config.h`, `src/app/shell/app_shell.h`, `src/main.cpp`, `src/platform/sim/sim_main.cpp` (modify) | `APPS_WEATHER`, slot, registration, knob wiring, fetch status, sim live forecast + screenshot harness |
| `src/theme/core/theme_style.{h,cpp}`, `src/theme_assets/default/theme.yaml` (modify) | Compiled roster and label; regenerated default theme |
| `platformio.ini` (modify) | Native build gains the new files and the weather client |
| `tests/weather_test.cpp`, `tests/run_weather_test.sh` (create), `tests/run_host_tests.sh` (modify) | Host tests |

---

### Task 1: Weather data layer — seven days, parser, URL, units, status, screen stepping

**Files:**
- Modify: `src/app/weather/weather.h`, `src/app/weather/weather.cpp`, `src/app/weather/weather_client.cpp`, `src/app/weather/weather_client.h`, `platformio.ini` (native `build_src_filter`)
- Create: `tests/weather_test.cpp`, `tests/run_weather_test.sh`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces (all in `weather.h`):
  - `#define WEATHER_DAYS 7`; `WeatherSnapshot` gains `bool isDay`, `days[WEATHER_DAYS]`.
  - `size_t weather_url(double lat, double lon, char *buf, size_t n);`
  - `bool weather_parse(const char *json, WeatherSnapshot &out);` (leaves `out` untouched on false)
  - `int weather_round(float v);` `float weather_temp_to(float celsius, bool imperial);` `float weather_wind_to(float kmh, bool imperial);` `const char *weather_temp_unit_name(bool imperial);` `const char *weather_wind_unit_name(bool imperial);`
  - `enum WeatherStatus { WEATHER_STATUS_NOT_ASKED, WEATHER_STATUS_NO_WIFI, WEATHER_STATUS_FAILED, WEATHER_STATUS_OK };` `void weather_status_set(WeatherStatus)`; `WeatherStatus weather_status_get()`; `const char *weather_status_text(WeatherStatus)`.
  - `enum WeatherScreen { WX_SCREEN_NOW = 0, WX_SCREEN_RADAR = 1, WX_SCREEN_WEEK = 2, WX_SCREEN_COUNT = 3 };` `int weather_screen_step(int current, int delta);`
  - `bool weather_fetch(double lat, double lon, WeatherSnapshot &out);` now also built on native.

- [ ] **Step 1: Write the failing test**

Create `tests/weather_test.cpp`:

```cpp
// Host test for src/app/weather/weather.{h,cpp}.   tests/run_weather_test.sh
#include "weather.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

// A real-shaped Open-Meteo answer for Brisbane, seven days.
static const char *BRISBANE = R"({
 "latitude":-27.45,"longitude":153.02,"timezone":"Australia/Brisbane",
 "current":{"time":"2026-09-20T06:30","interval":900,"temperature_2m":15.0,
   "apparent_temperature":13.2,"relative_humidity_2m":72,"is_day":1,
   "weather_code":3,"wind_speed_10m":11.5,"wind_direction_10m":140},
 "daily":{"time":["2026-09-20","2026-09-21","2026-09-22","2026-09-23","2026-09-24","2026-09-25","2026-09-26"],
   "weather_code":[3,1,0,2,61,80,95],
   "temperature_2m_max":[24.1,25.3,26.0,27.2,23.9,22.0,21.5],
   "temperature_2m_min":[13.0,12.5,14.1,15.0,16.2,15.8,14.4],
   "precipitation_probability_max":[5,3,0,10,80,65,90]}})";

static void a_real_response_parses() {
    WeatherSnapshot w = {};
    assert(weather_parse(BRISBANE, w));
    assert(w.valid && w.isDay);
    assert(strcmp(w.updated, "06:30") == 0);
    assert(near(w.tempC, 15.0f) && near(w.feelsC, 13.2f));
    assert(w.humidity == 72 && w.windDeg == 140 && near(w.windKmh, 11.5f));
    assert(w.code == 3);
    assert(w.dayCount == 7);
    assert(strcmp(w.days[0].date, "2026-09-20") == 0);
    assert(w.days[4].code == 61 && w.days[4].rainChance == 80);
    assert(near(w.days[6].tempMinC, 14.4f) && near(w.days[6].tempMaxC, 21.5f));
}

static void night_is_night() {
    std::string j = BRISBANE;
    j.replace(j.find("\"is_day\":1"), 10, "\"is_day\":0");
    WeatherSnapshot w = {};
    assert(weather_parse(j.c_str(), w) && !w.isDay);
}

// Review Focus 1. Open-Meteo answers 200 with nulls when a model has nothing for a place;
// "0 C" on the glass would be a lie about the weather.
static void null_current_temperature_is_not_a_reading() {
    std::string j = BRISBANE;
    j.replace(j.find("\"temperature_2m\":15.0"), 21, "\"temperature_2m\":null");
    WeatherSnapshot w = {};
    w.tempC = 99.0f;
    assert(!weather_parse(j.c_str(), w));
    assert(near(w.tempC, 99.0f) && !w.valid);   // untouched
}

static void a_null_weather_code_is_unknown_not_fatal() {
    std::string j = BRISBANE;
    j.replace(j.find("\"weather_code\":3,"), 17, "\"weather_code\":null,");
    WeatherSnapshot w = {};
    assert(weather_parse(j.c_str(), w) && w.code == -1);
}

// Review Focus 5.
static void ragged_daily_arrays_truncate() {
    std::string j = BRISBANE;
    // day 5's high goes null: keep days 0-4, drop the rest, never show a "0 / 0" row
    j.replace(j.find("23.9,22.0,21.5"), 14, "23.9,null,21.5");
    WeatherSnapshot w = {};
    assert(weather_parse(j.c_str(), w));
    assert(w.dayCount == 5);
    assert(w.days[4].code == 61);
}

static void more_days_than_we_hold_are_clipped() {
    std::string dates = "[", codes = "[", hi = "[", lo = "[", rain = "[";
    for (int i = 0; i < 9; ++i) {
        char d[16]; snprintf(d, sizeof d, "\"2026-09-%02d\"", 20 + i);
        const char *c = i ? "," : "";
        dates += c; dates += d; codes += c; codes += "1"; hi += c; hi += "20"; lo += c; lo += "10"; rain += c; rain += "0";
    }
    std::string j = std::string(R"({"current":{"time":"2026-09-20T06:30","temperature_2m":10},"daily":{"time":)") +
        dates + "],\"weather_code\":" + codes + "],\"temperature_2m_max\":" + hi + "],\"temperature_2m_min\":" + lo +
        "],\"precipitation_probability_max\":" + rain + "]}}";
    WeatherSnapshot w = {};
    assert(weather_parse(j.c_str(), w));
    assert(w.dayCount == WEATHER_DAYS && WEATHER_DAYS == 7);
}

static void current_only_is_still_a_forecast_of_zero_days() {
    WeatherSnapshot w = {};
    assert(weather_parse(R"({"current":{"time":"2026-09-20T06:30","temperature_2m":-3.5,"is_day":0},"daily":{"time":[],"weather_code":[],"temperature_2m_max":[],"temperature_2m_min":[],"precipitation_probability_max":[]}})", w));
    assert(w.dayCount == 0 && near(w.tempC, -3.5f) && !w.isDay);
}

static void anything_unusable_is_refused_and_leaves_the_snapshot_alone() {
    const char *bad[] = { "", "not json", "{}", R"({"current":{}})",
        R"({"current":{"temperature_2m":5},"daily":)",                       // truncated
        R"({"daily":{"time":[]}})" };
    for (const char *b : bad) {
        WeatherSnapshot w = {}; w.humidity = 42;
        assert(!weather_parse(b, w));
        assert(w.humidity == 42 && !w.valid);
    }
    WeatherSnapshot w = {};
    assert(!weather_parse(nullptr, w));
}

// Review Focus 3. The whole point of this exercise is that the weather does not go through
// the gateway, and this is the one place the address is built.
static void url_is_plain_http_and_never_the_gateway() {
    char u[512];
    const size_t n = weather_url(-27.47, 153.03, u, sizeof u);
    assert(n > 0 && n < sizeof u);
    assert(strncmp(u, "http://api.open-meteo.com/v1/forecast?", 38) == 0);
    assert(strstr(u, "latitude=-27.47000") && strstr(u, "longitude=153.03000"));
    assert(strstr(u, "forecast_days=7"));
    assert(strstr(u, "is_day"));
    assert(strstr(u, "timezone=auto"));
    assert(!strstr(u, "workers.dev") && !strstr(u, "zionbrock") && !strstr(u, "https"));
    char tiny[16];
    assert(weather_url(0, 0, tiny, sizeof tiny) == 0);   // does not fit: refused, not truncated
}

static void rounding_never_prints_negative_zero() {
    assert(weather_round(-0.4f) == 0 && weather_round(26.5f) == 27 && weather_round(-26.5f) == -27);
    assert(near(weather_temp_to(15.0f, false), 15.0f) && near(weather_temp_to(15.0f, true), 59.0f));
    assert(near(weather_temp_to(-40.0f, true), -40.0f));
    assert(near(weather_wind_to(100.0f, false), 100.0f) && near(weather_wind_to(100.0f, true), 62.1371f));
    assert(strcmp(weather_temp_unit_name(false), "C") == 0 && strcmp(weather_temp_unit_name(true), "F") == 0);
    assert(strcmp(weather_wind_unit_name(false), "km/h") == 0 && strcmp(weather_wind_unit_name(true), "mph") == 0);
}

static void the_three_empty_states_say_different_things() {
    const char *a = weather_status_text(WEATHER_STATUS_NOT_ASKED);
    const char *b = weather_status_text(WEATHER_STATUS_NO_WIFI);
    const char *c = weather_status_text(WEATHER_STATUS_FAILED);
    assert(a[0] && b[0] && c[0]);
    assert(strcmp(a, b) && strcmp(b, c) && strcmp(a, c));
    assert(weather_status_text(WEATHER_STATUS_OK)[0] == '\0');
    assert(weather_status_get() == WEATHER_STATUS_NOT_ASKED);   // a fresh process has asked nothing
    weather_status_set(WEATHER_STATUS_FAILED);
    assert(weather_status_get() == WEATHER_STATUS_FAILED);
    weather_status_set(WEATHER_STATUS_NOT_ASKED);
}

// Review Focus 4.
static void screen_step_wraps_both_ways() {
    assert(weather_screen_step(WX_SCREEN_NOW, +1) == WX_SCREEN_RADAR);
    assert(weather_screen_step(WX_SCREEN_RADAR, +1) == WX_SCREEN_WEEK);
    assert(weather_screen_step(WX_SCREEN_WEEK, +1) == WX_SCREEN_NOW);     // wraps forward
    assert(weather_screen_step(WX_SCREEN_NOW, -1) == WX_SCREEN_WEEK);     // wraps back
    assert(weather_screen_step(WX_SCREEN_WEEK, -1) == WX_SCREEN_RADAR);
    assert(weather_screen_step(WX_SCREEN_RADAR, -1) == WX_SCREEN_NOW);
    assert(weather_screen_step(WX_SCREEN_RADAR, +5) == WX_SCREEN_WEEK);   // any positive detent count is one step
    assert(weather_screen_step(WX_SCREEN_RADAR, 0) == WX_SCREEN_RADAR);   // no movement, no change
    assert(weather_screen_step(3, +1) == WX_SCREEN_NOW);                  // the dead CLOUDS mode: start over
    assert(weather_screen_step(-1, -1) == WX_SCREEN_NOW);
    assert(weather_screen_step(99, +1) == WX_SCREEN_NOW);
}

static void day_names() {
    assert(strcmp(weather_day_name("2026-09-20"), "Sun") == 0);
    assert(strcmp(weather_day_name("2026-09-21"), "Mon") == 0);
    assert(strcmp(weather_day_name("2026-09-26"), "Sat") == 0);
    assert(strcmp(weather_day_name("garbage"), "---") == 0);
}

int main() {
    a_real_response_parses();
    night_is_night();
    null_current_temperature_is_not_a_reading();
    a_null_weather_code_is_unknown_not_fatal();
    ragged_daily_arrays_truncate();
    more_days_than_we_hold_are_clipped();
    current_only_is_still_a_forecast_of_zero_days();
    anything_unusable_is_refused_and_leaves_the_snapshot_alone();
    url_is_plain_http_and_never_the_gateway();
    rounding_never_prints_negative_zero();
    the_three_empty_states_say_different_things();
    screen_step_wraps_both_ways();
    day_names();
    printf("weather tests passed\n");
    return 0;
}
```

Create `tests/run_weather_test.sh` (executable):

```bash
#!/bin/bash
# Builds and runs tests/weather_test.cpp on the host. Needs the native env's libdeps for ArduinoJson.
set -euo pipefail
cd "$(dirname "$0")/.."
AJ=.pio/libdeps/native/ArduinoJson/src
[ -d "$AJ" ] || { echo "ArduinoJson not found at $AJ; run: pio run -e native" >&2; exit 2; }
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
SRC="tests/weather_test.cpp src/app/weather/weather.cpp"
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/app/weather -I"$AJ" $SRC -o "$OUT/weather_test" -pthread
"$OUT/weather_test"
```

- [ ] **Step 2: Run it to verify it fails**

Run: `chmod +x tests/run_weather_test.sh && bash tests/run_weather_test.sh`
Expected: compile FAIL — `'weather_parse' was not declared` (and the other new names).

- [ ] **Step 3: Write the implementation**

Replace `src/app/weather/weather.h`:

```cpp
#pragma once

#include <stddef.h>

// Portable weather snapshot shared by the network task and LVGL UI.
// Temperatures are Celsius, wind is km/h and precipitation is millimetres;
// the UI converts these according to the selected unit preset.

// One row per day of the 7-Day screen. The theme's text tokens ({day1}..{day4}) still read
// only the first four.
#define WEATHER_DAYS 7

struct WeatherDay {
    char date[11];
    int code;
    float tempMinC;
    float tempMaxC;
    int rainChance;
};

struct WeatherSnapshot {
    bool valid;
    bool isDay;          // Open-Meteo's is_day for the current reading; true when it did not say
    char updated[6];
    int code;
    float tempC;
    float feelsC;
    int humidity;
    float windKmh;
    int windDeg;
    WeatherDay days[WEATHER_DAYS];
    int dayCount;
};

void weather_store(const WeatherSnapshot &snapshot);
bool weather_get(WeatherSnapshot &snapshot);
const char *weather_condition(int code);
const char *weather_day_name(const char *isoDate);

// The request. Plain HTTP, because this board cannot raise the two contiguous ~16 KB internal
// buffers a TLS handshake needs, and straight to Open-Meteo: this deliberately does not go
// through the Orb gateway. Returns the length written, or 0 when it did not fit.
size_t weather_url(double lat, double lon, char *buf, size_t n);

// Parse an Open-Meteo answer. False (and `out` untouched) for anything unusable, including a
// 200 whose current temperature is null: Open-Meteo answers that way when a model has no data
// for a place, and reading it as 0 would put "0 C" on the glass.
bool weather_parse(const char *json, WeatherSnapshot &out);

// Unit helpers, shared so the screens and the tests agree. weather_round returns an int on
// purpose: an int cannot print "-0".
int weather_round(float v);
float weather_temp_to(float celsius, bool imperial);
float weather_wind_to(float kmh, bool imperial);
const char *weather_temp_unit_name(bool imperial);   // "C" / "F"
const char *weather_wind_unit_name(bool imperial);   // "km/h" / "mph"

// How the forecast fetch is doing, so an empty screen can say WHICH thing is unwell: the three
// are different sentences and one shared "unavailable" would hide the difference.
enum WeatherStatus {
    WEATHER_STATUS_NOT_ASKED = 0,   // nothing requested yet
    WEATHER_STATUS_NO_WIFI,
    WEATHER_STATUS_FAILED,          // WiFi is up and the service did not give a usable answer
    WEATHER_STATUS_OK,
};
void weather_status_set(WeatherStatus s);
WeatherStatus weather_status_get(void);
const char *weather_status_text(WeatherStatus s);    // "" for OK

// The screens the knob steps between. Numbered because they are indexes into a cycle.
enum WeatherScreen { WX_SCREEN_NOW = 0, WX_SCREEN_RADAR = 1, WX_SCREEN_WEEK = 2, WX_SCREEN_COUNT = 3 };
// One detent: forward for delta > 0, back for delta < 0, unchanged for 0. A `current` outside the
// cycle (the retired satellite mode, or garbage) starts over from Now.
int weather_screen_step(int current, int delta);
```

Replace `src/app/weather/weather.cpp`:

```cpp
#include "weather.h"
#include <ArduinoJson.h>
#include <atomic>
#include <math.h>
#include <mutex>
#include <string.h>
#include <stdio.h>

static std::mutex s_mutex;
static WeatherSnapshot s_snapshot = {};
static std::atomic<int> s_status{ WEATHER_STATUS_NOT_ASKED };

void weather_store(const WeatherSnapshot &snapshot) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_snapshot = snapshot;
}

bool weather_get(WeatherSnapshot &snapshot) {
    std::lock_guard<std::mutex> lock(s_mutex);
    snapshot = s_snapshot;
    return snapshot.valid;
}

const char *weather_condition(int code) {
    // Before anything else: -1 is "the service did not say", and `-1 <= 2` used to read it as
    // "Partly cloudy", which is a confident forecast of nothing.
    if (code < 0) return "Unknown";
    if (code == 0) return "Clear";
    if (code == 1) return "Mostly clear";
    if (code == 2) return "Partly cloudy";
    if (code == 3) return "Overcast";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 57) return "Drizzle";
    if (code >= 61 && code <= 67) return "Rain";
    if (code >= 71 && code <= 77) return "Snow";
    if (code >= 80 && code <= 82) return "Showers";
    if (code >= 85 && code <= 86) return "Snow showers";
    if (code >= 95) return "Thunderstorm";
    return "Unknown";
}

const char *weather_day_name(const char *isoDate) {
    static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (!isoDate || strlen(isoDate) < 10) return "---";
    int y = 0, m = 0, d = 0;
    if (sscanf(isoDate, "%d-%d-%d", &y, &m, &d) != 3) return "---";
    if (m < 3) { m += 12; --y; }
    const int k = y % 100, j = y / 100;
    const int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return names[(h + 6) % 7];
}

size_t weather_url(double lat, double lon, char *buf, size_t n) {
    // Plain HTTP, same reason as the ADS-B feed: this board cannot raise the two contiguous
    // ~16 KB internal buffers a TLS handshake needs, so every HTTPS request here failed with
    // '-32512 SSL memory allocation failed' and the weather stayed blank. See the
    // ADSB_PRIMARY_TLS notes in config.h. No credentials are sent.
    const int len = snprintf(buf, n,
        "http://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
        "&current=temperature_2m,apparent_temperature,relative_humidity_2m,is_day,weather_code,"
        "wind_speed_10m,wind_direction_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
        "&forecast_days=%d&timezone=auto", lat, lon, WEATHER_DAYS);
    return (len > 0 && (size_t)len < n) ? (size_t)len : 0;
}

bool weather_parse(const char *json, WeatherSnapshot &out) {
    if (!json) return false;
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    JsonObjectConst current = doc["current"].as<JsonObjectConst>();
    JsonObjectConst daily = doc["daily"].as<JsonObjectConst>();
    if (current.isNull() || daily.isNull()) return false;
    if (current["temperature_2m"].isNull()) return false;   // see weather.h

    WeatherSnapshot next = {};
    const char *stamp = current["time"] | "";
    const char *clock = strchr(stamp, 'T');
    snprintf(next.updated, sizeof(next.updated), "%.5s", clock ? clock + 1 : "--:--");
    next.code = current["weather_code"] | -1;
    next.isDay = (current["is_day"] | 1) != 0;
    next.tempC = current["temperature_2m"] | 0.0f;
    next.feelsC = current["apparent_temperature"] | next.tempC;
    next.humidity = current["relative_humidity_2m"] | 0;
    next.windKmh = current["wind_speed_10m"] | 0.0f;
    next.windDeg = current["wind_direction_10m"] | 0;

    JsonArrayConst dates = daily["time"].as<JsonArrayConst>();
    JsonArrayConst codes = daily["weather_code"].as<JsonArrayConst>();
    JsonArrayConst highs = daily["temperature_2m_max"].as<JsonArrayConst>();
    JsonArrayConst lows = daily["temperature_2m_min"].as<JsonArrayConst>();
    JsonArrayConst rain = daily["precipitation_probability_max"].as<JsonArrayConst>();
    const size_t want = dates.size() < (size_t)WEATHER_DAYS ? dates.size() : (size_t)WEATHER_DAYS;
    int n = 0;
    for (size_t i = 0; i < want; ++i) {
        // Days are sequential and Open-Meteo only nulls the tail, so the first day without a
        // high or a low ends the list rather than leaving a "0 / 0" row in the middle of it.
        if (highs[i].isNull() || lows[i].isNull()) break;
        snprintf(next.days[n].date, sizeof(next.days[n].date), "%s", dates[i] | "");
        next.days[n].code = codes[i] | -1;
        next.days[n].tempMaxC = highs[i] | 0.0f;
        next.days[n].tempMinC = lows[i] | 0.0f;
        next.days[n].rainChance = rain[i] | 0;
        ++n;
    }
    next.dayCount = n;
    next.valid = true;
    out = next;
    return true;
}

int weather_round(float v) { return (int)lroundf(v); }
float weather_temp_to(float c, bool imperial) { return imperial ? c * 1.8f + 32.0f : c; }
float weather_wind_to(float kmh, bool imperial) { return imperial ? kmh * 0.621371f : kmh; }
const char *weather_temp_unit_name(bool imperial) { return imperial ? "F" : "C"; }
const char *weather_wind_unit_name(bool imperial) { return imperial ? "mph" : "km/h"; }

void weather_status_set(WeatherStatus s) { s_status.store((int)s); }
WeatherStatus weather_status_get(void) { return (WeatherStatus)s_status.load(); }
const char *weather_status_text(WeatherStatus s) {
    switch (s) {
    case WEATHER_STATUS_NOT_ASKED: return "Asking for the forecast...";
    case WEATHER_STATUS_NO_WIFI:   return "No WiFi, so no forecast yet";
    case WEATHER_STATUS_FAILED:    return "The forecast service is not answering. Trying again soon.";
    case WEATHER_STATUS_OK:        return "";
    }
    return "";
}

int weather_screen_step(int current, int delta) {
    if (current < 0 || current >= WX_SCREEN_COUNT) return WX_SCREEN_NOW;
    if (delta == 0) return current;
    return (current + (delta > 0 ? 1 : WX_SCREEN_COUNT - 1)) % WX_SCREEN_COUNT;
}
```

Replace `src/app/weather/weather_client.h`:

```cpp
#pragma once

#include "weather.h"

// Fetch current conditions and a seven-day forecast from Open-Meteo. Built on the device
// (HTTPClient) and in the simulator (libcurl).
bool weather_fetch(double lat, double lon, WeatherSnapshot &out);
```

Replace `src/app/weather/weather_client.cpp`:

```cpp
#include "weather_client.h"
#include "config.h"
#include <stdio.h>
#include <string>

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#define WLOG(...) Serial.printf(__VA_ARGS__)
#else
#include "native_http.h"
#define WLOG(...) printf(__VA_ARGS__)
#endif

#ifdef ARDUINO
static bool http_get_body(const char *url, std::string &body) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient client;
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(3500);
    http.setTimeout(7000);
    if (!http.begin(client, url)) {
        WLOG("[weather] HTTP begin failed\n");
        return false;
    }
    http.addHeader("User-Agent", ADSB_USER_AGENT);

    const int status = http.GET();
    if (status != 200) {
        // No TLS state to report now that this runs over plain HTTP; the heap numbers stay
        // because they are what diagnosed the original failure and are cheap to keep.
        WLOG("[weather] HTTP %d: %s heap=%u largest=%u psram=%u\n", status,
             status < 0 ? http.errorToString(status).c_str() : "unexpected response",
             (unsigned)ESP.getFreeHeap(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)ESP.getFreePsram());
        http.end();
        return false;
    }

    // Open-Meteo replies with Transfer-Encoding: chunked. HTTPClient::getString()
    // removes the chunk framing; parsing getStream() directly makes ArduinoJson
    // see the hexadecimal chunk size first and report InvalidInput.
    String payload = http.getString();
    http.end();
    body.assign(payload.c_str(), payload.length());
    return true;
}
#else
static bool http_get_body(const char *url, std::string &body) {
    return native_https_get(url, ADSB_USER_AGENT, body, 7000);   // handles http:// as well
}
#endif

bool weather_fetch(double lat, double lon, WeatherSnapshot &out) {
    char url[512];
    if (!weather_url(lat, lon, url, sizeof(url))) return false;

    std::string payload;
    if (!http_get_body(url, payload)) return false;
    if (payload.empty()) {
        WLOG("[weather] empty response body\n");
        return false;
    }
    if (!weather_parse(payload.c_str(), out)) {
        WLOG("[weather] unusable response (%u bytes, starts '%.24s')\n",
             (unsigned)payload.size(), payload.c_str());
        return false;
    }
    return true;
}
```

In `platformio.ini`, in the `[env:native]` `build_src_filter` line, change `+<app/weather/weather.cpp>` to `+<app/weather/weather.cpp> +<app/weather/weather_client.cpp>` and delete the `-<app/weather/weather_client.cpp>` entry (the old `-<app/weather_client.cpp>` two entries later is a different, stale path: leave it).

In `tests/run_host_tests.sh`, add after the `ip locate` line:

```bash
run "weather"                            bash tests/run_weather_test.sh
```

- [ ] **Step 4: Run to verify it passes**

Run: `bash tests/run_weather_test.sh`
Expected: `weather tests passed`. If `day_names` fails, that is a real bug in `weather_day_name` (2026-09-20 is a Sunday): stop and report it rather than editing the test.

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | tail -5`
Expected: `SUCCESS` (the native build now compiles `weather_client.cpp` through libcurl).

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -5`
Expected: `SUCCESS`. (`ui.cpp` still compiles: it uses `w.days[i]` bounded by `dayCount`, and its `i < 4` loops are unchanged until Task 5.)

- [ ] **Step 5: Commit**

```bash
git add src/app/weather/weather.h src/app/weather/weather.cpp src/app/weather/weather_client.h src/app/weather/weather_client.cpp platformio.ini tests/weather_test.cpp tests/run_weather_test.sh tests/run_host_tests.sh
git commit -m "$(cat <<'EOF'
Weather data: seven days, tested parser, no-gateway URL, honest empty states

weather_parse refuses a null current temperature instead of reading it as 0,
weather_condition(-1) is Unknown instead of Partly cloudy, and the fetch now
builds in the simulator too.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Icon classification (pure)

**Files:**
- Create: `src/app/weather/wx_icon_kind.h`, `src/app/weather/wx_icon_kind.cpp`
- Modify: `tests/weather_test.cpp`, `tests/run_weather_test.sh`, `platformio.ini` (native filter)

**Interfaces:**
- Consumes: `weather_condition(int)` from Task 1 (to check text and icon agree).
- Produces (`wx_icon_kind.h`):
  - `enum class WxIconKind : unsigned char { ClearDay, ClearNight, PartlyDay, PartlyNight, Cloudy, Fog, Drizzle, Rain, Showers, Snow, Thunder, Count };`
  - `WxIconKind wx_icon_classify(int wmoCode, bool isDay);`
  - `const char *wx_icon_name(WxIconKind k);`

- [ ] **Step 1: Write the failing test**

In `tests/weather_test.cpp`, add `#include "wx_icon_kind.h"` under the first include, and add before `int main()`:

```cpp
// Every code Open-Meteo documents, and what a person looking at the icon should conclude.
static void every_documented_code_has_the_right_icon() {
    struct { int code; WxIconKind day; WxIconKind night; } t[] = {
        {0,  WxIconKind::ClearDay,   WxIconKind::ClearNight},
        {1,  WxIconKind::ClearDay,   WxIconKind::ClearNight},
        {2,  WxIconKind::PartlyDay,  WxIconKind::PartlyNight},
        {3,  WxIconKind::Cloudy,     WxIconKind::Cloudy},
        {45, WxIconKind::Fog,        WxIconKind::Fog},
        {48, WxIconKind::Fog,        WxIconKind::Fog},
        {51, WxIconKind::Drizzle,    WxIconKind::Drizzle},
        {53, WxIconKind::Drizzle,    WxIconKind::Drizzle},
        {55, WxIconKind::Drizzle,    WxIconKind::Drizzle},
        {56, WxIconKind::Drizzle,    WxIconKind::Drizzle},
        {57, WxIconKind::Drizzle,    WxIconKind::Drizzle},
        {61, WxIconKind::Rain,       WxIconKind::Rain},
        {63, WxIconKind::Rain,       WxIconKind::Rain},
        {65, WxIconKind::Rain,       WxIconKind::Rain},
        {66, WxIconKind::Rain,       WxIconKind::Rain},
        {67, WxIconKind::Rain,       WxIconKind::Rain},
        {71, WxIconKind::Snow,       WxIconKind::Snow},
        {73, WxIconKind::Snow,       WxIconKind::Snow},
        {75, WxIconKind::Snow,       WxIconKind::Snow},
        {77, WxIconKind::Snow,       WxIconKind::Snow},
        {80, WxIconKind::Showers,    WxIconKind::Showers},
        {81, WxIconKind::Showers,    WxIconKind::Showers},
        {82, WxIconKind::Showers,    WxIconKind::Showers},
        {85, WxIconKind::Snow,       WxIconKind::Snow},
        {86, WxIconKind::Snow,       WxIconKind::Snow},
        {95, WxIconKind::Thunder,    WxIconKind::Thunder},
        {96, WxIconKind::Thunder,    WxIconKind::Thunder},
        {99, WxIconKind::Thunder,    WxIconKind::Thunder},
    };
    for (const auto &r : t) {
        assert(wx_icon_classify(r.code, true) == r.day);
        assert(wx_icon_classify(r.code, false) == r.night);
    }
}

// Review Focus 2. An unknown code must not look like a forecast.
static void unknown_codes_are_unknown() {
    assert(strcmp(weather_condition(-1), "Unknown") == 0);
    assert(strcmp(weather_condition(4), "Unknown") == 0);
    assert(strcmp(weather_condition(46), "Unknown") == 0);
    for (int code : { -1, 4, 10, 44, 46, 58, 60, 68, 78, 90, 1000 }) {
        assert(wx_icon_classify(code, true) == WxIconKind::Cloudy);
        assert(wx_icon_classify(code, false) == WxIconKind::Cloudy);
    }
}

// The words and the picture must not disagree on the same screen.
static void the_words_agree_with_the_icon() {
    assert(strcmp(weather_condition(0), "Clear") == 0);
    assert(strcmp(weather_condition(1), "Mostly clear") == 0);   // shown with the sun, so not "Partly cloudy"
    assert(strcmp(weather_condition(2), "Partly cloudy") == 0);
    assert(strcmp(weather_condition(3), "Overcast") == 0);
    assert(strcmp(weather_condition(61), "Rain") == 0);
    assert(strcmp(weather_condition(95), "Thunderstorm") == 0);
}

static void every_kind_has_a_name() {
    for (int k = 0; k < (int)WxIconKind::Count; ++k) {
        const char *n = wx_icon_name((WxIconKind)k);
        assert(n && n[0] && strcmp(n, "?") != 0);
    }
    assert(strcmp(wx_icon_name(WxIconKind::Count), "?") == 0);
}
```

Add the four calls to `main()` before the `printf`.

In `tests/run_weather_test.sh` change the `SRC` line to:

```bash
SRC="tests/weather_test.cpp src/app/weather/weather.cpp src/app/weather/wx_icon_kind.cpp"
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash tests/run_weather_test.sh`
Expected: FAIL — `fatal error: 'wx_icon_kind.h' file not found`.

- [ ] **Step 3: Write the implementation**

Create `src/app/weather/wx_icon_kind.h`:

```cpp
#pragma once
// Which picture goes with a WMO weather code. Pure, no LVGL, so it can be tested on the host;
// wx_icon.{h,cpp} draws the picture.

enum class WxIconKind : unsigned char {
    ClearDay, ClearNight,
    PartlyDay, PartlyNight,
    Cloudy, Fog, Drizzle, Rain, Showers, Snow, Thunder,
    Count
};

// `wmoCode` is Open-Meteo's weather_code. -1 (not stated) and any code the table does not know
// give Cloudy: a neutral picture, and never the sun.
WxIconKind wx_icon_classify(int wmoCode, bool isDay);
const char *wx_icon_name(WxIconKind k);   // "?" for Count and beyond
```

Create `src/app/weather/wx_icon_kind.cpp`:

```cpp
#include "wx_icon_kind.h"

WxIconKind wx_icon_classify(int code, bool isDay) {
    if (code == 0 || code == 1) return isDay ? WxIconKind::ClearDay : WxIconKind::ClearNight;
    if (code == 2)              return isDay ? WxIconKind::PartlyDay : WxIconKind::PartlyNight;
    if (code == 3)              return WxIconKind::Cloudy;
    if (code == 45 || code == 48) return WxIconKind::Fog;
    if (code >= 51 && code <= 57) return WxIconKind::Drizzle;   // includes freezing drizzle
    if (code >= 61 && code <= 67) return WxIconKind::Rain;      // includes freezing rain
    if (code >= 71 && code <= 77) return WxIconKind::Snow;      // includes snow grains
    if (code >= 80 && code <= 82) return WxIconKind::Showers;
    if (code == 85 || code == 86) return WxIconKind::Snow;      // snow showers
    if (code >= 95 && code <= 99) return WxIconKind::Thunder;
    return WxIconKind::Cloudy;
}

const char *wx_icon_name(WxIconKind k) {
    switch (k) {
    case WxIconKind::ClearDay:    return "clear-day";
    case WxIconKind::ClearNight:  return "clear-night";
    case WxIconKind::PartlyDay:   return "partly-day";
    case WxIconKind::PartlyNight: return "partly-night";
    case WxIconKind::Cloudy:      return "cloudy";
    case WxIconKind::Fog:         return "fog";
    case WxIconKind::Drizzle:     return "drizzle";
    case WxIconKind::Rain:        return "rain";
    case WxIconKind::Showers:     return "showers";
    case WxIconKind::Snow:        return "snow";
    case WxIconKind::Thunder:     return "thunder";
    case WxIconKind::Count:       break;
    }
    return "?";
}
```

In `platformio.ini` native `build_src_filter`, add `+<app/weather/wx_icon_kind.cpp>` right after `+<app/weather/weather_client.cpp>`.

- [ ] **Step 4: Run to verify it passes**

Run: `bash tests/run_weather_test.sh`
Expected: `weather tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/app/weather/wx_icon_kind.h src/app/weather/wx_icon_kind.cpp tests/weather_test.cpp tests/run_weather_test.sh platformio.ini
git commit -m "$(cat <<'EOF'
Weather: map every WMO code to an icon kind, tested

Pure function so it runs on the host. Unknown codes are a neutral cloud, never the sun.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Put Weather back on the roster and update the default theme

**Files:**
- Modify: `src/config.h`, `src/app/shell/app_shell.h`, `src/main.cpp`, `src/platform/sim/sim_main.cpp`, `src/theme/core/theme_style.h`, `src/theme/core/theme_style.cpp`, `src/theme_assets/default/theme.yaml` (regenerated)

**Interfaces:**
- Consumes: `weather_status_set` and the `WeatherStatus` values from Task 1.
- Produces: `#define APPS_WEATHER 1`; `app_shell::APP_WEATHER` present in the `Slot` enum; the default roster has `weather` true and the label "Weather". Weather is registered with the OLD press handler for now; Task 5 replaces it with the turn handler.

- [ ] **Step 1: Write the failing check**

The generated default theme is the test. Run it first to see it pass on the current tree, then change the source and watch it fail:

Run: `python3 -m unittest tests/test_default_theme.py 2>&1 | tail -5`
Expected now: `OK` (or `skipped` with a reason if the native libdeps are missing; if skipped, run `~/.platformio/penv/bin/pio run -e native` once and retry).

- [ ] **Step 2: Make the roster changes**

`src/config.h`: replace the whole comment block above `#define APPS_LAUNCH_ONE 1` (from `// ---------- Which apps this build ships (CUT-01) ----------` through the paragraph ending `... THEME_CAPS stays\n// at 34 for the same reason; Studio hides the controls behind its own flag rather than the\n// device pretending it never understood them.`) with:

```cpp
// ---------- Which apps this build ships (CUT-01) ----------
// Launch one is Clock, Flight tracker, Weather, News and Settings. Surveillance and the Stock
// Ticker come off the roster.
//
// ABSENT, not present and disabled, and the distinction is UX-042's: every app that ships
// is on every unit, so an app that is not ready is not shipped rather than shipped dark.
// A theme can already hide an app it does not want (theme.json's roster, and the `hidden`
// flag app_shell::add takes); that is a design's choice about a finished app, which is a
// different thing from a product not carrying one yet.
//
// One switch each rather than deleted code, because the cut list is explicit that these are
// "real, wanted, and waiting" and that nothing there is cancelled. Setting APPS_LAUNCH_ONE to
// 0 brings back Surveillance and the Ticker exactly as they were.
//
// This is uniform across every unit, which is what keeps it inside UX-042: the requirement
// forbids holding a feature back from SOME buyers, not shipping a product that does not
// have it yet.
//
// The firmware still PARSES ticker_style.json, and Apps/Names keep their fields, because
// TC-008 says a shipped parameter is never removed. THEME_CAPS stays at 34 for the same
// reason; Studio hides the controls behind its own flag rather than the device pretending it
// never understood them.
#define APPS_LAUNCH_ONE 1
// Weather rejoined the roster on its own switch: three screens on the knob (Now, Radar, 7-Day)
// on global sources, with no dependency on the Orb gateway. Set to 0 to take it off again.
#define APPS_WEATHER 1
```

`src/app/shell/app_shell.h`: change the roster comment sentence `// APPS_LAUNCH_ONE (config.h) takes Weather, Surveillance and the Ticker off the\n    // roster for launch one.` to `// APPS_LAUNCH_ONE (config.h) takes Surveillance and the Ticker off the roster for\n    // launch one, and APPS_WEATHER puts Weather on it.` and change the enum body to:

```cpp
    enum Slot {
        APP_CLOCK = 0,
        APP_FLIGHT,
#if APPS_WEATHER
        APP_WEATHER,
#endif
#if !APPS_LAUNCH_ONE
        APP_SURVEILLANCE,
#endif
        APP_INTEL,
#if !APPS_LAUNCH_ONE
        APP_TICKER,
#endif
        APP_SETTINGS,
        APP_COUNT,
    };
```

`src/main.cpp`:
1. In the comment above the forecast fetch, change the sentence saying this build (APPS_LAUNCH_ONE) does not carry the Weather screens to say that a build without APPS_WEATHER must not fetch the forecast, because no screen would show it. Keep the sentence about the ADS-B poll after it.
2. Replace the forecast-fetch block `#if !APPS_LAUNCH_ONE\n            if (theme_style::apps().weather && (int32_t)(nowMs - nextWeatherAt) >= 0) {` through its closing `#endif` with:

```cpp
#if APPS_WEATHER
            if (theme_style::apps().weather && (int32_t)(nowMs - nextWeatherAt) >= 0) {
                Serial.printf("[weather] fetching %.5f, %.5f...\n",
                              g_settings.homeLat, g_settings.homeLon);
                WeatherSnapshot forecast;
                if (weather_fetch(g_settings.homeLat, g_settings.homeLon, forecast)) {
                    weather_store(forecast);
                    weather_status_set(WEATHER_STATUS_OK);
                    g_weatherDirty = true;
                    nextWeatherAt = millis() + WEATHER_REFRESH_MS;
                    Serial.println("[weather] forecast updated");
                } else {
                    // Which thing is unwell, for the screen to say: no WiFi and a service that
                    // is not answering are different sentences.
                    weather_status_set(WiFi.status() == WL_CONNECTED ? WEATHER_STATUS_FAILED
                                                                     : WEATHER_STATUS_NO_WIFI);
                    g_weatherDirty = true;   // repaint the empty state with the new sentence
                    nextWeatherAt = millis() + 60000UL;
                    Serial.println("[weather] fetch failed; retrying in 60s");
                }
            }
#endif
```
3. In the WiFi-connect / lost logic, add `if (!conn) weather_status_set(WEATHER_STATUS_NO_WIFI);` directly after the `if (!conn && wasConnected) {...}` block, guarded so it never overwrites OK data's meaning: `if (!conn && weather_status_get() != WEATHER_STATUS_OK) weather_status_set(WEATHER_STATUS_NO_WIFI);`. (An Orb that has never connected then says "No WiFi", not "Asking for the forecast...".)
4. Change the `#if !APPS_LAUNCH_ONE` before `if (theme_style::apps().weather) wx_radar_begin();` to `#if APPS_WEATHER`.
5. Split the registration block. Replace:

```cpp
#if !APPS_LAUNCH_ONE
    app_shell::add(radarScreen, theme_style::names().weather,  weather_press_cycle, nullptr, false, radar_show_weather, radar_hide_weather, !theme_style::apps().weather);
    spycamview::init();
    psram_mark("after spycamview");
#endif
```
with:

```cpp
#if APPS_WEATHER
    app_shell::add(radarScreen, theme_style::names().weather,  weather_press_cycle, nullptr, false, radar_show_weather, radar_hide_weather, !theme_style::apps().weather);
#endif
#if !APPS_LAUNCH_ONE
    spycamview::init();
    psram_mark("after spycamview");
#endif
```
6. In the "theme asks for an app this build does not carry" block: leave `#if APPS_LAUNCH_ONE` but delete the `{ ta.weather, "Weather Radar" },` entry, and change the comment `(CUT-01, APPS_LAUNCH_ONE in config.h)` text is unchanged.

`src/platform/sim/sim_main.cpp`:
1. In `sim_register_apps`, replace the `#if !APPS_LAUNCH_ONE ... app_shell::add(radarScreen, ...weather...) ... app_shell::add(survScreen ...) #else (void)survScreen; #endif` block with:

```cpp
#if APPS_WEATHER
    app_shell::add(radarScreen, theme_style::names().weather,
                   []() { static bool fc = false; fc = !fc; ui_set_weather_forecast(fc); },  // push toggles WX/forecast (replaced by the turn handler in Task 5)
                   nullptr, false, []() { wx_map_prepare(g_set.homeLat, g_set.homeLon, 0); ui_weather_art_attach(); ui_show_view(1); }, nullptr, !theme_style::apps().weather);
#endif
#if !APPS_LAUNCH_ONE
    app_shell::add(survScreen,  theme_style::names().surveillance, nullptr, nullptr, false, nullptr, nullptr, !theme_style::apps().surveillance);
#else
    (void)survScreen;   // built above; not on launch one's roster (CUT-01)
#endif
```
2. In the `--wxshot` harness (`#if APPS_LAUNCH_ONE` ... `#else app_shell::selectApp(app_shell::APP_WEATHER); #endif`) change the guard to `#if !APPS_WEATHER` and the message to `--wxshot: the weather map is not in this build (APPS_WEATHER in config.h).`
3. Update the comment above `sim_register_apps` (`Clock(0), Flight Tracker(1), Weather Radar(2), Intel(3), Surveillance(4), Settings(5)`) to `Clock(0), Flight Tracker(1), Weather(2), Intel(3), Settings(4)`.

`src/theme/core/theme_style.h`: change `char weather[20]      = "Weather Radar";` to `char weather[20]      = "Weather";`.

`src/theme/core/theme_style.cpp`: replace `s_apps.weather      = (bool)CUSTOM_APP_WEATHER;` with:

```cpp
    // A build that carries Weather starts with it on. custom_apps.h says 0 because Launch Kit
    // generated it when Weather was off the roster; it is generated, so it is left alone and
    // this line reads it only for a build without APPS_WEATHER. A theme's own theme.json
    // roster still overrides either.
    s_apps.weather      = APPS_WEATHER ? true : (bool)CUSTOM_APP_WEATHER;
```

- [ ] **Step 3: Run the default-theme check to see it fail, then regenerate**

Run: `python3 -m unittest tests/test_default_theme.py 2>&1 | tail -8`
Expected: FAIL in `test_committed_theme_matches_the_firmware` (the committed `theme.yaml` still says `weather: false` and `"Weather Radar"`).

Run: `python3 tools/gen_default_theme.py && git diff --stat src/theme_assets/default/`
Expected: only `theme.yaml` changed (the clock PNGs are regenerated to identical bytes; if git shows them modified, run `git checkout -- src/theme_assets/default/*.png` so only the yaml is staged). Then `git diff src/theme_assets/default/theme.yaml` shows exactly two lines changed: `weather: true` and `weather: "Weather"`.

Run: `python3 -m unittest tests/test_default_theme.py 2>&1 | tail -3`
Expected: `OK`.

Run: `grep -rn "Weather Radar" src tests tools docs README.md 2>/dev/null | grep -v "^docs/superpowers/" | grep -v "^docs/lean-weather" | head`
Expected: any remaining hits are historical comments or the portal theme's own name; leave those. Fix a hit only if it is a user-visible label or a test expecting the old default.

- [ ] **Step 4: Build both targets**

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | tail -4` → `SUCCESS`
Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -4` → `SUCCESS`
Run: `bash tests/run_host_tests.sh 2>&1 | tail -4` → `all host tests passed`

Expected in the simulator log at startup: the roster check (`verifySlots`) prints nothing wrong. Confirm with:
Run: `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 20 .pio/build/native/program --newsshot /tmp/claude-501/newscheck 2>&1 | grep -E "newsshot|slot|verify" | head -12`
Expected: the `[newsshot]` lines list `Clock, Flight Tracker, Weather, News, Settings` (Weather present, not hidden).

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/app/shell/app_shell.h src/main.cpp src/platform/sim/sim_main.cpp src/theme/core/theme_style.h src/theme/core/theme_style.cpp src/theme_assets/default/theme.yaml
git commit -m "$(cat <<'EOF'
Weather rejoins the launch roster on its own switch

APPS_WEATHER splits Weather from Surveillance and the Ticker, which stay behind
APPS_LAUNCH_ONE. The compiled default roster turns it on, the label becomes "Weather"
(three screens now), and the default theme is regenerated to match.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: The icon renderer and the Now / 7-Day screens

**Files:**
- Create: `src/app/weather/wx_icon.h`, `src/app/weather/wx_icon.cpp`, `src/app/weather/wx_screens.h`, `src/app/weather/wx_screens.cpp`
- Modify: `platformio.ini` (native filter)

**Interfaces:**
- Consumes: `WxIconKind` / `wx_icon_classify` (Task 2); `WeatherSnapshot`, `WEATHER_DAYS`, `weather_round`, `weather_temp_to`, `weather_wind_to`, `weather_*_unit_name`, `weather_condition`, `weather_day_name` (Task 1).
- Produces:
  - `lv_obj_t *wx_icon::create(lv_obj_t *parent, int sizePx);` `void wx_icon::set(lv_obj_t *icon, WxIconKind kind);`
  - `wx_screens::Style { lv_color_t ink, soft, dim, accent, rain; }`, `enum class wx_screens::Screen { None, Now, Week };`
  - `void wx_screens::build(lv_obj_t *panel, const Style &st);` `void wx_screens::refresh(const WeatherSnapshot *w, bool imperial, const char *emptyLine);` `void wx_screens::show(Screen s);`

The drawing is checked by eye in the simulator (Task 6); this task's own gate is that both targets compile with the new files.

- [ ] **Step 1: Create the icon renderer**

`src/app/weather/wx_icon.h`:

```cpp
#pragma once
#include <lvgl.h>
#include "wx_icon_kind.h"

// One weather icon as one LVGL object: it draws itself from a WxIconKind with rectangles and
// lines, so there is no baked art to ship, decode or release, and the same code serves the
// large icon on the Now screen and the small ones on the 7-Day rows.
namespace wx_icon {
    // Square, transparent, not clickable. Draws Cloudy until set() says otherwise.
    lv_obj_t *create(lv_obj_t *parent, int sizePx);
    void set(lv_obj_t *icon, WxIconKind kind);
}
```

`src/app/weather/wx_icon.cpp`:

```cpp
#include "wx_icon.h"
#include <math.h>
#include <stdint.h>

namespace {

// Named colours, the icon's own palette. They are not the theme's: an outlook reads as an
// outlook (sun is yellow, rain is blue) whatever accent the design chose.
constexpr uint32_t COL_SUN        = 0xFFC02E;
constexpr uint32_t COL_MOON       = 0xDDE6F2;
constexpr uint32_t COL_MOON_SHADE = 0xB4C0D0;
constexpr uint32_t COL_CLOUD      = 0xD5DDE6;
constexpr uint32_t COL_CLOUD_DARK = 0x8A97A6;
constexpr uint32_t COL_RAIN       = 0x4DDCFF;
constexpr uint32_t COL_SNOW       = 0xFFFFFF;
constexpr uint32_t COL_BOLT       = 0xFFD84D;
constexpr uint32_t COL_FOG        = 0x9AA5B1;

constexpr int    SUN_RAYS      = 8;
constexpr float  PI_F          = 3.14159265f;
constexpr int    MIN_LINE_PX   = 2;

// Everything is placed in a unit square and scaled, so one description serves every size.
struct Pen {
    lv_draw_ctx_t *dc;
    lv_coord_t x0, y0;
    float s;
    lv_coord_t X(float u) const { return (lv_coord_t)(x0 + lroundf(u * s)); }
    lv_coord_t Y(float u) const { return (lv_coord_t)(y0 + lroundf(u * s)); }
    lv_coord_t W(float u) const { const int w = (int)lroundf(u * s); return (lv_coord_t)(w < MIN_LINE_PX ? MIN_LINE_PX : w); }
};

void disc(const Pen &p, float cx, float cy, float r, uint32_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(col);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    lv_area_t a;
    a.x1 = p.X(cx - r); a.y1 = p.Y(cy - r);
    a.x2 = p.X(cx + r) - 1; a.y2 = p.Y(cy + r) - 1;
    lv_draw_rect(p.dc, &d, &a);
}

void slab(const Pen &p, float x1, float y1, float x2, float y2, uint32_t col) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(col);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    lv_area_t a;
    a.x1 = p.X(x1); a.y1 = p.Y(y1); a.x2 = p.X(x2) - 1; a.y2 = p.Y(y2) - 1;
    lv_draw_rect(p.dc, &d, &a);
}

void bar(const Pen &p, float x1, float y1, float x2, float y2, float widthU, uint32_t col) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(col);
    d.width = p.W(widthU);
    d.opa = LV_OPA_COVER;
    d.round_start = 1;
    d.round_end = 1;
    lv_point_t a = { p.X(x1), p.Y(y1) };
    lv_point_t b = { p.X(x2), p.Y(y2) };
    lv_draw_line(p.dc, &d, &a, &b);
}

// A cloud is a flat base with three overlapping humps. (ox, oy) shift it, k scales it about
// the unit square's centre, so the same shape serves as the front cloud, a smaller one tucked
// behind a sun, or a darker storm cloud.
void cloud(const Pen &p, float ox, float oy, float k, uint32_t col) {
    auto U = [&](float v, float o) { return 0.5f + (v - 0.5f) * k + o; };
    slab(p, U(0.14f, ox), U(0.50f, oy), U(0.88f, ox), U(0.74f, oy), col);
    disc(p, U(0.34f, ox), U(0.52f, oy), 0.17f * k, col);
    disc(p, U(0.54f, ox), U(0.42f, oy), 0.22f * k, col);
    disc(p, U(0.72f, ox), U(0.54f, oy), 0.15f * k, col);
}

void sun(const Pen &p, float cx, float cy, float r, bool rays) {
    if (rays) {
        for (int i = 0; i < SUN_RAYS; ++i) {
            const float a = (2.0f * PI_F * i) / SUN_RAYS;
            const float c = cosf(a), s = sinf(a);
            bar(p, cx + c * r * 1.45f, cy + s * r * 1.45f, cx + c * r * 1.95f, cy + s * r * 1.95f, 0.05f, COL_SUN);
        }
    }
    disc(p, cx, cy, r, COL_SUN);
}

void moon(const Pen &p, float cx, float cy, float r) {
    disc(p, cx, cy, r, COL_MOON);
    disc(p, cx - r * 0.35f, cy - r * 0.25f, r * 0.18f, COL_MOON_SHADE);
    disc(p, cx + r * 0.30f, cy + r * 0.30f, r * 0.13f, COL_MOON_SHADE);
}

void drops(const Pen &p, int n, uint32_t col, float len, float width) {
    const float xs[3] = { 0.34f, 0.50f, 0.66f };
    for (int i = 0; i < n && i < 3; ++i)
        bar(p, xs[i] + 0.05f, 0.80f, xs[i] - 0.03f, 0.80f + len, width, col);
}

void draw_kind(const Pen &p, WxIconKind k) {
    switch (k) {
    case WxIconKind::ClearDay:
        sun(p, 0.5f, 0.5f, 0.22f, true);
        break;
    case WxIconKind::ClearNight:
        moon(p, 0.5f, 0.5f, 0.30f);
        break;
    case WxIconKind::PartlyDay:
        sun(p, 0.36f, 0.36f, 0.15f, true);
        cloud(p, 0.06f, 0.10f, 0.85f, COL_CLOUD);
        break;
    case WxIconKind::PartlyNight:
        moon(p, 0.36f, 0.36f, 0.20f);
        cloud(p, 0.06f, 0.10f, 0.85f, COL_CLOUD);
        break;
    case WxIconKind::Cloudy:
        cloud(p, -0.10f, -0.10f, 0.78f, COL_CLOUD_DARK);
        cloud(p, 0.04f, 0.06f, 0.92f, COL_CLOUD);
        break;
    case WxIconKind::Fog:
        cloud(p, 0.0f, -0.12f, 0.85f, COL_CLOUD);
        bar(p, 0.22f, 0.78f, 0.78f, 0.78f, 0.05f, COL_FOG);
        bar(p, 0.30f, 0.88f, 0.86f, 0.88f, 0.05f, COL_FOG);
        break;
    case WxIconKind::Drizzle:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD);
        drops(p, 3, COL_RAIN, 0.10f, 0.03f);
        break;
    case WxIconKind::Rain:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD_DARK);
        drops(p, 3, COL_RAIN, 0.17f, 0.05f);
        break;
    case WxIconKind::Showers:
        sun(p, 0.30f, 0.30f, 0.12f, false);
        cloud(p, 0.06f, -0.04f, 0.85f, COL_CLOUD);
        drops(p, 2, COL_RAIN, 0.15f, 0.05f);
        break;
    case WxIconKind::Snow:
        cloud(p, 0.0f, -0.08f, 0.88f, COL_CLOUD);
        disc(p, 0.34f, 0.86f, 0.045f, COL_SNOW);
        disc(p, 0.50f, 0.94f, 0.045f, COL_SNOW);
        disc(p, 0.66f, 0.86f, 0.045f, COL_SNOW);
        break;
    case WxIconKind::Thunder:
        cloud(p, 0.0f, -0.10f, 0.88f, COL_CLOUD_DARK);
        bar(p, 0.54f, 0.62f, 0.42f, 0.82f, 0.06f, COL_BOLT);
        bar(p, 0.42f, 0.82f, 0.58f, 0.82f, 0.06f, COL_BOLT);
        bar(p, 0.58f, 0.82f, 0.46f, 1.00f, 0.06f, COL_BOLT);
        break;
    case WxIconKind::Count:
        break;
    }
}

void draw_cb(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    Pen p;
    p.dc = lv_event_get_draw_ctx(e);
    p.x0 = a.x1;
    p.y0 = a.y1;
    p.s = (float)lv_obj_get_width(o);
    draw_kind(p, (WxIconKind)(uintptr_t)lv_obj_get_user_data(o));
}

}  // namespace

lv_obj_t *wx_icon::create(lv_obj_t *parent, int sizePx) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, sizePx, sizePx);
    lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_user_data(o, (void *)(uintptr_t)WxIconKind::Cloudy);
    lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

void wx_icon::set(lv_obj_t *icon, WxIconKind kind) {
    if (!icon) return;
    if ((WxIconKind)(uintptr_t)lv_obj_get_user_data(icon) == kind) return;
    lv_obj_set_user_data(icon, (void *)(uintptr_t)kind);
    lv_obj_invalidate(icon);
}
```

- [ ] **Step 2: Create the screens module**

`src/app/weather/wx_screens.h`:

```cpp
#pragma once
#include <lvgl.h>
#include "weather.h"

// The Now and 7-Day screens of the Weather app. They are built onto the weather panel that
// ui.cpp already owns (the radar lives on the same panel) and shown or hidden by mode.
// Nothing here fetches anything: ui.cpp hands it the snapshot.
namespace wx_screens {
    struct Style {
        lv_color_t ink;      // main text
        lv_color_t soft;     // secondary text
        lv_color_t dim;      // captions
        lv_color_t accent;   // day names
        lv_color_t rain;     // rain chance
    };
    enum class Screen { None, Now, Week };

    // Once, at UI construction. Everything starts hidden.
    void build(lv_obj_t *panel, const Style &st);
    // `w` is null when there is no forecast yet, and then `emptyLine` says which thing is
    // unwell (weather_status_text). `imperial` picks F / mph.
    void refresh(const WeatherSnapshot *w, bool imperial, const char *emptyLine);
    void show(Screen s);
}
```

`src/app/weather/wx_screens.cpp`:

```cpp
#include "wx_screens.h"
#include "wx_icon.h"
#include <stdio.h>

namespace {

// The weather panel is 462 px round, centred on a 466 px screen.
constexpr int PANEL_PX = 462;

// Now. Stacked from the top; the round edge only bites at the very top and bottom, and the
// narrowest line (the updated stamp, y 384) still has a 350 px chord.
constexpr int NOW_ICON_PX  = 128;
constexpr int NOW_ICON_Y   = 58;
constexpr int NOW_TEMP_Y   = 194;
constexpr int NOW_COND_Y   = 262;
constexpr int NOW_DETAIL_Y = 302;
constexpr int NOW_WIND_Y   = 328;
constexpr int NOW_STAMP_Y  = 384;
constexpr int NOW_TEXT_W   = 380;

// 7-Day. Seven rows in a column narrow enough for the top row's chord: at y 58 the circle is
// 306 px wide, so a 282 px column leaves a margin. One width for every row keeps the column
// straight; the middle rows are simply narrower than the circle they sit in.
constexpr int WEEK_TOP_Y   = 58;
constexpr int WEEK_ROW_H   = 46;
constexpr int WEEK_COL_W   = 282;
constexpr int WEEK_ICON_PX = 34;
constexpr int WEEK_DAY_X   = 0;
constexpr int WEEK_ICON_X  = 62;
constexpr int WEEK_TEMPS_X = 108;
constexpr int WEEK_RAIN_W  = 56;

wx_screens::Style s_st;
lv_obj_t *s_nowRoot = nullptr, *s_weekRoot = nullptr;
lv_obj_t *s_nowIcon = nullptr, *s_nowTempRow = nullptr, *s_nowTemp = nullptr, *s_nowUnit = nullptr;
lv_obj_t *s_nowCond = nullptr, *s_nowDetail = nullptr, *s_nowWind = nullptr, *s_nowStamp = nullptr, *s_nowEmpty = nullptr;

struct Row { lv_obj_t *box, *day, *icon, *temps, *rain; };
Row s_row[WEATHER_DAYS] = {};
lv_obj_t *s_weekEmpty = nullptr;

lv_obj_t *make_root(lv_obj_t *panel) {
    lv_obj_t *o = lv_obj_create(panel);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, PANEL_PX, PANEL_PX);
    lv_obj_center(o);
    lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t col, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, "");
    return l;
}

void set_hidden(lv_obj_t *o, bool hidden) {
    if (!o) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void wx_screens::build(lv_obj_t *panel, const Style &st) {
    s_st = st;

    // ---- Now ----
    s_nowRoot = make_root(panel);
    s_nowIcon = wx_icon::create(s_nowRoot, NOW_ICON_PX);
    lv_obj_align(s_nowIcon, LV_ALIGN_TOP_MID, 0, NOW_ICON_Y);

    // The temperature and its unit are two labels in a flex row so the pair stays centred
    // whatever the number's width, and the unit can be smaller and sit at the top of the digits.
    s_nowTempRow = lv_obj_create(s_nowRoot);
    lv_obj_remove_style_all(s_nowTempRow);
    lv_obj_set_size(s_nowTempRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_nowTempRow, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_flex_flow(s_nowTempRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nowTempRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s_nowTempRow, 6, 0);
    lv_obj_align(s_nowTempRow, LV_ALIGN_TOP_MID, 0, NOW_TEMP_Y);
    s_nowTemp = make_label(s_nowTempRow, &lv_font_montserrat_48, s_st.ink, LV_TEXT_ALIGN_CENTER);
    s_nowUnit = make_label(s_nowTempRow, &lv_font_montserrat_28, s_st.soft, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_pad_top(s_nowUnit, 4, 0);

    s_nowCond = make_label(s_nowRoot, &lv_font_montserrat_20, s_st.ink, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowCond, NOW_TEXT_W);
    lv_obj_align(s_nowCond, LV_ALIGN_TOP_MID, 0, NOW_COND_Y);
    s_nowDetail = make_label(s_nowRoot, &lv_font_montserrat_16, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowDetail, NOW_TEXT_W);
    lv_obj_align(s_nowDetail, LV_ALIGN_TOP_MID, 0, NOW_DETAIL_Y);
    s_nowWind = make_label(s_nowRoot, &lv_font_montserrat_16, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowWind, NOW_TEXT_W);
    lv_obj_align(s_nowWind, LV_ALIGN_TOP_MID, 0, NOW_WIND_Y);
    s_nowStamp = make_label(s_nowRoot, &lv_font_montserrat_12, s_st.dim, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowStamp, NOW_TEXT_W);
    lv_obj_align(s_nowStamp, LV_ALIGN_TOP_MID, 0, NOW_STAMP_Y);

    s_nowEmpty = make_label(s_nowRoot, &lv_font_montserrat_20, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowEmpty, NOW_TEXT_W - 60);
    lv_label_set_long_mode(s_nowEmpty, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_nowEmpty, LV_ALIGN_CENTER, 0, 0);

    // ---- 7-Day ----
    s_weekRoot = make_root(panel);
    for (int i = 0; i < WEATHER_DAYS; ++i) {
        Row &r = s_row[i];
        r.box = lv_obj_create(s_weekRoot);
        lv_obj_remove_style_all(r.box);
        lv_obj_set_size(r.box, WEEK_COL_W, WEEK_ROW_H - 2);
        lv_obj_clear_flag(r.box, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        lv_obj_align(r.box, LV_ALIGN_TOP_MID, 0, WEEK_TOP_Y + i * WEEK_ROW_H);
        lv_obj_set_style_border_side(r.box, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(r.box, 1, 0);
        lv_obj_set_style_border_color(r.box, s_st.dim, 0);
        lv_obj_set_style_border_opa(r.box, LV_OPA_30, 0);

        r.day = make_label(r.box, &lv_font_montserrat_16, s_st.accent, LV_TEXT_ALIGN_LEFT);
        lv_obj_align(r.day, LV_ALIGN_LEFT_MID, WEEK_DAY_X, 0);
        r.icon = wx_icon::create(r.box, WEEK_ICON_PX);
        lv_obj_align(r.icon, LV_ALIGN_LEFT_MID, WEEK_ICON_X, 0);
        r.temps = make_label(r.box, &lv_font_montserrat_16, s_st.ink, LV_TEXT_ALIGN_LEFT);
        lv_obj_align(r.temps, LV_ALIGN_LEFT_MID, WEEK_TEMPS_X, 0);
        r.rain = make_label(r.box, &lv_font_montserrat_16, s_st.rain, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_width(r.rain, WEEK_RAIN_W);
        lv_obj_align(r.rain, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    s_weekEmpty = make_label(s_weekRoot, &lv_font_montserrat_20, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_weekEmpty, NOW_TEXT_W - 60);
    lv_label_set_long_mode(s_weekEmpty, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_weekEmpty, LV_ALIGN_CENTER, 0, 0);
}

void wx_screens::refresh(const WeatherSnapshot *w, bool imperial, const char *emptyLine) {
    if (!s_nowRoot || !s_weekRoot) return;
    const bool have = w && w->valid;

    // ---- Now ----
    lv_obj_t *const nowBits[] = { s_nowIcon, s_nowTempRow, s_nowCond, s_nowDetail, s_nowWind, s_nowStamp };
    for (lv_obj_t *o : nowBits) set_hidden(o, !have);
    set_hidden(s_nowEmpty, have);
    if (have) {
        char buf[64];
        wx_icon::set(s_nowIcon, wx_icon_classify(w->code, w->isDay));
        snprintf(buf, sizeof buf, "%d", weather_round(weather_temp_to(w->tempC, imperial)));
        lv_label_set_text(s_nowTemp, buf);
        lv_label_set_text(s_nowUnit, weather_temp_unit_name(imperial));
        lv_label_set_text(s_nowCond, weather_condition(w->code));
        snprintf(buf, sizeof buf, "Feels %d %s     Humidity %d%%",
                 weather_round(weather_temp_to(w->feelsC, imperial)), weather_temp_unit_name(imperial), w->humidity);
        lv_label_set_text(s_nowDetail, buf);
        snprintf(buf, sizeof buf, "Wind %d %s", weather_round(weather_wind_to(w->windKmh, imperial)),
                 weather_wind_unit_name(imperial));
        lv_label_set_text(s_nowWind, buf);
        snprintf(buf, sizeof buf, "Updated %s", w->updated);
        lv_label_set_text(s_nowStamp, buf);
    } else {
        lv_label_set_text(s_nowEmpty, (emptyLine && emptyLine[0]) ? emptyLine : "Waiting for the forecast...");
    }

    // ---- 7-Day ----
    set_hidden(s_weekEmpty, have && w->dayCount > 0);
    if (!have || w->dayCount == 0)
        lv_label_set_text(s_weekEmpty, (emptyLine && emptyLine[0]) ? emptyLine : "No forecast days yet");
    for (int i = 0; i < WEATHER_DAYS; ++i) {
        Row &r = s_row[i];
        const bool shown = have && i < w->dayCount;
        set_hidden(r.box, !shown);
        if (!shown) continue;
        const WeatherDay &d = w->days[i];
        char buf[40];
        lv_label_set_text(r.day, i == 0 ? "Today" : weather_day_name(d.date));
        wx_icon::set(r.icon, wx_icon_classify(d.code, true));   // a day's outlook is a daytime one
        snprintf(buf, sizeof buf, "%d / %d %s", weather_round(weather_temp_to(d.tempMaxC, imperial)),
                 weather_round(weather_temp_to(d.tempMinC, imperial)), weather_temp_unit_name(imperial));
        lv_label_set_text(r.temps, buf);
        snprintf(buf, sizeof buf, "%d%%", d.rainChance);
        lv_label_set_text(r.rain, buf);
    }
}

void wx_screens::show(Screen s) {
    set_hidden(s_nowRoot, s != Screen::Now);
    set_hidden(s_weekRoot, s != Screen::Week);
}
```

In `platformio.ini` native `build_src_filter`, add `+<app/weather/wx_icon.cpp> +<app/weather/wx_screens.cpp>` after `+<app/weather/wx_icon_kind.cpp>`.

- [ ] **Step 3: Verify it compiles on both targets**

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|warning: unused|SUCCESS" | head`
Expected: `SUCCESS`, no errors. (The two modules are compiled but not yet called; that is fine, ui.cpp is wired in Task 5.)

Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS" | head`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/app/weather/wx_icon.h src/app/weather/wx_icon.cpp src/app/weather/wx_screens.h src/app/weather/wx_screens.cpp platformio.ini
git commit -m "$(cat <<'EOF'
Weather: icon renderer and the Now and 7-Day screens

One custom-draw LVGL object per icon, drawn from a kind, so no baked art and no PSRAM.
The screens are built but not yet wired into the weather panel.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Wire the screens into the weather panel and the knob

**Files:**
- Modify: `src/app/ui/ui.h`, `src/app/ui/ui.cpp`, `src/main.cpp`, `src/platform/sim/sim_main.cpp`

**Interfaces:**
- Consumes: `wx_screens::{Style, Screen, build, refresh, show}` (Task 4); `weather_screen_step`, `WX_SCREEN_*`, `weather_status_get/text` (Task 1).
- Produces (`ui.h`): `void ui_weather_step(int delta);` `int ui_weather_screen(void);` (returns a `WX_SCREEN_*` value) `void ui_weather_reset(void);` (lands on Now). Removes `ui_set_weather_forecast` and `ui_weather_is_forecast`.

All `ui.cpp` line numbers below are from the current file; find the text, not the number.

- [ ] **Step 1: `ui.h`**

Replace the last two declarations:

```cpp
void ui_set_weather_forecast(bool forecast); // false = WX radar, true = 3-day forecast
bool ui_weather_is_forecast(void);           // current weather sub-view, for the knob-push cycle
```
with:

```cpp
// The Weather app's three screens: Now, Radar, 7-Day (WX_SCREEN_* in weather.h). A turn of the
// knob steps them, wrapping either way; entering the app lands on Now.
void ui_weather_step(int delta);             // one detent: >0 forward, <0 back
int  ui_weather_screen(void);                // the screen showing now, as a WX_SCREEN_* value
void ui_weather_reset(void);                 // back to Now; call before ui_show_view(1)
```

- [ ] **Step 2: `ui.cpp` — mode and declarations**

Add `#include "wx_screens.h"` after `#include "wx_radar.h"`.

Replace:

```cpp
enum WeatherViewMode { WEATHER_RADAR, WEATHER_CLOUDS, WEATHER_FORECAST };
static WeatherViewMode s_weatherMode = WEATHER_RADAR;
static lv_obj_t *s_fcCurrent = nullptr, *s_fcCondition = nullptr, *s_fcUpdated = nullptr;
static lv_obj_t *s_fcMetricName[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcMetricValue[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDay[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayCondition[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayTemp[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayRain[3] = { nullptr, nullptr, nullptr };
```
with:

```cpp
// The three screens the knob steps between are WX_SCREEN_* (weather.h); CLOUDS is the retired
// satellite view, unreachable from the knob and kept only as the comment further down says.
enum WeatherViewMode {
    WEATHER_NOW    = WX_SCREEN_NOW,
    WEATHER_RADAR  = WX_SCREEN_RADAR,
    WEATHER_WEEK   = WX_SCREEN_WEEK,
    WEATHER_CLOUDS = WX_SCREEN_COUNT,
};
// Lands on Now: the app opens on the temperature, and the radar loads behind it.
static WeatherViewMode s_weatherMode = WEATHER_NOW;
```

- [ ] **Step 3: `ui.cpp` — the radar-only timers and text canvas**

In `wx_status_timer_cb`, replace `if (s_weatherMode == WEATHER_FORECAST) return;` with `if (s_weatherMode != WEATHER_RADAR) return;`.

In `wx_text_refresh`, immediately after `const theme_style::Weather &ws = theme_style::weather();` insert:

```cpp
    // Only the radar draws these. The canvas is full-screen, so left showing it would print a
    // design's radar text across the Now and 7-Day screens. Hidden, not freed: the buffer is
    // 434 KB and turning the knob back to the radar should not have to find it again.
    if (s_weatherMode != WEATHER_RADAR) {
        if (s_wxTextCanvas) lv_obj_add_flag(s_wxTextCanvas, LV_OBJ_FLAG_HIDDEN);
        return;
    }
```

- [ ] **Step 4: `ui.cpp` — `build_weather`**

1. Replace `WeatherSnapshot w;\n    if (!weather_get(w)) {` (the first two lines after the early `return`) with:

```cpp
    WeatherSnapshot w;
    const bool haveWx = weather_get(w);
    if (!haveWx) {
```
2. Delete the whole block from `        char current[24];` through the closing brace of the `for (int col = 0; col < 3; ++col) { ... }` loop (it populated the old `s_fc*` labels). Keep the `char days[320] = "";` block that follows.
3. Replace `    const bool cloudMode = s_weatherMode == WEATHER_CLOUDS;\n    const bool forecastMode = s_weatherMode == WEATHER_FORECAST;` with:

```cpp
    const bool cloudMode = s_weatherMode == WEATHER_CLOUDS;
    // Now and 7-Day are drawn by wx_screens; only these two use the map's own objects.
    const bool radarLike = s_weatherMode == WEATHER_RADAR || cloudMode;
    wx_screens::refresh(haveWx ? &w : nullptr, s_wxImperial, weather_status_text(weather_status_get()));
    wx_screens::show(s_weatherMode == WEATHER_NOW  ? wx_screens::Screen::Now
                   : s_weatherMode == WEATHER_WEEK ? wx_screens::Screen::Week
                                                   : wx_screens::Screen::None);
```
4. Delete the `lv_obj_t *forecastObjs[] = { ... };` array (nine lines ending `s_fcDayRain[0], s_fcDayRain[1], s_fcDayRain[2]\n    };`) and the loop `for (lv_obj_t *o : forecastObjs) if (o) { ... }` (three lines).
5. Replace every remaining use of `forecastMode`:
   - `for (lv_obj_t *o : radarObjs) if (o) {\n        if (forecastMode) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);` → `if (!radarLike) lv_obj_add_flag(...); else lv_obj_clear_flag(...);`
   - `if (s_wxAttrib && !forecastMode) {` → `if (s_wxAttrib && radarLike) {`
   - `if (wxSlots && !forecastMode) {` → `if (wxSlots && radarLike) {`
   - `if (!forecastMode && haveImage) lv_obj_add_flag(s_wxStatus, ...)` → `if (radarLike && haveImage) ...`
   - `if (!forecastMode && !haveImage) lv_obj_add_flag(s_wxCanvas, ...)` → `if (radarLike && !haveImage) ...`
6. Replace the title assignment:

```cpp
    if (s_weatherTitle) lv_label_set_text(s_weatherTitle,
        s_weatherMode == WEATHER_RADAR ? "WX RADAR" :
        s_weatherMode == WEATHER_CLOUDS ? "SAT CLOUDS" : "WEATHER");
```
with:

```cpp
    if (s_weatherTitle) lv_label_set_text(s_weatherTitle,
        s_weatherMode == WEATHER_NOW    ? "WEATHER" :
        s_weatherMode == WEATHER_RADAR  ? "WX RADAR" :
        s_weatherMode == WEATHER_WEEK   ? "7 DAY" : "SAT CLOUDS");
```

- [ ] **Step 5: `ui.cpp` — the API functions**

Replace `ui_set_weather_forecast`, its comment block and `ui_weather_is_forecast` with:

```cpp
// WEATHER_CLOUDS is no longer reachable from here (nothing assigns it) — left in place rather
// than ripped out in case satellite cloud view comes back some other way. The knob steps the
// three screens in weather_screen_step()'s cycle.
void ui_weather_step(int delta) {
    s_weatherMode = (WeatherViewMode)weather_screen_step((int)s_weatherMode, delta);
    build_weather();
}
int ui_weather_screen(void) { return (int)s_weatherMode; }
void ui_weather_reset(void) {
    s_weatherMode = WEATHER_NOW;
    build_weather();
}
```

- [ ] **Step 6: `ui.cpp` — construction**

Replace the whole block from the comment `    // Forecast mode: independent, aligned objects instead of a tiny text table.` through the `lv_obj_align(s_fcUpdated, LV_ALIGN_TOP_MID, 0, 365);` line (the `s_fc*` construction, ending just before `umark("after weather tile");`) with:

```cpp
    // The Now and 7-Day screens. Built last so they sit above the radar's objects; both start
    // hidden and build_weather() shows whichever the knob has chosen.
    wx_screens::build(wp, { UI_INK, UI_SOFT, UI_DIM, UI_GREEN, lv_color_hex(0x4DDCFF) });
```

- [ ] **Step 7: `main.cpp` and `sim_main.cpp` — knob and entry**

`main.cpp`: replace `weather_press_cycle` and its comment block (from `// Knob push while on Weather: one cycle ...` through the closing brace) with:

```cpp
// A turn on Weather steps its three screens (Now, Radar, 7-Day), wrapping either way. Push is
// deliberately unassigned, so the shell shows its standard hint. The rock still opens the
// switcher, as everywhere.
static void weather_turn(int delta) { ui_weather_step(delta); }
```
Change the registration to `app_shell::add(radarScreen, theme_style::names().weather, nullptr, weather_turn, false, radar_show_weather, radar_hide_weather, !theme_style::apps().weather);`. In `radar_show_weather()`, add `ui_weather_reset();` on the line before `ui_show_view(1);   // tile 1 since list/stats went`. Update the comment above `weather_press_cycle` users in the adsb task (the one that says `weather_press_cycle() in main.cpp`) to say `ui_weather_step()`.

`sim_main.cpp`: replace the weather registration (the `#if APPS_WEATHER` block's `app_shell::add`) with:

```cpp
#if APPS_WEATHER
    app_shell::add(radarScreen, theme_style::names().weather,
                   nullptr,                                       // push: unassigned, as on the device
                   [](int d) { ui_weather_step(d); },             // turn steps Now / Radar / 7-Day
                   false,
                   []() { wx_map_prepare(g_set.homeLat, g_set.homeLon, 0); ui_weather_art_attach(); ui_weather_reset(); ui_show_view(1); },
                   nullptr, !theme_style::apps().weather);
#endif
```
And in the `--wxshot` harness, after `app_shell::selectApp(app_shell::APP_WEATHER);` add `ui_weather_step(1);   // the app lands on Now; this harness photographs the radar`.

- [ ] **Step 8: Build, host tests, and a first look**

Run: `grep -rn "ui_set_weather_forecast\|ui_weather_is_forecast\|WEATHER_FORECAST\|s_fcCurrent\|weather_press_cycle" src | grep -v "^src/app/weather/weather_view.cpp"`
Expected: no output (comments in `main.cpp` about the retired cycle are updated in Step 7).

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS"` → `SUCCESS`
Run: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS"` → `SUCCESS`
Run: `bash tests/run_host_tests.sh 2>&1 | tail -3` → `all host tests passed`

- [ ] **Step 9: Commit**

```bash
git add src/app/ui/ui.h src/app/ui/ui.cpp src/main.cpp src/platform/sim/sim_main.cpp
git commit -m "$(cat <<'EOF'
Weather: knob turns step Now, Radar and 7-Day

The weather panel gains a three-way mode; Now and 7-Day are drawn by wx_screens and the radar is
untouched. Entering the app lands on Now, push is unassigned, the old 3-day forecast objects go.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Simulator — live Brisbane forecast, a three-screen harness, and looking at the result

**Files:**
- Modify: `src/platform/sim/sim_main.cpp`

**Interfaces:**
- Consumes: `weather_fetch`, `weather_store`, `weather_status_set` (Tasks 1, 3); `ui_weather_step`, `ui_weather_screen`, `ui_weather_reset`, `WX_SCREEN_*` (Task 5).
- Produces: `--wxscreens <prefix>` writes `<prefix>-now.bmp`, `<prefix>-radar.bmp`, `<prefix>-week.bmp` and prints `[wxscreens]` check lines; the simulator fetches the real forecast for its home location.

- [ ] **Step 1: Replace the mock forecast with the real fetch**

Add above `sim_apply_home_location`:

```cpp
// The forecast, from the same Open-Meteo request the device makes (weather_fetch builds it for
// both). No mock: a screenshot of a made-up forecast tells nobody whether the parser and the
// screens agree with the real service. Offline, the Now screen shows its honest empty state,
// which is itself worth being able to look at.
static void sim_refresh_forecast(double lat, double lon) {
    WeatherSnapshot forecast = {};
    if (weather_fetch(lat, lon, forecast)) {
        weather_store(forecast);
        weather_status_set(WEATHER_STATUS_OK);
        printf("[sim] forecast %.4f, %.4f: %d C, code %d, %d days\n", lat, lon,
               weather_round(forecast.tempC), forecast.code, forecast.dayCount);
    } else {
        weather_status_set(WEATHER_STATUS_FAILED);
        printf("[sim] forecast fetch failed for %.4f, %.4f (offline?)\n", lat, lon);
    }
}
```
Make sure `#include "weather_client.h"` is present in `sim_main.cpp`'s includes (add it after `#include "weather.h"` if not).

In `sim_apply_home_location`, add `sim_refresh_forecast(lat, lon);` before `sim_refresh_weather(lat, lon);`.

In `main`, delete the mock block from `WeatherSnapshot forecast = {};` through `weather_store(forecast);   // still-mock forecast panel ...` and put in its place:

```cpp
    sim_refresh_forecast(g_set.homeLat, g_set.homeLon);
```
(`g_set`, not the compiled constants, for the same reason the radar fetch below it uses it: ORBLAT/ORBLON move the home.)

- [ ] **Step 2: Add the three-screen harness**

Next to the other shot flags (after the `wxShot` definition), add:

```cpp
    // --wxscreens <prefix> drives the Weather app the way a person does, with knob turns, and
    // photographs each of its three screens: <prefix>-now.bmp, -radar.bmp, -week.bmp. It also
    // checks the two things a photograph cannot: that a turn past the last screen wraps, in
    // both directions, and that leaving the app on 7-Day and coming back lands on Now.
    const char *wxScreens  = (argc >= 3 && strcmp(argv[1], "--wxscreens")  == 0) ? argv[2] : NULL;
```
and `(void)wxScreens;` next to `(void)wxShot;`.

After the `--wxshot` `if (wxShot) { ... }` block, add:

```cpp
        static int wsStep = 0;
        static Uint32 wsAt = 0;
        if (wxScreens) {
            auto shot = [&](const char *name) {
                char path[300]; snprintf(path, sizeof(path), "%s-%s.bmp", wxScreens, name);
                sim_save_frame(path);
            };
            auto check = [&](const char *what, int got, int want) {
                printf("[wxscreens] %-34s %s (screen=%d, want %d)\n", what, got == want ? "ok" : "FAIL", got, want);
            };
#if !APPS_WEATHER
            if (wsStep == 0) { printf("[wxscreens] Weather is not in this build (APPS_WEATHER).\n"); run = false; }
#else
            if (wsStep == 0 && now - start > 3000) {
                app_shell::selectApp(app_shell::APP_WEATHER);
                wsStep = 1; wsAt = now;
            } else if (wsStep == 1 && now - wsAt > 1500) {
                check("entering the app lands on Now", ui_weather_screen(), WX_SCREEN_NOW);
                shot("now");
                input_router::dispatch(1, false);
                wsStep = 2; wsAt = now;
            } else if (wsStep == 2 && now - wsAt > 3000) {
                check("one turn forward is Radar", ui_weather_screen(), WX_SCREEN_RADAR);
                shot("radar");
                input_router::dispatch(1, false);
                wsStep = 3; wsAt = now;
            } else if (wsStep == 3 && now - wsAt > 800) {
                check("two turns forward is 7-Day", ui_weather_screen(), WX_SCREEN_WEEK);
                shot("week");
                input_router::dispatch(1, false);
                wsStep = 4; wsAt = now;
            } else if (wsStep == 4 && now - wsAt > 300) {
                check("a third turn wraps to Now", ui_weather_screen(), WX_SCREEN_NOW);
                input_router::dispatch(-1, false);
                wsStep = 5; wsAt = now;
            } else if (wsStep == 5 && now - wsAt > 300) {
                check("a turn back from Now wraps to 7-Day", ui_weather_screen(), WX_SCREEN_WEEK);
                app_shell::selectApp(app_shell::APP_CLOCK);
                wsStep = 6; wsAt = now;
            } else if (wsStep == 6 && now - wsAt > 600) {
                app_shell::selectApp(app_shell::APP_WEATHER);
                wsStep = 7; wsAt = now;
            } else if (wsStep == 7 && now - wsAt > 600) {
                check("re-entering after 7-Day lands on Now", ui_weather_screen(), WX_SCREEN_NOW);
                run = false;
            }
#endif
        }
```

- [ ] **Step 3: Build and run it at Brisbane**

Run: `~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS"` → `SUCCESS`

Run:
```bash
mkdir -p /tmp/claude-501/wx && ORBLAT=-27.47 ORBLON=153.03 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout 90 .pio/build/native/program --wxscreens /tmp/claude-501/wx/bne 2>&1 | grep -E "\[sim\] (forecast|weather)|wxscreens|error|FAIL"
```
Expected: a `[sim] forecast -27.4700, 153.0300: NN C, code N, 7 days` line, then seven `[wxscreens] ... ok` lines and no `FAIL`. (The simulator ignores SIGTERM; if it is still running after `timeout` fires, `pkill -9 program`.)

If the forecast line says `fetch failed`, check `curl -s "http://api.open-meteo.com/v1/forecast?latitude=-27.47&longitude=153.03&current=temperature_2m&forecast_days=7" | head -c 200` to tell a network problem from a code fault, and say which in the report.

- [ ] **Step 4: Look at the three screenshots, and fix what is wrong**

Read each of `/tmp/claude-501/wx/bne-now.bmp`, `bne-radar.bmp`, `bne-week.bmp` with the Read tool (it displays images; if it refuses `.bmp`, convert with `sips -s format png <in> --out <out>.png` first).

Check, and fix in `wx_screens.cpp` / `wx_icon.cpp` until each is true:
- **Now:** a recognisable icon for the real condition, the temperature large and centred with a small unit `C` beside it (Brisbane must be Celsius), the condition word, the feels/humidity line, the wind line, `Updated HH:MM`; nothing clipped by the round edge; the title reads `WEATHER`.
- **Radar:** the same radar the app showed before (rain frames over roads and coast, range label), title `WX RADAR`; none of the Now or 7-Day objects visible behind it.
- **7-Day:** seven rows, `Today` first, each with an icon, `hi / lo C` and a rain percentage; the top and bottom rows inside the circle; title `7 DAY`.
- The icons look like what they are at both sizes. If a shape is off (clouds too small, rays colliding with the humps), adjust the unit-square numbers in `wx_icon.cpp`, rebuild, re-shoot.

The live week will not exercise every icon kind. Classification is covered by the host test; the drawing is checked by eye only for the kinds the live forecast actually shows, and the report must list which kinds were and were not seen rather than claiming all eleven.

- [ ] **Step 5: Commit**

```bash
git add src/platform/sim/sim_main.cpp src/app/weather/wx_screens.cpp src/app/weather/wx_icon.cpp
git commit -m "$(cat <<'EOF'
Simulator: live forecast and a --wxscreens harness for the three Weather screens

Drives the knob, photographs Now, Radar and 7-Day, and checks wrap-around and re-entry.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
(Stage `wx_screens.cpp` and `wx_icon.cpp` only if Step 4 changed them.)

---

### Task 7: Docs, version, and the final gate

**Files:**
- Modify: `src/config.h` (`FW_VERSION`), `README.md`, `docs/ARCHITECTURE.md`

**Interfaces:**
- Consumes: everything above.
- Produces: `FW_VERSION "2.17.0"`; docs that match the roster.

- [ ] **Step 1: Docs and version**

`src/config.h`: change `#define FW_VERSION "2.16.25"` to `#define FW_VERSION "2.17.0"` (a new feature, and the version Studio compares to decide an Orb is behind).

`README.md`: replace the sentence on the `A weather radar, a stock ticker and a camera view are in the tree but compiled out of launch one ...` line with: `Weather (temperature and outlook, rain radar, seven-day forecast, chosen with the knob) is on launch one. A stock ticker and a camera view are in the tree but compiled out of it (`APPS_LAUNCH_ONE` in [`src/config.h`](src/config.h)), so they are absent from the menu rather than present and switched off.` and change `the weather radar, out of launch one` on the repo-layout line to `the weather radar`.

`docs/ARCHITECTURE.md`: on the app-list line (`Clock`, `Flight Tracker`, `Weather Radar`, `Intel`, `Surveillance`, `Settings`) change `Weather Radar` to `Weather (Now / Radar / 7-Day on the knob)`.

- [ ] **Step 2: The full gate**

Run each and confirm the stated result; report any that differ instead of adjusting the expectation:

```bash
bash tests/run_host_tests.sh 2>&1 | tail -3                         # all host tests passed
python3 -m unittest tests/test_default_theme.py 2>&1 | tail -3       # OK
~/.platformio/penv/bin/pio run -e native 2>&1 | grep -E "error|SUCCESS"                      # SUCCESS
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | grep -E "error|SUCCESS|RAM:|Flash:"   # SUCCESS
ORBLAT=-27.47 ORBLON=153.03 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 90 .pio/build/native/program --wxscreens /tmp/claude-501/wx/final 2>&1 | grep -cE "wxscreens.* ok"   # 7
grep -rn "workers.dev\|INTEL_GATEWAY_HOST\|zionbrock" src/app/weather src/core/net_fetch.cpp | grep -v "^src/app/weather/weather_view.cpp"    # no output: the weather path never names the gateway
python3 tools/audit.py 2>&1 | tail -5     # needs the Studio checkout; if ORB_STUDIO_DIR is absent it will say so: report that, do not skip silently
git status --short                         # clean apart from files you meant to leave
```

Do **not** run `tools/publish-firmware.sh`, `tools/ship.sh` or `wrangler deploy`. The binary has not booted on hardware.

- [ ] **Step 3: Commit**

```bash
git add src/config.h README.md docs/ARCHITECTURE.md
git commit -m "$(cat <<'EOF'
Bump FW_VERSION to 2.17.0; docs: Weather is back on the launch roster

Not published: this build has not booted on an Orb yet (CLAUDE.md rule 1).

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 4: Report, honestly**

The report to the owner states: what was verified (host tests, both builds, the simulator run at Brisbane with the seven check lines, the three screenshots and what they showed), what was **not** (real AMOLED rendering; PSRAM behaviour of the radar buffers beside the new screens; knob feel; which icon kinds the live week happened to exercise; `tools/audit.py` if Studio was unreachable; large-text mode leaves the new screens at fixed sizes), and the follow-ups (Orb Studio parity for Now / 7-Day per `docs/adding-a-screen.md`; hardware boot before `publish-firmware.sh`; the dead `weather_view.cpp`, left alone). Then follow `superpowers:finishing-a-development-branch` — merge `--no-ff`, explicit staging, no deploy — only when the owner says to.

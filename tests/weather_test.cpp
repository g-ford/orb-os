// Host test for src/app/weather/weather.{h,cpp}.   tests/run_weather_test.sh
#include "weather.h"
#include "wx_icon_kind.h"

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
    every_documented_code_has_the_right_icon();
    unknown_codes_are_unknown();
    the_words_agree_with_the_icon();
    every_kind_has_a_name();
    printf("weather tests passed\n");
    return 0;
}

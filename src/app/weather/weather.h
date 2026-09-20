#pragma once

#include <stddef.h>

// Portable weather snapshot shared by the network task and LVGL UI.
// Temperatures are Celsius, wind is km/h and precipitation is millimetres;
// the UI converts these according to the selected unit preset.

// One row per day of the 7-Day screen. The theme's text tokens ({day1}..{day4}) still read
// only the first four.
#define WEATHER_DAYS 7

// A rain chance the service had no data for. Shown as "--", never as 0: a null probability is
// not a forecast of dry weather.
#define WEATHER_RAIN_UNKNOWN (-1)

// The forecast is Open-Meteo's, whose free tier asks for attribution. Shown on the Now and 7-Day
// screens, where it cannot be switched off, and in the README.
#define WEATHER_CREDIT "Weather data by Open-Meteo.com"

struct WeatherDay {
    char date[11];
    int code;
    float tempMinC;
    float tempMaxC;
    int rainChance;      // percent, or WEATHER_RAIN_UNKNOWN
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

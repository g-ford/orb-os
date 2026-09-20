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

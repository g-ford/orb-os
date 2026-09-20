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

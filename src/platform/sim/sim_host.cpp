// The mock host_* API: what Settings sees of the device (brightness, WiFi, location, recents, units). The real
// definitions live in main.cpp, which is not part of the native build. Fake, reasonable values so the screens
// render with real copy and layout; location is the exception and goes to the real endpoints (see below).
#include <SDL.h>
#include <math.h>
#include <lvgl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "config.h"
#include "radar_view.h"
#include "radar_sprite.h"   // radar_sprite_release() — Flight Tracker's onExit
#include "roads_sd.h"       // roads_sd::set_root() — this desktop build's stand-in for the SD card
#include "ui.h"
#include "route.h"
#include "weather.h"
#include "weather_client.h"
#include "wx_radar.h"
#include "wx_radar_client.h"
#include "cloud_image.h"
#include "aircraft.h"
#include "clock_view.h"
#include "intel_view.h"
#include "ticker_view.h"
#include "ticker.h"
#include "app_shell.h"
#include "theme_select.h"
#include "theme_font.h"
#include "theme_select.h"
#include "update_ui.h"   // --updateshot, below
#include "knob_help.h"  // --knobshot, below
#include "swipe.h"      // --swipeshot, below
#include <functional>
#include <string>
#include <vector>
#include "clock_wind.h"  // --windshot, below
#include "wind_notice.h"
#include "theme_style.h"   // per-theme app roster (apps()) + the scope's operational values (radar())
#include "settings_view.h"
#include "wheel.h"
#include "custom_boot_target.h"  // CUSTOM_BOOT_TARGET — set by whichever theme push (clock/splash/radar) ran last
#include "custom_apps.h"         // CUSTOM_APP_* — which apps a theme flash includes in the menu
#include "custom_sprite.h"       // custom_shadow(), custom_sprite_release()
#include "custom_radar.h"        // CUSTOM_HAS_RADAR — a theme push changes the Flight Tracker knob's behavior
#include "knob.h"           // consumed-input API (implemented by sim_knob.cpp on native)
#include "sim_knob.h"       // inject SDL events into the knob:: backend
#include "input_router.h"   // shared knob->app_shell routing (same as the device)
#include "native_http.h"
#include "sim_internal.h"
#include <ArduinoJson.h>
#include <string>
#include "sim_internal.h"

// ---- host_* stubs (sim only): Settings reads these at render time; the real
// definitions live in main.cpp, which isn't part of the native build. Fake,
// reasonable values so the Settings screen renders with real copy and layout.
//
// Location is the exception: host_set_location[_named]/host_locate_current/host_geocode
// hit the SAME real endpoints main.cpp does (ip-api.com for IP-locate, Open-Meteo's
// geocoding API for search), just without the reboot-to-apply the device uses — the sim
// applies immediately via sim_apply_home_location (re-centers the mock radar, re-fetches
// live weather + Intel). Recents are simple in-memory storage here (no NVS on desktop);
// Settings seeds it with a handful of cities (incl. Phoenix) on first run.

int  host_get_brightness() { return 80; }
void host_set_brightness(int, bool) {}
void host_update_bright(bool) {}       // the device forces full brightness under an update notice
uint32_t host_get_idle_ms() { return 0; }
void host_set_idle_ms(uint32_t) {}
void host_set_location(double lat, double lon) { sim_apply_home_location("", lat, lon); }

namespace {
    struct SimRecent { char name[40]; double lat, lon; };
    constexpr int SIM_RECENTS_MAX = 8;
    SimRecent g_recents[SIM_RECENTS_MAX];
    int       g_recentCount = 0;
}

int host_recents_get(char names[][40], double *lats, double *lons, int maxN) {
    const int n = (g_recentCount < maxN) ? g_recentCount : maxN;
    for (int i = 0; i < n; ++i) {
        snprintf(names[i], 40, "%s", g_recents[i].name);
        lats[i] = g_recents[i].lat;
        lons[i] = g_recents[i].lon;
    }
    return n;
}

void host_recents_add(const char *name, double lat, double lon) {
    if (!name || !name[0]) return;
    const int n = (g_recentCount < SIM_RECENTS_MAX) ? g_recentCount + 1 : SIM_RECENTS_MAX;
    for (int i = n - 1; i > 0; --i) g_recents[i] = g_recents[i - 1];   // shift down, insert at front
    snprintf(g_recents[0].name, sizeof(g_recents[0].name), "%s", name);
    g_recents[0].lat = lat; g_recents[0].lon = lon;
    g_recentCount = n;
}

// Mirrors host_set_location_named() on the device: record in recents, then apply.
void host_set_location_named(const char *name, double lat, double lon) {
    host_recents_add(name, lat, lon);
    sim_apply_home_location(name, lat, lon);
}

// Approximate current location from the public IP (city-level) — same ip-api.com call
// and fields main.cpp's host_locate_current() uses. On-device this reboots to apply; the
// sim just applies directly and returns (no timezone-offset handling — the sim's clock
// is already a standalone mock, see the 1Hz loop below, so there's no TZ state to sync).
bool host_locate_current() {
    std::string body;
    if (!native_https_get("http://ip-api.com/json/?fields=status,message,city,region,lat,lon,offset",
                          ORB_USER_AGENT, body, 6000)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, body)) return false;
    if (strcmp(doc["status"] | "", "success") != 0) return false;
    const double lat = doc["lat"] | 1000.0;
    const double lon = doc["lon"] | 1000.0;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
    const char *city = doc["city"] | "";
    const char *region = doc["region"] | "";
    char nm[40] = "";
    if (city[0]) snprintf(nm, sizeof(nm), "%s%s%s", city, region[0] ? ", " : "", region);
    if (nm[0]) host_recents_add(nm, lat, lon);   // matches the device: recorded directly, not via _named
    sim_apply_home_location(nm[0] ? nm : "current location", lat, lon);
    return true;
}

// Free city search (Open-Meteo geocoding, no key) — same endpoint main.cpp's
// host_geocode() uses. Settings already debounces this to one blocking call per pause
// in typing (see settings_view.cpp's search_tick), so a synchronous curl fetch is fine.
int host_geocode(const char *query, char names[][40], double *lats, double *lons, int maxN) {
    if (!query || strlen(query) < 2) return 0;
    std::string q;
    for (const char *p = query; *p; ++p) q += (*p == ' ') ? std::string("%20") : std::string(1, *p);
    char url[256];
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&language=en&format=json",
             q.c_str(), maxN);
    std::string body;
    if (!native_https_get(url, ORB_USER_AGENT, body, 6000)) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, body)) return 0;
    JsonArrayConst results = doc["results"].as<JsonArrayConst>();
    int n = 0;
    for (JsonObjectConst r : results) {
        if (n >= maxN) break;
        const char *name   = r["name"]   | "";
        const char *admin1 = r["admin1"] | "";
        const char *cc     = r["country_code"] | "";
        const char *sub    = admin1[0] ? admin1 : cc;
        snprintf(names[n], 40, "%s%s%s", name, sub[0] ? ", " : "", sub);
        lats[n] = r["latitude"]  | 1000.0;
        lons[n] = r["longitude"] | 1000.0;
        if (lats[n] <= 90 && lats[n] >= -90) n++;
    }
    return n;
}

int  host_get_volume() { return 70; }
void host_set_volume(int, bool) {}
bool host_sound_radar() { return true; }
void host_sound_set_radar(bool) {}
bool host_sound_chime() { return true; }
void host_sound_set_chime(bool) {}
void host_sound_preview_chime() {}
void host_sound_preview_beep() {}
int  host_chime_count() { return 1; }
const char *host_chime_name(int) { return "Westminster"; }
int  host_chime_index() { return 0; }
void host_chime_set(int) {}
void host_chime_preview(int) {}
void host_wifi_scan_start() {}
// A plausible scan, so --wifishot can photograph the network list with something in it.
// Deliberately includes a name too long for the dial: truncation is a layout decision and a
// decision nobody can see is one nobody has checked.
int host_wifi_scan_result(char names[][33], int8_t *rssi, bool *isOpen, int maxN) {
    static const char *FAKE[] = {
        "Brock Home", "BT-HUB-9F2A", "Pixel_4821",
        "VM-Superhub-Guest-Network-5G", "eduroam",
    };
    static const int8_t RSSI[] = { -42, -58, -67, -71, -80 };
    const int n = (int)(sizeof(FAKE) / sizeof(FAKE[0]));
    const int use = n < maxN ? n : maxN;
    for (int i = 0; i < use; ++i) {
        snprintf(names[i], 33, "%s", FAKE[i]);
        rssi[i] = RSSI[i];
        isOpen[i] = (i == 4);
    }
    return use;
}
void host_wifi_connect(const char *, const char *) {}
// No NVS and no radio here, so there is nothing to protect and nothing to commit. The
// device version is where the work is: see host_wifi_connect() in main.cpp.
void host_wifi_commit_credentials(const char *, const char *) {}
void host_wifi_restore_saved() {}
void host_wifi_forget_backup() {}
void host_wifi_saved_ssid(char *out, size_t n) { snprintf(out, n, "%s", ""); }
int  host_wifi_connect_status() { return 0; }
void host_wifi_connected_reboot() {}
void host_factory_reset() {}
bool host_wx_is_imperial() { return false; }
int  host_wx_units_mode() { return 0; }
void host_wx_units_set(int) {}

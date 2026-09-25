#pragma once
// settings_view.cpp's shared internals: the modes and constants, the state every page reads and writes, and the
// prototypes between the files it was split into (settings_view.cpp, settings_pages.cpp, settings_location.cpp,
// settings_wifi.cpp). Private to src/app/settings.
//
// The state used to sit in an anonymous namespace, which cannot be shared between files, so it is in
// namespace settings_impl now; the using-directive at the bottom lets each file use it unqualified.
#include "settings_view.h"
#include <stdio.h>   // snprintf: glibc/libstdc++ do not pull it in for us
#include "app_shell.h"
#include "app_theme.h"      // app_theme::palette().bg — the built-in look's navy, for C_BG in init()
#include "theme_select.h"   // which theme (of however many are installed on the SD card) is active
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>   // strdup, free: the wheel rows keep their full text on the label (fit_label)
#include "config.h"     // SCREEN_W / SCREEN_H
#include "splash_art.h" // splash_art_decode() — the boot splash, reused for the About page
#include "splash_lines.h" // the three standing lines and the glass over them
#include "diag_log.h"
#include "wheel.h"              // the one wheel canvas and its drawing
#include "wheel_layout.h"       // row geometry, for the plain-label fallback
#include "wheel_look.h"         // what a theme (or the setup path) makes of it
#include "plate_sprite.h"       // the settings plate and glass
#include "theme_style.h"
#include "theme_font.h"   // per-theme fonts, with the compiled font as fallback

// Shared with main.cpp.
extern int  host_get_brightness();
extern void host_set_brightness(int v, bool save);
extern uint32_t host_get_idle_ms();
extern void     host_set_idle_ms(uint32_t ms);
extern void host_set_location(double lat, double lon);              // saves + reboots
extern void host_set_location_named(const char *name, double lat, double lon);  // + records in recents
extern bool host_locate_current();                                 // IP-locate + set + reboot; false = failed, didn't reboot
extern int  host_geocode(const char *query, char names[][40], double *lats, double *lons, int maxN);
extern int  host_recents_get(char names[][40], double *lats, double *lons, int maxN);
extern void host_recents_add(const char *name, double lat, double lon);
extern int  host_get_volume();
extern void host_set_volume(int v, bool save);
extern bool host_sound_radar();
extern void host_sound_set_radar(bool on);
extern bool host_sound_chime();
extern void host_sound_set_chime(bool on);
extern void host_sound_preview_chime();
extern void host_sound_preview_beep();
extern int  host_chime_count();
extern const char *host_chime_name(int idx);
extern int  host_chime_index();
extern void host_chime_set(int idx);
extern void host_chime_preview(int idx);
extern void host_wifi_scan_start();
extern int  host_wifi_scan_result(char names[][33], int8_t *rssi, bool *isOpen, int maxN);
extern void host_wifi_connect(const char *ssid, const char *pass);
extern void host_wifi_commit_credentials(const char *ssid, const char *pass);  // only once associated
extern void host_wifi_restore_saved();   // put the previous network back after a failed attempt
extern int  host_wifi_connect_status();
extern void host_wifi_connected_reboot();
extern void host_factory_reset();          // wipes WiFi + all saved settings, reboots
extern bool host_wx_is_imperial();         // resolved (mode + Auto-detected location) weather units
extern int  host_wx_units_mode();          // 0=Auto 1=Metric 2=Imperial (raw saved mode)
extern void host_wx_units_set(int mode);
// Flight Tracker display range. Lives here because touch was removed: this used to be
// the on-screen zoom button in ui.cpp, which was the only way to change range and died
// with the touchscreen. host_set_range_km() persists to NVS and re-renders, exactly as
// that button's callback did.
extern float host_get_range_km();
extern void  host_set_range_km(float km);


namespace settings_impl {
    // MODE_LOCATION is a 4-item menu (current / search / recent / back); MODE_RECENT is
    // the scrollable list of recent cities you reach from that menu.
    enum Mode { MODE_MENU, MODE_DISPLAY, MODE_BRIGHT, MODE_LOCATION, MODE_RECENT, MODE_SEARCH, MODE_SOUND, MODE_VOLUME, MODE_ABOUT,
                MODE_WIFI_LIST, MODE_WIFI_PASSWORD, MODE_WIFI_STATUS, MODE_RESET_CONFIRM, MODE_UNITS, MODE_CHIME_SELECT,
                MODE_DESIGN_SELECT, MODE_DESIGN_NOTICE, MODE_RANGE,
                MODE_FIRSTBOOT, MODE_FIRSTBOOT_PHONE, MODE_NO_SDCARD };

    // --- main settings menu ---
    enum { ITEM_DISPLAY = 0, ITEM_LOCATION, ITEM_SOUND, ITEM_UNITS, ITEM_RANGE, ITEM_WIFI, ITEM_DESIGN, ITEM_ABOUT, ITEM_RESET, ITEM_BACK, ITEM_COUNT };
    const char *const ITEM_LABELS[ITEM_COUNT] = { "Display", "Location", "Sound", "Units", "Range", "WiFi", "Theme", "About", "Reset", "Back" };

    // --- units submenu (Weather app metric/imperial, Auto by default) ---
    enum { UNIT_MODE = 0, UNIT_BACK, UNIT_COUNT };

    // --- range submenu (Flight Tracker display range; steps come from config.h) ---
    enum { RNG_VALUE = 0, RNG_BACK, RNG_COUNT };
    const int RANGE_N = (int)(sizeof(RANGE_STEPS_KM) / sizeof(RANGE_STEPS_KM[0]));

    // --- display submenu (screen timeout + brightness) ---
    // The Display page has Screen and Brightness only. Themes are chosen in the top-level
    // Theme item (MODE_DESIGN_SELECT internally).
    enum { DSP_SCREEN = 0, DSP_BRIGHT, DSP_BACK, DSP_COUNT };
    const uint32_t IDLE_MS[] = { 0, 28800000UL, 14400000UL, 7200000UL, 3600000UL, 1800000UL, 600000UL, 120000UL };
    const char *const IDLE_LABELS[] = { "Always on", "8 hours", "4 hours", "2 hours", "1 hour", "30 min", "10 min", "2 min" };
    const int IDLE_N = (int)(sizeof(IDLE_MS) / sizeof(IDLE_MS[0]));

    constexpr int WIFI_MAX = 12;   // most-scanned networks shown, strongest signal wins on duplicates

    // The wheel (src/platform/wheel) draws every list in this file: the selected row at the panel centre, the rest
    // falling away along the dial. Its shape is fixed and its colours and faces come from look() below, so no
    // page carries a pair of colours of its own.
    // The stock chrome on the wheel pages: the small page title at the top ("Display",
    // "Theme") and the one-line knob hint at the foot ("turn to browse, push to select").
    // Compiled grey Montserrat, so on a themed Orb they are the one thing on the page the
    // design did not dress, they land on whatever the plate has painted there, and
    // the settings preview shows neither. The owner, on a Steam Punk Orb: "why is it giving
    // me that notification now?" They stay on the setup path, where a stranger meets the
    // knob for the first time, and go with the theme everywhere else; the main wheel never
    // had either. Registered as they are built, shown or hidden in show_page().
    extern lv_obj_t *s_hints[24];
    extern int       s_hintN;

    // --- sound submenu ---
    enum { SND_RADAR = 0, SND_CHIME, SND_CHIME_SEL, SND_VOLUME, SND_BACK, SND_COUNT };

    constexpr int VOL_STEP = 10;

    // --- chime picker (Sound > Chime sound) ---
    // Only "Westminster" exists today, but the list is sized for future named chimes
    // (see audio_chime_count() / chime_westminster.h) without any UI changes needed.
    // Room for every chime the device can offer: the flash library plus one per installed
    // theme, which is theme_select::MAX_THEMES. It was 8, sized when the only source was
    // flash and only Westminster existed; the moment themes started carrying chimes, a card
    // with nine of them would have written "Back" past the end of s_chimeSelItems.
    constexpr int CHIME_UI_MAX = theme_select::MAX_THEMES + 4;

    // --- location submenu ---
    enum { LM_CURRENT = 0, LM_SEARCH, LM_RECENT, LM_BACK, LM_COUNT };
    const char *const LM_LABELS[LM_COUNT] = { "Current location", "Search city", "Recent cities", "Back" };

    // Seed the recents list once (first boot) so "Recent cities" isn't empty.
    struct City { const char *name; double lat; double lon; };
    const City SEED_CITIES[] = {
        {"Phoenix, AZ",    33.4484, -112.0740},
        {"Ithaca, NY",     42.4406,  -76.4966},
        {"Pueblo, CO",     38.2544, -104.6091},
        {"Denver, CO",     39.7400, -104.9900},
        {"Dallas, TX",     32.7831,  -96.8067},
        {"Chicago, IL",    41.8500,  -87.6500},
        {"New York, NY",   40.7100,  -74.0100},
        {"Los Angeles",    34.0522, -118.2437},
    };
    const int SEED_COUNT = (int)(sizeof(SEED_CITIES) / sizeof(SEED_CITIES[0]));

    constexpr int RECENTS_MAX = 8;

    // Search keyboard: 26 letters + '<' (backspace) + '_' (space)
    const char KEYS[]   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ<_";
    const int  N_KEYS   = 28;

    constexpr int BRI_MIN = 8, BRI_MAX = 255, BRI_STEP = 13;

    extern Mode s_mode;
    extern int  s_sel;
    extern int  s_bri;
    extern int  s_lmSel;
    extern int  s_sndSel;
    extern int  s_chimeSel;
    extern int  s_unitsSel;
    extern int  s_rangeSel;
    extern int  s_dspSel;
    extern int  s_designSel;
    extern int  s_vol;

    // Installed themes (/themes/<slug>/ on the SD card), rescanned each
    // time the Design picker is entered — see refresh_designSelect().
    extern char s_designSlugs[theme_select::MAX_THEMES][theme_select::MAX_SLUG_LEN];
    extern int  s_designCount;

    // recent cities
    extern char   s_recNames[RECENTS_MAX][40];
    extern double s_recLat[RECENTS_MAX], s_recLon[RECENTS_MAX];
    extern int    s_recCount;
    extern int    s_recSel;

    // WiFi setup (fully encoder-driven, styled like the rest of Settings)
    extern char    s_wifiNames[WIFI_MAX][33];
    extern int8_t  s_wifiRssi[WIFI_MAX];
    extern bool    s_wifiOpen[WIFI_MAX];
    extern int     s_wifiCount;
    extern int     s_wifiSel;
    extern bool    s_wifiScanning;
    extern char    s_wifiSelSsid[33];
    extern bool    s_wifiSelOpen;
    extern bool     s_wifiConnecting;
    extern uint32_t s_connectStartMs;
    // How long to let an attempt run before calling it failed. Association plus DHCP is a
    // few seconds on a healthy network; twenty is generous and, crucially, finite.
    static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

    // password entry (same character-strip picker as the city search)
    extern char    s_pass[65];
    extern int     s_wkbIdx;
    // printable password charset + two trailing virtual keys: DEL and OK(connect)
    const char WKEYS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()-_=+.,:;?/";
    const int  N_WKEYS = (int)(sizeof(WKEYS) - 1);
    const int  WK_DEL  = N_WKEYS;          // strip index for backspace
    const int  WK_OK   = N_WKEYS + 1;      // strip index for connect
    // A VISIBLE way out, and the reason it exists is worth keeping.
    //
    // This screen first got an escape as a gesture: backspace past the start of an empty
    // field, matching the city search. It worked, and it did not help. The owner walked the
    // screen the next morning, emptied the field, found nothing that looked like an exit,
    // pressed OK because OK was the only exit that could be seen, and ended up power cycling —
    // which is the outcome CUT-05 exists to prevent. The gesture was not just
    // undiscoverable, it was counterintuitive: it asks you to press backspace on an
    // already-empty field, which nobody has a reason to do.
    //
    // UX-004 says nothing in normal use requires reading documentation, and a way out you
    // have to be told about is documentation. So there is now an item you can scroll to.
    const int  WK_BACK = N_WKEYS + 2;      // strip index for "give up and go back"
    const int  WK_TOTAL = N_WKEYS + 3;

    // Four, not five. At 26 px five rows spanned 280 px and pushed the title off the top
    // while the hint collided with the last row — caught by rendering it, not by reasoning
    // about it. Larger type means fewer rows, which is the same trade the character strip
    // makes: this is a scrolling list, so what is visible at once was never the whole set.
    constexpr int WIFI_VISIBLE = 4;        // rows shown at once in the scrolling network list
    constexpr int WIFI_ROW_DY  = 56;   // was 44; 26 px rows need the room

    // search state
    extern char   s_str[28];
    extern int    s_kbIdx;
    extern char   s_sugName[4][40];
    extern double s_sugLat[4], s_sugLon[4];
    extern int    s_sugCount;
    extern bool   s_pending;
    extern int    s_countdown;
    extern bool   s_searching;

    extern lv_obj_t *s_screen;
    extern lv_obj_t *s_menu;
    extern lv_obj_t *s_items[ITEM_COUNT];
    extern lv_obj_t *s_bright;
    extern lv_obj_t *s_barFill;
    extern lv_obj_t *s_pct;
    extern lv_obj_t *s_lmPage;
    extern lv_obj_t *s_lmItems[LM_COUNT];
    extern lv_obj_t *s_recPage;
    extern lv_obj_t *s_recName;
    extern lv_obj_t *s_recCoord;
    extern lv_obj_t *s_srchPage;
    extern lv_obj_t *s_srchText;
    extern lv_obj_t *s_strip[7];
    extern lv_obj_t *s_sug[4];
    extern lv_obj_t *s_dspPage;
    extern lv_obj_t *s_dspItems[DSP_COUNT];
    extern lv_obj_t *s_sndPage;
    extern lv_obj_t *s_sndItems[SND_COUNT];
    extern lv_obj_t *s_unitsPage;
    extern lv_obj_t *s_unitsItems[UNIT_COUNT];
    extern lv_obj_t *s_rangePage;
    extern lv_obj_t *s_rangeItems[RNG_COUNT];
    extern lv_obj_t *s_chimeSelPage;
    extern lv_obj_t *s_chimeSelItems[CHIME_UI_MAX + 1];
    extern lv_obj_t *s_designPage;
    extern lv_obj_t *s_designItems[theme_select::MAX_THEMES + 1];
    extern lv_obj_t *s_designNoticePage;
    extern lv_obj_t *s_volPage;
    extern lv_obj_t *s_volFill;
    extern lv_obj_t *s_volPct;
    extern lv_obj_t *s_plateImg;
    extern lv_obj_t *s_ovImg;
    extern lv_obj_t *s_aboutPage;
    // The version, address and credit lines used to be three labels built here at hardcoded
    // offsets. They are theme data now (splash_style.json) and are drawn by splash_lines,
    // which the boot splash also uses, so the two places that show this picture cannot
    // drift apart again.
    extern char      s_netInfo[112];
    extern char      s_homeCoords[48];
    extern lv_obj_t *s_lmCoords;
    extern lv_obj_t *s_aboutImg;
    // --- first-boot WiFi choice (UX-019 as amended 2026-08-30, UX-022) ---
    //
    // The one screen on the device that offers a choice the owner did not arrive wanting to
    // make. UX-005 forbids that everywhere else and the amendment carves out this screen
    // only: the first minute with a new object is the one moment where somebody who cannot
    // find their way has no fallback and no reason to persist. The on-device path is
    // pre-selected, so the default is still a single press.
    enum { FB_ONDEVICE = 0, FB_PHONE, FB_COUNT };
    const char *const FB_LABELS[FB_COUNT] = { "Choose a network here", "Use my phone instead" };
    extern lv_obj_t *s_fbPage;
    extern lv_obj_t *s_fbItems[FB_COUNT];
    // Fixed rows. Type is large across this whole path because of UX-058: large text mode
    // is the accessibility answer, and it lives in Settings — which needs a working device,
    // which needs this path. So these screens are the one place that cannot lean on the
    // accessibility feature and must be legible to everybody by default. Same circular
    // dependency as the theme, one layer up.
    const int FB_ROW1_Y = -40;
    const int FB_ROW2_Y =  48;
    extern int       s_fbSel;
    // Set when boot wanted the WiFi choice but the card notice had to come first.
    extern bool      s_pendingWifiSetup;
    extern lv_obj_t *s_fbPhonePage;

    // --- no readable SD card (UX-024) ---
    //
    // Dismissible with a press, deliberately. UX-024 asks the Orb to SAY it needs a card,
    // in words rather than an error code; it does not ask it to refuse to run, and UX-041
    // says a screen the owner cannot turn away from is its own fault. The device does in
    // fact still work without a card — it falls back to the flash-baked artwork — so
    // trapping somebody on this notice would break a working Orb to report a degraded one.
    // Said once, unmissably, at the only moment it is actionable.
    extern lv_obj_t *s_noSdPage;

    // Is the page currently on screen part of the path from "no network" to "network"?
    // Those screens are SYSTEM CHROME: fixed white on black, the compiled font, no theme
    // lookups at all. Everything else in Settings stays themed as before.
    //
    // Three reasons, and the third is the one that decides it.
    //
    // A theme can wreck the layout, which is the obvious one. Themes live on the SD card,
    // and this path is reachable when there is no readable card at all — the no-card notice
    // most of all — so styling it from theme data is a recovery screen depending on a file
    // whose absence is the thing being recovered from. And there is no way out: a theme that
    // renders these screens illegibly can only be changed from Settings, which needs a
    // working device, which needs setup. A bad theme would strand a stranger with no path
    // back and nothing on screen to explain why.
    //
    // The network list and the character strip are reachable from Settings on a working
    // device too, so they are unthemed there as well. That is deliberate rather than a side
    // effect: they are recovery surfaces and it is honest for them to look like it.
    //
    // The splash is NOT in this set even though it precedes setup. It is decorative and
    // transient, and a splash that renders badly still lets you reach everything below.
    extern bool s_systemChrome;

    extern lv_obj_t *s_resetPage;

    // WiFi setup pages (encoder-driven, same look as the rest of Settings)
    extern lv_obj_t *s_wifiListPage;
    extern lv_obj_t *s_wifiRows[WIFI_VISIBLE];
    extern lv_obj_t *s_wifiListHint;
    extern bool      s_firstBootPrompt;
    extern lv_obj_t *s_wifiPassPage;
    extern lv_obj_t *s_wifiPassTitle;
    extern lv_obj_t *s_passText;
    extern lv_obj_t *s_wkStrip[7];
    extern lv_obj_t *s_wifiPassHint;
    extern lv_obj_t *s_wifiStatusPage;
    extern lv_obj_t *s_wifiStatusLbl;
    extern lv_obj_t *s_wifiStatusHint;

    // The Settings pages' static text colours (titles, hints, values). A SELECTED row is never coloured from
    // these: its colour and face come from look().
    extern lv_color_t C_WHITE;
    extern lv_color_t C_GREY;
    extern lv_color_t C_DIM;
    extern lv_color_t C_ACCENT;
    extern lv_color_t C_BG;
    extern lv_color_t C_TRACK;

    // The most rows any wheel list has. Every label array feeding show_wheel() must fit, and the
    // static_assert is what fails the build when a list outgrows it.
    constexpr int MAX_WHEEL_ROWS = 32;
    static_assert(ITEM_COUNT <= MAX_WHEEL_ROWS && theme_select::MAX_THEMES + 1 <= MAX_WHEEL_ROWS
                  && CHIME_UI_MAX + 1 <= MAX_WHEEL_ROWS, "raise MAX_WHEEL_ROWS: a wheel list would be cut short");

    extern void refresh_wifi_list();
    extern void refresh_wifi_pass();

    void reg_hint(lv_obj_t *h);
    bool mode_is_system_chrome(Mode m);
    bool mode_paints_its_own_screen(Mode m);
    wheel::Look look();
    bool is_cut_of(const char *shown, const char *full);
    void fit_label(lv_obj_t *lbl, const lv_font_t *font, float maxW);
    void show_wheel(lv_obj_t **items, int count, int sel);
    void refresh_menu();
    int idle_index();
    void refresh_display();
    void refresh_bright();
    void refresh_sound();
    int chime_shown();
    int chime_item_count();
    void refresh_chimeSelect();
    int design_item_count();
    void refresh_designSelect();
    void refresh_units();
    void refresh_range();
    void refresh_vol();
    void refresh_locmenu();
    void refresh_firstboot();
    void refresh_recent();
    void load_recents();
    void refresh_search();
    void refresh_about();
    void show_page(Mode m);
    void mark_dirty();
    void search_tick(lv_timer_t * /*t*/);
    int wifi_item_count();
    void wifi_item_name(int idx, char *out, size_t n);
    void refresh_wifi_list();
    void refresh_wifi_pass();
    void start_wifi_scan();
    void wifi_begin_connect(const char *pass);
    void wifi_pick_network(int idx);
    void wifi_tick(lv_timer_t * /*t*/);
    void build_menu_and_brightness_pages();   // settings_pages.cpp
    void build_location_menu();               // settings_location.cpp
    void build_setup_pages();                 // settings_wifi.cpp: first-boot choice, phone path, no-SD notice
    void build_recent_and_search();           // settings_location.cpp
    void build_option_pages();                // settings_pages.cpp: display, sound, chime, theme, range, units, volume, About, reset
    void build_wifi_pages();                  // settings_wifi.cpp: network list, password strip, connect status

    inline void reg_hint(lv_obj_t *h) { if (h && s_hintN < 24) s_hints[s_hintN++] = h; }

    // What this page is drawn with. Setup pages take the stock look and every other page the theme's, so one
    // function decides for the wheel lists, the two-row first-boot page, the network list and the password strip
    // alike. A page that marks its selected row calls this and nothing else.
    inline wheel::Look look() { return s_systemChrome ? wheel_look::system() : wheel_look::themed(); }

    inline void refresh_menu() { show_wheel(s_items, ITEM_COUNT, s_sel); }
    inline int chime_item_count() { return chime_shown() + 1; }   // chimes + Back

    inline int design_item_count() { return s_designCount + 1; }   // installed themes + Back

    inline void mark_dirty() { s_pending = true; s_countdown = 3; refresh_search(); }   // ~600ms debounce

    // ---- WiFi setup (encoder-driven: scrolling list -> character-strip password) ----

    inline int wifi_item_count() { return s_wifiCount + 2; }   // networks + "Rescan" + "Back"
}  // namespace settings_impl

using namespace settings_impl;

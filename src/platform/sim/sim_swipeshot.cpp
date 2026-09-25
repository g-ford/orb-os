// The --swipeshot plan: a scripted run of swipes and the assertions about where each one lands. sim_main.cpp
// steps through it one entry per interval and prints whether every check held.
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

// --swipeshot <prefix>: what a recognised swipe DOES to a real roster, driven straight at
// input_router::onSwipe() the way --rockshot drives the knob. The detector has its own host test;
// this checks which apps a swipe visits, which screens it refuses to land on, and what stops it.
// One action per tick (450 ms apart) so a slide has finished before the next swipe lands, except
// where a step means to land inside one.
namespace {
std::vector<SimStep> g_swPlan;
bool g_swFailed = false;
static void sw_check(const char *what, bool ok) {
    printf("[swipeshot] %-58s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) g_swFailed = true;
}
static void sw_swipe(swipe::Dir d, int wantApp, const char *what) {
    g_swPlan.push_back([=]() { input_router::onSwipe(d); sw_check(what, app_shell::index() == wantApp); });
}
static void sw_page(swipe::Dir d, int wantScreen, const char *what) {
    g_swPlan.push_back([=]() { input_router::onSwipe(d); sw_check(what, ui_weather_screen() == wantScreen); });
}
static void sw_build_plan(const std::string &prefix) {
    using swipe::Dir;
    g_swPlan.push_back([]() {
        if (app_shell::browsing()) app_shell::browsePress();
        app_shell::selectApp(app_shell::APP_CLOCK);
    });
    // The first swipe is also photographed 100 ms into its slide, to see what the outgoing screen
    // looks like once its onExit has run.
    g_swPlan.push_back([prefix]() {
        input_router::onSwipe(Dir::Left);
#if SWIPE_SLIDE
        lv_tick_inc(100); lv_timer_handler(); lv_refr_now(NULL);
        sim_save_frame((prefix + "-mid-slide.bmp").c_str());
#endif
        sw_check("left from Clock visits Flight Tracker", app_shell::index() == app_shell::APP_FLIGHT);
    });
#if APPS_WEATHER
    sw_swipe(Dir::Left,  app_shell::APP_WEATHER, "left again visits Weather");
#endif
    sw_swipe(Dir::Left,  app_shell::APP_INTEL,   "left again visits Intel");
    sw_swipe(Dir::Left,  app_shell::APP_CLOCK,   "left from Intel wraps to Clock, skipping Settings");
    sw_swipe(Dir::Right, app_shell::APP_INTEL,   "right from Clock wraps to Intel, skipping Settings");
    g_swPlan.push_back([prefix]() {      // a settled frame after two animated loads: is the art all there?
        lv_timer_handler(); lv_refr_now(NULL);
        sim_save_frame((prefix + "-after-ring.bmp").c_str());
    });

    // Things that must stop a swipe.
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_SETTINGS); });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        input_router::onSwipe(Dir::Right);
        sw_check("swipes are dropped inside Settings (it holds the knob)",
                 app_shell::index() == app_shell::APP_SETTINGS);
        app_shell::selectApp(app_shell::APP_CLOCK);
        app_shell::openSwitcher();
    });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        sw_check("swipes are dropped while the app switcher is up",
                 app_shell::browsing() && app_shell::index() == app_shell::APP_CLOCK);
        app_shell::browsePress();        // commit back into the clock
        knob_help::show();
    });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);
        sw_check("swipes are dropped while the knob-help panel is up",
                 knob_help::showing() && app_shell::index() == app_shell::APP_CLOCK);
        knob_help::dismiss();
    });
#if SWIPE_SLIDE
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Left);    // Clock -> Flight: a real slide between two screens
        input_router::onSwipe(Dir::Left);    // lands inside it
        sw_check("a second swipe inside a slide is dropped, not queued",
                 app_shell::index() == app_shell::APP_FLIGHT);
    });
#endif
#if APPS_WEATHER
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_WEATHER); });
    sw_page(Dir::Down, WX_SCREEN_NOW,   "down at Now stops: touch has ends");
    g_swPlan.push_back([prefix]() {
        input_router::onSwipe(Dir::Up);
        lv_tick_inc(60); lv_timer_handler(); lv_refr_now(NULL);       // 60 ms into the fade
        sim_save_frame((prefix + "-mid-fade.bmp").c_str());
        sw_check("up steps Now to Radar", ui_weather_screen() == WX_SCREEN_RADAR);
    });
    sw_page(Dir::Up,   WX_SCREEN_WEEK,  "up steps Radar to 7-Day");
    sw_page(Dir::Up,   WX_SCREEN_WEEK,  "up at 7-Day stops");
    sw_page(Dir::Down, WX_SCREEN_RADAR, "down steps 7-Day to Radar");
    sw_page(Dir::Down, WX_SCREEN_NOW,   "down steps Radar to Now");
    // Review finding: the fade is a transition too, so a second flick inside it is dropped like
    // one inside a slide, not stepped again (Now to Radar to 7-Day from one split gesture).
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Up);
        input_router::onSwipe(Dir::Up);
        sw_check("a second page swipe inside the fade is dropped, not queued",
                 ui_weather_screen() == WX_SCREEN_RADAR);
    });
    sw_page(Dir::Down, WX_SCREEN_NOW,   "and the next swipe, after the fade, steps back to Now");
#endif
    g_swPlan.push_back([]() { app_shell::selectApp(app_shell::APP_FLIGHT); });
    g_swPlan.push_back([]() {
        input_router::onSwipe(Dir::Up);
        input_router::onSwipe(Dir::Down);
        sw_check("up and down do nothing where no pager is registered",
                 app_shell::index() == app_shell::APP_FLIGHT);
    });
}

}  // namespace

const std::vector<SimStep> &sim_swipe_plan(const std::string &screenshotPrefix) {
    if (g_swPlan.empty()) sw_build_plan(screenshotPrefix);   // built on first use, then reused
    return g_swPlan;
}

bool sim_swipe_failed() { return g_swFailed; }

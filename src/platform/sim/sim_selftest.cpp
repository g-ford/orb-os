// The simulator's headless self-test: virtual knob -> input router -> app shell -> Settings, and the checks added
// since. Run with SIM_SELFTEST=1 (see tests/run_sim_selftest.sh); prints [selftest] lines and exits.
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
#include <ArduinoJson.h>
#include <string>
#include "sim_internal.h"

int sim_selftest() {
    auto pump = [&]() {
        int32_t kd = knob::takeDelta();
        bool pressed = knob::takePress();
        input_router::dispatch((int)kd, pressed);
        lv_timer_handler();
    };
    // Force a known starting state. Boot position is not fixed: CUSTOM_BOOT_TARGET
    // (set by whichever theme push ran last) can land the device in Settings >
    // About with the knob captured, which silently invalidates every assertion below.
    app_shell::setCaptured(false);
    if (app_shell::browsing()) { simknob::injectPress(true, SDL_GetTicks()); simknob::injectPress(false, SDL_GetTicks()); lv_timer_handler(); }
    app_shell::selectApp(app_shell::APP_CLOCK);
    lv_timer_handler();

    // What the THEME asks for, which is not the same as what this build carries — see
    // APPS_LAUNCH_ONE. Reported as the theme's opinion so the two cannot be confused.
    printf("[selftest] roster from theme '%s': clock=%d flight=%d weather=%d surv=%d "
           "(build carries %d apps)\n",
           theme_select::activeSlug(), theme_style::apps().clock, theme_style::apps().flight,
           theme_style::apps().weather, theme_style::apps().surveillance,
           app_shell::count());
    {   // Hand geometry now travels per theme too (clock_style.json "hands"), so a
        // theme switch no longer leaves the previous theme's hands on the new face.
        const theme_style::Clock &cs = theme_style::clock();
        printf("[selftest] hands: hour show=%d pivot=%d,%d center=%d,%d blend=%d | second show=%d | orderN=%d\n",
               cs.hand[0].show, cs.hand[0].pivotX, cs.hand[0].pivotY,
               cs.hand[0].centerX, cs.hand[0].centerY, cs.hand[0].blend,
               cs.hand[2].show, cs.orderN);
    }
    printf("[selftest] boot app: %s (idx %d)\n", app_shell::name(), app_shell::index());
    auto press = [&]() { simknob::injectPress(true, SDL_GetTicks()); simknob::injectPress(false, SDL_GetTicks()); pump(); };
    // A rock is a quick reversal and then a STOP: the second detent has to arrive inside
    // ROCK_QUICK_MS of the first, and the router only opens the menu once nothing more
    // has arrived for ROCK_SETTLE_MS (input_router.cpp). The delays are those two rules.
    auto rock  = [&]() {
        simknob::injectTurn(-1); pump();
        SDL_Delay(60); simknob::injectTurn(+1); pump();
        SDL_Delay(200); input_router::tick(); pump();
    };
    // Everything in this block happens in the space of a few milliseconds, which is not
    // how a knob is used: a leftward turn from one test phase would still be inside the
    // Rock window when the next phase turns right, and read as a gesture nobody made.
    // Waiting past the window between phases is what makes these assertions mean
    // anything about real use.
    auto settle = [&]() { SDL_Delay(420); pump(); };

    // THE ROCK. An ordinary turn belongs to the app on screen; only a quick left-then-
    // right opens the switcher. These two assertions are the whole contract, and they
    // used to say the opposite: a single turn opened the menu, which is what made the
    // knob unusable for anything else.
    settle();
    simknob::injectTurn(+1); pump();
    const bool plainTurnStayed = !app_shell::browsing();
    printf("[selftest] plain turn: browsing=%d (expect 0 = stays in the app)\n", app_shell::browsing());

    settle();
    rock();
    const bool rockOpened = app_shell::browsing();
    printf("[selftest] rock: browsing=%d (expect 1 = switcher opened)\n", app_shell::browsing());
    printf("[selftest] rock opens the menu: %s\n", (plainTurnStayed && rockOpened) ? "PASS" : "FAIL");

    // Right-then-left must NOT open it. Requiring one order is what keeps ordinary
    // direction changes from being read as the gesture.
    if (app_shell::browsing()) press();          // commit out of the switcher first
    // A scroll that changes direction is NOT a rock: down three, up one, however quick.
    settle();
    simknob::injectTurn(+3); pump();
    SDL_Delay(60); simknob::injectTurn(-1); pump();
    SDL_Delay(200); input_router::tick(); pump();
    printf("[selftest] scroll reversal: browsing=%d (expect 0 = a scroll, not a rock)\n", app_shell::browsing());
    printf("[selftest] a scroll reversal is not a rock: %s\n", !app_shell::browsing() ? "PASS" : "FAIL");
    // And an unhurried reversal is not one either: down one, a moment, up one.
    settle();
    simknob::injectTurn(-1); pump();
    SDL_Delay(500); simknob::injectTurn(+1); pump();
    SDL_Delay(200); input_router::tick(); pump();
    printf("[selftest] slow reversal: browsing=%d (expect 0 = too slow to be a rock)\n", app_shell::browsing());
    printf("[selftest] a slow reversal is not a rock: %s\n", !app_shell::browsing() ? "PASS" : "FAIL");

    // Browsing: turns cycle apps, a press commits.
    settle();
    rock(); pump();
    const int browseStart = app_shell::index();
    simknob::injectTurn(+1); pump();
    simknob::injectTurn(+1); pump();
    printf("[selftest] browsing turns: %s (idx %d, browsing=%d)\n", app_shell::name(), app_shell::index(), app_shell::browsing());
    press();
    printf("[selftest] press -> committed to %s (idx %d, browsing=%d)\n", app_shell::name(), app_shell::index(), app_shell::browsing());
    printf("[selftest] switcher cycles and commits: %s\n",
           (!app_shell::browsing() && app_shell::index() != browseStart) ? "PASS" : "FAIL");

    // The Flight Tracker takes a plain turn now. Nothing is captured any more: the knob
    // is never taken from the shell, because the Rock is what leaves rather than a press.
    settle();
    app_shell::selectApp(app_shell::APP_FLIGHT); pump();
    printf("[selftest] FT enter: app=%s captured=%d (expect 0)\n", app_shell::name(), app_shell::captured());
    settle();
    simknob::injectTurn(+1); pump();
    printf("[selftest] FT turn: browsing=%d captured=%d (expect 0, 0 = selecting, not browsing)\n",
           app_shell::browsing(), app_shell::captured());
    printf("[selftest] FT turn selects rather than browsing: %s\n",
           (!app_shell::browsing() && !app_shell::captured()) ? "PASS" : "FAIL");

    // Settings > Range, added when touch removal killed the on-screen zoom button.
    // Navigation is made deterministic by the main menu's clamping: turning down
    // past the end parks on the last item (Back), so counting up from there hits a
    // known item regardless of whichever theme's "default selection" we started on.
    // Menu order: Display Location Sound Units Range WiFi Design About Reset Back.
    // Close the switcher overlay first. input_router checks browsing() BEFORE
    // captured(), so leaving the overlay up sends every turn to the app switcher
    // and Settings never sees it. The previous step deliberately left it open.
    if (app_shell::browsing()) press();
    settle();
    app_shell::selectApp(app_shell::APP_SETTINGS); pump();          // Settings; onEnter resets to the menu
    settingsview::onEnter(); pump();
    printf("[selftest] Settings enter: app=%s captured=%d browsing=%d (expect 1, 0)\n",
           app_shell::name(), app_shell::captured(), app_shell::browsing());
    for (int i = 0; i < 15; ++i) { simknob::injectTurn(+1); pump(); }   // clamp on Back
    for (int i = 0; i < 5;  ++i) { simknob::injectTurn(-1); pump(); }   // Back -> Range
    const float before = host_get_range_km();
    press();                                   // open the Range page
    press();                                   // push the value item: cycle one step
    const float after = host_get_range_km();
    printf("[selftest] Settings>Range: %.0f km -> %.0f km (expect a change, steps 10/20/30/50/100)\n",
           (double)before, (double)after);
    printf("[selftest] Settings>Range: %s\n", (before != after) ? "PASS" : "FAIL (range did not move)");

    // Settings > Theme: one push opens the picker and ONLY opens it. The owner, 2026-09-16,
    // on a freshly synced Orb: "went to theme, there was nothing, it immediately said
    // restarting with the new theme". Two things are asserted: a single press from the
    // menu does not restart anything, and every row of the picker shows a name.
    {
        static int s_restarts = 0;
        theme_select::setRestartHook([]() { ++s_restarts; });
        app_shell::setCaptured(false);
        if (app_shell::browsing()) press();
        settle();
        app_shell::selectApp(app_shell::APP_SETTINGS); pump();
        settingsview::onEnter(); pump();
        for (int i = 0; i < 15; ++i) { simknob::injectTurn(-1); pump(); }   // clamp on Display
        for (int i = 0; i < 6;  ++i) { simknob::injectTurn(+1); pump(); }   // Display -> Theme
        press();                                                              // open the picker
        settle();
        int rows = 0, blank = 0;
        for (int i = 0; i < 40; ++i) {
            const char *t = settingsview::designRowText(i);
            if (!t) break;
            ++rows; if (!t[0]) ++blank;
            if (i < 4) printf("[selftest] Settings>Theme row %d: \"%s\"\n", i, t);
        }
        printf("[selftest] Settings>Theme: rows=%d blank=%d restarts=%d (expect rows>=2, blank 0, restarts 0)\n", rows, blank, s_restarts);
        printf("[selftest] Settings>Theme: %s\n", (rows >= 2 && blank == 0 && s_restarts == 0) ? "PASS" : "FAIL");
        theme_select::setRestartHook(sim_restart);
        app_shell::setCaptured(false);
    }

    // The wheel has ONE canvas, and Settings holds it while it is the active app. The app picker opens over
    // the active app and takes the canvas; closing it back onto Settings must give Settings its canvas again
    // (the shell calls onEnter), or Settings would be left on plain labels for the rest of the visit.
    {
        app_shell::setCaptured(false);
        if (app_shell::browsing()) press();
        settle();
        app_shell::selectApp(app_shell::APP_SETTINGS); pump();
        settingsview::onEnter(); pump();
        const bool heldBefore = wheel::available();
        app_shell::browseTurn(+1); pump();                       // the first turn opens the picker ON Settings
        const bool openedOver = app_shell::browsing() && wheel::available();
        app_shell::browsePress(); pump();                        // commit: closes the picker, re-enters Settings
        const bool heldAfter = wheel::available() && !app_shell::browsing();
        printf("[selftest] picker over Settings: held before=%d over=%d after=%d (expect 1 1 1)\n",
               (int)heldBefore, (int)openedOver, (int)heldAfter);
        printf("[selftest] picker hands the wheel back: %s\n", (heldBefore && openedOver && heldAfter) ? "PASS" : "FAIL");
        app_shell::setCaptured(false);
    }

    // The clock's hand shadows must load again after the clock releases its sprites, which it does every time
    // it leaves the screen. custom_sprite_release() used to reset the "already tried" flag for five of its eight
    // slots and free all eight, so the three shadows were gone until a reboot. Only meaningful on a theme that
    // ships shadows (Elegant does); the built-in look ships none and reports n/a.
    {
        const bool had = custom_shadow(0).data != nullptr;
        custom_sprite_release();
        const bool back = custom_shadow(0).data != nullptr;
        if (!had) printf("[selftest] clock shadows reload after a release: n/a (this theme ships none)\n");
        else      printf("[selftest] clock shadows reload after a release: %s\n", back ? "PASS" : "FAIL");
    }

    // Headlines scroll mode (THEME_CAPS 11): same press-to-own-the-knob grammar as
    // the Flight Tracker's selection, but only when the theme's type size actually
    // overflows the dial. With everything fitting, a push stays a refresh and must
    // NOT capture — both behaviours are asserted, whichever this theme exhibits.
    app_shell::setCaptured(false);
    if (app_shell::browsing()) press();
    app_shell::selectApp(app_shell::APP_INTEL); pump();          // Intel (the news screen); onEnter resets to the top
    int iFirst, iVis, iCount;
    intelview::scrollState(iFirst, iVis, iCount);
    const bool iScrollable = iCount > iVis;
    printf("[selftest] Intel enter: items=%d visible=%d first=%d captured=%d (expect captured 0)\n",
           iCount, iVis, iFirst, app_shell::captured());
    press();
    const bool iCap1 = app_shell::captured();
    printf("[selftest] Intel push: captured=%d (expect %d = %s)\n",
           iCap1, iScrollable ? 1 : 0, iScrollable ? "scroll mode" : "refresh, nothing to scroll");
    simknob::injectTurn(+1); pump();
    intelview::scrollState(iFirst, iVis, iCount);
    printf("[selftest] Intel turn: first=%d (expect %d)\n", iFirst, iScrollable ? 1 : 0);
    const bool iTurnOk = iFirst == (iScrollable ? 1 : 0);
    press();
    const bool iCap2 = app_shell::captured();
    printf("[selftest] Intel push again: captured=%d (expect 0)\n", iCap2);
    const bool iOk = iScrollable ? (iCap1 && iTurnOk && !iCap2)
                                 : (!iCap1 && iTurnOk && !iCap2);
    printf("[selftest] Intel scroll: %s\n", iOk ? "PASS" : "FAIL");

    SDL_Quit();
    return 0;
}

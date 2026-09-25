#pragma once
#include "config.h"   // APPS_LAUNCH_ONE — which apps this build carries
#include <lvgl.h>
// The app shell: the "channel changer". Holds an ordered list of full-screen
// apps (each is one LVGL screen) and flips between them when the knob turns.
// Turn right -> next app, turn left -> previous app; the list wraps around.
// Per-app knob handlers. onPress = a push; onTurn = a detent while the app has
// "captured" the knob (used by menus that scroll with the knob instead of switching
// apps). An app that captures the knob keeps it until it calls next()/prev() itself.
// onExit fires right before the shell switches away to a different app — the
// counterpart to onEnter — so an app can drop memory it only needs while shown
// (e.g. a custom clock face's decoded PSRAM sprites) and reclaim it for whichever
// app is coming to the front.
typedef void (*app_action_t)();
typedef void (*app_turn_t)(int delta);
// A pager steps the screens INSIDE an app for touch: +1 is the later screen, -1 the earlier one.
// It returns whether it moved. Touch stops at the ends (a north/south layout has ends); the
// knob's own turn handler still wraps.
typedef bool (*app_pager_t)(int delta);

namespace app_shell {
    // The menu's running order, written down once.
    //
    // These used to be numbers typed at each call site, and the numbers then dictated the
    // menu: the Intel screen had to be registered AFTER Settings, leaving Settings stranded
    // mid-list, because moving it would have turned selectApp(4) into the wrong screen on a
    // factory-reset boot. The running order of a menu should be a design decision, not a
    // consequence of which integers somebody already wrote down.
    //
    // Registration in main.cpp and sim_main.cpp follows this order, and every jump names a
    // slot. To move an app in the menu, move it here and move its add() call to match.
    // Settings stays last: it is the drawer everything else is not.
    //
    // APP_TICKER was missing from here for six weeks, and this is the second time this list
    // has gone out of step with the registration order. The Stock Ticker was added as a
    // seventh app in 1.58.0 and registered between News and Settings, but nobody added it
    // to this enum — so APP_SETTINGS stayed 5, which by then was the Ticker. Every
    // selectApp(APP_SETTINGS) in the tree quietly went to the wrong screen, including the
    // one at main.cpp's boot that is supposed to open WiFi setup on a device with no
    // network. The result was an Orb that came up after a factory reset showing the
    // Ticker's background plate with no quotes on it, no WiFi prompt anywhere, and a knob
    // captured by a screen nobody could see. Its owner concluded his own product was
    // frozen. See CUT-03 / UX-022.
    //
    // The comment above already warned that moving an app "would have turned selectApp(4)
    // into the wrong screen on a factory-reset boot", and sim_main.cpp records the same
    // fault happening once before with APP_INTEL. Two warnings in prose, two occurrences.
    // So this is now checked at boot rather than trusted: see app_shell::verifySlots().
    // APPS_LAUNCH_ONE (config.h) takes Surveillance and the Ticker off the roster for
    // launch one, and APPS_WEATHER puts Weather on it. The slots go with them rather than
    // being left as holes:
    // a slot naming an app nobody registers is the fault verifySlots() exists to catch,
    // and leaving three of them deliberately would make the check cry wolf for ever.
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

    // `hidden` apps stay registered (so app indices and selectApp(n) never shift)
    // but are skipped when the knob cycles the menu — a theme flash uses
    // this to ship only the apps that theme includes, without renumbering the rest.
    void add(lv_obj_t *screen, const char *name,
             app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
             app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false);
    void add_active(const char *name,
                    app_action_t onPress = nullptr, app_turn_t onTurn = nullptr, bool capture = false,
                    app_action_t onEnter = nullptr, app_action_t onExit = nullptr, bool hidden = false);
    void begin();                                  // show the first app (no animation)

    // Check that the Slot enum above still describes the roster that actually registered,
    // and shout if it does not. Call once after the last add(), before anything jumps to a
    // slot. Returns true when the two agree.
    //
    // This exists because the enum has drifted from the registration order twice, both
    // times silently, and both times the prose comment that warned about it was already
    // there. A wrong slot number does not crash and does not log: it just quietly shows the
    // wrong screen, which is indistinguishable from a broken device to the person holding
    // it. `settingsScreen` is checked by POINTER rather than by name, because a theme may
    // relabel Settings to anything it likes and the label is therefore worthless as proof.
    bool verifySlots(lv_obj_t *settingsScreen);

    void next();          // advance to the next app (knob right), slides left
    void prev();          // go to the previous app (knob left), slides right
    // Touch. A swipe moves through the same apps the knob switcher does, except that it skips any
    // app registered with capture=true (Settings): touch never lands anywhere touch cannot leave.
    // dir > 0 is the next app, dir < 0 the previous one; the ring wraps. Returns whether it moved.
    // Whether a swipe is ALLOWED right now is input_router::onSwipe()'s decision, not this one's.
    bool swipeApp(int dir);
    // True while a swipe's slide is still running, so a second flick cannot land on top of it.
    bool transitioning();
    // Opt an app in to up/down swipes. `slot` is an app_shell::Slot. An app with no pager simply
    // ignores up and down.
    void setPager(int slot, app_pager_t fn);
    bool pageCurrent(int delta);   // false when the current app has no pager, or it did not move
    // Knob pushed: run the current app's press handler. Returns whether there WAS one,
    // so the caller can tell a press that did something from a press that vanished. The
    // clock registers none, which is the dead end knob_help exists to answer.
    bool pressCurrent();
    void selectApp(int idx);  // jump straight to an app by index, no slide (e.g. forced setup at boot)

    // App-switcher overlay: turning shows a big app-name label over a live preview;
    // the first turn opens the switcher on the current app, further turns cycle apps,
    // and a push commits (hides the overlay) into the shown app.
    void browseTurn(int delta);
    void browsePress();
    bool browsing();       // is the switcher overlay currently up?
    void openSwitcher();   // reopen the switcher on the current app (e.g. from a menu's Back)

    bool captured();               // does the current app own the knob (menu mode)?
    void setCaptured(bool on);     // an app grabs (true) / releases (false) the knob at runtime
    void turnCurrent(int delta);   // deliver a detent to the captured app

    int         count();
    int         index();
    const char *name();
    // By index, for anything enumerating the shell from outside it (the simulator's capture modes).
    // Returns "" for an index that does not exist, so a caller cannot walk off the end.
    const char *nameAt(int idx);
    bool        hiddenAt(int idx);
}

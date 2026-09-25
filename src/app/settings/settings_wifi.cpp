// The setup path and WiFi: the first-boot choice, the phone path, the no-SD-card notice, and the network list, password strip and connect status.
// Split out of settings_view.cpp; the shared modes, constants and state are in settings_internal.h.
#include "settings_internal.h"

namespace settings_impl {


// NOT the wheel, and that is the point rather than an omission.
//
// A wheel is the right shape for a list of unknown length — the network list is one —
// and the wrong shape for a fixed choice of two. On the wheel both rows moved and
// resized as the knob turned, which reads as "there is more below", and there is not.
// Both rows are pinned here and only the highlight travels.
void refresh_firstboot() {
    const wheel::Look lk = look();
    for (int i = 0; i < FB_COUNT; ++i) {
        const bool sel = (i == s_fbSel);
        lv_obj_align(s_fbItems[i], LV_ALIGN_CENTER, 0, i == 0 ? FB_ROW1_Y : FB_ROW2_Y);
        lv_obj_set_style_text_font(s_fbItems[i], sel ? lk.selFont : lk.itemFont, 0);
        lv_obj_set_style_text_color(s_fbItems[i], sel ? lk.selColor : lk.itemColor, 0);
        lv_obj_set_style_text_opa(s_fbItems[i], LV_OPA_COVER, 0);
    }
}


void wifi_item_name(int idx, char *out, size_t n) {
    if (idx < s_wifiCount)       snprintf(out, n, "%s", s_wifiNames[idx]);
    else if (idx == s_wifiCount) snprintf(out, n, "Rescan");
    else                         snprintf(out, n, "Back");
}


void refresh_wifi_list() {
    if (s_wifiScanning) {
        for (int r = 0; r < WIFI_VISIBLE; ++r) lv_label_set_text(s_wifiRows[r], "");
        lv_label_set_text(s_wifiRows[WIFI_VISIBLE / 2], "Scanning...");
        lv_obj_set_style_text_color(s_wifiRows[WIFI_VISIBLE / 2], C_DIM, 0);
        lv_label_set_text(s_wifiListHint, "");
        return;
    }
    const int total = wifi_item_count();
    int top = s_wifiSel - WIFI_VISIBLE / 2;
    if (top > total - WIFI_VISIBLE) top = total - WIFI_VISIBLE;
    if (top < 0) top = 0;
    for (int r = 0; r < WIFI_VISIBLE; ++r) {
        const int idx = top + r;
        if (idx < total) {
            char nm[40];
            wifi_item_name(idx, nm, sizeof(nm));
            const bool isSel = (idx == s_wifiSel);
            // A name too long for the dial is END-elided, because the beginning is the
            // part people recognise. Three full stops rather than an ellipsis: Montserrat
            // has no glyph at U+2026 and draws an empty box for it, which intel_view.cpp
            // already learned the hard way.
            //
            // The SELECTED row scrolls instead, so the one network you are about to pick
            // can always be read in full. Unselected rows never move, so a screen nobody
            // is touching is still.
            //
            // The face goes on BEFORE the text: dot mode cuts the text against the face the label has at the
            // moment it is set, so a face applied afterwards leaves a cut worked out for the wrong size.
            const wheel::Look lk = look();
            lv_obj_set_style_text_font(s_wifiRows[r], isSel ? lk.selFont : lk.itemFont, 0);
            lv_label_set_long_mode(s_wifiRows[r],
                isSel ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT);
            lv_label_set_text(s_wifiRows[r], nm);
            lv_obj_set_style_text_color(s_wifiRows[r], isSel ? lk.selColor : lk.itemColor, 0);
        } else {
            lv_label_set_text(s_wifiRows[r], "");
        }
    }
    lv_label_set_text(s_wifiListHint, s_firstBootPrompt ? "Start by connecting to your local Wi-Fi"
                                                         : "turn to choose, push to select");
}


void refresh_wifi_pass() {
    lv_label_set_text(s_passText, s_pass[0] ? s_pass : "(enter password)");
    lv_obj_set_style_text_font(s_passText, &lv_font_montserrat_26, 0);   // reading back what you typed matters
    for (int k = 0; k < 7; ++k) {
        const int idx = s_wkbIdx - 3 + k;
        const bool hot = (k == 3);
        char c[8] = { 0 };
        if (idx < 0 || idx >= WK_TOTAL) c[0] = 0;
        else if (idx == WK_DEL) snprintf(c, sizeof(c), "DEL");
        else if (idx == WK_OK)  snprintf(c, sizeof(c), "OK");
        else if (idx == WK_BACK) snprintf(c, sizeof(c), "Back");
        else if (WKEYS[idx] == ' ') snprintf(c, sizeof(c), "SP");   // show space as "SP"
        else { c[0] = WKEYS[idx]; c[1] = 0; }
        lv_label_set_text(s_wkStrip[k], c);
        lv_obj_set_style_text_color(s_wkStrip[k], hot ? look().selColor : look().itemColor, 0);
        // The strip already fades off both edges — it is a scroll, not a row, so it was
        // never showing the whole set and fewer visible at once costs nothing. Somebody
        // is picking one character at a time, so the one they are on is the only one
        // that has to be truly readable, and it was the same size as its neighbours.
        lv_obj_set_style_text_font(s_wkStrip[k], hot ? &lv_font_montserrat_44 : &lv_font_montserrat_22, 0);
    }
    // Position from MEASURED widths, not a fixed pitch.
    //
    // These sat on a 48 px pitch set once when the page was built, which is right for
    // single glyphs and wrong for DEL, OK and Back — they are words, so they are wider
    // than any character even at the small size, and the selected one at 44 px is wider
    // still. The three of them are adjacent at the end of the strip, so scrolling into
    // them put a large OK straight through DEL on one side and Back on the other.
    //
    // Measuring and walking outward from the selected cell handles any mix of widths,
    // so a longer key label later cannot bring this back.
    {
        constexpr int GAP = 18;      // clear space between neighbouring keys, px
        constexpr int Y   = 20;      // the strip's line, unchanged
        int w[7];
        for (int k = 0; k < 7; ++k) {
            lv_point_t sz;
            lv_txt_get_size(&sz, lv_label_get_text(s_wkStrip[k]),
                            (k == 3) ? &lv_font_montserrat_44 : &lv_font_montserrat_22,
                            0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            w[k] = sz.x;
        }
        lv_obj_align(s_wkStrip[3], LV_ALIGN_CENTER, 0, Y);
        int edge = w[3] / 2;                       // outward to the right
        for (int k = 4; k < 7; ++k) {
            const int cx = edge + GAP + w[k] / 2;
            lv_obj_align(s_wkStrip[k], LV_ALIGN_CENTER, cx, Y);
            edge = cx + w[k] / 2;
        }
        edge = -w[3] / 2;                          // and to the left
        for (int k = 2; k >= 0; --k) {
            const int cx = edge - GAP - w[k] / 2;
            lv_obj_align(s_wkStrip[k], LV_ALIGN_CENTER, cx, Y);
            edge = cx - w[k] / 2;
        }
    }
}


void start_wifi_scan() {
    s_wifiScanning = true;
    s_wifiCount = 0;
    s_wifiSel = 0;
    diag::log("wifi: scan start");
    host_wifi_scan_start();
    refresh_wifi_list();
}


void wifi_begin_connect(const char *pass) {
    s_wifiConnecting = true;
    s_connectStartMs = lv_tick_get();   // lv_tick_get, not millis: this file also builds for the desktop simulator
    // s_firstBootPrompt stays set through to the connect result — wifi_tick() uses it
    // to decide whether to auto-locate on success (see there).
    lv_label_set_text_fmt(s_wifiStatusLbl, "Connecting to\n%s...", s_wifiSelSsid);
    lv_label_set_text(s_wifiStatusHint, "");
    host_wifi_connect(s_wifiSelSsid, pass);
    show_page(MODE_WIFI_STATUS);
}


// Selecting a network from the list: connect straight away if open, else ask for a password.
void wifi_pick_network(int idx) {
    snprintf(s_wifiSelSsid, sizeof(s_wifiSelSsid), "%s", s_wifiNames[idx]);
    s_wifiSelOpen = s_wifiOpen[idx];
    if (s_wifiSelOpen) {
        wifi_begin_connect("");
    } else {
        s_pass[0] = 0; s_wkbIdx = 0;
        lv_label_set_text_fmt(s_wifiPassTitle, "Password: %s", s_wifiSelSsid);
        refresh_wifi_pass();
        show_page(MODE_WIFI_PASSWORD);
    }
}


void wifi_tick(lv_timer_t * /*t*/) {
    if (s_mode == MODE_WIFI_LIST && s_wifiScanning) {
        const int n = host_wifi_scan_result(s_wifiNames, s_wifiRssi, s_wifiOpen, WIFI_MAX);
        if (n == -1) return;                        // scan still running
        s_wifiScanning = false;
        s_wifiCount = (n < 0) ? 0 : n;
        s_wifiSel = 0;
        diag::log("wifi: scan done, %d networks", s_wifiCount);
        refresh_wifi_list();
    } else if (s_mode == MODE_WIFI_STATUS && s_wifiConnecting) {
        int st = host_wifi_connect_status();
        // A BOUNDED attempt, and without this the screen never came back.
        //
        // host_wifi_connect_status() only reports failure for WL_CONNECT_FAILED and
        // WL_NO_SSID_AVAIL. A wrong password on this chip usually settles at
        // WL_DISCONNECTED or WL_IDLE_STATUS, neither of which is in that list, so the
        // status stayed 0 for ever. And the press handler ignores input while
        // s_wifiConnecting is true, so the knob went dead and stayed dead: the exact
        // hang UX-015 and UX-041 forbid, reached by typing a password wrong.
        if (st == 0 && lv_tick_get() - s_connectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
            diag::log("wifi: connect to %s timed out after %lus", s_wifiSelSsid,
                      (unsigned long)(WIFI_CONNECT_TIMEOUT_MS / 1000));
            st = 2;   // treat as a plain failure, which already has a screen and a way back
        }
        if (st == 0) return;                        // still connecting
        s_wifiConnecting = false;
        // Failed or timed out: give the owner back the network they had. host_wifi_connect
        // stashed it before the attempt, precisely so a wrong password on somebody else's
        // SSID cannot cost them their own. Costs nothing when there was nothing stored.
        if (st != 1) host_wifi_restore_saved();
        if (st == 1) {
            // Associated, so the credentials are finally a fact and can be written. Up
            // to here WiFi.persistent(false) has kept the previously saved network
            // untouched, so every path that reaches the failure branch below leaves the
            // owner exactly where they started. See host_wifi_connect() in main.cpp.
            host_wifi_commit_credentials(s_wifiSelSsid, s_pass);
            lv_label_set_text(s_wifiStatusLbl, "Connected!");
            if (s_firstBootPrompt) {
                // First-time setup (fresh out of the box, or right after a Reset):
                // auto-detect location from the IP instead of making them go set it
                // manually. host_locate_current() reboots on success and never
                // returns; it only returns (false) if the lookup itself failed, in
                // which case fall back to a plain reboot so this doesn't just hang.
                lv_label_set_text(s_wifiStatusHint, "finding your location...");
                lv_refr_now(NULL);
                if (!host_locate_current()) {
                    lv_label_set_text(s_wifiStatusHint, "restarting...");
                    host_wifi_connected_reboot();
                }
            } else {
                lv_label_set_text(s_wifiStatusHint, "restarting...");
                host_wifi_connected_reboot();
            }
        } else {
            lv_label_set_text_fmt(s_wifiStatusLbl, "Couldn't connect to\n%s.\nCheck the password.", s_wifiSelSsid);
            lv_label_set_text(s_wifiStatusHint, "push to go back");
        }
    }
}

void build_setup_pages() {
    // --- first boot: which way do you want to give it WiFi? ---
    //
    // Five lines in a 466 px circle. At 90 px above and below centre the chord is still
    // 430 px wide and the inscribed area is about 330 px tall, so this sits inside the
    // budget at readable sizes with room left: nothing here is shrunk to fit.
    s_fbPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_fbPage);
    lv_obj_set_size(s_fbPage, SCREEN_W, SCREEN_H); lv_obj_center(s_fbPage);
    // Opaque black, not a transparent overlay: this is the first thing a stranger sees and
    // it must not have the clock it cannot trust showing through from behind.
    lv_obj_set_style_bg_color(s_fbPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_fbPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_fbPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *fbTitle = lv_label_create(s_fbPage);
    lv_label_set_text(fbTitle, "The Orb needs WiFi");
    lv_obj_set_style_text_color(fbTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(fbTitle, &lv_font_montserrat_28, 0);
    lv_obj_align(fbTitle, LV_ALIGN_CENTER, 0, -146);
    // Load-bearing, not decoration. The S3 has no 5 GHz radio, so a 5 GHz-only network
    // never appears in the scan at all — and before this line the screen offered no reason
    // why, leaving a stranger looking at a list with their own network missing from it.
    lv_obj_t *fbBand = lv_label_create(s_fbPage);
    lv_label_set_text(fbBand, "2.4 GHz only");
    lv_obj_set_style_text_color(fbBand, C_DIM, 0);
    lv_obj_set_style_text_font(fbBand, &lv_font_montserrat_20, 0);
    lv_obj_align(fbBand, LV_ALIGN_CENTER, 0, -108);
    for (int i = 0; i < FB_COUNT; ++i) {
        s_fbItems[i] = lv_label_create(s_fbPage);
        lv_label_set_text(s_fbItems[i], FB_LABELS[i]);
        // Font, opacity, position: show_wheel(), called from refresh_firstboot().
    }
    // A sixth line, and the one addition to the agreed copy. This is the only screen whose
    // audience has never touched the knob before, so the one place the grammar cannot be
    // assumed. Same wording and position the location menu already uses.
    // "OR", so the two read as alternatives rather than a sequence. Dim and small: it is
    // punctuation between the choices, not a third thing to choose.
    lv_obj_t *fbOr = lv_label_create(s_fbPage);
    lv_label_set_text(fbOr, "OR");
    lv_obj_set_style_text_color(fbOr, C_GREY, 0);
    lv_obj_set_style_text_font(fbOr, &lv_font_montserrat_18, 0);
    lv_obj_align(fbOr, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t *fbHint = lv_label_create(s_fbPage);
    lv_label_set_text(fbHint, "turn to choose, push to select");
    lv_obj_set_style_text_color(fbHint, C_GREY, 0);
    lv_obj_set_style_text_font(fbHint, &lv_font_montserrat_18, 0);
    lv_obj_align(fbHint, LV_ALIGN_CENTER, 0, 150);
    reg_hint(fbHint);

    // --- first boot: the phone path ---
    //
    // Instruction only. The access point is already running by the time this screen can be
    // reached: autoConnect() raises "The Orb Setup" before main.cpp selects Settings, so
    // there is nothing to start here and nothing to tear down on the way back.
    s_fbPhonePage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_fbPhonePage);
    lv_obj_set_size(s_fbPhonePage, SCREEN_W, SCREEN_H); lv_obj_center(s_fbPhonePage);
    lv_obj_set_style_bg_color(s_fbPhonePage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_fbPhonePage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_fbPhonePage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *fpLead = lv_label_create(s_fbPhonePage);
    lv_label_set_text(fpLead, "On your phone, join");
    lv_obj_set_style_text_color(fpLead, C_DIM, 0);
    lv_obj_set_style_text_font(fpLead, &lv_font_montserrat_20, 0);
    lv_obj_align(fpLead, LV_ALIGN_CENTER, 0, -78);
    // The network name is the one thing on this screen a person has to copy correctly, so
    // it is the one thing set larger than everything around it.
    lv_obj_t *fpSsid = lv_label_create(s_fbPhonePage);
    lv_label_set_text(fpSsid, "The Orb Setup");
    lv_obj_set_style_text_color(fpSsid, lv_color_white(), 0);
    lv_obj_set_style_text_font(fpSsid, &lv_font_montserrat_36, 0);
    lv_obj_align(fpSsid, LV_ALIGN_CENTER, 0, -36);
    lv_obj_t *fpL1 = lv_label_create(s_fbPhonePage);
    lv_label_set_text(fpL1, "A page opens by itself.");
    lv_obj_set_style_text_color(fpL1, lv_color_white(), 0);
    lv_obj_set_style_text_font(fpL1, &lv_font_montserrat_22, 0);
    lv_obj_align(fpL1, LV_ALIGN_CENTER, 0, 24);
    lv_obj_t *fpL2 = lv_label_create(s_fbPhonePage);
    lv_label_set_text(fpL2, "Pick your network there.");
    lv_obj_set_style_text_color(fpL2, lv_color_white(), 0);
    lv_obj_set_style_text_font(fpL2, &lv_font_montserrat_22, 0);
    lv_obj_align(fpL2, LV_ALIGN_CENTER, 0, 52);
    lv_obj_t *fpBack = lv_label_create(s_fbPhonePage);
    lv_label_set_text(fpBack, LV_SYMBOL_LEFT "  Back");
    lv_obj_set_style_text_color(fpBack, C_GREY, 0);
    lv_obj_set_style_text_font(fpBack, &lv_font_montserrat_26, 0);
    lv_obj_align(fpBack, LV_ALIGN_CENTER, 0, 120);

    // --- no readable SD card ---
    s_noSdPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_noSdPage);
    lv_obj_set_size(s_noSdPage, SCREEN_W, SCREEN_H); lv_obj_center(s_noSdPage);
    lv_obj_set_style_bg_color(s_noSdPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_noSdPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_noSdPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sdTitle = lv_label_create(s_noSdPage);
    // The same voice as "The Orb needs WiFi", on purpose: these are the two things it can
    // be missing, and a person who meets both should not have to learn two tones.
    lv_label_set_text(sdTitle, "The Orb needs\nan SD card");
    lv_obj_set_style_text_color(sdTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(sdTitle, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(sdTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sdTitle, LV_ALIGN_CENTER, 0, -60);
    // Why, in one line. UX-006: nothing on screen is unexplained. Without this the notice
    // is a demand with no reason attached, which is the shape of an error code in words.
    lv_obj_t *sdWhy = lv_label_create(s_noSdPage);
    lv_label_set_text(sdWhy, "Designs live on the card.");
    lv_obj_set_style_text_color(sdWhy, C_DIM, 0);
    lv_obj_set_style_text_font(sdWhy, &lv_font_montserrat_20, 0);
    lv_obj_align(sdWhy, LV_ALIGN_CENTER, 0, 16);
    lv_obj_t *sdHow = lv_label_create(s_noSdPage);
    lv_label_set_text(sdHow, "Insert one and restart.");
    lv_obj_set_style_text_color(sdHow, lv_color_white(), 0);
    lv_obj_set_style_text_font(sdHow, &lv_font_montserrat_22, 0);
    lv_obj_align(sdHow, LV_ALIGN_CENTER, 0, 46);
    lv_obj_t *sdHint = lv_label_create(s_noSdPage);
    lv_label_set_text(sdHint, "push to carry on without one");
    lv_obj_set_style_text_color(sdHint, C_GREY, 0);
    lv_obj_set_style_text_font(sdHint, &lv_font_montserrat_18, 0);
    lv_obj_align(sdHint, LV_ALIGN_CENTER, 0, 130);

}

void build_wifi_pages() {
    // --- WiFi: scrolling network list (same look as the main menu) ---
    s_wifiListPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_wifiListPage);
    lv_obj_set_size(s_wifiListPage, SCREEN_W, SCREEN_H); lv_obj_center(s_wifiListPage);
    // Its own opaque ground. remove_style_all leaves a page transparent, which is why the
    // theme's plate was still showing through the setup path after the text was fixed.
    lv_obj_set_style_bg_color(s_wifiListPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_wifiListPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_wifiListPage, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *wtitle = lv_label_create(s_wifiListPage);
        lv_label_set_text(wtitle, "WiFi");
        lv_obj_set_style_text_color(wtitle, C_DIM, 0);
        lv_obj_set_style_text_font(wtitle, &lv_font_montserrat_20, 0);
        lv_obj_align(wtitle, LV_ALIGN_CENTER, 0, -122);   // below the persistent "SETTINGS" header

        for (int r = 0; r < WIFI_VISIBLE; ++r) {
            s_wifiRows[r] = lv_label_create(s_wifiListPage);
            lv_label_set_text(s_wifiRows[r], "");
            lv_label_set_long_mode(s_wifiRows[r], LV_LABEL_LONG_DOT);
            lv_obj_set_width(s_wifiRows[r], 300);
            lv_obj_set_style_text_align(s_wifiRows[r], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(s_wifiRows[r], &lv_font_montserrat_26, 0);
            // Only the selected row ever scrolls (set per-refresh below), so a resting
            // screen stays still. Width is bounded so LVGL knows when to start.
            lv_obj_set_width(s_wifiRows[r], 400);
            lv_obj_align(s_wifiRows[r], LV_ALIGN_CENTER, 0, -(WIFI_VISIBLE - 1) * WIFI_ROW_DY / 2 + r * WIFI_ROW_DY);
        }
        s_wifiListHint = lv_label_create(s_wifiListPage);
        lv_label_set_text(s_wifiListHint, "");
        lv_obj_set_style_text_color(s_wifiListHint, C_GREY, 0);
        lv_obj_set_style_text_font(s_wifiListHint, &lv_font_montserrat_14, 0);
        lv_obj_align(s_wifiListHint, LV_ALIGN_CENTER, 0, 150);
    }

    // --- WiFi: password entry (character strip, same as the city search) ---
    s_wifiPassPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_wifiPassPage);
    lv_obj_set_size(s_wifiPassPage, SCREEN_W, SCREEN_H); lv_obj_center(s_wifiPassPage);
    // Its own opaque ground. remove_style_all leaves a page transparent, which is why the
    // theme's plate was still showing through the setup path after the text was fixed.
    lv_obj_set_style_bg_color(s_wifiPassPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_wifiPassPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_wifiPassPage, LV_OBJ_FLAG_SCROLLABLE);
    {
        s_wifiPassTitle = lv_label_create(s_wifiPassPage);
        lv_label_set_text(s_wifiPassTitle, "Password");
        lv_label_set_long_mode(s_wifiPassTitle, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_wifiPassTitle, 300);
        lv_obj_set_style_text_color(s_wifiPassTitle, C_DIM, 0);
        lv_obj_set_style_text_font(s_wifiPassTitle, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(s_wifiPassTitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_wifiPassTitle, LV_ALIGN_CENTER, 0, -118);   // below the "SETTINGS" header

        s_passText = lv_label_create(s_wifiPassPage);
        lv_label_set_text(s_passText, "(enter password)");
        lv_label_set_long_mode(s_passText, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_passText, 320);
        lv_obj_set_style_text_align(s_passText, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_passText, C_WHITE, 0);
        lv_obj_set_style_text_font(s_passText, &lv_font_montserrat_20, 0);
        lv_obj_align(s_passText, LV_ALIGN_CENTER, 0, -70);

        for (int k = 0; k < 7; ++k) {
            s_wkStrip[k] = lv_label_create(s_wifiPassPage);
            lv_label_set_text(s_wkStrip[k], "");
            lv_obj_set_style_text_color(s_wkStrip[k], C_GREY, 0);
            lv_obj_set_style_text_font(s_wkStrip[k], &lv_font_montserrat_18, 0);
            lv_obj_align(s_wkStrip[k], LV_ALIGN_CENTER, (k - 3) * 48, 20);
        }
        s_wifiPassHint = lv_label_create(s_wifiPassPage);
        lv_label_set_text(s_wifiPassHint, // Names Back as well as OK. The old wording listed only OK, which is the same
        // discoverability gap the Back key was added to close — a way out nobody is told
        // about is a way out nobody finds.
        "turn to a key, push to enter it\nOK connects, Back returns");
        lv_obj_set_style_text_align(s_wifiPassHint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(s_wifiPassHint, C_GREY, 0);
        lv_obj_set_style_text_font(s_wifiPassHint, &lv_font_montserrat_14, 0);
        lv_obj_align(s_wifiPassHint, LV_ALIGN_CENTER, 0, 120);
    }

    // --- WiFi: connect status page ---
    s_wifiStatusPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_wifiStatusPage);
    lv_obj_set_size(s_wifiStatusPage, SCREEN_W, SCREEN_H); lv_obj_center(s_wifiStatusPage);
    // Its own opaque ground. remove_style_all leaves a page transparent, which is why the
    // theme's plate was still showing through the setup path after the text was fixed.
    lv_obj_set_style_bg_color(s_wifiStatusPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_wifiStatusPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_wifiStatusPage, LV_OBJ_FLAG_SCROLLABLE);
    {
        s_wifiStatusLbl = lv_label_create(s_wifiStatusPage);
        lv_label_set_text(s_wifiStatusLbl, "");
        lv_obj_set_style_text_color(s_wifiStatusLbl, C_WHITE, 0);
        lv_obj_set_style_text_font(s_wifiStatusLbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(s_wifiStatusLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_wifiStatusLbl, LV_ALIGN_CENTER, 0, -20);

        s_wifiStatusHint = lv_label_create(s_wifiStatusPage);
        lv_label_set_text(s_wifiStatusHint, "");
        lv_obj_set_style_text_color(s_wifiStatusHint, C_GREY, 0);
        lv_obj_set_style_text_font(s_wifiStatusHint, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s_wifiStatusHint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_wifiStatusHint, LV_ALIGN_CENTER, 0, 90);
    }

}

}  // namespace settings_impl

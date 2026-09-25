#include "settings_internal.h"

namespace settings_impl {

// ---- the state declared in settings_internal.h ----
lv_obj_t *s_hints[24] = { nullptr };
int       s_hintN = 0;
Mode s_mode  = MODE_MENU;
int  s_sel   = 0;           // main-menu selection
int  s_bri   = 200;
int  s_lmSel = 0;          // location-menu selection
int  s_sndSel = 0;         // sound-menu selection
int  s_chimeSel = 0;       // chime-picker selection (0..count-1 = a chime, count = Back)
int  s_unitsSel = 0;       // units-menu selection
int  s_rangeSel = 0;       // range-menu selection
int  s_dspSel = 0;         // display-menu selection
int  s_designSel = 0;      // design-picker selection (0..s_designCount-1 = a theme, s_designCount = Back)
int  s_vol   = 60;         // volume working value
char s_designSlugs[theme_select::MAX_THEMES][theme_select::MAX_SLUG_LEN];
int  s_designCount = 0;
char   s_recNames[RECENTS_MAX][40];
double s_recLat[RECENTS_MAX], s_recLon[RECENTS_MAX];
int    s_recCount = 0;
int    s_recSel   = 0;     // 0..s_recCount-1 = a city, s_recCount = Back
char    s_wifiNames[WIFI_MAX][33];
int8_t  s_wifiRssi[WIFI_MAX];
bool    s_wifiOpen[WIFI_MAX];
int     s_wifiCount   = 0;
int     s_wifiSel     = 0;         // list selection: 0..count-1 networks, count=Rescan, count+1=Back
bool    s_wifiScanning = false;
char    s_wifiSelSsid[33] = "";
bool    s_wifiSelOpen     = false;
bool     s_wifiConnecting  = false;
uint32_t s_connectStartMs  = 0;    // bounds the attempt; see wifi_tick()
char    s_pass[65]  = "";
int     s_wkbIdx    = 0;
char   s_str[28]   = "";
int    s_kbIdx     = 0;
char   s_sugName[4][40];
double s_sugLat[4], s_sugLon[4];
int    s_sugCount  = 0;
bool   s_pending   = false;
int    s_countdown = 0;
bool   s_searching = false;
lv_obj_t *s_screen  = nullptr;
lv_obj_t *s_menu    = nullptr;
lv_obj_t *s_items[ITEM_COUNT] = { nullptr };
lv_obj_t *s_bright  = nullptr;
lv_obj_t *s_barFill = nullptr;
lv_obj_t *s_pct     = nullptr;
lv_obj_t *s_lmPage  = nullptr;   // location menu
lv_obj_t *s_lmItems[LM_COUNT] = { nullptr };
lv_obj_t *s_recPage = nullptr;   // recent cities scroller
lv_obj_t *s_recName = nullptr;
lv_obj_t *s_recCoord= nullptr;
lv_obj_t *s_srchPage= nullptr;
lv_obj_t *s_srchText= nullptr;
lv_obj_t *s_strip[7]= { nullptr };
lv_obj_t *s_sug[4]  = { nullptr };
lv_obj_t *s_dspPage = nullptr;   // display menu (screen timeout + brightness)
lv_obj_t *s_dspItems[DSP_COUNT] = { nullptr };
lv_obj_t *s_sndPage = nullptr;   // sound menu
lv_obj_t *s_sndItems[SND_COUNT] = { nullptr };
lv_obj_t *s_unitsPage = nullptr;   // units menu
lv_obj_t *s_unitsItems[UNIT_COUNT] = { nullptr };
lv_obj_t *s_rangePage = nullptr;   // range menu (Flight Tracker display range)
lv_obj_t *s_rangeItems[RNG_COUNT] = { nullptr };
lv_obj_t *s_chimeSelPage = nullptr;   // chime picker (Sound > Chime sound)
lv_obj_t *s_chimeSelItems[CHIME_UI_MAX + 1] = { nullptr };   // chimes + Back
lv_obj_t *s_designPage = nullptr;   // design picker (top-level Design item)
lv_obj_t *s_designItems[theme_select::MAX_THEMES + 1] = { nullptr };   // installed themes + Back
lv_obj_t *s_designNoticePage = nullptr;   // "restarting..." heads-up, shown right before the reboot
lv_obj_t *s_volPage = nullptr;   // volume adjuster
lv_obj_t *s_volFill = nullptr;
lv_obj_t *s_volPct  = nullptr;
lv_obj_t *s_plateImg = nullptr;    // themed background, built on enter / freed on exit
lv_obj_t *s_ovImg    = nullptr;    // themed CRT+glass, same lifecycle
lv_obj_t *s_aboutPage = nullptr;   // About: the boot splash, push to return
char      s_netInfo[112] = "";     // last line handed to setNetInfo(), replayed on page open
char      s_homeCoords[48] = "";   // last value handed to setHomeCoords(), same contract
lv_obj_t *s_lmCoords = nullptr;    // the readout under the Location page's title
lv_obj_t *s_aboutImg  = nullptr;   // decoded fresh each time (see refresh_about()) — cheap, avoids relying on splash_art's shared decode buffer staying valid
lv_obj_t *s_fbPage  = nullptr;
lv_obj_t *s_fbItems[FB_COUNT] = { nullptr, nullptr };
int       s_fbSel   = FB_ONDEVICE;
bool      s_pendingWifiSetup = false;
lv_obj_t *s_fbPhonePage = nullptr;
lv_obj_t *s_noSdPage = nullptr;
bool s_systemChrome = false;
lv_obj_t *s_resetPage = nullptr;   // Reset: warning + confirm, push to wipe, turn to cancel
lv_obj_t *s_wifiListPage  = nullptr;    // scrolling network list
lv_obj_t *s_wifiRows[WIFI_VISIBLE] = { nullptr };
lv_obj_t *s_wifiListHint  = nullptr;
bool      s_firstBootPrompt = false;    // set by openWifiSetupPrompt(); swaps the WiFi list's hint text
lv_obj_t *s_wifiPassPage  = nullptr;    // password: text line + character strip
lv_obj_t *s_wifiPassTitle = nullptr;
lv_obj_t *s_passText      = nullptr;
lv_obj_t *s_wkStrip[7]    = { nullptr };
lv_obj_t *s_wifiPassHint  = nullptr;
lv_obj_t *s_wifiStatusPage= nullptr;
lv_obj_t *s_wifiStatusLbl = nullptr;
lv_obj_t *s_wifiStatusHint= nullptr;
lv_color_t C_WHITE = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);   // primary text (selected row)
lv_color_t C_GREY  = LV_COLOR_MAKE(0x6A, 0x70, 0x78);   // secondary text (unselected rows)
lv_color_t C_DIM   = LV_COLOR_MAKE(0x9A, 0xA0, 0xA6);   // hints
lv_color_t C_ACCENT= LV_COLOR_MAKE(0x4F, 0xC3, 0xF7);   // slider fill / links
lv_color_t C_BG    = lv_color_black();                  // screen background (and the opaque sub-page backings)
lv_color_t C_TRACK = lv_color_hex(0x2A2E33);            // slider track
void refresh_wifi_list();     // defined below (used by show_page)
void refresh_wifi_pass();

bool mode_is_system_chrome(Mode m) {
    return m == MODE_NO_SDCARD  || m == MODE_FIRSTBOOT      || m == MODE_FIRSTBOOT_PHONE
        || m == MODE_WIFI_LIST  || m == MODE_WIFI_PASSWORD  || m == MODE_WIFI_STATUS;
}


// Pages that paint their OWN full-screen picture, so the Settings plate and glass must
// not sit over them.
//
// Deliberately a wider set than mode_is_system_chrome(), and asked as its own question
// rather than folded into that one. System chrome means "this is the setup path, so the
// wheel's VALUES go stock too", because a theme must never be able to render the screen
// somebody fixes their WiFi on illegible. About is not a setup screen - it is the boot
// splash, opened from Settings - and it wants none of those value swaps. It just owns
// every pixel while it is up. Conflating the two is what left the Settings glass
// composited over the splash: settings_overlay.png is move_foreground()'d, so Aviator's
// "SETTINGS" wordmark and winged badge sat on top of the ORB logo.
bool mode_paints_its_own_screen(Mode m) {
    return m == MODE_ABOUT || mode_is_system_chrome(m);
}



// Cut a row's text to fit maxW in its face, ending in "...", from the full text the
// label was last given. The full text lives on the label's user data: set by a
// refresh_*() through lv_label_set_text, and recognised here because anything the
// label shows that is not "the remembered text, or a cut of it" is a new text.
bool is_cut_of(const char *shown, const char *full) {
    const size_t n = strlen(shown);
    if (n < 3 || strcmp(shown + n - 3, "...") != 0) return false;
    return strncmp(shown, full, n - 3) == 0;
}

void fit_label(lv_obj_t *lbl, const lv_font_t *font, float maxW) {
    const char *shown = lv_label_get_text(lbl);
    char *full = (char *)lv_obj_get_user_data(lbl);
    if (!full || (strcmp(shown, full) != 0 && !is_cut_of(shown, full))) {
        free(full);
        full = strdup(shown);
        lv_obj_set_user_data(lbl, full);
    }
    if (!full) return;
    char buf[64];
    wheel::fit(font, full, maxW, buf, sizeof(buf));   // the same cut the canvas applies; the full text if it fits
    if (strcmp(shown, buf) != 0) lv_label_set_text(lbl, buf);
}


// Shared by every fixed-item list in Settings, so they all roll the same way. The selected row sits at the
// panel centre and the rest fall away along the dial (wheel_layout.h). The rows are drawn on the wheel's
// canvas; the labels only hold the text. When the canvas could not be allocated the labels are laid out and
// shown instead, in the same colours and faces, because Settings must stay readable and navigable.
void show_wheel(lv_obj_t **items, int count, int sel) {
    const wheel::Look lk = look();
    if (count > MAX_WHEEL_ROWS) count = MAX_WHEEL_ROWS;
    if (wheel::available()) {
        const char *texts[MAX_WHEEL_ROWS];
        for (int i = 0; i < count; ++i) {
            texts[i] = lv_label_get_text(items[i]);
            lv_obj_set_style_text_opa(items[i], LV_OPA_TRANSP, 0);   // the canvas draws the real glyphs
        }
        wheel::draw(texts, count, sel, lk);
        return;
    }
    for (int i = 0; i < count; ++i) {
        const wheel_layout::Row r = wheel_layout::row(i, sel);
        const bool isSel = (i == sel);
        const lv_font_t *f = isSel ? lk.selFont : lk.itemFont;
        lv_obj_set_style_text_font(items[i], f, 0);
        lv_obj_set_size(items[i], (lv_coord_t)lroundf(r.maxW), lv_font_get_line_height(f));
        lv_obj_set_style_text_align(items[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(items[i], LV_LABEL_LONG_CLIP);
        fit_label(items[i], f, r.maxW);
        lv_obj_align(items[i], LV_ALIGN_CENTER, (lv_coord_t)lroundf(r.sx), (lv_coord_t)lroundf(r.sy));
        lv_obj_set_style_text_color(items[i], isSel ? lk.selColor : lk.itemColor, 0);
        lv_obj_set_style_text_opa(items[i], r.offDial ? LV_OPA_TRANSP : (lv_opa_t)r.opa, 0);
    }
}


void show_page(Mode m) {
    s_mode = m;
    // Before the refresh_*() calls below, which is where the drawing decisions happen.
    s_systemChrome = mode_is_system_chrome(m);
    // The plate and the glass are OBJECTS rather than values, so look() cannot reach
    // them and they have to be hidden here. The glass is the worse of the two: it is
    // move_foreground()'d, so a themed CRT layer sat OVER the setup text rather than
    // under it. Both are PNGs on the SD card, which is the circularity this whole rule
    // is about — the no-card notice was being dressed by a file whose absence it exists
    // to report.
    // Gated on mode_paints_its_own_screen(), NOT on s_systemChrome. The two were the same
    // test until About turned out to need the art hidden without needing stock values.
    const bool ownsScreen = mode_paints_its_own_screen(m);
    if (s_plateImg) { if (ownsScreen) lv_obj_add_flag(s_plateImg, LV_OBJ_FLAG_HIDDEN);
                      else            lv_obj_clear_flag(s_plateImg, LV_OBJ_FLAG_HIDDEN); }
    if (s_ovImg)    { if (ownsScreen) lv_obj_add_flag(s_ovImg, LV_OBJ_FLAG_HIDDEN);
                      else            lv_obj_clear_flag(s_ovImg, LV_OBJ_FLAG_HIDDEN); }
    // The guard, and it is here rather than in a comment because this is the second pass
    // at the same rule and the first one was a comment. Rule six: a comment cannot fail,
    // a guard can.
    //
    // look() hands the setup path stock VALUES by construction, which handles the class
    // of mistake that caused this. Themed ART is an object and cannot be routed the same
    // way, so it is asserted instead: on a setup page nothing themed may be visible, and
    // if it is, the device says so by name rather than waiting for somebody to notice a
    // background at the knob. Costs one comparison per page change.
    if (s_systemChrome) {
        const bool plateShown = s_plateImg && !lv_obj_has_flag(s_plateImg, LV_OBJ_FLAG_HIDDEN);
        const bool glassShown = s_ovImg    && !lv_obj_has_flag(s_ovImg,    LV_OBJ_FLAG_HIDDEN);
        if (plateShown || glassShown) {
            diag::log("settings: THEMED ART ON A SETUP SCREEN (mode %d): plate=%d glass=%d",
                      (int)m, (int)plateShown, (int)glassShown);
            diag::log("settings: a theme that renders a setup screen illegibly cannot be "
                      "changed - see mode_is_system_chrome()");
        }
    }
    // The wheel's text canvas is the topmost child of
    // s_screen — drawn over whichever page is visible — but it's only ever
    // cleared inside show_wheel(), called from each *list* page's own
    // refresh_*(). A non-list page (About, Reset confirm, WiFi status, the
    // theme/design notices) never calls that, so without this the canvas
    // just keeps showing whatever list was drawn last, bled on top of
    // whatever's underneath. Clearing unconditionally here, before the mode
    // switch below, means every page starts blank and a list page's own
    // refresh_*() (called a few lines down) redraws its own items right
    // back — cheap (one canvas clear) and safe when the canvas could not be
    // allocated, where clear() is a no-op.
    wheel::clear();
    // The page titles and picker hints: stock chrome only. See s_hints.
    for (int i = 0; i < s_hintN; ++i) {
        if (!s_systemChrome) lv_obj_add_flag(s_hints[i], LV_OBJ_FLAG_HIDDEN);
        else                 lv_obj_clear_flag(s_hints[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_bright, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_lmPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_recPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_srchPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_sndPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_chimeSelPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_unitsPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_rangePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_volPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_aboutPage, LV_OBJ_FLAG_HIDDEN);
    // Hiding the About page does not free its canvas, and that canvas is 651 KB of
    // PSRAM. Every other page here is a handful of labels; this one is not, so it is
    // the one page that has to give its memory back when it leaves the screen.
    splash_lines::release();
    lv_obj_add_flag(s_resetPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_fbPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_fbPhonePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_noSdPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiListPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiPassPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiStatusPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dspPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_designPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_designNoticePage, LV_OBJ_FLAG_HIDDEN);
    if (m == MODE_MENU)          { lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);    refresh_menu(); }
    else if (m == MODE_DISPLAY)  { lv_obj_clear_flag(s_dspPage, LV_OBJ_FLAG_HIDDEN); refresh_display(); }
    else if (m == MODE_BRIGHT)   { lv_obj_clear_flag(s_bright, LV_OBJ_FLAG_HIDDEN);  refresh_bright(); }
    else if (m == MODE_LOCATION) { lv_obj_clear_flag(s_lmPage, LV_OBJ_FLAG_HIDDEN);  refresh_locmenu(); }
    else if (m == MODE_RECENT)   { lv_obj_clear_flag(s_recPage, LV_OBJ_FLAG_HIDDEN); refresh_recent(); }
    else if (m == MODE_SOUND)    { lv_obj_clear_flag(s_sndPage, LV_OBJ_FLAG_HIDDEN); refresh_sound(); }
    else if (m == MODE_CHIME_SELECT) { lv_obj_clear_flag(s_chimeSelPage, LV_OBJ_FLAG_HIDDEN); refresh_chimeSelect(); }
    else if (m == MODE_UNITS)    { lv_obj_clear_flag(s_unitsPage, LV_OBJ_FLAG_HIDDEN); refresh_units(); }
    else if (m == MODE_RANGE)    { lv_obj_clear_flag(s_rangePage, LV_OBJ_FLAG_HIDDEN); refresh_range(); }
    else if (m == MODE_VOLUME)   { lv_obj_clear_flag(s_volPage, LV_OBJ_FLAG_HIDDEN); refresh_vol(); }
    else if (m == MODE_ABOUT)    { lv_obj_clear_flag(s_aboutPage, LV_OBJ_FLAG_HIDDEN); refresh_about(); }
    else if (m == MODE_RESET_CONFIRM) { lv_obj_clear_flag(s_resetPage, LV_OBJ_FLAG_HIDDEN); }
    else if (m == MODE_FIRSTBOOT)       { lv_obj_clear_flag(s_fbPage, LV_OBJ_FLAG_HIDDEN); refresh_firstboot(); }
    else if (m == MODE_FIRSTBOOT_PHONE) { lv_obj_clear_flag(s_fbPhonePage, LV_OBJ_FLAG_HIDDEN); }
    else if (m == MODE_NO_SDCARD)       { lv_obj_clear_flag(s_noSdPage, LV_OBJ_FLAG_HIDDEN); }
    else if (m == MODE_WIFI_LIST)     { lv_obj_clear_flag(s_wifiListPage, LV_OBJ_FLAG_HIDDEN); refresh_wifi_list(); }
    else if (m == MODE_WIFI_PASSWORD) { lv_obj_clear_flag(s_wifiPassPage, LV_OBJ_FLAG_HIDDEN); refresh_wifi_pass(); }
    else if (m == MODE_WIFI_STATUS)   { lv_obj_clear_flag(s_wifiStatusPage, LV_OBJ_FLAG_HIDDEN); }
    else if (m == MODE_DESIGN_SELECT) { lv_obj_clear_flag(s_designPage, LV_OBJ_FLAG_HIDDEN); refresh_designSelect(); }
    else if (m == MODE_DESIGN_NOTICE) { lv_obj_clear_flag(s_designNoticePage, LV_OBJ_FLAG_HIDDEN); }
    else                         { lv_obj_clear_flag(s_srchPage, LV_OBJ_FLAG_HIDDEN); refresh_search(); }
}

}  // namespace settings_impl


void settingsview::onTurn(int delta) {
    const int step = (delta > 0) ? 1 : -1;
    if (s_mode == MODE_MENU) {
        s_sel = (s_sel + step < 0) ? 0 : (s_sel + step >= ITEM_COUNT ? ITEM_COUNT - 1 : s_sel + step);
        refresh_menu();
    } else if (s_mode == MODE_DISPLAY) {
        s_dspSel += step;
        if (s_dspSel < 0) s_dspSel = 0;
        if (s_dspSel >= DSP_COUNT) s_dspSel = DSP_COUNT - 1;
        refresh_display();
    } else if (s_mode == MODE_BRIGHT) {
        s_bri += delta * BRI_STEP;
        if (s_bri < BRI_MIN) s_bri = BRI_MIN;
        if (s_bri > BRI_MAX) s_bri = BRI_MAX;
        host_set_brightness(s_bri, false);
        refresh_bright();
    } else if (s_mode == MODE_FIRSTBOOT) {
        s_fbSel += step;
        if (s_fbSel < 0) s_fbSel = 0;
        if (s_fbSel >= FB_COUNT) s_fbSel = FB_COUNT - 1;
        refresh_firstboot();
    } else if (s_mode == MODE_FIRSTBOOT_PHONE) {
        // Nothing to move between. The only action is Back, and it is on the press.
    } else if (s_mode == MODE_LOCATION) {
        s_lmSel += step;
        if (s_lmSel < 0) s_lmSel = 0;
        if (s_lmSel >= LM_COUNT) s_lmSel = LM_COUNT - 1;
        refresh_locmenu();
    } else if (s_mode == MODE_RECENT) {
        s_recSel += step;
        if (s_recSel < 0) s_recSel = 0;
        if (s_recSel > s_recCount) s_recSel = s_recCount;   // last stop is Back
        refresh_recent();
    } else if (s_mode == MODE_SOUND) {
        s_sndSel += step;
        if (s_sndSel < 0) s_sndSel = 0;
        if (s_sndSel >= SND_COUNT) s_sndSel = SND_COUNT - 1;
        refresh_sound();
    } else if (s_mode == MODE_CHIME_SELECT) {
        const int total = chime_item_count();
        s_chimeSel += step;
        if (s_chimeSel < 0) s_chimeSel = 0;
        if (s_chimeSel >= total) s_chimeSel = total - 1;
        refresh_chimeSelect();
        if (s_chimeSel < chime_shown()) host_chime_preview(s_chimeSel);   // hear it as you browse
    } else if (s_mode == MODE_UNITS) {
        s_unitsSel += step;
        if (s_unitsSel < 0) s_unitsSel = 0;
        if (s_unitsSel >= UNIT_COUNT) s_unitsSel = UNIT_COUNT - 1;
        refresh_units();
    } else if (s_mode == MODE_RANGE) {
        s_rangeSel += step;
        if (s_rangeSel < 0) s_rangeSel = 0;
        if (s_rangeSel >= RNG_COUNT) s_rangeSel = RNG_COUNT - 1;
        refresh_range();
    } else if (s_mode == MODE_VOLUME) {
        s_vol += delta * VOL_STEP;
        if (s_vol < 0) s_vol = 0;
        if (s_vol > 100) s_vol = 100;
        host_set_volume(s_vol, false);      // live preview level
        refresh_vol();
    } else if (s_mode == MODE_WIFI_LIST) {
        const int total = wifi_item_count();
        s_wifiSel += step;
        if (s_wifiSel < 0) s_wifiSel = 0;
        if (s_wifiSel >= total) s_wifiSel = total - 1;
        refresh_wifi_list();
    } else if (s_mode == MODE_WIFI_PASSWORD) {
        // Wraps, per UX-013: no dead end, and no first or last item to get stuck against.
        // It clamped before, which on an 88-key strip meant scrolling the whole way back to
        // reach DEL or OK. It also compounds with WK_BACK being last: one detent LEFT from
        // the first character now lands on Back, so the way out is one turn away wherever
        // you are standing, rather than eighty-odd.
        s_wkbIdx = (s_wkbIdx + step + WK_TOTAL) % WK_TOTAL;
        refresh_wifi_pass();
    } else if (s_mode == MODE_DESIGN_SELECT) {
        const int total = design_item_count();
        s_designSel += step;
        if (s_designSel < 0) s_designSel = 0;
        if (s_designSel >= total) s_designSel = total - 1;
        show_wheel(s_designItems, total, s_designSel);   // rescanning the card every turn would be wasteful — just re-layout
    } else if (s_mode == MODE_ABOUT) {
        // Turning backs out to the list, the same way the reset confirmation does.
        //
        // This page used to ignore the knob entirely, and a page that ignores the knob is
        // the one place on this device where the only control does nothing. Pressing left
        // to the app switcher, which is a fine way out of Settings but the wrong way out of
        // ONE PAGE of it: you came here from the list and the list is where back means.
        s_sel = ITEM_ABOUT;      // land on the row you left from, not at the top
        show_page(MODE_MENU);
    } else if (s_mode == MODE_WIFI_STATUS || s_mode == MODE_DESIGN_NOTICE) {
        // static pages — turning does nothing here
    } else if (s_mode == MODE_RESET_CONFIRM) {
        s_sel = ITEM_RESET;      // turning either way backs out — this page is confirm/cancel only
        show_page(MODE_MENU);
    } else {  // MODE_SEARCH
        const int total = N_KEYS + s_sugCount;
        s_kbIdx += step;
        if (s_kbIdx < 0) s_kbIdx = 0;
        if (s_kbIdx >= total) s_kbIdx = total - 1;
        refresh_search();
    }
}

// Called by the app shell when Settings becomes the active app (fresh entry from
// the switcher). Reset to the top menu; the shell has already captured the knob.
namespace {
    // The plate and the glass are the settings art the theme ships. plate_sprite tries flash, then the card,
    // then nothing, and a design with no picture simply has a flat background.
    plate_sprite::Plate s_plate { "settings_plate.png",   "settings_plate" };
    plate_sprite::Plate s_glass { "settings_overlay.png", "settings_overlay", nullptr, {}, false, true };

    void settings_art_acquire() {
        if (!s_plateImg) {
            if (const lv_img_dsc_t *plate = plate_sprite::get(s_plate)) {
                s_plateImg = lv_img_create(s_screen);
                lv_img_set_src(s_plateImg, plate);
                lv_obj_center(s_plateImg);
                lv_obj_move_background(s_plateImg);   // behind every page
            }
        }
        if (!s_ovImg) {
            if (const lv_img_dsc_t *ov = plate_sprite::get(s_glass)) {
                s_ovImg = lv_img_create(s_screen);
                lv_img_set_src(s_ovImg, ov);
                lv_obj_center(s_ovImg);
            }
        }
        if (s_ovImg) lv_obj_move_foreground(s_ovImg);   // CRT/glass over everything
    }

    void settings_art_release() {
        if (s_plateImg) { lv_obj_del(s_plateImg); s_plateImg = nullptr; }
        if (s_ovImg)    { lv_obj_del(s_ovImg);    s_ovImg    = nullptr; }
        plate_sprite::release(s_plate);   // hand the decoded PSRAM back too
        plate_sprite::release(s_glass);
    }
}

// Called by app_shell when the shell switches away from Settings. Gives back the wheel canvas and the
// background art so they are not held while another app needs the PSRAM.
void settingsview::onExit() {
    wheel::release();
    settings_art_release();
}

void settingsview::onEnter() {
    settings_art_acquire();
    wheel::acquire(s_screen);
    if (s_ovImg) lv_obj_move_foreground(s_ovImg);   // the canvas was just created on top: glass goes back over it
    s_sel = 0;
    show_page(MODE_MENU);
}

void settingsview::onPress() {
    if (s_mode == MODE_FIRSTBOOT) {
        if (s_fbSel == FB_ONDEVICE) {
            // Straight into the scan/pick/type flow that has existed in this file all
            // along and was unreachable at boot for six weeks behind the CUT-03 enum bug.
            s_firstBootPrompt = true;
            start_wifi_scan();
            show_page(MODE_WIFI_LIST);
        } else {
            show_page(MODE_FIRSTBOOT_PHONE);
        }
        return;
    }
    if (s_mode == MODE_NO_SDCARD) {
        // Said once. Where it goes next is whatever the boot would have shown anyway: the
        // WiFi choice if there is also no network, otherwise out to the app switcher.
        if (s_pendingWifiSetup) { s_pendingWifiSetup = false; s_fbSel = FB_ONDEVICE; show_page(MODE_FIRSTBOOT); }
        else { app_shell::setCaptured(false); app_shell::openSwitcher(); }
        return;
    }
    if (s_mode == MODE_FIRSTBOOT_PHONE) {
        s_fbSel = FB_ONDEVICE;      // Back lands on the on-device row, as agreed
        show_page(MODE_FIRSTBOOT);
        return;
    }
    if (s_mode == MODE_MENU) {
        if (s_sel == ITEM_DISPLAY) { s_dspSel = 0; show_page(MODE_DISPLAY); }
        else if (s_sel == ITEM_LOCATION) { s_lmSel = 0; show_page(MODE_LOCATION); }
        else if (s_sel == ITEM_SOUND) { s_sndSel = 0; show_page(MODE_SOUND); }
        else if (s_sel == ITEM_UNITS) { s_unitsSel = 0; show_page(MODE_UNITS); }
        else if (s_sel == ITEM_RANGE) { s_rangeSel = 0; show_page(MODE_RANGE); }
        else if (s_sel == ITEM_WIFI) { diag::log("wifi: enter (open list)"); start_wifi_scan(); show_page(MODE_WIFI_LIST); }
        else if (s_sel == ITEM_DESIGN) { s_designSel = 0; show_page(MODE_DESIGN_SELECT); }
        else if (s_sel == ITEM_ABOUT) { show_page(MODE_ABOUT); }
        else if (s_sel == ITEM_RESET) { show_page(MODE_RESET_CONFIRM); }
        else {                                          // Back -> return to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_DISPLAY) {
        if (s_dspSel == DSP_SCREEN) {                   // cycle the screen-dim timeout
            host_set_idle_ms(IDLE_MS[(idle_index() + 1) % IDLE_N]);
            refresh_display();
        } else if (s_dspSel == DSP_BRIGHT) {
            s_bri = host_get_brightness();
            show_page(MODE_BRIGHT);
        } else {                                        // Back -> exit Settings to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_BRIGHT) {
        host_set_brightness(s_bri, true);
        app_shell::setCaptured(false);      // back always exits to the switcher, not one level up
        app_shell::openSwitcher();
    } else if (s_mode == MODE_ABOUT) {
        app_shell::setCaptured(false);      // push anywhere on this page exits to the switcher
        app_shell::openSwitcher();
    } else if (s_mode == MODE_RESET_CONFIRM) {
        host_factory_reset();          // wipes + shows its own countdown + reboots; doesn't return
    } else if (s_mode == MODE_WIFI_LIST) {
        if (s_wifiScanning) { /* wait for scan */ }
        else if (s_wifiSel < s_wifiCount) wifi_pick_network(s_wifiSel);
        else if (s_wifiSel == s_wifiCount) start_wifi_scan();          // Rescan
        else { app_shell::setCaptured(false); app_shell::openSwitcher(); }   // Back
    } else if (s_mode == MODE_WIFI_PASSWORD) {
        if (s_wkbIdx < N_WKEYS) {                                       // add a character
            const int L = (int)strlen(s_pass);
            if (L < (int)sizeof(s_pass) - 1) { s_pass[L] = WKEYS[s_wkbIdx]; s_pass[L + 1] = 0; }
            refresh_wifi_pass();
        } else if (s_wkbIdx == WK_DEL) {                               // backspace, or the way out
            const int L = (int)strlen(s_pass);
            // Backspace on an empty password goes BACK, and until now it did nothing at all.
            //
            // This screen had no exit. The strip is characters, DEL and OK — no Back row —
            // and Settings captures the knob on entry, so once you were here the only ways
            // off it were typing a password that connects or holding the knob to reboot.
            // UX-041 says an app never traps the knob and a broken app is still a screen the
            // owner can turn away from; a reboot is not turning away from a screen.
            //
            // It sits on the first-boot path a stranger walks, which is where it costs most:
            // pick the wrong network, or reach the password prompt and realise you do not
            // know it, and the device has nothing to offer but a power cycle.
            //
            // Same gesture the city search already uses — backspace past the start of an
            // empty field leaves — so this is the grammar the device has taught, not a new
            // one. Back to the network list rather than out to the switcher, because the
            // list is where the mistake was made and it is one turn from the right network.
            if (L > 0) { s_pass[L - 1] = 0; refresh_wifi_pass(); }
            else       { show_page(MODE_WIFI_LIST); }
        } else if (s_wkbIdx == WK_BACK) {                              // give up, keep the network list
            show_page(MODE_WIFI_LIST);
        } else {                                                       // OK -> connect
            wifi_begin_connect(s_pass);
        }
    } else if (s_mode == MODE_WIFI_STATUS) {
        if (!s_wifiConnecting) show_page(MODE_WIFI_LIST);   // ignore while actively connecting
    } else if (s_mode == MODE_UNITS) {
        if (s_unitsSel == UNIT_MODE) {
            host_wx_units_set((host_wx_units_mode() + 1) % 3);   // Auto -> Metric -> Imperial -> Auto
            refresh_units();
        } else {                                        // Back -> exit Settings to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_RANGE) {
        if (s_rangeSel == RNG_VALUE) {
            // Cycle to the next step up, wrapping at the top. Find where we are by
            // nearest match rather than storing an index, so a range restored from NVS
            // (or set by a theme push) that is not exactly on a step still lands
            // somewhere sensible instead of jumping to 10 km.
            const float cur = host_get_range_km();
            int best = 0; float bd = 1e9f;
            for (int i = 0; i < RANGE_N; ++i) {
                float d = cur - RANGE_STEPS_KM[i];
                if (d < 0) d = -d;
                if (d < bd) { bd = d; best = i; }
            }
            host_set_range_km(RANGE_STEPS_KM[(best + 1) % RANGE_N]);
            refresh_range();
        } else {                                        // Back -> exit Settings to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_SOUND) {
        if (s_sndSel == SND_RADAR) {
            const bool on = !host_sound_radar();
            host_sound_set_radar(on);
            refresh_sound();
            if (on) host_sound_preview_beep();
        } else if (s_sndSel == SND_CHIME) {
            const bool on = !host_sound_chime();
            host_sound_set_chime(on);
            refresh_sound();
            if (on) host_sound_preview_chime();
        } else if (s_sndSel == SND_CHIME_SEL) {
            s_chimeSel = host_chime_index();
            show_page(MODE_CHIME_SELECT);
            host_chime_preview(s_chimeSel);             // preview the current pick on entry
        } else if (s_sndSel == SND_VOLUME) {
            s_vol = host_get_volume();
            show_page(MODE_VOLUME);
        } else {                                        // Back -> exit Settings to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_CHIME_SELECT) {
        if (s_chimeSel < host_chime_count()) host_chime_set(s_chimeSel);   // Back leaves it unchanged
        app_shell::setCaptured(false);      // back always exits to the switcher, not one level up
        app_shell::openSwitcher();
    } else if (s_mode == MODE_DESIGN_SELECT) {
        if (s_designSel < s_designCount && strcmp(s_designSlugs[s_designSel], theme_select::activeSlug()[0] ? theme_select::activeSlug() : theme_select::BUILTIN_SLUG) != 0) {
            show_page(MODE_DESIGN_NOTICE);
            lv_refr_now(NULL);                              // force the notice onto the panel before the blocking reboot below
            theme_select::set(s_designSlugs[s_designSel]);  // reboots on device (never returns there); re-execs on the sim
        }
        app_shell::setCaptured(false);   // back always exits to the switcher, not one level up
        app_shell::openSwitcher();
    } else if (s_mode == MODE_DESIGN_NOTICE) {
        // transitional page — the device reboots before this could ever fire; only
        // reachable at all on the sim, and only if something presses during that instant
    } else if (s_mode == MODE_VOLUME) {
        host_set_volume(s_vol, true);
        host_sound_preview_beep();                      // hear the new level
        app_shell::setCaptured(false);      // back always exits to the switcher, not one level up
        app_shell::openSwitcher();
    } else if (s_mode == MODE_LOCATION) {
        if (s_lmSel == LM_CURRENT) {
            lv_label_set_text(s_lmItems[LM_CURRENT], "Locating...");
            lv_refr_now(NULL);
            host_locate_current();                      // reboots on success
            lv_label_set_text(s_lmItems[LM_CURRENT], LM_LABELS[LM_CURRENT]);  // came back = failed
        } else if (s_lmSel == LM_SEARCH) {
            s_str[0] = 0; s_kbIdx = 0; s_sugCount = 0; s_pending = false; s_searching = false;
            show_page(MODE_SEARCH);
        } else if (s_lmSel == LM_RECENT) {
            load_recents();
            show_page(MODE_RECENT);
        } else {                                        // Back -> exit Settings to the app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else if (s_mode == MODE_RECENT) {
        if (s_recCount > 0 && s_recSel < s_recCount)
            host_set_location_named(s_recNames[s_recSel], s_recLat[s_recSel], s_recLon[s_recSel]);
        else {                                              // Back (or empty list) -> app switcher
            app_shell::setCaptured(false);
            app_shell::openSwitcher();
        }
    } else {  // MODE_SEARCH
        const int L = (int)strlen(s_str);
        if (s_kbIdx < 26) {                                 // a letter
            if (L < (int)sizeof(s_str) - 1) { s_str[L] = KEYS[s_kbIdx]; s_str[L + 1] = 0; }
            mark_dirty();
        } else if (s_kbIdx == 26) {                         // backspace (empty -> exit search)
            if (L > 0) { s_str[L - 1] = 0; mark_dirty(); }
            else { app_shell::setCaptured(false); app_shell::openSwitcher(); }   // exit to switcher
        } else if (s_kbIdx == 27) {                         // space
            if (L > 0 && L < (int)sizeof(s_str) - 1) { s_str[L] = ' '; s_str[L + 1] = 0; }
            mark_dirty();
        } else {                                            // a suggestion
            const int j = s_kbIdx - N_KEYS;
            if (j >= 0 && j < s_sugCount) host_set_location_named(s_sugName[j], s_sugLat[j], s_sugLon[j]);
        }
    }
}

void settingsview::init() {
    // C_BG defaults to plain black, which was invisible against the old built-in look (also black). Now that the
    // built-in background is navy, Settings needs to say so explicitly or it shows a seam against every other screen.
    // No other C_* here follows the theme: that was only ever true for the retired Office skin.
    if (theme_style::paletteMode() == theme_style::PaletteMode::BuiltIn) {
        C_BG = app_theme::palette().bg;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, C_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    // A theme push's baked background sits right on top of the plain
    // bg_color fill above. Every sub-page below (brightness, WiFi, Display,
    // etc.) is its own transparent container (lv_obj_remove_style_all, no
    // bg_opa of its own) stacked on s_screen, so this one background shows
    // through consistently across all of Settings, not just the main wheel
    // menu the design was authored against.
    // Background art is NOT decoded here any more: see settings_art_acquire(). At 466x466
    // it costs ~636 KB of PSRAM, and holding it from boot (plus the same again for the
    // glass layer) starved screens that were actually on display.

    build_menu_and_brightness_pages();
    build_location_menu();
    build_setup_pages();
    build_recent_and_search();
    build_option_pages();
    build_wifi_pages();

    // First boot: seed the recents list so "Recent cities" starts populated.
    {
        char   tn[RECENTS_MAX][40];
        double tla[RECENTS_MAX], tlo[RECENTS_MAX];
        if (host_recents_get(tn, tla, tlo, RECENTS_MAX) == 0)
            for (int i = SEED_COUNT - 1; i >= 0; --i)      // reversed so SEED_CITIES[0] lands on top
                host_recents_add(SEED_CITIES[i].name, SEED_CITIES[i].lat, SEED_CITIES[i].lon);
    }

    // The wheel's text canvas is created on entry (onEnter), not here; onEnter raises the glass above it so
    // scanlines and glass still sit over the text, the same background -> content -> overlay order every other
    // compositor uses.

    // CRT + glass on top of everything — created last, after every sub-page
    // above, so it's the topmost child of s_screen regardless of which page is
    // currently shown (same background -> content -> overlay order the other
    // custom compositors use).
    // Glass likewise built on entry, not at boot (see settings_art_acquire()).

    s_bri = host_get_brightness();
    show_page(MODE_MENU);
    lv_timer_create(search_tick, 200, nullptr);
    lv_timer_create(wifi_tick, 300, nullptr);
}

lv_obj_t *settingsview::screen() { return s_screen; }

// Called once from main.cpp's setup() when the device booted with the "needs WiFi
// setup" flag set (fresh out of the box, or just after a Reset) — jumps straight past
// the main menu into the WiFi list with a first-run prompt instead of the usual hint.
// UX-024. `alsoNeedsWifi` chains the two notices rather than letting one hide the other: a
// device out of the box with neither card nor network is missing two things and should say
// both, in the order they have to be fixed. The card comes first because it is the one that
// needs somebody to go and find a physical object.
void settingsview::openNoSdCardNotice(bool alsoNeedsWifi) {
    s_pendingWifiSetup = alsoNeedsWifi;
    s_sel = ITEM_WIFI;
    show_page(MODE_NO_SDCARD);
}

void settingsview::openWifiSetupPrompt() {
    // The choice screen first, not the scan. Both paths lead somewhere that works, and the
    // on-device one is pre-selected so the default is still a single press — see the
    // FB_LABELS block above for why this screen is allowed to ask at all.
    //
    // s_firstBootPrompt is set when the on-device row is taken rather than here, because it
    // is what swaps the WiFi list's hint text and the list is no longer where this lands.
    s_sel   = ITEM_WIFI;
    s_fbSel = FB_ONDEVICE;
    show_page(MODE_FIRSTBOOT);
}

// Called once from main.cpp's setup() right after a theme push leaves a
// custom splash active — jumps straight to the About page, which holds the same
// splash art up indefinitely (push the knob to leave) instead of the normal boot
// splash's 2s-then-fade, so a just-pushed design stays on screen to look at.
const char *settingsview::designRowText(int i) {
    if (s_mode != MODE_DESIGN_SELECT || i < 0 || i >= design_item_count()) return nullptr;
    return lv_label_get_text(s_designItems[i]);
}

void settingsview::openAboutPage() {
    s_sel = ITEM_ABOUT;
    show_page(MODE_ABOUT);
}

// Called from the host's status loop. Stored rather than drawn immediately: the About
// page is usually hidden, and the loop runs far more often than anyone opens it.
void settingsview::setHomeCoords(double lat, double lon, bool set) {
    if (set) snprintf(s_homeCoords, sizeof(s_homeCoords), "%.5f, %.5f", lat, lon);
    else     s_homeCoords[0] = '\0';
    if (s_mode == MODE_LOCATION) refresh_locmenu();
}

void settingsview::setNetInfo(const char *line) {
    if (!line) return;
    snprintf(s_netInfo, sizeof(s_netInfo), "%s", line);
    if (s_mode == MODE_ABOUT) splash_lines::setNetwork(s_netInfo);
}

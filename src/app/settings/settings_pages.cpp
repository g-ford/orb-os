// The simple Settings pages: the main wheel, Display and Brightness, Sound, Chime, Theme picker, Range, Units, Volume, About and Reset.
// Split out of settings_view.cpp; the shared modes, constants and state are in settings_internal.h.
#include "settings_internal.h"

namespace settings_impl {


int idle_index() {   // which IDLE_MS entry the current timeout matches (default 1 hour)
    const uint32_t cur = host_get_idle_ms();
    for (int i = 0; i < IDLE_N; ++i) if (IDLE_MS[i] == cur) return i;
    return 4;   // 1 hour
}


void refresh_display() {
    char b[28];
    snprintf(b, sizeof(b), "Screen   %s", IDLE_LABELS[idle_index()]);
    lv_label_set_text(s_dspItems[DSP_SCREEN], b);
    lv_label_set_text(s_dspItems[DSP_BRIGHT], "Brightness");
    lv_label_set_text(s_dspItems[DSP_BACK], "Back");
    show_wheel(s_dspItems, DSP_COUNT, s_dspSel);
}


void refresh_bright() {
    int pct = (int)lroundf((s_bri - BRI_MIN) * 100.0f / (BRI_MAX - BRI_MIN));
    lv_obj_set_width(s_barFill, (lv_coord_t)(4 + pct * (236 - 4) / 100));
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(s_pct, buf);
}


void refresh_sound() {
    char b[28];
    snprintf(b, sizeof(b), "Radar sounds   %s", host_sound_radar() ? "ON" : "OFF");
    lv_label_set_text(s_sndItems[SND_RADAR], b);
    snprintf(b, sizeof(b), "Clock chime   %s", host_sound_chime() ? "ON" : "OFF");
    lv_label_set_text(s_sndItems[SND_CHIME], b);
    snprintf(b, sizeof(b), "Audio: %s", host_chime_name(host_chime_index()));
    lv_label_set_text(s_sndItems[SND_CHIME_SEL], b);
    snprintf(b, sizeof(b), "Volume   %d%%", host_get_volume());
    lv_label_set_text(s_sndItems[SND_VOLUME], b);
    lv_label_set_text(s_sndItems[SND_BACK], "Back");
    show_wheel(s_sndItems, SND_COUNT, s_sndSel);
}


// Chime picker: turning previews each chime live (host_chime_preview), pressing
// confirms it (host_chime_set) and returns to Sound. Sized for CHIME_UI_MAX chimes
// though only one ("Westminster") exists today.
// Clamped HERE as well as sized above, because this count feeds the wheel's navigation
// and the array index that writes "Back". Two guards for one array, and the cheaper one
// is the one that cannot be defeated by a card holding more themes than anybody expected.
int chime_shown() {
    const int n = host_chime_count();
    return n > CHIME_UI_MAX ? CHIME_UI_MAX : n;
}


void refresh_chimeSelect() {
    const int n = chime_shown();
    for (int i = 0; i < n; ++i)
        lv_label_set_text(s_chimeSelItems[i], host_chime_name(i));
    lv_label_set_text(s_chimeSelItems[n], "Back");
    show_wheel(s_chimeSelItems, chime_item_count(), s_chimeSel);
}


// Design picker (top-level "Theme" item, MODE_DESIGN_SELECT internally): turning browses the built-in look and
// whichever themes are installed on the SD card, pressing shows the restart notice
// and applies it (settingsview::onPress). Rescans the card every time this page
// is entered (see show_page's MODE_DESIGN_SELECT dispatch) rather than once at
// boot, so a card swapped since boot (or a fresh export copied over) shows up
// without a full reboot just to see it.
void refresh_designSelect() {
    s_designCount = theme_select::listInstalled(s_designSlugs);
    // The built-in look is always the first choice: nothing to install, and the way back from any theme.
    if (s_designCount > theme_select::MAX_THEMES - 1) s_designCount = theme_select::MAX_THEMES - 1;
    for (int i = s_designCount; i > 0; --i)
        memcpy(s_designSlugs[i], s_designSlugs[i - 1], theme_select::MAX_SLUG_LEN);
    strncpy(s_designSlugs[0], theme_select::BUILTIN_SLUG, theme_select::MAX_SLUG_LEN - 1);
    s_designSlugs[0][theme_select::MAX_SLUG_LEN - 1] = 0;
    ++s_designCount;
    // Show each theme's display name, never its folder slug. The theme shown as
    // "Modern" lives in a folder called `the-office` (its former name), and putting
    // the slug on screen made the two look like different themes.
    //
    // UNLESS two of them say the same thing, which happens more than it should: the theme tool
    // gives a new theme a new folder, so saving a design twice under one name leaves two
    // folders on the card, both calling themselves Aviator. Two identical rows and no
    // way to tell which is which is worse than showing a folder name, so a name that
    // collides earns its slug in brackets and a name that does not is left alone.
    for (int i = 0; i < s_designCount; ++i) {
        char label[32];
        theme_style::labelFor(s_designSlugs[i], label, sizeof(label));
        bool clash = false;
        for (int j = 0; j < s_designCount && !clash; ++j) {
            if (j == i) continue;
            char other[32];
            theme_style::labelFor(s_designSlugs[j], other, sizeof(other));
            clash = strcmp(label, other) == 0;
        }
        if (clash) {
            char shown[64];
            snprintf(shown, sizeof(shown), "%.20s (%.16s)", label, s_designSlugs[i]);
            lv_label_set_text(s_designItems[i], shown);
        } else {
            lv_label_set_text(s_designItems[i], label);
        }
    }
    lv_label_set_text(s_designItems[s_designCount], "Back");
    for (int i = s_designCount + 1; i < theme_select::MAX_THEMES + 1; ++i) lv_label_set_text(s_designItems[i], "");
    if (s_designSel > s_designCount) s_designSel = s_designCount;
    show_wheel(s_designItems, design_item_count(), s_designSel);
}


void refresh_units() {
    const int mode = host_wx_units_mode();
    const char *resolved = host_wx_is_imperial() ? "F, mi" : "C, km";
    char b[36];
    if (mode == 0) snprintf(b, sizeof(b), "Units   Auto (%s)", resolved);
    else           snprintf(b, sizeof(b), "Units   %s", mode == 2 ? "Imperial (F, mi)" : "Metric (C, km)");
    lv_label_set_text(s_unitsItems[UNIT_MODE], b);
    lv_label_set_text(s_unitsItems[UNIT_BACK], "Back");
    show_wheel(s_unitsItems, UNIT_COUNT, s_unitsSel);
}


void refresh_range() {
    char b[36];
    snprintf(b, sizeof(b), "Range   %.0f km", (double)host_get_range_km());
    lv_label_set_text(s_rangeItems[RNG_VALUE], b);
    lv_label_set_text(s_rangeItems[RNG_BACK], "Back");
    show_wheel(s_rangeItems, RNG_COUNT, s_rangeSel);
}


void refresh_vol() {
    lv_obj_set_width(s_volFill, (lv_coord_t)(4 + s_vol * (236 - 4) / 100));
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", s_vol);
    lv_label_set_text(s_volPct, buf);
}


// Re-decodes on every entry rather than caching: splash_art_decode()'s target buffer
// is shared with ui_splash_show(), so holding onto a stale lv_img_dsc_t across a boot
// splash redecode would be wrong. Decoding is a one-time-per-visit PNG unpack, cheap.
void refresh_about() {
    static lv_img_dsc_t aboutImg;
    if (splash_art_decode(&aboutImg))
        lv_img_set_src(s_aboutImg, &aboutImg);
    // Built on entry rather than at init: the canvas is 651 KB of PSRAM and this page
    // is visited, not lived on. release() runs when the page closes.
    splash_lines::attach(s_aboutPage);
    splash_lines::setNetwork(s_netInfo);
}

void build_menu_and_brightness_pages() {
    // --- main menu page ---
    s_menu = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_menu);
    lv_obj_set_size(s_menu, SCREEN_W, SCREEN_H); lv_obj_center(s_menu);
    lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_SCROLLABLE);


    for (int i = 0; i < ITEM_COUNT; ++i) {
        s_items[i] = lv_label_create(s_menu);
        lv_label_set_text(s_items[i], ITEM_LABELS[i]);
        // Font, opacity, and position are all set dynamically in refresh_menu() —
        // they depend on distance from the current selection (the wheel effect).
    }
    // --- brightness page ---
    s_bright = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_bright);
    lv_obj_set_size(s_bright, SCREEN_W, SCREEN_H); lv_obj_center(s_bright);
    lv_obj_clear_flag(s_bright, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *blabel = lv_label_create(s_bright);
    lv_label_set_text(blabel, "Brightness");
    lv_obj_set_style_text_color(blabel, C_WHITE, 0);
    lv_obj_set_style_text_font(blabel, &lv_font_montserrat_20, 0);
    lv_obj_align(blabel, LV_ALIGN_CENTER, 0, -70);
    lv_obj_t *track = lv_obj_create(s_bright);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, 240, 18);
    lv_obj_set_style_radius(track, 9, 0);
    lv_obj_set_style_bg_color(track, C_TRACK, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 0);
    s_barFill = lv_obj_create(track);
    lv_obj_remove_style_all(s_barFill);
    lv_obj_set_size(s_barFill, 120, 14);
    lv_obj_set_style_radius(s_barFill, 7, 0);
    lv_obj_set_style_bg_color(s_barFill, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(s_barFill, LV_OPA_COVER, 0);
    lv_obj_align(s_barFill, LV_ALIGN_LEFT_MID, 2, 0);
    s_pct = lv_label_create(s_bright);
    lv_label_set_text(s_pct, "--%");
    lv_obj_set_style_text_color(s_pct, C_WHITE, 0);
    lv_obj_set_style_text_font(s_pct, &lv_font_montserrat_20, 0);
    lv_obj_align(s_pct, LV_ALIGN_CENTER, 0, 50);
    lv_obj_t *bhint = lv_label_create(s_bright);
    lv_label_set_text(bhint, "turn to adjust, push to save");
    lv_obj_set_style_text_color(bhint, C_GREY, 0);
    lv_obj_set_style_text_font(bhint, &lv_font_montserrat_14, 0);
    lv_obj_align(bhint, LV_ALIGN_CENTER, 0, 110);
    reg_hint(bhint);

}

void build_option_pages() {
    // --- display menu page (Screen timeout / Brightness / Back) ---
    s_dspPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_dspPage);
    lv_obj_set_size(s_dspPage, SCREEN_W, SCREEN_H); lv_obj_center(s_dspPage);
    lv_obj_clear_flag(s_dspPage, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *dtitle = lv_label_create(s_dspPage);
        lv_label_set_text(dtitle, "Display");
        lv_obj_set_style_text_color(dtitle, C_DIM, 0);
        lv_obj_set_style_text_font(dtitle, &lv_font_montserrat_16, 0);
        lv_obj_align(dtitle, LV_ALIGN_CENTER, 0, -110);
        reg_hint(dtitle);
        for (int i = 0; i < DSP_COUNT; ++i) {
            s_dspItems[i] = lv_label_create(s_dspPage);
            lv_label_set_text(s_dspItems[i], "");
            // Font, opacity, position: show_wheel(), called from refresh_display().
        }
        lv_obj_t *dhint = lv_label_create(s_dspPage);
        lv_label_set_text(dhint, "turn to choose, push to select");
        lv_obj_set_style_text_color(dhint, C_GREY, 0);
        lv_obj_set_style_text_font(dhint, &lv_font_montserrat_14, 0);
        lv_obj_align(dhint, LV_ALIGN_CENTER, 0, 150);
        reg_hint(dhint);
    }

    // --- sound menu page (Radar / Chime / Volume / Back) ---
    s_sndPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_sndPage);
    lv_obj_set_size(s_sndPage, SCREEN_W, SCREEN_H); lv_obj_center(s_sndPage);
    lv_obj_clear_flag(s_sndPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sndtitle = lv_label_create(s_sndPage);
    lv_label_set_text(sndtitle, "Sound");
    lv_obj_set_style_text_color(sndtitle, C_DIM, 0);
    lv_obj_set_style_text_font(sndtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(sndtitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(sndtitle);
    for (int i = 0; i < SND_COUNT; ++i) {
        s_sndItems[i] = lv_label_create(s_sndPage);
        lv_label_set_text(s_sndItems[i], "");
        // Font, opacity, position: show_wheel(), called from refresh_sound().
    }
    lv_obj_t *sndhint = lv_label_create(s_sndPage);
    lv_label_set_text(sndhint, "turn to choose, push to toggle");
    lv_obj_set_style_text_color(sndhint, C_GREY, 0);
    lv_obj_set_style_text_font(sndhint, &lv_font_montserrat_14, 0);
    lv_obj_align(sndhint, LV_ALIGN_CENTER, 0, 150);

    // --- chime picker page (Sound > Chime sound) ---
    s_chimeSelPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_chimeSelPage);
    lv_obj_set_size(s_chimeSelPage, SCREEN_W, SCREEN_H); lv_obj_center(s_chimeSelPage);
    lv_obj_clear_flag(s_chimeSelPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *chimetitle = lv_label_create(s_chimeSelPage);
    lv_label_set_text(chimetitle, "Chime sound");
    lv_obj_set_style_text_color(chimetitle, C_DIM, 0);
    lv_obj_set_style_text_font(chimetitle, &lv_font_montserrat_16, 0);
    lv_obj_align(chimetitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(chimetitle);
    for (int i = 0; i < CHIME_UI_MAX + 1; ++i) {
        s_chimeSelItems[i] = lv_label_create(s_chimeSelPage);
        lv_label_set_text(s_chimeSelItems[i], "");
        // Font, opacity, position: show_wheel(), called from refresh_chimeSelect().
    }
    lv_obj_t *chimehint = lv_label_create(s_chimeSelPage);
    lv_label_set_text(chimehint, "turn to preview, push to select");
    lv_obj_set_style_text_color(chimehint, C_GREY, 0);
    lv_obj_set_style_text_font(chimehint, &lv_font_montserrat_14, 0);
    lv_obj_align(chimehint, LV_ALIGN_CENTER, 0, 150);
    reg_hint(chimehint);

    // --- design picker page (top-level Design item) — sized for
    // theme_select::MAX_THEMES installed slugs + Back. Labels are set in refresh_designSelect().
    s_designPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_designPage);
    lv_obj_set_size(s_designPage, SCREEN_W, SCREEN_H); lv_obj_center(s_designPage);
    lv_obj_clear_flag(s_designPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *designtitle = lv_label_create(s_designPage);
    lv_label_set_text(designtitle, "Theme");
    lv_obj_set_style_text_color(designtitle, C_DIM, 0);
    lv_obj_set_style_text_font(designtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(designtitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(designtitle);
    for (int i = 0; i < theme_select::MAX_THEMES + 1; ++i) {
        s_designItems[i] = lv_label_create(s_designPage);
        lv_label_set_text(s_designItems[i], "");
        // Font, opacity, position: show_wheel(), called from refresh_designSelect().
    }
    lv_obj_t *designhint = lv_label_create(s_designPage);
    lv_label_set_text(designhint, "turn to browse, push to select");
    lv_obj_set_style_text_color(designhint, C_GREY, 0);
    lv_obj_set_style_text_font(designhint, &lv_font_montserrat_14, 0);
    lv_obj_align(designhint, LV_ALIGN_CENTER, 0, 150);
    reg_hint(designhint);

    // --- design restart notice (Design > pick one) ---
    // Reuses the picker's own background/ink so it reads as one continuous flow (pick ->
    // notice -> reboot) instead of a jarring color flash right before the screen blanks.
    s_designNoticePage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_designNoticePage);
    lv_obj_set_size(s_designNoticePage, SCREEN_W, SCREEN_H); lv_obj_center(s_designNoticePage);
    lv_obj_set_style_bg_color(s_designNoticePage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_designNoticePage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_designNoticePage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *designNoticeMsg = lv_label_create(s_designNoticePage);
    lv_label_set_text(designNoticeMsg, "The Orb will now\nrestart under the\nnew design.");
    lv_obj_set_style_text_align(designNoticeMsg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(designNoticeMsg, C_WHITE, 0);
    lv_obj_set_style_text_font(designNoticeMsg, &lv_font_montserrat_20, 0);
    lv_obj_center(designNoticeMsg);

    // --- range menu page (Flight Tracker display range cycle / Back) ---
    s_rangePage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_rangePage);
    lv_obj_set_size(s_rangePage, SCREEN_W, SCREEN_H); lv_obj_center(s_rangePage);
    lv_obj_clear_flag(s_rangePage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *rangetitle = lv_label_create(s_rangePage);
    lv_label_set_text(rangetitle, "Range");
    lv_obj_set_style_text_color(rangetitle, C_DIM, 0);
    lv_obj_set_style_text_font(rangetitle, &lv_font_montserrat_16, 0);
    lv_obj_align(rangetitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(rangetitle);
    for (int i = 0; i < RNG_COUNT; ++i) {
        s_rangeItems[i] = lv_label_create(s_rangePage);
        lv_label_set_text(s_rangeItems[i], "");
        // Font, opacity, position: show_wheel(), called from refresh_range().
    }
    lv_obj_t *rangehint = lv_label_create(s_rangePage);
    lv_label_set_text(rangehint, "push to cycle how far the scope sees");
    lv_obj_set_style_text_color(rangehint, C_GREY, 0);
    lv_obj_set_style_text_font(rangehint, &lv_font_montserrat_14, 0);
    lv_obj_align(rangehint, LV_ALIGN_CENTER, 0, 122);

    // --- units menu page (Auto/Metric/Imperial cycle / Back) ---
    s_unitsPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_unitsPage);
    lv_obj_set_size(s_unitsPage, SCREEN_W, SCREEN_H); lv_obj_center(s_unitsPage);
    lv_obj_clear_flag(s_unitsPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *unitstitle = lv_label_create(s_unitsPage);
    lv_label_set_text(unitstitle, "Units");
    lv_obj_set_style_text_color(unitstitle, C_DIM, 0);
    lv_obj_set_style_text_font(unitstitle, &lv_font_montserrat_16, 0);
    lv_obj_align(unitstitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(unitstitle);
    for (int i = 0; i < UNIT_COUNT; ++i) {
        s_unitsItems[i] = lv_label_create(s_unitsPage);
        lv_label_set_text(s_unitsItems[i], "");
        // Font, opacity, position: show_wheel(), called from refresh_units().
    }
    lv_obj_t *unitshint = lv_label_create(s_unitsPage);
    lv_label_set_text(unitshint, "push to cycle Auto / Metric / Imperial");
    lv_obj_set_style_text_color(unitshint, C_GREY, 0);
    lv_obj_set_style_text_font(unitshint, &lv_font_montserrat_14, 0);
    lv_obj_align(unitshint, LV_ALIGN_CENTER, 0, 150);

    // --- volume page ---
    s_volPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_volPage);
    lv_obj_set_size(s_volPage, SCREEN_W, SCREEN_H); lv_obj_center(s_volPage);
    lv_obj_clear_flag(s_volPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *vlabel = lv_label_create(s_volPage);
    lv_label_set_text(vlabel, "Volume");
    lv_obj_set_style_text_color(vlabel, C_WHITE, 0);
    lv_obj_set_style_text_font(vlabel, &lv_font_montserrat_20, 0);
    lv_obj_align(vlabel, LV_ALIGN_CENTER, 0, -70);
    lv_obj_t *vtrack = lv_obj_create(s_volPage);
    lv_obj_remove_style_all(vtrack);
    lv_obj_set_size(vtrack, 240, 18);
    lv_obj_set_style_radius(vtrack, 9, 0);
    lv_obj_set_style_bg_color(vtrack, C_TRACK, 0);
    lv_obj_set_style_bg_opa(vtrack, LV_OPA_COVER, 0);
    lv_obj_clear_flag(vtrack, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(vtrack, LV_ALIGN_CENTER, 0, 0);
    s_volFill = lv_obj_create(vtrack);
    lv_obj_remove_style_all(s_volFill);
    lv_obj_set_size(s_volFill, 120, 14);
    lv_obj_set_style_radius(s_volFill, 7, 0);
    lv_obj_set_style_bg_color(s_volFill, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(s_volFill, LV_OPA_COVER, 0);
    lv_obj_align(s_volFill, LV_ALIGN_LEFT_MID, 2, 0);
    s_volPct = lv_label_create(s_volPage);
    lv_label_set_text(s_volPct, "--%");
    lv_obj_set_style_text_color(s_volPct, C_WHITE, 0);
    lv_obj_set_style_text_font(s_volPct, &lv_font_montserrat_20, 0);
    lv_obj_align(s_volPct, LV_ALIGN_CENTER, 0, 50);
    lv_obj_t *vhint = lv_label_create(s_volPage);
    lv_label_set_text(vhint, "turn to adjust, push to test");
    lv_obj_set_style_text_color(vhint, C_GREY, 0);
    lv_obj_set_style_text_font(vhint, &lv_font_montserrat_14, 0);
    lv_obj_align(vhint, LV_ALIGN_CENTER, 0, 110);

    // --- About page: the boot splash image, push anywhere to return ---
    s_aboutPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_aboutPage);
    // Explicit opaque backing, same defensive reasoning as s_resetPage below:
    // this page is meant to be a full-screen splash with nothing else visible,
    // unlike every other sub-page (see the comment above s_screen's own bg),
    // which deliberately stay transparent so the shared Settings backdrop
    // shows through. Without this, the persistent "SETTINGS" header label
    // (drawn directly on s_screen, never hidden by show_page()) showed
    // through whenever the splash image didn't land as a perfectly opaque
    // pixel-for-pixel 466x466 cover.
    lv_obj_set_style_bg_color(s_aboutPage, C_BG, 0);
    lv_obj_set_style_bg_opa(s_aboutPage, LV_OPA_COVER, 0);
    lv_obj_set_size(s_aboutPage, SCREEN_W, SCREEN_H); lv_obj_center(s_aboutPage);
    lv_obj_clear_flag(s_aboutPage, LV_OBJ_FLAG_SCROLLABLE);
    s_aboutImg = lv_img_create(s_aboutPage);
    lv_obj_center(s_aboutImg);

    // The version, the config address and the data credits are drawn by splash_lines on
    // entry, from splash_style.json. They were three labels nailed here at CENTER +120,
    // +152 and +186; the compiled defaults in theme_style::Splash are those same offsets,
    // so a theme that says nothing about them is unchanged.

    // --- Reset confirm: warning + push-to-confirm/turn-to-cancel, same red as the
    // other destructive-action warning (main.cpp's g_holdWarning) ---
    s_resetPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_resetPage);
    lv_obj_set_size(s_resetPage, SCREEN_W, SCREEN_H); lv_obj_center(s_resetPage);
    lv_obj_set_style_bg_color(s_resetPage, lv_color_hex(0x3A0000), 0);
    lv_obj_set_style_bg_opa(s_resetPage, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_resetPage, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *warn = lv_label_create(s_resetPage);
        lv_label_set_text(warn, "This erases Wi-Fi and\nall saved settings.");
        lv_obj_set_style_text_color(warn, lv_color_white(), 0);
        lv_obj_set_style_text_font(warn, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(warn, LV_ALIGN_CENTER, 0, -30);

        lv_obj_t *action = lv_label_create(s_resetPage);
        lv_label_set_text(action, "push to confirm\nturn to cancel");
        lv_obj_set_style_text_color(action, lv_color_hex(0xFFB2B2), 0);
        lv_obj_set_style_text_font(action, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(action, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(action, LV_ALIGN_CENTER, 0, 40);
    }

}

}  // namespace settings_impl

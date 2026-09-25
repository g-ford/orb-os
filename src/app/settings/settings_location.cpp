// The Location pages: the menu, recent cities, and the city search with its character strip.
// Split out of settings_view.cpp; the shared modes, constants and state are in settings_internal.h.
#include "settings_internal.h"

namespace settings_impl {


void refresh_locmenu() {
    // The centre point first, so somebody arriving to check their location can read the
    // answer without selecting anything. This is the receipt for CUT-18 - the location
    // you set is the location it uses - and it used to be legible only on the splash,
    // for three seconds, at the bottom of a crowded dial.
    if (s_lmCoords) lv_label_set_text(s_lmCoords, s_homeCoords[0] ? s_homeCoords : "location not set");
    show_wheel(s_lmItems, LM_COUNT, s_lmSel);
}


void refresh_recent() {
    if (s_recCount == 0) {
        lv_label_set_text(s_recName, "No recent cities");
        lv_label_set_text(s_recCoord, "search to add one");
    } else if (s_recSel < s_recCount) {
        lv_label_set_text(s_recName, s_recNames[s_recSel]);
        char b[32]; snprintf(b, sizeof(b), "%.2f, %.2f", s_recLat[s_recSel], s_recLon[s_recSel]);
        lv_label_set_text(s_recCoord, b);
    } else {
        lv_label_set_text(s_recName, "Back");
        lv_label_set_text(s_recCoord, "");
    }
}


void load_recents() {
    s_recCount = host_recents_get(s_recNames, s_recLat, s_recLon, RECENTS_MAX);
    s_recSel   = 0;
}


void refresh_search() {
    lv_label_set_text(s_srchText, s_str[0] ? s_str : "type a city name");
    const bool inKeys = (s_kbIdx < N_KEYS);
    const int  center = inKeys ? s_kbIdx : N_KEYS - 1;
    for (int k = 0; k < 7; ++k) {
        const int idx = center - 3 + k;
        char c[2] = { 0, 0 };
        if (idx >= 0 && idx < N_KEYS) c[0] = KEYS[idx];
        lv_label_set_text(s_strip[k], c);
        const bool hot = inKeys && (k == 3);
        lv_obj_set_style_text_color(s_strip[k], hot ? look().selColor : look().itemColor, 0);
        lv_obj_set_style_text_font(s_strip[k], hot ? &lv_font_montserrat_28 : &lv_font_montserrat_20, 0);
    }
    if (s_searching) {
        lv_label_set_text(s_sug[0], "searching...");
        lv_obj_set_style_text_color(s_sug[0], C_GREY, 0);
        for (int j = 1; j < 4; ++j) lv_label_set_text(s_sug[j], "");
    } else {
        for (int j = 0; j < 4; ++j) {
            if (j < s_sugCount) {
                lv_label_set_text(s_sug[j], s_sugName[j]);
                const bool hot = !inKeys && (s_kbIdx - N_KEYS == j);
                lv_obj_set_style_text_color(s_sug[j], hot ? C_ACCENT : C_DIM, 0);
            } else lv_label_set_text(s_sug[j], "");
        }
    }
}


void search_tick(lv_timer_t * /*t*/) {
    if (s_mode != MODE_SEARCH) return;
    if (s_searching) {                        // "searching" is already painted; do the blocking fetch now
        s_searching = false;
        s_sugCount = host_geocode(s_str, s_sugName, s_sugLat, s_sugLon, 4);
        const int total = N_KEYS + s_sugCount;
        if (s_kbIdx >= total) s_kbIdx = total - 1;
        refresh_search();
        return;
    }
    if (!s_pending) return;
    if (--s_countdown > 0) return;
    s_pending = false;
    if (strlen(s_str) < 2) { s_sugCount = 0; refresh_search(); return; }
    s_searching = true; refresh_search();     // paints "searching"; fetch fires next tick
}

void build_location_menu() {
    // --- location menu page (Current / Search / Recent / Back) ---
    s_lmPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_lmPage);
    lv_obj_set_size(s_lmPage, SCREEN_W, SCREEN_H); lv_obj_center(s_lmPage);
    lv_obj_clear_flag(s_lmPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lmtitle = lv_label_create(s_lmPage);
    lv_label_set_text(lmtitle, "Location");
    lv_obj_set_style_text_color(lmtitle, C_DIM, 0);
    lv_obj_set_style_text_font(lmtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(lmtitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(lmtitle);
    // Directly under the title and above the wheel, in the dim ink the other secondary
    // readouts use. Text is set in refresh_locmenu(), which runs on every entry.
    s_lmCoords = lv_label_create(s_lmPage);
    lv_label_set_text(s_lmCoords, "");
    lv_obj_set_style_text_color(s_lmCoords, C_GREY, 0);
    lv_obj_set_style_text_font(s_lmCoords, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_lmCoords, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lmCoords, LV_ALIGN_CENTER, 0, -100);
    for (int i = 0; i < LM_COUNT; ++i) {
        s_lmItems[i] = lv_label_create(s_lmPage);
        lv_label_set_text(s_lmItems[i], LM_LABELS[i]);
        // Font, opacity, position: show_wheel(), called from refresh_locmenu().
    }
    lv_obj_t *lmhint = lv_label_create(s_lmPage);
    lv_label_set_text(lmhint, "turn to choose, push to select");
    lv_obj_set_style_text_color(lmhint, C_GREY, 0);
    lv_obj_set_style_text_font(lmhint, &lv_font_montserrat_14, 0);
    lv_obj_align(lmhint, LV_ALIGN_CENTER, 0, 150);
    reg_hint(lmhint);

}

void build_recent_and_search() {
    // --- recent cities page (single-item scroller) ---
    s_recPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_recPage);
    lv_obj_set_size(s_recPage, SCREEN_W, SCREEN_H); lv_obj_center(s_recPage);
    lv_obj_clear_flag(s_recPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *rtitle = lv_label_create(s_recPage);
    lv_label_set_text(rtitle, "Recent cities");
    lv_obj_set_style_text_color(rtitle, C_DIM, 0);
    lv_obj_set_style_text_font(rtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(rtitle, LV_ALIGN_CENTER, 0, -70);
    reg_hint(rtitle);
    s_recName = lv_label_create(s_recPage);
    lv_label_set_text(s_recName, "");
    lv_obj_set_style_text_color(s_recName, C_WHITE, 0);
    lv_obj_set_style_text_font(s_recName, &lv_font_montserrat_20, 0);
    lv_obj_align(s_recName, LV_ALIGN_CENTER, 0, -14);
    s_recCoord = lv_label_create(s_recPage);
    lv_label_set_text(s_recCoord, "");
    lv_obj_set_style_text_color(s_recCoord, C_GREY, 0);
    lv_obj_set_style_text_font(s_recCoord, &lv_font_montserrat_14, 0);
    lv_obj_align(s_recCoord, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t *rhint = lv_label_create(s_recPage);
    lv_label_set_text(rhint, "turn to choose, push to set");
    lv_obj_set_style_text_color(rhint, C_GREY, 0);
    lv_obj_set_style_text_font(rhint, &lv_font_montserrat_14, 0);
    lv_obj_align(rhint, LV_ALIGN_CENTER, 0, 110);
    reg_hint(rhint);

    // --- search page ---
    s_srchPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_srchPage);
    lv_obj_set_size(s_srchPage, SCREEN_W, SCREEN_H); lv_obj_center(s_srchPage);
    lv_obj_clear_flag(s_srchPage, LV_OBJ_FLAG_SCROLLABLE);
    s_srchText = lv_label_create(s_srchPage);
    lv_label_set_text(s_srchText, "type a city name");
    lv_obj_set_style_text_color(s_srchText, C_WHITE, 0);
    lv_obj_set_style_text_font(s_srchText, &lv_font_montserrat_20, 0);
    lv_obj_align(s_srchText, LV_ALIGN_CENTER, 0, -135);
    for (int k = 0; k < 7; ++k) {
        s_strip[k] = lv_label_create(s_srchPage);
        lv_label_set_text(s_strip[k], "");
        lv_obj_set_style_text_color(s_strip[k], C_GREY, 0);
        lv_obj_set_style_text_font(s_strip[k], &lv_font_montserrat_20, 0);
        lv_obj_align(s_strip[k], LV_ALIGN_CENTER, (k - 3) * 48, -75);
    }
    for (int j = 0; j < 4; ++j) {
        s_sug[j] = lv_label_create(s_srchPage);
        lv_label_set_text(s_sug[j], "");
        lv_obj_set_style_text_color(s_sug[j], C_DIM, 0);
        lv_obj_set_style_text_font(s_sug[j], &lv_font_montserrat_16, 0);
        lv_obj_align(s_sug[j], LV_ALIGN_CENTER, 0, -5 + j * 34);
    }
    lv_obj_t *shint = lv_label_create(s_srchPage);
    lv_label_set_text(shint, "turn to letters then cities");
    lv_obj_set_style_text_color(shint, C_GREY, 0);
    lv_obj_set_style_text_font(shint, &lv_font_montserrat_14, 0);
    lv_obj_align(shint, LV_ALIGN_CENTER, 0, 150);

}

}  // namespace settings_impl

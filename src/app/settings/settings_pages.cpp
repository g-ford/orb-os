// The simple Settings pages: the main wheel, Location, Chime picker, Theme picker, About and Reset.
// Split out of settings_view.cpp; the shared modes, constants and state are in settings_internal.h.
#include "settings_internal.h"

namespace settings_impl {


int top_item_count() {
    int n = ITEM_HEAD_COUNT + (int)settings_registry::count() + ITEM_TAIL_COUNT + 1;   // +1 for Back
    return n > MAX_WHEEL_ROWS ? MAX_WHEEL_ROWS : n;   // fail safe, never overflow s_items[]
}

int top_back_index() { return top_item_count() - 1; }

// The tail (About, Reset) sits right after however many groups are registered, so its
// position is a function of the group count, not a fixed constant like the head's.
int item_about() { return ITEM_HEAD_COUNT + (int)settings_registry::count(); }
int item_reset() { return item_about() + 1; }

const char *top_item_label(int i) {
    if (i < ITEM_HEAD_COUNT) return ITEM_HEAD_LABELS[i];
    int g = i - ITEM_HEAD_COUNT;
    const int groups = (int)settings_registry::count();
    if (g < groups) return settings_registry::group((size_t)g).label;
    g -= groups;
    if (g < ITEM_TAIL_COUNT) return ITEM_TAIL_LABELS[g];
    return "Back";
}

// One extra, non-descriptor row some groups carry: a link into a bespoke sub-page for a
// setting that can't be a plain SettingDescriptor. Identified by comparing the group's
// `items` pointer -- a stable identity regardless of what a theme renames the owning app's
// label to (settings_registry::Group::label IS that theme-chosen name, e.g.
// theme_style::names().clock, so matching on it would be fragile in exactly the way this
// codebase already has a name for).
bool group_has_extra_row(const settings::SettingDescriptor *items) {
    return items == kClockSettings || items == kSystemSettings;
}

void group_extra_row_text(const settings::SettingDescriptor *items, char *buf, size_t n) {
    if (items == kClockSettings) snprintf(buf, n, "Chime sound: %s", host_chime_name(host_chime_index()));
    else if (items == kSystemSettings) snprintf(buf, n, "Location");
    else if (n) buf[0] = 0;
}

// Same shape as ITEM_CHIME's/ITEM_LOCATION's old top-level press handlers, just reached from
// inside a group's page instead of the main menu.
void group_extra_row_enter(const settings::SettingDescriptor *items) {
    if (items == kClockSettings) {
        s_chimeSel = host_chime_index();
        show_page(MODE_CHIME_SELECT);
        host_chime_preview(s_chimeSel);
    } else if (items == kSystemSettings) {
        s_lmSel = 0;
        show_page(MODE_LOCATION);
    }
}

// s_groupItems is MAX_WHEEL_ROWS long, like every other wheel array here, but a group's
// row count comes from whatever an app passed to app_shell::add() -- settings_registry
// does not bound it, and nothing stops a future app from registering more descriptors
// than fit. Capped the same way top_item_count() caps the main menu: one row is always
// reserved for Back, so the cap leaves MAX_WHEEL_ROWS - 1 for real settings rows. Not
// reachable today (radar registers 12, the largest group, against a cap of MAX_WHEEL_ROWS - 1
// = 31), but every wheel list here fails safe, not just the ones a current caller happens to
// exercise.
int group_item_count() {   // active group's rows + its extra row (if any), capped, + 1 for Back
    const settings_registry::Group &g = settings_registry::group((size_t)s_activeGroup);
    const size_t n = g.count + (group_has_extra_row(g.items) ? 1 : 0);
    const size_t shown = n > (size_t)(MAX_WHEEL_ROWS - 1) ? (size_t)(MAX_WHEEL_ROWS - 1) : n;
    return (int)shown + 1;
}

void refresh_group() {
    const settings_registry::Group &g = settings_registry::group((size_t)s_activeGroup);
    lv_label_set_text(s_groupTitle, g.label);
    const bool extra = group_has_extra_row(g.items);
    const int total = group_item_count();
    const size_t shown = (size_t)(total - 1);            // rows before Back, capped
    const size_t descShown = extra ? shown - 1 : shown;  // the extra row (if it fit) is the last one before Back
    char buf[56];
    char label[48];
    for (size_t i = 0; i < descShown; ++i) {
        const settings::SettingDescriptor &d = g.items[i];
        const int v = settings::display_int(d);   // the effective value, if readLive overrides it
        // d.note is a short aside (e.g. "(restarts the device)") appended to the label before
        // the value, same restored wording as the web card -- see Finding 4,
        // docs/superpowers/specs/2026-10-05-app-settings-registry-design.md. Device screen
        // space is tighter than the web page's, so this is only ever the short notes the
        // descriptors actually carry today.
        if (d.note) snprintf(label, sizeof(label), "%s %s", d.label, d.note);
        else        snprintf(label, sizeof(label), "%s", d.label);
        if (d.control == settings::Control::Toggle) {
            snprintf(buf, sizeof(buf), "%s   %s", label, v ? "On" : "Off");
        } else if (d.control == settings::Control::Enum) {
            snprintf(buf, sizeof(buf), "%s   %s", label, d.optionLabels[v - d.storage.asInt->lo]);
        } else if (d.unitSuffix) {   // Slider, with a unit ("Display range   30 km")
            snprintf(buf, sizeof(buf), "%s   %d %s", label, v, d.unitSuffix);
        } else {                     // Slider, no unit ("Max aircraft   12")
            snprintf(buf, sizeof(buf), "%s   %d", label, v);
        }
        lv_label_set_text(s_groupItems[i], buf);
    }
    if (extra) {
        group_extra_row_text(g.items, buf, sizeof(buf));
        lv_label_set_text(s_groupItems[descShown], buf);
    }
    lv_label_set_text(s_groupItems[shown], "Back");
    show_wheel(s_groupItems, total, s_groupSel);
}


// Chime picker: turning previews each chime live (host_chime_preview), pressing
// confirms it (host_chime_set) and returns to the app switcher. Sized for CHIME_UI_MAX
// chimes though only one ("Westminster") exists today.
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


    // All MAX_WHEEL_ROWS objects, not just top_item_count() of them: settings_registry
    // can still be growing when this runs (app_shell::add() calls after this one register
    // more groups -- the native sim's app list does exactly that), so the row COUNT at
    // build time cannot be trusted to size this array. refresh_menu() sets each row's
    // text fresh from top_item_label() every time, the same way every other refresh_*()
    // here re-labels its own fixed-size page, so a group registered after this point still
    // gets its row text the first time Settings is actually entered.
    for (int i = 0; i < MAX_WHEEL_ROWS; ++i) {
        s_items[i] = lv_label_create(s_menu);
        lv_label_set_text(s_items[i], "");
        // Font, opacity, and position are all set dynamically in refresh_menu() —
        // they depend on distance from the current selection (the wheel effect).
    }
}

void build_group_page() {
    s_groupPage = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_groupPage);
    lv_obj_set_size(s_groupPage, SCREEN_W, SCREEN_H); lv_obj_center(s_groupPage);
    lv_obj_clear_flag(s_groupPage, LV_OBJ_FLAG_SCROLLABLE);
    // Same title styling as every other option page (see the Range/Units titles in
    // build_option_pages()) -- font, colour and position, built once here. This page is
    // reused across however many settings_registry groups are registered, so unlike those
    // pages' titles (set once to a fixed string at build time) this one's TEXT is set fresh
    // in refresh_group(), from the active group's own label ("Flight Tracker" today).
    s_groupTitle = lv_label_create(s_groupPage);
    lv_label_set_text(s_groupTitle, "");
    lv_obj_set_style_text_color(s_groupTitle, C_DIM, 0);
    lv_obj_set_style_text_font(s_groupTitle, &lv_font_montserrat_16, 0);
    lv_obj_align(s_groupTitle, LV_ALIGN_CENTER, 0, -122);
    reg_hint(s_groupTitle);
    for (int i = 0; i < MAX_WHEEL_ROWS; ++i) {
        s_groupItems[i] = lv_label_create(s_groupPage);
        lv_label_set_text(s_groupItems[i], "");
        // Font, opacity and position: show_wheel(), called from refresh_group().
    }
}

void build_option_pages() {
    build_group_page();

    // --- chime picker page (Clock's "Chime sound" row) ---
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

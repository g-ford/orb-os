#pragma once
// The nine-role palette every screen draws its chrome from. It is read off the active theme's colour roles
// (theme_roles.h), or the built-in night-vision set when no theme is selected or a theme has no palette of its own.
// There used to be a second, compiled palette here with two whole-device skins, Default and Office, chosen in
// Settings and stored under the NVS key "appTheme". Both are gone. The key is left in place on any Orb that wrote it
// and is never read: the namespace is permanent.
#include <lvgl.h>

struct AppPalette {
    lv_color_t bg;         // screen background
    lv_color_t panel;      // cards, tracks, the boot-splash halo
    lv_color_t highlight;  // selected menu row / active control fill
    lv_color_t ink;        // primary text
    lv_color_t soft;       // secondary text / labels
    lv_color_t dim;        // hints, disabled, least-emphasis text
    lv_color_t accent;     // clock hand, links, active state, radar blips
    lv_color_t hairline;   // list dividers
    lv_color_t onAccent;   // text drawn on top of an accent-filled control
};

namespace app_theme {

const AppPalette &palette();        // active palette (by const ref: no copies in draw callbacks)

} // namespace app_theme

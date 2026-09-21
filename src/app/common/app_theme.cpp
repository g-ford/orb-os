#include "app_theme.h"
#include "theme_style.h"

namespace app_theme {

// Everything reads the theme's roles (the built-in ones when the theme has no palette), so a themed Orb's menu,
// splash and Settings share its colours.
const AppPalette &palette() {
    static AppPalette s;
    const theme_roles::Palette &r = theme_style::palette();
    s.bg        = lv_color_hex(r.v[theme_roles::R_bg]);
    s.panel     = lv_color_hex(r.v[theme_roles::R_panel]);
    s.highlight = lv_color_hex(r.v[theme_roles::R_highlight]);
    s.ink       = lv_color_hex(r.v[theme_roles::R_text]);
    s.soft      = lv_color_hex(r.v[theme_roles::R_secondary]);
    s.dim       = lv_color_hex(r.v[theme_roles::R_dim]);
    s.accent    = lv_color_hex(r.v[theme_roles::R_primary]);
    s.hairline  = lv_color_hex(r.v[theme_roles::R_hairline]);
    s.onAccent  = lv_color_hex(r.v[theme_roles::R_onPrimary]);
    return s;
}

} // namespace app_theme

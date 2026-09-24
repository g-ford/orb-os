#include "wheel_look.h"
#include "theme_style.h"
#include "theme_font.h"
#include "theme_roles.h"

namespace wheel_look {

wheel::Look themed() {
    const theme_roles::Palette &p = theme_style::palette();
    const lv_color_t primary = lv_color_hex(p.v[theme_roles::R_primary]);
    return { primary, lv_color_hex(p.v[theme_roles::R_muted]), primary, wheel::SELECTED_GLOW,
             theme_font::wheel_sel(), theme_font::wheel_item() };
}

wheel::Look system() {
    // White against the grey the setup pages have always used, no glow. The selected row steps up a size so the
    // selection reads without a background behind it; tuned by eye on the network list, where rows are closest.
    return { lv_color_hex(0xFFFFFF), lv_color_hex(0x6A7078), lv_color_hex(0xFFFFFF), 0,
             &lv_font_montserrat_28, &lv_font_montserrat_24 };
}

} // namespace wheel_look

#pragma once
#include <lvgl.h>
#include "wx_icon_kind.h"

// One weather icon as one LVGL object: it draws itself from a WxIconKind with rectangles and
// lines, so there is no baked art to ship, decode or release, and the same code serves the
// large icon on the Now screen and the small ones on the 7-Day rows.
namespace wx_icon {
    // Square, transparent, not clickable. Draws Cloudy until set() says otherwise.
    lv_obj_t *create(lv_obj_t *parent, int sizePx);
    void set(lv_obj_t *icon, WxIconKind kind);
}

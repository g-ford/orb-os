#pragma once
#include <lvgl.h>
#include "weather.h"

// The Now and 7-Day screens of the Weather app. They are built onto the weather panel that
// ui.cpp already owns (the radar lives on the same panel) and shown or hidden by mode.
// Nothing here fetches anything: ui.cpp hands it the snapshot.
namespace wx_screens {
    struct Style {
        lv_color_t ink;      // main text
        lv_color_t soft;     // secondary text
        lv_color_t dim;      // captions
        lv_color_t accent;   // day names
        lv_color_t rain;     // rain chance
    };
    enum class Screen { None, Now, Week };

    // Once, at UI construction. Everything starts hidden.
    void build(lv_obj_t *panel, const Style &st);
    // `w` is null when there is no forecast yet, and then `emptyLine` says which thing is
    // unwell (weather_status_text). `imperial` picks F / mph.
    void refresh(const WeatherSnapshot *w, bool imperial, const char *emptyLine);
    void show(Screen s);
}

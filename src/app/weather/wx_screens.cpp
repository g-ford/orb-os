#include "wx_screens.h"
#include "wx_icon.h"
#include <stdio.h>

namespace {

// The weather panel is 462 px round, centred on a 466 px screen.
constexpr int PANEL_PX = 462;

// Now. Stacked from the top; the round edge only bites at the very top and bottom, and the
// narrowest line (the updated stamp, y 384) still has a 350 px chord.
constexpr int NOW_ICON_PX  = 128;
constexpr int NOW_ICON_Y   = 58;
constexpr int NOW_TEMP_Y   = 194;
constexpr int NOW_COND_Y   = 262;
constexpr int NOW_DETAIL_Y = 302;
constexpr int NOW_WIND_Y   = 328;
constexpr int NOW_STAMP_Y  = 384;
constexpr int NOW_TEXT_W   = 380;

// 7-Day. Seven rows in a column narrow enough for the top row's chord: at y 58 the circle is
// 306 px wide, so a 282 px column leaves a margin. One width for every row keeps the column
// straight; the middle rows are simply narrower than the circle they sit in.
constexpr int WEEK_TOP_Y   = 58;
constexpr int WEEK_ROW_H   = 46;
constexpr int WEEK_COL_W   = 282;
constexpr int WEEK_ICON_PX = 34;
constexpr int WEEK_DAY_X   = 0;
constexpr int WEEK_ICON_X  = 62;
constexpr int WEEK_TEMPS_X = 108;
constexpr int WEEK_RAIN_W  = 56;

wx_screens::Style s_st;
lv_obj_t *s_nowRoot = nullptr, *s_weekRoot = nullptr;
lv_obj_t *s_nowIcon = nullptr, *s_nowTempRow = nullptr, *s_nowTemp = nullptr, *s_nowUnit = nullptr;
lv_obj_t *s_nowCond = nullptr, *s_nowDetail = nullptr, *s_nowWind = nullptr, *s_nowStamp = nullptr, *s_nowEmpty = nullptr;

struct Row { lv_obj_t *box, *day, *icon, *temps, *rain; };
Row s_row[WEATHER_DAYS] = {};
lv_obj_t *s_weekEmpty = nullptr;

lv_obj_t *make_root(lv_obj_t *panel) {
    lv_obj_t *o = lv_obj_create(panel);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, PANEL_PX, PANEL_PX);
    lv_obj_center(o);
    lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t col, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, "");
    return l;
}

void set_hidden(lv_obj_t *o, bool hidden) {
    if (!o) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void wx_screens::build(lv_obj_t *panel, const Style &st) {
    s_st = st;

    // ---- Now ----
    s_nowRoot = make_root(panel);
    s_nowIcon = wx_icon::create(s_nowRoot, NOW_ICON_PX);
    lv_obj_align(s_nowIcon, LV_ALIGN_TOP_MID, 0, NOW_ICON_Y);

    // The temperature and its unit are two labels in a flex row so the pair stays centred
    // whatever the number's width, and the unit can be smaller and sit at the top of the digits.
    s_nowTempRow = lv_obj_create(s_nowRoot);
    lv_obj_remove_style_all(s_nowTempRow);
    lv_obj_set_size(s_nowTempRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_nowTempRow, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_flex_flow(s_nowTempRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_nowTempRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(s_nowTempRow, 6, 0);
    lv_obj_align(s_nowTempRow, LV_ALIGN_TOP_MID, 0, NOW_TEMP_Y);
    s_nowTemp = make_label(s_nowTempRow, &lv_font_montserrat_48, s_st.ink, LV_TEXT_ALIGN_CENTER);
    s_nowUnit = make_label(s_nowTempRow, &lv_font_montserrat_28, s_st.soft, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_pad_top(s_nowUnit, 4, 0);

    s_nowCond = make_label(s_nowRoot, &lv_font_montserrat_20, s_st.ink, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowCond, NOW_TEXT_W);
    lv_obj_align(s_nowCond, LV_ALIGN_TOP_MID, 0, NOW_COND_Y);
    s_nowDetail = make_label(s_nowRoot, &lv_font_montserrat_16, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowDetail, NOW_TEXT_W);
    lv_obj_align(s_nowDetail, LV_ALIGN_TOP_MID, 0, NOW_DETAIL_Y);
    s_nowWind = make_label(s_nowRoot, &lv_font_montserrat_16, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowWind, NOW_TEXT_W);
    lv_obj_align(s_nowWind, LV_ALIGN_TOP_MID, 0, NOW_WIND_Y);
    s_nowStamp = make_label(s_nowRoot, &lv_font_montserrat_12, s_st.dim, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowStamp, NOW_TEXT_W);
    lv_obj_align(s_nowStamp, LV_ALIGN_TOP_MID, 0, NOW_STAMP_Y);

    s_nowEmpty = make_label(s_nowRoot, &lv_font_montserrat_20, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_nowEmpty, NOW_TEXT_W - 60);
    lv_label_set_long_mode(s_nowEmpty, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_nowEmpty, LV_ALIGN_CENTER, 0, 0);

    // ---- 7-Day ----
    s_weekRoot = make_root(panel);
    for (int i = 0; i < WEATHER_DAYS; ++i) {
        Row &r = s_row[i];
        r.box = lv_obj_create(s_weekRoot);
        lv_obj_remove_style_all(r.box);
        lv_obj_set_size(r.box, WEEK_COL_W, WEEK_ROW_H - 2);
        lv_obj_clear_flag(r.box, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        lv_obj_align(r.box, LV_ALIGN_TOP_MID, 0, WEEK_TOP_Y + i * WEEK_ROW_H);
        lv_obj_set_style_border_side(r.box, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(r.box, 1, 0);
        lv_obj_set_style_border_color(r.box, s_st.dim, 0);
        lv_obj_set_style_border_opa(r.box, LV_OPA_30, 0);

        r.day = make_label(r.box, &lv_font_montserrat_16, s_st.accent, LV_TEXT_ALIGN_LEFT);
        lv_obj_align(r.day, LV_ALIGN_LEFT_MID, WEEK_DAY_X, 0);
        r.icon = wx_icon::create(r.box, WEEK_ICON_PX);
        lv_obj_align(r.icon, LV_ALIGN_LEFT_MID, WEEK_ICON_X, 0);
        // The high is what a person scans for, so the temperatures are the largest thing in the
        // row, and the low is dimmed by an inline colour code instead of a second label.
        r.temps = make_label(r.box, &lv_font_montserrat_20, s_st.ink, LV_TEXT_ALIGN_LEFT);
        lv_label_set_recolor(r.temps, true);
        lv_obj_align(r.temps, LV_ALIGN_LEFT_MID, WEEK_TEMPS_X, 0);
        r.rain = make_label(r.box, &lv_font_montserrat_16, s_st.rain, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_width(r.rain, WEEK_RAIN_W);
        lv_obj_align(r.rain, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    s_weekEmpty = make_label(s_weekRoot, &lv_font_montserrat_20, s_st.soft, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(s_weekEmpty, NOW_TEXT_W - 60);
    lv_label_set_long_mode(s_weekEmpty, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_weekEmpty, LV_ALIGN_CENTER, 0, 0);
}

void wx_screens::refresh(const WeatherSnapshot *w, bool imperial, const char *emptyLine) {
    if (!s_nowRoot || !s_weekRoot) return;
    const bool have = w && w->valid;

    // ---- Now ----
    lv_obj_t *const nowBits[] = { s_nowIcon, s_nowTempRow, s_nowCond, s_nowDetail, s_nowWind, s_nowStamp };
    for (lv_obj_t *o : nowBits) set_hidden(o, !have);
    set_hidden(s_nowEmpty, have);
    if (have) {
        char buf[64];
        wx_icon::set(s_nowIcon, wx_icon_classify(w->code, w->isDay));
        snprintf(buf, sizeof buf, "%d", weather_round(weather_temp_to(w->tempC, imperial)));
        lv_label_set_text(s_nowTemp, buf);
        lv_label_set_text(s_nowUnit, weather_temp_unit_name(imperial));
        lv_label_set_text(s_nowCond, weather_condition(w->code));
        snprintf(buf, sizeof buf, "Feels %d %s     Humidity %d%%",
                 weather_round(weather_temp_to(w->feelsC, imperial)), weather_temp_unit_name(imperial), w->humidity);
        lv_label_set_text(s_nowDetail, buf);
        snprintf(buf, sizeof buf, "Wind %d %s", weather_round(weather_wind_to(w->windKmh, imperial)),
                 weather_wind_unit_name(imperial));
        lv_label_set_text(s_nowWind, buf);
        snprintf(buf, sizeof buf, "Updated %s", w->updated);
        lv_label_set_text(s_nowStamp, buf);
    } else {
        lv_label_set_text(s_nowEmpty, (emptyLine && emptyLine[0]) ? emptyLine : "Waiting for the forecast...");
    }

    // ---- 7-Day ----
    set_hidden(s_weekEmpty, have && w->dayCount > 0);
    if (!have || w->dayCount == 0)
        lv_label_set_text(s_weekEmpty, (emptyLine && emptyLine[0]) ? emptyLine : "No forecast days yet");
    for (int i = 0; i < WEATHER_DAYS; ++i) {
        Row &r = s_row[i];
        const bool shown = have && i < w->dayCount;
        set_hidden(r.box, !shown);
        if (!shown) continue;
        const WeatherDay &d = w->days[i];
        char buf[40];
        lv_label_set_text(r.day, i == 0 ? "Today" : weather_day_name(d.date));
        wx_icon::set(r.icon, wx_icon_classify(d.code, true));   // a day's outlook is a daytime one
        snprintf(buf, sizeof buf, "%d #%06x / %d %s#", weather_round(weather_temp_to(d.tempMaxC, imperial)),
                 (unsigned)(lv_color_to32(s_st.soft) & 0xFFFFFFu),
                 weather_round(weather_temp_to(d.tempMinC, imperial)), weather_temp_unit_name(imperial));
        lv_label_set_text(r.temps, buf);
        snprintf(buf, sizeof buf, "%d%%", d.rainChance);
        lv_label_set_text(r.rain, buf);
    }
}

void wx_screens::show(Screen s) {
    set_hidden(s_nowRoot, s != Screen::Now);
    set_hidden(s_weekRoot, s != Screen::Week);
}

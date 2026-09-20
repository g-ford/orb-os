#pragma once
// Which picture goes with a WMO weather code. Pure, no LVGL, so it can be tested on the host;
// wx_icon.{h,cpp} draws the picture.

enum class WxIconKind : unsigned char {
    ClearDay, ClearNight,
    PartlyDay, PartlyNight,
    Cloudy, Fog, Drizzle, Rain, Showers, Snow, Thunder,
    Count
};

// `wmoCode` is Open-Meteo's weather_code. -1 (not stated) and any code the table does not know
// give Cloudy: a neutral picture, and never the sun.
WxIconKind wx_icon_classify(int wmoCode, bool isDay);
const char *wx_icon_name(WxIconKind k);   // "?" for Count and beyond

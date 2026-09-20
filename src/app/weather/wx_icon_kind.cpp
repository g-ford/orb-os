#include "wx_icon_kind.h"

WxIconKind wx_icon_classify(int code, bool isDay) {
    if (code == 0 || code == 1) return isDay ? WxIconKind::ClearDay : WxIconKind::ClearNight;
    if (code == 2)              return isDay ? WxIconKind::PartlyDay : WxIconKind::PartlyNight;
    if (code == 3)              return WxIconKind::Cloudy;
    if (code == 45 || code == 48) return WxIconKind::Fog;
    if (code >= 51 && code <= 57) return WxIconKind::Drizzle;   // includes freezing drizzle
    if (code >= 61 && code <= 67) return WxIconKind::Rain;      // includes freezing rain
    if (code >= 71 && code <= 77) return WxIconKind::Snow;      // includes snow grains
    if (code >= 80 && code <= 82) return WxIconKind::Showers;
    if (code == 85 || code == 86) return WxIconKind::Snow;      // snow showers
    if (code >= 95 && code <= 99) return WxIconKind::Thunder;
    return WxIconKind::Cloudy;
}

const char *wx_icon_name(WxIconKind k) {
    switch (k) {
    case WxIconKind::ClearDay:    return "clear-day";
    case WxIconKind::ClearNight:  return "clear-night";
    case WxIconKind::PartlyDay:   return "partly-day";
    case WxIconKind::PartlyNight: return "partly-night";
    case WxIconKind::Cloudy:      return "cloudy";
    case WxIconKind::Fog:         return "fog";
    case WxIconKind::Drizzle:     return "drizzle";
    case WxIconKind::Rain:        return "rain";
    case WxIconKind::Showers:     return "showers";
    case WxIconKind::Snow:        return "snow";
    case WxIconKind::Thunder:     return "thunder";
    case WxIconKind::Count:       break;
    }
    return "?";
}

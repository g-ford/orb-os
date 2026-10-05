#include "radar_settings.h"
#include "settings_store.h"

static const char *const kUnitsLabels[] = {"Aviation", "Metric", "Imperial", nullptr};
static const char *const kTrailLabels[] = {"Off", "Short", "Medium", "Long", nullptr};

// Field order: label, control, storage, optionLabels, unitSuffix, note, onChanged, readLive, step.
const settings::SettingDescriptor kRadarSettings[] = {
    // Max aircraft, Hide ground, Min altitude and Range are theme-overridable in RAM
    // (applyThemeSettings() in main.cpp) without persisting -- readLive reports that effective
    // value instead of the stored one, so a theme like Elegant (maxAircraft: 5, hideGround:
    // true) doesn't make either renderer show a value the Orb isn't actually using.
    {"Max aircraft", settings::Control::Slider, &settings::MAX_AC,      nullptr,      nullptr, nullptr,                  radar_on_max_ac_changed,       radar_read_max_ac_live},
    {"Units",        settings::Control::Enum,   &settings::UNITS,       kUnitsLabels, nullptr, nullptr,                  radar_on_units_changed,        nullptr},
    {"Hide ground",  settings::Control::Toggle, &settings::HIDE_GROUND, nullptr,      nullptr, nullptr,                  radar_on_hide_ground_changed,  radar_read_hide_ground_live},
    {"Military only",settings::Control::Toggle, &settings::MIL_ONLY,    nullptr,      nullptr, nullptr,                  radar_on_mil_only_changed,     nullptr},
    // Large text reboots the device on toggle (see radar_on_big_text_changed) -- restored the
    // pre-registry web checkbox's "(restarts the device)" wording as the descriptor's note, so
    // both renderers warn about it again (Finding 4, 2026-10-05 final review).
    {"Large text",   settings::Control::Toggle, &settings::BIG_TEXT,    nullptr,      nullptr, "(restarts the device)", radar_on_big_text_changed,     nullptr},
    {"Aircraft trails", settings::Control::Enum,   &settings::TRAIL_LEN,  kTrailLabels, nullptr, nullptr, radar_on_trail_len_changed,  nullptr},
    // step 5000: the old curated web dropdown offered 0/5000/10000/20000/33000 ft -- a continuous
    // 0..60000 slider is the same accepted behaviour change MAX_AC's dropdown->slider move was;
    // 5000 keeps the knob's press count in the same ballpark as the old list's own spacing.
    {"Minimum altitude", settings::Control::Slider, &settings::MIN_ALT_FT, nullptr, "ft", nullptr, radar_on_min_alt_ft_changed, radar_read_min_alt_ft_live, 5000},
    {"Show radar sweep", settings::Control::Toggle, &settings::SWEEP,     nullptr, nullptr, nullptr, radar_on_sweep_changed,    nullptr},
    {"Show airports",    settings::Control::Toggle, &settings::AIRPORTS, nullptr, nullptr, nullptr, radar_on_airports_changed, nullptr},
    // step 15: 24 presses for a full 360 degree turn, instead of 359. "deg" rather than a
    // degree glyph: no existing device text uses one, and the compiled font's glyph range
    // isn't confirmed to include it.
    {"Screen rotation",  settings::Control::Slider, &settings::ROT_DEG,   nullptr, "deg", nullptr, radar_on_rot_deg_changed, nullptr, 15},
    // step 10, lo/hi 10/100: the old on-device page cycled RANGE_STEPS_KM (10/20/30/50/100 km)
    // by nearest-match-then-next; a flat step is the same accepted simplification MAX_AC's move
    // from a curated list to a continuous range already established.
    {"Display range",    settings::Control::Slider, &settings::RANGE_KM,  nullptr, "km", nullptr, radar_on_range_km_changed, radar_read_range_km_live, 10},
};
const size_t kRadarSettingsCount = sizeof(kRadarSettings) / sizeof(kRadarSettings[0]);

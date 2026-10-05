#include "radar_settings.h"
#include "settings_store.h"

static const char *const kUnitsLabels[] = {"Aviation", "Metric", "Imperial", nullptr};

const settings::SettingDescriptor kRadarSettings[] = {
    // Max aircraft and Hide ground are theme-overridable in RAM (applyThemeSettings() in
    // main.cpp) without persisting -- readLive reports that effective value instead of the
    // stored one, so a theme like Elegant (maxAircraft: 5, hideGround: true) doesn't make
    // either renderer show a value the Orb isn't actually using.
    {"Max aircraft", settings::Control::Slider, &settings::MAX_AC,      nullptr,      nullptr, nullptr,                  radar_on_max_ac_changed,       radar_read_max_ac_live},
    {"Units",        settings::Control::Enum,   &settings::UNITS,       kUnitsLabels, nullptr, nullptr,                  radar_on_units_changed,        nullptr},
    {"Hide ground",  settings::Control::Toggle, &settings::HIDE_GROUND, nullptr,      nullptr, nullptr,                  radar_on_hide_ground_changed,  radar_read_hide_ground_live},
    {"Military only",settings::Control::Toggle, &settings::MIL_ONLY,    nullptr,      nullptr, nullptr,                  radar_on_mil_only_changed,     nullptr},
    // Large text reboots the device on toggle (see radar_on_big_text_changed) -- restored the
    // pre-registry web checkbox's "(restarts the device)" wording as the descriptor's note, so
    // both renderers warn about it again (Finding 4, 2026-10-05 final review).
    {"Large text",   settings::Control::Toggle, &settings::BIG_TEXT,    nullptr,      nullptr, "(restarts the device)", radar_on_big_text_changed,     nullptr},
};
const size_t kRadarSettingsCount = sizeof(kRadarSettings) / sizeof(kRadarSettings[0]);

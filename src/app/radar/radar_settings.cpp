#include "radar_settings.h"
#include "settings_store.h"

static const char *const kUnitsLabels[] = {"Aviation", "Metric", "Imperial", nullptr};

const settings::SettingDescriptor kRadarSettings[] = {
    {"Max aircraft", settings::Control::Slider, &settings::MAX_AC,      nullptr,      nullptr, radar_on_max_ac_changed},
    {"Units",        settings::Control::Enum,   &settings::UNITS,       kUnitsLabels, nullptr, radar_on_units_changed},
    {"Hide ground",  settings::Control::Toggle, &settings::HIDE_GROUND, nullptr,      nullptr, radar_on_hide_ground_changed},
    {"Military only",settings::Control::Toggle, &settings::MIL_ONLY,    nullptr,      nullptr, radar_on_mil_only_changed},
    {"Large text",   settings::Control::Toggle, &settings::BIG_TEXT,    nullptr,      nullptr, radar_on_big_text_changed},
};
const size_t kRadarSettingsCount = sizeof(kRadarSettings) / sizeof(kRadarSettings[0]);

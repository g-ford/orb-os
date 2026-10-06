#include "weather_settings.h"
#include "settings_store.h"

static const char *const kWxUnitsLabels[] = {"Auto", "Metric", "Imperial", nullptr};

// Field order: label, control, storage, optionLabels, unitSuffix, note, onChanged, readLive, step.
const settings::SettingDescriptor kWeatherSettings[] = {
    {"Weather units", settings::Control::Enum, &settings::WX_UNITS, kWxUnitsLabels, nullptr, nullptr, weather_on_units_changed, nullptr},
};
const size_t kWeatherSettingsCount = sizeof(kWeatherSettings) / sizeof(kWeatherSettings[0]);

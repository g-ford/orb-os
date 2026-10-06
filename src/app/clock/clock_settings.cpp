#include "clock_settings.h"
#include "settings_store.h"

// Field order: label, control, storage, optionLabels, unitSuffix, note, onChanged, readLive, step.
const settings::SettingDescriptor kClockSettings[] = {
    {"Clock chime", settings::Control::Toggle, &settings::SND_CHIME, nullptr, nullptr, nullptr, clock_on_chime_toggle_changed, nullptr},
};
const size_t kClockSettingsCount = sizeof(kClockSettings) / sizeof(kClockSettings[0]);

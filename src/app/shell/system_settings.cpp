#include "system_settings.h"
#include "settings_store.h"

// Field order: label, control, storage, optionLabels, unitSuffix, note, onChanged, readLive, step.
// Volume (VOL) deliberately isn't here: its on-device page keeps a knob-turn-to-adjust-with-
// live-preview interaction the generic group model (press-to-cycle only) can't express. Only
// Mute, a plain on/off with no such interaction need, joins this group.
const settings::SettingDescriptor kSystemSettings[] = {
    {"Mute alerts", settings::Control::Toggle, &settings::MUTE, nullptr, nullptr, nullptr, system_on_mute_changed, nullptr},
};
const size_t kSystemSettingsCount = sizeof(kSystemSettings) / sizeof(kSystemSettings[0]);

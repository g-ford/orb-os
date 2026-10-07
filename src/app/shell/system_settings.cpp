#include "system_settings.h"
#include "settings_store.h"

const uint32_t kIdleDimMs[8]   = { 0, 28800000UL, 14400000UL, 7200000UL, 3600000UL, 1800000UL, 600000UL, 120000UL };
const uint32_t kAutoCycleMs[6] = { 0, 60000UL, 300000UL, 600000UL, 900000UL, 1800000UL };

static const char *const kUnitsLabels[]     = { "Aviation", "Metric", "Imperial", nullptr };
static const char *const kIdleDimLabels[]   = { "Always on", "8 hours", "4 hours", "2 hours", "1 hour", "30 min", "10 min", "2 min", nullptr };
static const char *const kAutoCycleLabels[] = { "Off", "1 min", "5 min", "10 min", "15 min", "30 min", nullptr };

// The existing Enum-vs-labels guards (this file's own test, test_setting_descriptors.py) check
// IDLE_DIM_IDX/AUTO_CYCLE_IDX's range against kIdleDimLabels/kAutoCycleLabels, but nothing
// checked the range against kIdleDimMs/kAutoCycleMs themselves -- a widened hi with a matching
// new label but no new array entry would read past the end of a runtime-indexed array, which
// no compiler warning catches. Caught in code review, 2026-10-07 (CLAUDE.md rule 4: a comment
// can't fail, a guard can).
static_assert(sizeof(kIdleDimMs) / sizeof(kIdleDimMs[0]) ==
              (size_t)(settings::IDLE_DIM_IDX.hi - settings::IDLE_DIM_IDX.lo + 1),
              "kIdleDimMs[] and IDLE_DIM_IDX's range must describe the same list");
static_assert(sizeof(kAutoCycleMs) / sizeof(kAutoCycleMs[0]) ==
              (size_t)(settings::AUTO_CYCLE_IDX.hi - settings::AUTO_CYCLE_IDX.lo + 1),
              "kAutoCycleMs[] and AUTO_CYCLE_IDX's range must describe the same list");

// Field order: label, control, storage, optionLabels, unitSuffix, note, onChanged, readLive, step.
const settings::SettingDescriptor kSystemSettings[] = {
    // step 10: the old dedicated Volume page's VOL_STEP, kept for the same knob-press feel.
    // Trades that page's turn-to-adjust-with-live-preview interaction for the generic group's
    // press-to-cycle -- a deliberate, acknowledged simplification, not an oversight.
    {"Volume",      settings::Control::Slider, &settings::VOL,   nullptr,        nullptr, nullptr, system_on_volume_changed,     nullptr, 10},
    {"Mute alerts", settings::Control::Toggle, &settings::MUTE,  nullptr,        nullptr, nullptr, system_on_mute_changed,       nullptr},
    {"Units",       settings::Control::Enum,   &settings::UNITS, kUnitsLabels,   nullptr, nullptr, system_on_units_changed,      nullptr},
    // step 13: the old dedicated Brightness page's BRI_STEP.
    {"Brightness",  settings::Control::Slider, &settings::BRIGHT, nullptr,       nullptr, nullptr, system_on_brightness_changed, nullptr, 13},
    // Enum over an index into kIdleDimMs/kAutoCycleMs (above), not a continuous range -- these
    // are curated, unevenly-spaced durations, the same shape ALERT_MODE/TRAIL_LEN already use.
    {"Dim screen after", settings::Control::Enum, &settings::IDLE_DIM_IDX,   kIdleDimLabels,   nullptr, nullptr, system_on_idle_dim_changed,   nullptr},
    {"Auto-cycle apps",  settings::Control::Enum, &settings::AUTO_CYCLE_IDX, kAutoCycleLabels, nullptr, nullptr, system_on_auto_cycle_changed, nullptr},
};
const size_t kSystemSettingsCount = sizeof(kSystemSettings) / sizeof(kSystemSettings[0]);

#pragma once
// The "System" settings group: settings owned by no single app (or that every app might rely
// on), registered once as data the same way an app's own settings are -- see
// docs/superpowers/specs/2026-10-05-app-settings-registry-design.md's "system" pseudo-group
// section, and radar_settings.h for the reference implementation this mirrors.
//
// Unlike every other group, this one is NOT registered through app_shell::add() (there's no
// screen, no app-list entry to piggyback on): main.cpp and sim_main.cpp each call
// settings_registry::register_group() directly, right next to where the real apps' own
// add() calls happen. settings_registry::register_group() has no dependency on app_shell or
// a screen object, so this needs no new mechanism -- see settings_registry.cpp.
//
// What belongs here: Mute and Volume gate every audio cue uniformly (audio.cpp's
// audio_play()) -- radar's proximity/new-aircraft beeps, the clock's on-the-hour chime, and
// the web/on-device test pings alike. Units (Aviation/Metric/Imperial) is a device-wide
// display preference the Flight Tracker's own ALT/SPD/DIST readout AND the web's Proximity
// alert dropdown both read -- not radar's alone. Brightness, the idle-dim timeout and the
// auto-cycle timeout are the device's own, not any one app's. None of these are the setting
// "range is different for every app" describes -- Flight Tracker's own Display range and
// Weather's own map range each stay in their owning app's group, never here.
//
// The onChanged callbacks are declared here and defined in main.cpp (the real live-apply
// side effects) and again in sim_main.cpp (mostly true no-ops there -- the native sim has no
// audio hardware and no physical screen to dim/sleep; Units is the one exception, since the
// sim does have its own LVGL UI to apply it to), same device/native split every other group's
// callbacks use.
#include "settings_descriptor.h"
#include <cstddef>
#include <cstdint>

extern void system_on_mute_changed(int v);
extern void system_on_volume_changed(int v);
extern void system_on_units_changed(int v);
extern void system_on_brightness_changed(int v);
extern void system_on_idle_dim_changed(int v);
extern void system_on_auto_cycle_changed(int v);

// Index (settings::IDLE_DIM_IDX/AUTO_CYCLE_IDX in settings_store.h) -> real millisecond value,
// shared so main.cpp's/sim_main.cpp's onChanged hooks (which own g_idleDimMs/g_autoCycleMs)
// and this file's optionLabels describe the exact same list from one definition.
extern const uint32_t kIdleDimMs[8];
extern const uint32_t kAutoCycleMs[6];

extern const settings::SettingDescriptor kSystemSettings[];
extern const size_t kSystemSettingsCount;

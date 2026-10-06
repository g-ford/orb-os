#pragma once
// Clock's settings, declared once as data. Both the on-device Settings menu and the web config
// page render this same array (see settings_registry.h) -- see
// docs/superpowers/specs/2026-10-05-app-settings-registry-design.md, and radar_settings.h for
// the reference implementation this mirrors.
//
// The onChanged callback is declared here and defined in main.cpp (the real live-apply side
// effect: previews the chime and keeps the on-the-hour mirror current) and again in
// sim_main.cpp (a true no-op there -- the native sim has no audio hardware at all), same
// device/native split radar's and weather's callbacks use, needed because this array is
// shared by both builds.
//
// Clock chime's sibling, which chime plays (CHIME_IDX), stays its own bespoke picker (Settings
// > Sound > Chime sound) rather than joining this group: its option count varies at runtime
// (one entry per installed theme, plus the flash built-ins -- see CHIME_UI_MAX), which the
// registry's fixed-size Enum has no way to represent. Same reason Theme selection isn't in
// the registry either.
#include "settings_descriptor.h"
#include <cstddef>

extern void clock_on_chime_toggle_changed(int v);

extern const settings::SettingDescriptor kClockSettings[];
extern const size_t kClockSettingsCount;

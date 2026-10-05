#pragma once
// Radar's settings, declared once as data. Both the on-device Settings menu and the web config
// page render this same array (see settings_registry.h) -- see
// docs/superpowers/specs/2026-10-05-app-settings-registry-design.md.
//
// The five onChanged callbacks are declared here and defined in main.cpp, mirroring the
// existing host_get_*/host_set_* extern pattern in settings_internal.h: this file can't reach
// main.cpp's g_maxAc/g_adsb/radar::setMaxOnScreen() directly, so main.cpp (which already owns
// all of that) supplies the function bodies.
#include "settings_descriptor.h"
#include <cstddef>

extern void radar_on_max_ac_changed(int v);
extern void radar_on_units_changed(int v);
extern void radar_on_hide_ground_changed(int v);
extern void radar_on_mil_only_changed(int v);
extern void radar_on_big_text_changed(int v);

extern const settings::SettingDescriptor kRadarSettings[];
extern const size_t kRadarSettingsCount;

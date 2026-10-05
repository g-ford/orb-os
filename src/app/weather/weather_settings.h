#pragma once
// Weather's settings, declared once as data. Both the on-device Settings menu and the web config
// page render this same array (see settings_registry.h) -- see
// docs/superpowers/specs/2026-10-05-app-settings-registry-design.md, and radar_settings.h for
// the reference implementation this mirrors.
//
// The onChanged callback is declared here and defined in main.cpp (the real live-apply side
// effect), and again in sim_main.cpp (a true no-op there -- the native sim never wired weather
// units to anything live even before this migration; host_wx_units_set was already a no-op in
// sim_host.cpp), same device/native split radar's callbacks use, needed because this array is
// shared by both builds.
#include "settings_descriptor.h"
#include <cstddef>

extern void weather_on_units_changed(int v);

// No readLive: unlike radar's theme-overridden fields, "Auto" is itself a legitimate stored
// mode, not a live value diverging from what's stored -- there's nothing for a readLive hook
// to report that display_int()'s default (falling back to get_int()) doesn't already show.

extern const settings::SettingDescriptor kWeatherSettings[];
extern const size_t kWeatherSettingsCount;

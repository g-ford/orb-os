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

// No readLive, and deliberately not: readLive exists for a value that DIVERGES from storage
// (a theme override, a driver that silently refused what was asked). Auto doesn't diverge --
// it resolves (host_wx_is_imperial() in main.cpp) to an effective imperial/metric flag, but
// Auto itself is the stored mode, not a stand-in for one. A readLive reporting that resolved
// flag would break the control: settings_view.cpp's press handler advances from the DISPLAYED
// value (display_int), so advancing from "resolved to Imperial" would skip straight past Auto
// on the very next press, making it unreachable from the knob.
//
// Accepted loss from this: the old on-device "Units" page showed the resolution alongside the
// mode ("Auto (F, mi)"); the registry's generic Enum renderer can only print the raw
// optionLabel ("Auto"), so that hint is gone on-device (the web card never had it). This is a
// value-dependent label the descriptor model has no field for -- `note` is static text, not a
// per-value suffix -- not something a readLive hook can supply without the breakage above.

extern const settings::SettingDescriptor kWeatherSettings[];
extern const size_t kWeatherSettingsCount;

#pragma once
// The "System" settings group: settings owned by no single app, registered once as data the
// same way an app's own settings are -- see docs/superpowers/specs/2026-10-05-app-settings-
// registry-design.md's "system" pseudo-group section, and radar_settings.h for the reference
// implementation this mirrors.
//
// Unlike every other group, this one is NOT registered through app_shell::add() (there's no
// screen, no app-list entry to piggyback on): main.cpp and sim_main.cpp each call
// settings_registry::register_group() directly, right next to where the real apps' own
// add() calls happen. settings_registry::register_group() has no dependency on app_shell or
// a screen object, so this needs no new mechanism -- see settings_registry.cpp.
//
// Master volume and mute both gate every audio cue uniformly (audio.cpp's audio_play()):
// radar's proximity/new-aircraft beeps, the clock's on-the-hour chime, and the web/on-device
// test pings alike. That's what makes them "system", not radar's or the clock's: confirmed by
// grep, nothing scopes either to one subsystem. (Volume itself stays off this list -- see
// system_settings.cpp.)
//
// The onChanged callback is declared here and defined in main.cpp (the real live-apply side
// effect) and again in sim_main.cpp (a true no-op there -- the native sim has no audio
// hardware at all), same device/native split every other group's callbacks use.
#include "settings_descriptor.h"
#include <cstddef>

extern void system_on_mute_changed(int v);

extern const settings::SettingDescriptor kSystemSettings[];
extern const size_t kSystemSettingsCount;

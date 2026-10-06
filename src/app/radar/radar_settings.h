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
extern void radar_on_trail_len_changed(int v);
extern void radar_on_min_alt_ft_changed(int v);
extern void radar_on_sweep_changed(int v);
extern void radar_on_airports_changed(int v);
extern void radar_on_rot_deg_changed(int v);
extern void radar_on_range_km_changed(int v);
extern void radar_on_sound_changed(int v);
extern void radar_on_alert_mode_changed(int v);

// readLive hooks: the stored/requested value and the value actually in effect can diverge,
// either because a theme overrides it in RAM without persisting (see
// applyThemeSettings()/loadSettings() in main.cpp) or because the hardware silently can't
// honor what was asked (ROT_DEG: display::setRotation() normalizes an angle it can't apply
// back to 0 when there's no PSRAM scratch/framebuffer for it). Declared here, defined twice --
// once in main.cpp (the real live value), once in sim_main.cpp (the native sim has no theme-
// override or rotation-normalization concept, so its version just mirrors the last
// set_int'd value for every one of these, including rotation -- the sim has nothing to
// disagree with, so mirroring is the honest "live" value there) -- same device/native split
// as the onChanged callbacks above, needed because this array is shared by both builds.
// TRAIL_LEN, SWEEP, AIRPORTS have neither a theme-override field nor a hardware-normalization
// path and so no readLive hook.
extern int radar_read_max_ac_live();
extern int radar_read_hide_ground_live();
extern int radar_read_min_alt_ft_live();
extern int radar_read_range_km_live();
extern int radar_read_rot_deg_live();

extern const settings::SettingDescriptor kRadarSettings[];
extern const size_t kRadarSettingsCount;

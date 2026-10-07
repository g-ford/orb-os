#include "radar_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-ins for the real main.cpp side effects (ui_set_units, g_adsb.*, radar::*,
// display::*, ...) -- they only need to be linkable and to record what they were called with.
static int s_lastMaxAc = -1, s_lastHideGround = -1, s_lastMilOnly = -1, s_lastBigText = -1;
static int s_lastTrailLen = -1, s_lastMinAltFt = -1, s_lastSweep = -1, s_lastAirports = -1, s_lastRotDeg = -1, s_lastRangeKm = -1;
static int s_lastSound = -1, s_lastAlertMode = -1;
void radar_on_max_ac_changed(int v)      { s_lastMaxAc = v; }
void radar_on_hide_ground_changed(int v) { s_lastHideGround = v; }
void radar_on_mil_only_changed(int v)    { s_lastMilOnly = v; }
void radar_on_big_text_changed(int v)    { s_lastBigText = v; }
void radar_on_trail_len_changed(int v)   { s_lastTrailLen = v; }
void radar_on_min_alt_ft_changed(int v)  { s_lastMinAltFt = v; }
void radar_on_sweep_changed(int v)       { s_lastSweep = v; }
void radar_on_airports_changed(int v)    { s_lastAirports = v; }
void radar_on_rot_deg_changed(int v)     { s_lastRotDeg = v; }
void radar_on_range_km_changed(int v)    { s_lastRangeKm = v; }
void radar_on_sound_changed(int v)       { s_lastSound = v; }
void radar_on_alert_mode_changed(int v)  { s_lastAlertMode = v; }

static int s_liveMaxAc = 77, s_liveHideGround = 1, s_liveMinAltFt = 999, s_liveRangeKm = 55, s_liveRotDeg = 33;
int radar_read_max_ac_live()      { return s_liveMaxAc; }
int radar_read_hide_ground_live() { return s_liveHideGround; }
int radar_read_min_alt_ft_live()  { return s_liveMinAltFt; }
int radar_read_range_km_live()    { return s_liveRangeKm; }
int radar_read_rot_deg_live()     { return s_liveRotDeg; }

static const char *const kExpectedKeys[] = {
    "maxac", "hideground", "milonly", "bigtext",
    "traillen", "minalt", "sweep", "airports", "rotDeg", "rangeKm",
    "sndRadar", "alertmode",
};
static const size_t kExpectedCount = sizeof(kExpectedKeys) / sizeof(kExpectedKeys[0]);

static void the_full_set_is_twelve_descriptors_over_the_twelve_catalogue_keys() {
    assert(kRadarSettingsCount == kExpectedCount);
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        bool found = false;
        for (const char *k : kExpectedKeys) if (std::string(settings::key(kRadarSettings[i])) == k) found = true;
        assert(found);
    }
    printf("ok: the_full_set_is_twelve_descriptors_over_the_twelve_catalogue_keys\n");
}

static void every_enum_descriptors_option_labels_span_matches_its_int_range() {
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kRadarSettings[i];
        if (d.control != settings::Control::Enum) continue;
        assert(d.storage.kind == settings::StorageKind::Int);
        const int span = d.storage.asInt->hi - d.storage.asInt->lo + 1;
        int labelCount = 0;
        while (d.optionLabels[labelCount]) ++labelCount;
        assert(labelCount == span);
    }
    printf("ok: every_enum_descriptors_option_labels_span_matches_its_int_range\n");
}

static void every_descriptor_has_an_onchanged_hook() {
    FakePrefs::disk.clear();
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kRadarSettings[i];
        assert(d.onChanged != nullptr);   // every setting here has a live side effect, see spec
        settings::set_int<FakePrefs>(d, 1);
    }
    // 1 is in every descriptor's range except RANGE_KM's (lo=10) -- set_int clamps that
    // one to 10, so the onChanged hook correctly sees 10, not the raw 1 passed in.
    assert(s_lastMaxAc == 1 && s_lastHideGround == 1 && s_lastMilOnly == 1 && s_lastBigText == 1);
    assert(s_lastTrailLen == 1 && s_lastMinAltFt == 1 && s_lastSweep == 1 && s_lastAirports == 1 && s_lastRotDeg == 1 && s_lastRangeKm == 10);
    assert(s_lastSound == 1 && s_lastAlertMode == 1);
    printf("ok: every_descriptor_has_an_onchanged_hook\n");
}

static const settings::SettingDescriptor &find(const char *key) {
    for (size_t i = 0; i < kRadarSettingsCount; ++i)
        if (std::string(settings::key(kRadarSettings[i])) == key) return kRadarSettings[i];
    assert(false);
    return kRadarSettings[0];
}

static void settings_where_the_display_can_diverge_from_storage_have_read_live_wired() {
    FakePrefs::disk.clear();
    const settings::SettingDescriptor &maxAc = find("maxac");
    const settings::SettingDescriptor &hideGround = find("hideground");
    const settings::SettingDescriptor &minAlt = find("minalt");
    const settings::SettingDescriptor &rangeKm = find("rangeKm");
    const settings::SettingDescriptor &rotDeg = find("rotDeg");
    assert(maxAc.readLive != nullptr);
    assert(hideGround.readLive != nullptr);
    assert(minAlt.readLive != nullptr);
    assert(rangeKm.readLive != nullptr);
    assert(rotDeg.readLive != nullptr);   // the driver's read-back, not a theme field -- see radar_settings.cpp
    settings::set_int<FakePrefs>(maxAc, 3);
    settings::set_int<FakePrefs>(hideGround, 0);
    assert(settings::get_int<FakePrefs>(maxAc) == 3);                  // stored value
    assert(settings::display_int<FakePrefs>(maxAc) == s_liveMaxAc);    // display prefers the live one
    assert(settings::get_int<FakePrefs>(hideGround) == 0);
    assert(settings::display_int<FakePrefs>(hideGround) == s_liveHideGround);
    assert(settings::display_int<FakePrefs>(minAlt) == s_liveMinAltFt);
    assert(settings::display_int<FakePrefs>(rangeKm) == s_liveRangeKm);
    assert(settings::display_int<FakePrefs>(rotDeg) == s_liveRotDeg);

    // No theme-override field in theme_style.h's Radar struct, and nothing hardware-side can
    // make these diverge from what was last set: no readLive.
    assert(find("milonly").readLive == nullptr);
    assert(find("bigtext").readLive == nullptr);
    assert(find("traillen").readLive == nullptr);
    assert(find("sweep").readLive == nullptr);
    assert(find("airports").readLive == nullptr);
    assert(find("sndRadar").readLive == nullptr);
    assert(find("alertmode").readLive == nullptr);
    printf("ok: settings_where_the_display_can_diverge_from_storage_have_read_live_wired\n");
}

static void sliders_step_by_their_configured_amount() {
    const settings::SettingDescriptor &minAlt = find("minalt");    // step 5000
    const settings::SettingDescriptor &rotDeg = find("rotDeg");    // step 15
    const settings::SettingDescriptor &rangeKm = find("rangeKm");  // step 10
    const settings::SettingDescriptor &maxAc = find("maxac");      // step defaults to 1
    assert(minAlt.step == 5000);
    assert(rotDeg.step == 15);
    assert(rangeKm.step == 10);
    assert(maxAc.step == 1);
    assert(settings::advance_int(minAlt, 0) == 5000);
    assert(settings::advance_int(rotDeg, 0) == 15);
    assert(settings::advance_int(rangeKm, 10) == 20);
    printf("ok: sliders_step_by_their_configured_amount\n");
}

int main() {
    the_full_set_is_twelve_descriptors_over_the_twelve_catalogue_keys();
    every_enum_descriptors_option_labels_span_matches_its_int_range();
    every_descriptor_has_an_onchanged_hook();
    settings_where_the_display_can_diverge_from_storage_have_read_live_wired();
    sliders_step_by_their_configured_amount();
    printf("radar_settings: all checks passed\n");
    return 0;
}

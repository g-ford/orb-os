#include "system_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-ins for the real main.cpp side effects (audio_set_volume/muted, ui_set_units,
// display::setBrightness/noteActivity, ...) -- they only need to be linkable and to record
// what they were called with.
static int s_lastVolume = -1, s_lastMute = -1, s_lastUnits = -1, s_lastBrightness = -1;
static int s_lastIdleDim = -1, s_lastAutoCycle = -1;
void system_on_volume_changed(int v)     { s_lastVolume = v; }
void system_on_mute_changed(int v)       { s_lastMute = v; }
void system_on_units_changed(int v)      { s_lastUnits = v; }
void system_on_brightness_changed(int v) { s_lastBrightness = v; }
void system_on_idle_dim_changed(int v)   { s_lastIdleDim = v; }
void system_on_auto_cycle_changed(int v) { s_lastAutoCycle = v; }

static const char *const kExpectedKeys[] = {
    "vol", "mute", "units", "bright", "idleDimIdx", "autoCycleIdx",
};
static const size_t kExpectedCount = sizeof(kExpectedKeys) / sizeof(kExpectedKeys[0]);

static void the_full_set_is_six_descriptors_over_the_six_catalogue_keys() {
    assert(kSystemSettingsCount == kExpectedCount);
    for (size_t i = 0; i < kSystemSettingsCount; ++i) {
        bool found = false;
        for (const char *k : kExpectedKeys) if (std::string(settings::key(kSystemSettings[i])) == k) found = true;
        assert(found);
    }
    printf("ok: the_full_set_is_six_descriptors_over_the_six_catalogue_keys\n");
}

static void every_enum_descriptors_option_labels_span_matches_its_int_range() {
    for (size_t i = 0; i < kSystemSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kSystemSettings[i];
        if (d.control != settings::Control::Enum) continue;
        assert(d.storage.kind == settings::StorageKind::Int);
        const int span = d.storage.asInt->hi - d.storage.asInt->lo + 1;
        int labelCount = 0;
        while (d.optionLabels[labelCount]) ++labelCount;
        assert(labelCount == span);
    }
    printf("ok: every_enum_descriptors_option_labels_span_matches_its_int_range\n");
}

static const settings::SettingDescriptor &find(const char *key) {
    for (size_t i = 0; i < kSystemSettingsCount; ++i)
        if (std::string(settings::key(kSystemSettings[i])) == key) return kSystemSettings[i];
    assert(false);
    return kSystemSettings[0];
}

static void every_descriptor_has_an_onchanged_hook_and_no_read_live() {
    FakePrefs::disk.clear();
    for (size_t i = 0; i < kSystemSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kSystemSettings[i];
        assert(d.onChanged != nullptr);
        assert(d.readLive == nullptr);   // nothing overrides a System setting live
        // 1 is in every descriptor's range except BRIGHT's (lo=8) -- set_int clamps that
        // one to 8, so the onChanged hook correctly sees 8, not the raw 1 passed in.
        const bool isBright = std::string(settings::key(d)) == "bright";
        settings::set_int<FakePrefs>(d, isBright ? 50 : 1);
    }
    assert(s_lastBrightness == 50);
    printf("ok: every_descriptor_has_an_onchanged_hook_and_no_read_live\n");
}

static void sliders_step_by_their_configured_amount() {
    const settings::SettingDescriptor &vol = find("vol");       // step 10 (the old Volume page's VOL_STEP)
    const settings::SettingDescriptor &bright = find("bright"); // step 13 (the old Brightness page's BRI_STEP)
    assert(vol.step == 10);
    assert(bright.step == 13);
    assert(settings::advance_int(vol, 0) == 10);
    assert(settings::advance_int(bright, 8) == 21);
    printf("ok: sliders_step_by_their_configured_amount\n");
}

static void idle_dim_and_auto_cycle_cycle_through_their_curated_lists() {
    FakePrefs::disk.clear();
    const settings::SettingDescriptor &idle = find("idleDimIdx");
    const settings::SettingDescriptor &cycle = find("autoCycleIdx");
    assert(idle.control == settings::Control::Enum);
    assert(cycle.control == settings::Control::Enum);
    settings::set_int<FakePrefs>(idle, 7);    // last entry ("2 min")
    assert(s_lastIdleDim == 7);
    assert(settings::advance_int(idle, 7) == 0);    // wraps back to "Always on"
    settings::set_int<FakePrefs>(cycle, 5);   // last entry ("30 min")
    assert(s_lastAutoCycle == 5);
    assert(settings::advance_int(cycle, 5) == 0);   // wraps back to "Off"
    printf("ok: idle_dim_and_auto_cycle_cycle_through_their_curated_lists\n");
}

static void volume_mute_units_brightness_fire_their_hooks() {
    FakePrefs::disk.clear();
    settings::set_int<FakePrefs>(find("vol"), 80);
    assert(s_lastVolume == 80);
    settings::set_int<FakePrefs>(find("mute"), 1);
    assert(s_lastMute == 1);
    settings::set_int<FakePrefs>(find("units"), 2);
    assert(s_lastUnits == 2);
    settings::set_int<FakePrefs>(find("bright"), 200);
    assert(s_lastBrightness == 200);
    printf("ok: volume_mute_units_brightness_fire_their_hooks\n");
}

int main() {
    the_full_set_is_six_descriptors_over_the_six_catalogue_keys();
    every_enum_descriptors_option_labels_span_matches_its_int_range();
    every_descriptor_has_an_onchanged_hook_and_no_read_live();
    sliders_step_by_their_configured_amount();
    idle_dim_and_auto_cycle_cycle_through_their_curated_lists();
    volume_mute_units_brightness_fire_their_hooks();
    printf("system_settings: all checks passed\n");
    return 0;
}

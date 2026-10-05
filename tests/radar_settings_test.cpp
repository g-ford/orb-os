#include "radar_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-ins for the real main.cpp side effects (ui_set_units, g_adsb.*, ...) --
// they only need to be linkable and to record what they were called with.
static int s_lastMaxAc = -1, s_lastUnits = -1, s_lastHideGround = -1, s_lastMilOnly = -1, s_lastBigText = -1;
void radar_on_max_ac_changed(int v)     { s_lastMaxAc = v; }
void radar_on_units_changed(int v)      { s_lastUnits = v; }
void radar_on_hide_ground_changed(int v){ s_lastHideGround = v; }
void radar_on_mil_only_changed(int v)   { s_lastMilOnly = v; }
void radar_on_big_text_changed(int v)   { s_lastBigText = v; }

static int s_liveMaxAc = 77, s_liveHideGround = 1;
int radar_read_max_ac_live()      { return s_liveMaxAc; }
int radar_read_hide_ground_live() { return s_liveHideGround; }

static void the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys() {
    assert(kRadarSettingsCount == 5);
    const char *expect[] = {"maxac", "units", "hideground", "milonly", "bigtext"};
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        bool found = false;
        for (const char *k : expect) if (std::string(settings::key(kRadarSettings[i])) == k) found = true;
        assert(found);
    }
    printf("ok: the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys\n");
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

static void each_onchanged_fires_with_the_clamped_stored_value() {
    FakePrefs::disk.clear();
    for (size_t i = 0; i < kRadarSettingsCount; ++i) {
        const settings::SettingDescriptor &d = kRadarSettings[i];
        assert(d.onChanged != nullptr);   // every pilot setting has a live side effect, see spec
        settings::set_int<FakePrefs>(d, 1);
    }
    assert(s_lastMaxAc == 1 && s_lastUnits == 1 && s_lastHideGround == 1 && s_lastMilOnly == 1 && s_lastBigText == 1);
    printf("ok: each_onchanged_fires_with_the_clamped_stored_value\n");
}

static const settings::SettingDescriptor &find(const char *key) {
    for (size_t i = 0; i < kRadarSettingsCount; ++i)
        if (std::string(settings::key(kRadarSettings[i])) == key) return kRadarSettings[i];
    assert(false);
    return kRadarSettings[0];
}

static void only_the_theme_overridable_settings_have_read_live_wired() {
    FakePrefs::disk.clear();
    const settings::SettingDescriptor &maxAc = find("maxac");
    const settings::SettingDescriptor &hideGround = find("hideground");
    assert(maxAc.readLive != nullptr);
    assert(hideGround.readLive != nullptr);
    settings::set_int<FakePrefs>(maxAc, 3);
    settings::set_int<FakePrefs>(hideGround, 0);
    assert(settings::get_int<FakePrefs>(maxAc) == 3);           // stored value
    assert(settings::display_int<FakePrefs>(maxAc) == s_liveMaxAc);   // display prefers the live one
    assert(settings::get_int<FakePrefs>(hideGround) == 0);
    assert(settings::display_int<FakePrefs>(hideGround) == s_liveHideGround);

    assert(find("units").readLive == nullptr);
    assert(find("milonly").readLive == nullptr);
    assert(find("bigtext").readLive == nullptr);
    printf("ok: only_the_theme_overridable_settings_have_read_live_wired\n");
}

int main() {
    the_pilot_set_is_exactly_five_descriptors_over_the_five_catalogue_keys();
    every_enum_descriptors_option_labels_span_matches_its_int_range();
    each_onchanged_fires_with_the_clamped_stored_value();
    only_the_theme_overridable_settings_have_read_live_wired();
    printf("radar_settings: all checks passed\n");
    return 0;
}

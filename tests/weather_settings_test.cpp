#include "weather_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-in for main.cpp's weather_on_units_changed() side effect (ui_set_wx_units) --
// it only needs to be linkable and to record what it was called with.
static int s_lastUnits = -1;
void weather_on_units_changed(int v) { s_lastUnits = v; }

static void the_full_set_is_one_descriptor_over_the_one_catalogue_key() {
    assert(kWeatherSettingsCount == 1);
    assert(std::string(settings::key(kWeatherSettings[0])) == "wxUnits");
    printf("ok: the_full_set_is_one_descriptor_over_the_one_catalogue_key\n");
}

static void the_enum_descriptors_option_labels_span_matches_its_int_range() {
    const settings::SettingDescriptor &d = kWeatherSettings[0];
    assert(d.control == settings::Control::Enum);
    assert(d.storage.kind == settings::StorageKind::Int);
    const int span = d.storage.asInt->hi - d.storage.asInt->lo + 1;
    int labelCount = 0;
    while (d.optionLabels[labelCount]) ++labelCount;
    assert(labelCount == span);
    printf("ok: the_enum_descriptors_option_labels_span_matches_its_int_range\n");
}

static void the_descriptor_has_an_onchanged_hook_and_no_read_live() {
    FakePrefs::disk.clear();
    const settings::SettingDescriptor &d = kWeatherSettings[0];
    assert(d.onChanged != nullptr);
    assert(d.readLive == nullptr);   // "Auto" is itself a legitimate stored mode, not a live override
    settings::set_int<FakePrefs>(d, 2);
    assert(s_lastUnits == 2);
    assert(settings::get_int<FakePrefs>(d) == 2);
    assert(settings::display_int<FakePrefs>(d) == 2);   // falls back to get_int() with no readLive
    printf("ok: the_descriptor_has_an_onchanged_hook_and_no_read_live\n");
}

int main() {
    the_full_set_is_one_descriptor_over_the_one_catalogue_key();
    the_enum_descriptors_option_labels_span_matches_its_int_range();
    the_descriptor_has_an_onchanged_hook_and_no_read_live();
    printf("weather_settings: all checks passed\n");
    return 0;
}

#include "clock_settings.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

// Host-test stand-in for main.cpp's clock_on_chime_toggle_changed() side effect (the preview
// chime) -- it only needs to be linkable and to record what it was called with.
static int s_lastChime = -1;
void clock_on_chime_toggle_changed(int v) { s_lastChime = v; }

static void the_full_set_is_one_descriptor_over_the_one_catalogue_key() {
    assert(kClockSettingsCount == 1);
    assert(std::string(settings::key(kClockSettings[0])) == "sndChime");
    printf("ok: the_full_set_is_one_descriptor_over_the_one_catalogue_key\n");
}

static void the_descriptor_has_an_onchanged_hook_and_no_read_live() {
    FakePrefs::disk.clear();
    const settings::SettingDescriptor &d = kClockSettings[0];
    assert(d.control == settings::Control::Toggle);
    assert(d.onChanged != nullptr);
    assert(d.readLive == nullptr);   // nothing overrides the clock chime toggle live
    settings::set_int<FakePrefs>(d, 1);
    assert(s_lastChime == 1);
    assert(settings::get_int<FakePrefs>(d) == 1);
    assert(settings::display_int<FakePrefs>(d) == 1);   // falls back to get_int() with no readLive
    printf("ok: the_descriptor_has_an_onchanged_hook_and_no_read_live\n");
}

int main() {
    the_full_set_is_one_descriptor_over_the_one_catalogue_key();
    the_descriptor_has_an_onchanged_hook_and_no_read_live();
    printf("clock_settings: all checks passed\n");
    return 0;
}

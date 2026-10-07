#include "settings_registry.h"
#include "clock_settings.h"    // kClockSettings -- extra_row_for() now links against it
#include "system_settings.h"   // kSystemSettings -- same

#include <assert.h>
#include <stdio.h>
#include <string>

using namespace settings_registry;

// Host-test stand-ins for the onChanged side effects kClockSettings/kSystemSettings reference
// by function pointer -- same stubs tests/clock_settings_test.cpp and
// tests/system_settings_test.cpp each already define for their own, narrower link; this test
// now needs both arrays (extra_row_for()'s comparisons), so it needs both sets of stubs.
void clock_on_chime_toggle_changed(int) {}
void system_on_mute_changed(int) {}
void system_on_volume_changed(int) {}
void system_on_units_changed(int) {}
void system_on_brightness_changed(int) {}
void system_on_idle_dim_changed(int) {}
void system_on_auto_cycle_changed(int) {}

static constexpr settings::Int kA{"testa", 0, 0, 1};
static const settings::SettingDescriptor kGroupA[] = {
    {"A", settings::Control::Toggle, &kA},
};

static void registering_a_group_makes_it_visible_by_index() {
    assert(count() == 0);
    register_group("Alpha", kGroupA, 1);
    assert(count() == 1);
    assert(std::string(group(0).label) == "Alpha");
    assert(group(0).count == 1);
    assert(group(0).items == kGroupA);
    printf("ok: registering_a_group_makes_it_visible_by_index\n");
}

static void registering_past_capacity_is_ignored_not_undefined() {
    for (int i = (int)count(); i < (int)MAX_GROUPS + 2; ++i) register_group("Extra", kGroupA, 1);
    assert(count() == MAX_GROUPS);   // the two past capacity were dropped, not overrun
    printf("ok: registering_past_capacity_is_ignored_not_undefined\n");
}

// extra_row_for() is the one place settings_pages.cpp (on-device) and main.cpp (web) both
// consult to learn which group (if any) carries a non-descriptor row -- Clock's "Chime
// sound", System's "Location". Identity by pointer, not label, is the whole point: a plain
// group (kGroupA above) must answer None even though nothing else distinguishes it.
static void extra_row_for_identifies_clock_and_system_only() {
    assert(extra_row_for(kGroupA) == ExtraRow::None);
    assert(extra_row_for(kClockSettings) == ExtraRow::ClockChime);
    assert(extra_row_for(kSystemSettings) == ExtraRow::SystemLocation);
    printf("ok: extra_row_for_identifies_clock_and_system_only\n");
}

int main() {
    registering_a_group_makes_it_visible_by_index();
    registering_past_capacity_is_ignored_not_undefined();
    extra_row_for_identifies_clock_and_system_only();
    printf("settings_registry: all checks passed\n");
    return 0;
}

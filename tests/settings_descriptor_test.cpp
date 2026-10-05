// tests/settings_descriptor_test.cpp
#include "settings_descriptor.h"
#include "fake_prefs.h"

#include <assert.h>
#include <stdio.h>
#include <string>

using namespace settings;

static const char *kUnitsLabels[] = {"Metric", "Imperial", "Nautical"};
static int g_lastChanged = -1000;
static void record_changed(int v) { g_lastChanged = v; }

static constexpr Int  kTestRange {"testrange", 5, 1, 10};
static constexpr Bool kTestFlag  {"testflag", false};

static void toggle_round_trips_as_zero_or_one() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Flag", Control::Toggle, &kTestFlag};
    assert(get_int<FakePrefs>(d) == 0);              // default false
    set_int<FakePrefs>(d, 1);
    assert(get_int<FakePrefs>(d) == 1);
    printf("ok: toggle_round_trips_as_zero_or_one\n");
}

static void slider_clamps_before_storing() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Range", Control::Slider, &kTestRange};
    set_int<FakePrefs>(d, 999);
    assert(get_int<FakePrefs>(d) == 10);             // clamped to hi
    set_int<FakePrefs>(d, -5);
    assert(get_int<FakePrefs>(d) == 1);              // clamped to lo
    printf("ok: slider_clamps_before_storing\n");
}

static void enum_label_lookup_uses_value_minus_lo() {
    FakePrefs::disk.clear();
    SettingDescriptor d{"Units", Control::Enum, &kTestRange, kUnitsLabels};
    set_int<FakePrefs>(d, 2);                         // kTestRange.lo == 1
    const int v = get_int<FakePrefs>(d);
    assert(std::string(d.optionLabels[v - 1]) == "Imperial");
    printf("ok: enum_label_lookup_uses_value_minus_lo\n");
}

static void set_int_calls_on_changed_with_the_stored_value_not_the_raw_one() {
    FakePrefs::disk.clear();
    g_lastChanged = -1000;
    SettingDescriptor d{"Range", Control::Slider, &kTestRange, nullptr, nullptr, nullptr, record_changed};
    set_int<FakePrefs>(d, 999);                      // clamps to 10 before the callback runs
    assert(g_lastChanged == 10);
    printf("ok: set_int_calls_on_changed_with_the_stored_value_not_the_raw_one\n");
}

static void key_reports_the_underlying_catalogue_key_for_every_kind() {
    SettingDescriptor t{"Flag", Control::Toggle, &kTestFlag};
    SettingDescriptor s{"Range", Control::Slider, &kTestRange};
    assert(std::string(key(t)) == "testflag");
    assert(std::string(key(s)) == "testrange");
    printf("ok: key_reports_the_underlying_catalogue_key_for_every_kind\n");
}

int main() {
    toggle_round_trips_as_zero_or_one();
    slider_clamps_before_storing();
    enum_label_lookup_uses_value_minus_lo();
    set_int_calls_on_changed_with_the_stored_value_not_the_raw_one();
    key_reports_the_underlying_catalogue_key_for_every_kind();
    printf("settings_descriptor: all checks passed\n");
    return 0;
}

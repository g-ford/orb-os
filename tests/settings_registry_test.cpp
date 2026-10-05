#include "settings_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string>

using namespace settings_registry;

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

int main() {
    registering_a_group_makes_it_visible_by_index();
    registering_past_capacity_is_ignored_not_undefined();
    printf("settings_registry: all checks passed\n");
    return 0;
}

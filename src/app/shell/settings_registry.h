#pragma once
// A flat, fixed-capacity list of settings groups — one per app (or pseudo-app) that has
// registered settings, in the order it was registered. No heap: the number of groups is
// bounded at compile time, so a plain array is enough.
//
// A real app's group is added through app_shell::add()'s settingsGroup/settingsCount
// parameters (see app_shell.cpp) rather than calling register_group() directly. A group
// owned by no single app -- the "system" pseudo-group (system_settings.h) is the first one --
// has no screen and no app-list entry to piggyback on, so it calls register_group() directly
// instead, right next to where the real apps' own add() calls happen in main.cpp/sim_main.cpp.
#include "settings_descriptor.h"
#include <cstddef>

namespace settings_registry {

struct Group {
    const char *label;
    const settings::SettingDescriptor *items;
    size_t count;
};

constexpr size_t MAX_GROUPS = 8;

void register_group(const char *label, const settings::SettingDescriptor *items, size_t count);
size_t count();
const Group &group(size_t index);

}  // namespace settings_registry

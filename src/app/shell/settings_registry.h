#pragma once
// A flat, fixed-capacity list of settings groups — one per app that has registered settings,
// in the order it was registered. No heap: the number of groups is bounded by the number of
// apps, known at compile time, so a plain array is enough.
//
// Groups are added through app_shell::add()'s settingsGroup/settingsCount parameters, not by
// calling register_group() directly from app code — see app_shell.cpp.
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

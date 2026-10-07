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

// Which (if any) extra, non-descriptor row a group's page shows after its own descriptors --
// Clock's "Chime sound", System's "Location" -- neither of which can be a plain
// SettingDescriptor (Chime's option count varies at runtime; Location is a whole
// map/search/recents flow). Both the on-device Settings menu (settings_pages.cpp) and the web
// config page (main.cpp's handleRoot()) need this same answer; extra_row_for() is the one
// place that knows it, identified by a group's own `items` pointer -- stable regardless of
// what a theme renames the owning app's label to -- so a group added later needs this table
// updated in exactly one place, not wherever a caller happened to copy the check (CLAUDE.md
// rule 4: a comment can't fail, a guard can).
enum class ExtraRow { None, ClockChime, SystemLocation };
ExtraRow extra_row_for(const settings::SettingDescriptor *items);

}  // namespace settings_registry

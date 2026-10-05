#include "settings_registry.h"

namespace settings_registry {
namespace {
    Group  s_groups[MAX_GROUPS];
    size_t s_count = 0;
}

void register_group(const char *label, const settings::SettingDescriptor *items, size_t count) {
    if (s_count < MAX_GROUPS) {
        s_groups[s_count++] = Group{label, items, count};
    }
}

size_t count() { return s_count; }
const Group &group(size_t index) { return s_groups[index]; }

}  // namespace settings_registry

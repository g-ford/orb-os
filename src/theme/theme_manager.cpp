#include "theme_manager.h"
#include "theme_select.h"
#include "theme_style.h"

#include <cstring>
#include <cstdio>

namespace {

char s_active[theme_select::MAX_SLUG_LEN] = "";

bool uses_default_theme() {
    return !s_active[0] || std::strcmp(s_active, theme_manager::DEFAULT_SLUG) == 0;
}

void sync_active_from_selection() {
    const char *slug = theme_select::activeSlug();
    if (!slug || !slug[0]) {
        std::snprintf(s_active, sizeof(s_active), "%s", theme_manager::DEFAULT_SLUG);
        return;
    }
    std::snprintf(s_active, sizeof(s_active), "%s", slug);
}

} // namespace

namespace theme_manager {

void init() {
    theme_select::init();
    sync_active_from_selection();
    load();
}

const char *activeSlug() {
    return s_active[0] ? s_active : DEFAULT_SLUG;
}

bool isDefault() {
    return uses_default_theme();
}

bool load() {
    sync_active_from_selection();
    theme_style::load();
    return true;
}

bool ensureDefaultBaked() {
    if (!isDefault()) return false;
    // The default theme is the one shipped with the firmware: it always exists and is
    // never read from the SD card. If a future iteration adds a default theme asset
    // cache, this is the place to request it, but custom themes stay out of this path.
    return true;
}

void setRestartHook(void (*hook)()) {
    theme_select::setRestartHook(hook);
}

} // namespace theme_manager

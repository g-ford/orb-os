#pragma once

namespace theme_manager {

constexpr const char *DEFAULT_SLUG = "default";

// Boot-time initialization: resolve the selected/stock theme, load its runtime style,
// and leave custom SD themes alone unless the user actually chose one.
void init();

// Return the effective active slug. Empty or unset selections fall back to the
// built-in default theme shipped with the firmware.
const char *activeSlug();

// Reports whether the effective theme is the built-in stock/default theme.
bool isDefault();

// Load the currently active theme config.
bool load();

// Only the built-in default theme is auto-baked: custom SD themes remain on the card
// until the user explicitly selects and installs them.
bool ensureDefaultBaked();

// Native sim re-exec hook, forwarded to theme_select so a restart keeps the same
// selected theme across a re-run of the process.
void setRestartHook(void (*hook)());

} // namespace theme_manager

#pragma once
#include "theme_roles.h"
#include "theme_style.h"

// The role bindings: which colour option takes which role by default.
//
// theme_style::load() calls this before it reads a theme's own JSON, so a theme that is only a palette gets a
// designed look and anything a theme states wins. It is a function of explicit assignments on purpose: the
// compiler checks every field name, a reader can see the whole mapping on one screen, and moving a colour to a
// different role is a one-line change that shows in a diff.
namespace theme_palette {

void apply_role_defaults(const theme_roles::Palette &p,
                         theme_style::Clock &clock, theme_style::Radar &radar, theme_style::Weather &weather,
                         theme_style::Ticker &ticker, theme_style::Menu &menu, theme_style::Settings &settings,
                         theme_style::Splash &splash, theme_style::Intel &intel);

} // namespace theme_palette

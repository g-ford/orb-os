#pragma once
// The one place a theme becomes a wheel::Look. The app picker and Settings both call this, so the two cannot be
// dressed differently. It lives here, not in the wheel, because it is the part that knows about themes.
#include "wheel.h"

namespace wheel_look {

// What a theme decides: palette `primary` for the selected row and its glow, `muted` for every other row, and
// the theme's two wheel faces (compiled Montserrat when it ships none).
wheel::Look themed();

// Stock and unthemed, for the recovery pages (first boot, network list, password): a theme must not be able to
// make the screen somebody fixes their WiFi on illegible.
wheel::Look system();

} // namespace wheel_look

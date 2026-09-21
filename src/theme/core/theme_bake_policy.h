#pragma once
#include <stdint.h>

// Whether the boot bake should run for the active theme. A pure function so the decision can be tested on
// the desktop; theme_art_bake.cpp asks it before it touches the flash cache.
//
//   cardMounted    the SD card is readable. Without it theme.json, and so the theme's declared asset list,
//                  is unreadable: its fingerprint is 0 and every read of an asset would fail. A bake would
//                  erase the flash index, find nothing it can read and commit nothing, destroying the cache
//                  the Orb is meant to keep running on when the card is out.
//   slugBaked      the flash cache already holds this theme.
//   bakedManifest  the fingerprint stored with that bake.
//   want           the theme's fingerprint now; 0 when it declares no asset list.
namespace theme_art {

inline bool should_bake(bool cardMounted, bool slugBaked, uint32_t bakedManifest, uint32_t want) {
    if (!cardMounted) return false;
    // "Unchanged" needs a real fingerprint on both sides: a theme with no declared list (0) can never be
    // told from one that has not changed, so it is baked as it always has been.
    if (slugBaked && bakedManifest == want && want != 0) return false;
    return true;
}

} // namespace theme_art

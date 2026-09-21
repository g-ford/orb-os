// Host test for src/theme/core/theme_bake_policy.h.   tests/run_theme_bake_policy_test.sh
#include "theme_bake_policy.h"

#include <assert.h>
#include <stdio.h>

using theme_art::should_bake;

// The review finding: with no card the theme's declared asset list is unreadable, so its fingerprint is 0 and
// the "already baked" early-out was skipped. The bake then erased the flash index, found nothing it could read
// and committed nothing, so an Orb booted without its card destroyed the cache it is meant to keep running on.
static void a_missing_card_never_bakes_and_so_never_erases() {
    assert(!should_bake(/*card*/ false, /*baked*/ true,  /*was*/ 0x1234, /*want*/ 0));
    assert(!should_bake(false, true,  0x1234, 0x1234));
    assert(!should_bake(false, true,  0x1234, 0x9999));
    assert(!should_bake(false, false, 0, 0));
}

static void a_theme_not_yet_baked_is_baked_when_the_card_is_there() {
    assert(should_bake(true, false, 0, 0xABCD));
}

static void an_unchanged_theme_is_left_alone() {
    assert(!should_bake(true, true, 0xABCD, 0xABCD));
}

static void a_changed_asset_list_rebakes() {
    assert(should_bake(true, true, 0xABCD, 0xABCE));
}

// A theme with no declared asset list has no fingerprint (0). That could never be told from "unchanged", so it is
// baked as it always has been; only a missing card stops it.
static void a_theme_that_declares_no_assets_is_still_baked_with_a_card() {
    assert(should_bake(true, true, 0, 0));
    assert(should_bake(true, false, 0, 0));
}

int main() {
    a_missing_card_never_bakes_and_so_never_erases();
    a_theme_not_yet_baked_is_baked_when_the_card_is_there();
    an_unchanged_theme_is_left_alone();
    a_changed_asset_list_rebakes();
    a_theme_that_declares_no_assets_is_still_baked_with_a_card();
    printf("theme_bake_policy: all tests passed\n");
    return 0;
}

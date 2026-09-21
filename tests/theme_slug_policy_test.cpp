// Host test for src/theme/core/theme_slug_policy.h.   tests/run_theme_slug_policy_test.sh
#include "theme_slug_policy.h"

#include <assert.h>
#include <stdio.h>

using namespace theme_select;

static void the_reserved_slug_is_default() {
    assert(strcmp(BUILTIN_SLUG, "default") == 0);
    assert(is_builtin("default"));
    assert(!is_builtin("elegant"));
    assert(!is_builtin("Default"));            // slugs are lowercase; a folder called Default is an ordinary theme
    assert(!is_builtin(""));
    assert(!is_builtin(nullptr));
}

// A card folder called `default` must not appear in the list: it would be a second, dead "Default".
static void a_card_scan_never_offers_the_reserved_slug_or_dotfiles() {
    assert(listable("elegant"));
    assert(listable("the-office"));
    assert(!listable("default"));
    assert(!listable(""));
    assert(!listable(nullptr));
    assert(!listable("."));
    assert(!listable(".."));
    assert(!listable(".Trashes"));
    assert(listable("default2"));              // only the exact name is reserved
}

int main() {
    the_reserved_slug_is_default();
    a_card_scan_never_offers_the_reserved_slug_or_dotfiles();
    printf("theme_slug_policy: all tests passed\n");
    return 0;
}

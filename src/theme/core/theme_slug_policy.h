#pragma once
#include <string.h>

// Which theme slugs mean something special. Pure, so it is tested on the desktop; theme_select.cpp uses it.
namespace theme_select {

// The built-in look. A saved slug of "default" selects it, no folder is read for it, and a card folder called
// "default" is ignored (it would be a second, dead "Default" in the list).
constexpr const char *BUILTIN_SLUG = "default";

inline bool is_builtin(const char *slug) { return slug && strcmp(slug, BUILTIN_SLUG) == 0; }

// A folder name a card scan may offer as a theme: non-empty, not a dotfile, not the reserved slug.
inline bool listable(const char *leaf) { return leaf && leaf[0] && leaf[0] != '.' && !is_builtin(leaf); }

} // namespace theme_select

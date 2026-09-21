#pragma once
// Which font file each theme text slot loads, and which of those files are distinct.
//
// Pure logic on purpose: no LVGL and no theme_style. theme_font.cpp hands it the slot list and a
// slot-to-file map, and tests/theme_font_resolve_test.cpp runs it on the desktop. A theme maps its
// slots to named faces (theme.json "fonts": {"radar2": "font_body16.bin"}); slots that name the same
// file are one face, loaded once. A slot the map does not mention keeps its own font_<slot>.bin, which
// is how themes built before the map existed keep working.
//
// The same map is stored in flash as a `fonts.map` blob ("slot file" lines) by the bake, because
// theme.json is read from the SD card only and an Orb with no card must still know which face each
// slot loads.
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace theme_font {

constexpr size_t MAX_SLOTS = 32;         // theme_font.cpp static_asserts that its slot count fits
constexpr size_t MAP_MAX = 32;
constexpr size_t MAP_NAME_BYTES = 24;    // theme_art stores an asset name in 24 bytes, NUL included

struct FontMap {
    char   slot[MAP_MAX][MAP_NAME_BYTES];
    char   file[MAP_MAX][MAP_NAME_BYTES];
    size_t n = 0;
};

// A name that does not fit is refused, never truncated: a truncated asset name is one that is
// stored fine and then never found.
inline bool map_add(FontMap &m, const char *slot, const char *file) {
    if (!slot || !file || !*slot || !*file) return false;
    if (m.n >= MAP_MAX) return false;
    if (strlen(slot) >= MAP_NAME_BYTES || strlen(file) >= MAP_NAME_BYTES) return false;
    strcpy(m.slot[m.n], slot);
    strcpy(m.file[m.n], file);
    ++m.n;
    return true;
}

inline const char *map_lookup(const char *slotName, void *ctx) {
    const FontMap *m = static_cast<const FontMap *>(ctx);
    for (size_t i = 0; i < m->n; ++i)
        if (strcmp(m->slot[i], slotName) == 0) return m->file[i];
    return nullptr;
}

// "slot file\n" per line: the form of the flash blob. Returns the length, or 0 when the map does not
// fit `cap` (it never writes half a map).
inline size_t map_serialize(const FontMap &m, char *buf, size_t cap) {
    size_t at = 0;
    for (size_t i = 0; i < m.n; ++i) {
        const size_t need = strlen(m.slot[i]) + 1 + strlen(m.file[i]) + 1;
        if (at + need + 1 > cap) return 0;
        at += (size_t)snprintf(buf + at, cap - at, "%s %s\n", m.slot[i], m.file[i]);
    }
    return at;
}

inline void map_parse(FontMap &m, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t e = i;
        while (e < len && text[e] != '\n') ++e;
        char line[2 * MAP_NAME_BYTES + 2];
        const size_t n = e - i;
        if (n > 0 && n < sizeof(line)) {
            memcpy(line, text + i, n);
            line[n] = 0;
            char *sp = strchr(line, ' ');
            if (sp) {
                *sp = 0;
                map_add(m, line, sp + 1);
            }
        }
        i = e + 1;
    }
}

// "font_radar2.bin" -> "radar2". False when the name is not shaped like a slot file.
inline bool slot_name(const char *legacyFile, char *out, size_t cap) {
    const char *prefix = "font_";
    const char *suffix = ".bin";
    const size_t n = strlen(legacyFile), p = strlen(prefix), s = strlen(suffix);
    if (n <= p + s || strncmp(legacyFile, prefix, p) != 0 || strcmp(legacyFile + n - s, suffix) != 0) return false;
    const size_t len = n - p - s;
    if (len + 1 > cap) return false;
    memcpy(out, legacyFile + p, len);
    out[len] = 0;
    return true;
}

using MapLookup = const char *(*)(const char *slotName, void *ctx);

struct Resolved {
    const char *file[MAX_SLOTS];       // per slot: the file it loads
    int         group[MAX_SLOTS];      // per slot: index of that file in distinct[]
    const char *distinct[MAX_SLOTS];   // each distinct file, in first-seen order
    size_t      slots = 0;
    size_t      distinctCount = 0;
};

// legacy[i] is slot i's own file name ("font_radar2.bin"); `mapped` answers "which file does the
// theme's map give this slot name", or nullptr. The pointers in `out` point into `legacy` and into
// whatever `mapped` returned, so both must outlive it.
inline void resolve(const char *const *legacy, size_t n, MapLookup mapped, void *ctx, Resolved &out) {
    out.slots = n < MAX_SLOTS ? n : MAX_SLOTS;
    out.distinctCount = 0;
    for (size_t i = 0; i < out.slots; ++i) {
        const char *file = legacy[i];
        char name[MAP_NAME_BYTES + 8];
        if (mapped && slot_name(legacy[i], name, sizeof(name))) {
            const char *m = mapped(name, ctx);
            if (m && *m) file = m;
        }
        out.file[i] = file;
        int g = -1;
        for (size_t d = 0; d < out.distinctCount; ++d)
            if (strcmp(out.distinct[d], file) == 0) { g = (int)d; break; }
        if (g < 0) {
            g = (int)out.distinctCount;
            out.distinct[out.distinctCount++] = file;
        }
        out.group[i] = g;
    }
}

} // namespace theme_font

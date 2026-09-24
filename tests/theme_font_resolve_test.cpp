// Host test for src/theme/core/theme_font_resolve.h.   tests/run_theme_font_resolve_test.sh
#include "theme_font_resolve.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace theme_font;

static void must_add(FontMap &m, const char *slot, const char *file) {
    const bool ok = map_add(m, slot, file);
    assert(ok);
    (void)ok;
}

static const char *SLOTS[] = { "font_wheel_sel.bin", "font_radar1.bin", "font_radar2.bin",
                               "font_radar3.bin", "font_wheel_item.bin" };
constexpr size_t N = sizeof(SLOTS) / sizeof(SLOTS[0]);

static void with_no_map_every_slot_loads_its_own_file() {
    Resolved r;
    resolve(SLOTS, N, nullptr, nullptr, r);
    assert(r.slots == N && r.distinctCount == N);
    for (size_t i = 0; i < N; ++i) {
        assert(strcmp(r.file[i], SLOTS[i]) == 0);
        assert(r.group[i] == (int)i);
    }
}

static void slots_mapped_to_one_face_share_one_group() {
    FontMap m;
    must_add(m, "radar1", "font_body.bin");
    must_add(m, "radar2", "font_body.bin");
    must_add(m, "radar3", "font_body.bin");
    Resolved r;
    resolve(SLOTS, N, map_lookup, &m, r);
    assert(r.distinctCount == 3);                                 // wheel_sel, body, wheel_item
    assert(r.group[1] == r.group[2] && r.group[2] == r.group[3]);
    assert(strcmp(r.file[1], "font_body.bin") == 0);
    assert(strcmp(r.file[0], "font_wheel_sel.bin") == 0);      // an unmapped slot keeps its own file
    assert(strcmp(r.file[4], "font_wheel_item.bin") == 0);
}

static void fallout_shaped_22_slots_become_10_faces() {
    static const struct { const char *slot; int px; } FALLOUT[] = {
        {"wheel_sel", 46}, {"wheel_item", 27}, {"radar1", 22}, {"radar2", 16}, {"radar3", 16}, {"radar4", 16},
        {"weather1", 22}, {"weather2", 22}, {"weather3", 16}, {"weather4", 16}, {"intel_title", 32},
        {"intel_text", 22}, {"intel_source", 12}, {"intel_age", 12}, {"intel_brief", 18}, {"ticker_name", 16},
        {"ticker_price", 40}, {"ticker_change", 22}, {"ticker_strip", 16}, {"wind_title", 28},
        {"wind_ask", 20}, {"wind_turns", 16} };
    constexpr size_t COUNT = sizeof(FALLOUT) / sizeof(FALLOUT[0]);
    static char legacy[COUNT][40];
    const char *legacyPtr[COUNT];
    FontMap m;
    for (size_t i = 0; i < COUNT; ++i) {
        snprintf(legacy[i], sizeof(legacy[i]), "font_%s.bin", FALLOUT[i].slot);
        legacyPtr[i] = legacy[i];
        char face[MAP_NAME_BYTES];
        snprintf(face, sizeof(face), "font_st%d.bin", FALLOUT[i].px);
        must_add(m, FALLOUT[i].slot, face);
    }
    Resolved r;
    resolve(legacyPtr, COUNT, map_lookup, &m, r);
    assert(r.slots == COUNT);
    assert(r.distinctCount == 10);
}

static void an_empty_mapping_value_is_ignored() {
    FontMap m;
    assert(!map_add(m, "radar1", ""));
    assert(!map_add(m, "", "font_a.bin"));
    assert(!map_add(m, nullptr, "font_a.bin"));
    assert(!map_add(m, "radar1", nullptr));
    assert(m.n == 0);
    Resolved r;
    resolve(SLOTS, N, map_lookup, &m, r);
    assert(strcmp(r.file[1], "font_radar1.bin") == 0);
}

static void a_name_too_long_for_the_flash_index_is_rejected() {
    FontMap m;
    char longName[MAP_NAME_BYTES + 1];
    memset(longName, 'a', sizeof(longName));
    longName[MAP_NAME_BYTES] = 0;                                  // 24 characters: one too many
    assert(!map_add(m, "radar1", longName));
    assert(!map_add(m, longName, "font_a.bin"));
    longName[MAP_NAME_BYTES - 1] = 0;                              // 23 characters: the most that fits
    assert(map_add(m, "radar1", longName));
}

static void a_full_map_refuses_more() {
    FontMap m;
    for (size_t i = 0; i < MAP_MAX; ++i) {
        char slot[16];
        snprintf(slot, sizeof(slot), "s%zu", i);
        must_add(m, slot, "font_a.bin");
    }
    assert(!map_add(m, "extra", "font_a.bin"));
    assert(m.n == MAP_MAX);
}

static void slot_names_come_from_legacy_file_names() {
    char out[40];
    assert(slot_name("font_radar2.bin", out, sizeof(out)) && strcmp(out, "radar2") == 0);
    assert(slot_name("font_intel_title.bin", out, sizeof(out)) && strcmp(out, "intel_title") == 0);
    assert(!slot_name("radar2.bin", out, sizeof(out)));
    assert(!slot_name("font_.bin", out, sizeof(out)));
    assert(!slot_name("font_radar2.png", out, sizeof(out)));
    char tiny[4];
    assert(!slot_name("font_radar2.bin", tiny, sizeof(tiny)));
}

// Review focus 1: an Orb with no card resolves from the flash blob, and must get the same answer.
static void the_blob_round_trips_and_resolves_identically() {
    FontMap fromJson;
    must_add(fromJson, "radar1", "font_body.bin");
    must_add(fromJson, "radar3", "font_body.bin");
    must_add(fromJson, "wheel_sel", "font_big.bin");
    char blob[512];
    const size_t len = map_serialize(fromJson, blob, sizeof(blob));
    assert(len > 0);
    FontMap fromBlob;
    map_parse(fromBlob, blob, len);
    assert(fromBlob.n == fromJson.n);
    Resolved a, b;
    resolve(SLOTS, N, map_lookup, &fromJson, a);
    resolve(SLOTS, N, map_lookup, &fromBlob, b);
    assert(a.distinctCount == b.distinctCount);
    for (size_t i = 0; i < N; ++i) assert(strcmp(a.file[i], b.file[i]) == 0 && a.group[i] == b.group[i]);
}

static void a_map_that_does_not_fit_the_buffer_writes_nothing() {
    FontMap m;
    must_add(m, "radar1", "font_body.bin");
    char tiny[8];
    assert(map_serialize(m, tiny, sizeof(tiny)) == 0);
}

static void malformed_lines_in_the_blob_are_ignored() {
    const char text[] = "radar1 font_a.bin\nbadline\n  \nradar2\nradar3 font_b.bin";   // no trailing newline
    FontMap m;
    map_parse(m, text, sizeof(text) - 1);
    assert(m.n == 2);
    assert(strcmp(map_lookup("radar1", &m), "font_a.bin") == 0);
    assert(strcmp(map_lookup("radar3", &m), "font_b.bin") == 0);
    assert(map_lookup("radar2", &m) == nullptr);
}

int main() {
    with_no_map_every_slot_loads_its_own_file();
    slots_mapped_to_one_face_share_one_group();
    fallout_shaped_22_slots_become_10_faces();
    an_empty_mapping_value_is_ignored();
    a_name_too_long_for_the_flash_index_is_rejected();
    a_full_map_refuses_more();
    slot_names_come_from_legacy_file_names();
    the_blob_round_trips_and_resolves_identically();
    a_map_that_does_not_fit_the_buffer_writes_nothing();
    malformed_lines_in_the_blob_are_ignored();
    printf("theme_font_resolve: all tests passed\n");
    return 0;
}

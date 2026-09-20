// Loads one baked font with LVGL's own lv_font_load (the path theme_font.cpp takes on the device) and
// checks it has every codepoint asked for. Used by tests/test_fallout_theme.py: a font that fails to parse
// is not an error on the device, it silently draws the compiled face instead, so only a run through the
// loader shows it. Built against the native env's liblvgl.a, with the repo's own lv_conf.h.
//   font_load_check <dir> <font.bin> <hex codepoint>...      exit 0 if it loads and has them all
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

static std::string g_dir;
static void *fs_open(lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) {
    return mode == LV_FS_MODE_RD ? fopen((g_dir + "/" + path).c_str(), "rb") : nullptr;
}
static lv_fs_res_t fs_close(lv_fs_drv_t *, void *f) { fclose((FILE *)f); return LV_FS_RES_OK; }
static lv_fs_res_t fs_read(lv_fs_drv_t *, void *f, void *buf, uint32_t btr, uint32_t *br) {
    const size_t n = fread(buf, 1, btr, (FILE *)f);
    if (br) *br = (uint32_t)n;          // LVGL's fast path passes NULL here; theme_font.cpp guards the same way
    return LV_FS_RES_OK;
}
static lv_fs_res_t fs_seek(lv_fs_drv_t *, void *f, uint32_t pos, lv_fs_whence_t w) {
    const int wh = w == LV_FS_SEEK_SET ? SEEK_SET : w == LV_FS_SEEK_CUR ? SEEK_CUR : SEEK_END;
    return fseek((FILE *)f, (long)pos, wh) == 0 ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}
static lv_fs_res_t fs_tell(lv_fs_drv_t *, void *f, uint32_t *pos) { *pos = (uint32_t)ftell((FILE *)f); return LV_FS_RES_OK; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <dir> <font.bin> [hex codepoint]...\n", argv[0]); return 2; }
    g_dir = argv[1];
    lv_init();
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'F'; drv.open_cb = fs_open; drv.close_cb = fs_close; drv.read_cb = fs_read;
    drv.seek_cb = fs_seek; drv.tell_cb = fs_tell;
    lv_fs_drv_register(&drv);

    lv_font_t *f = lv_font_load((std::string("F:") + argv[2]).c_str());
    if (!f) { printf("lv_font_load failed\n"); return 3; }
    int missing = 0;
    for (int i = 3; i < argc; ++i) {
        const uint32_t cp = (uint32_t)strtoul(argv[i], nullptr, 16);
        lv_font_glyph_dsc_t d;
        if (!lv_font_get_glyph_dsc(f, &d, cp, 0)) { printf("missing U+%04X\n", cp); ++missing; }
    }
    printf("size %d, line height %d\n", (int)f->line_height, (int)f->line_height);
    lv_font_free(f);
    return missing ? 4 : 0;
}

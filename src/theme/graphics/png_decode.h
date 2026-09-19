#pragma once
#include <stddef.h>
#include <stdint.h>

// One PNG decoder for the whole firmware, and one place where "PNG bytes -> pixel buffer"
// is done.
//
// Every sprite loader used to keep its own PNGdec instance for good. sizeof(PNG) is about
// 51 KB (32 KB of zlib window, the inflate state, an 8 KB scanline buffer), so a boot that
// touched the clock, radar, menu, settings, intel, splash, plate and office loaders held
// most of half a megabyte of PSRAM for eight objects that never ran at the same time.
//
// This header deliberately does not include PNGdec.h. That library bundles zlib, whose
// `#define local static` breaks lvgl's lv_meter.h if lvgl is included afterwards, so the
// callers that need the decoder object include PNGdec.h themselves, first, as they always
// have.
class PNG;

namespace png_decode {

// How a truecolor+alpha PNG is stored. Anything that is not 8-bit RGBA decodes to zeros.
enum Format {
    FMT_RGB565,         // 2 B/px, little-endian, alpha discarded
    FMT_RGB565_ALPHA,   // 3 B/px: RGB565 low byte, RGB565 high byte, alpha
};

// A borrowed decoder. There is one resident instance, allocated on first use and kept, and
// it is handed out to whoever asks while it is free. If it is already out (the weather task
// on core 0 decoding a radar tile while the UI loads a plate on core 1) the second caller
// gets a temporary one that is freed on release, rather than waiting for the first. Nothing
// here blocks, so it cannot stall the render loop.
class Lease {
public:
    Lease();
    ~Lease();
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;

    PNG *get() const { return m_png; }
    explicit operator bool() const { return m_png != nullptr; }

private:
    PNG *m_png;
    bool m_resident;
};

// Decode `png` into a new PSRAM buffer sized for `fmt` and return it, with the image size in
// w/h. The caller frees it with heap_caps_free(). Returns nullptr on ANY failure, and on
// failure nothing is left allocated: the loaders it replaced returned false but left their
// output pointer aimed at a half-written buffer, which one of them then handed to LVGL as a
// finished image. `who` and `tag` only prefix the log line: "[who] tag: ...".
uint8_t *to_buffer(const uint8_t *png, uint32_t len, Format fmt, int &w, int &h,
                   const char *who, const char *tag);

}  // namespace png_decode

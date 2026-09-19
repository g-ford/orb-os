#include "png_decode.h"
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
#else
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <chrono>
static struct { void printf(const char *fmt, ...) const { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); } } Serial;
static void *heap_caps_malloc(size_t sz, int) { return malloc(sz); }
static void heap_caps_free(void *p) { free(p); }
static uint32_t millis() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#endif
#include <PNGdec.h>
#include <atomic>
#include <new>

namespace {

PNG                *s_resident = nullptr;
std::atomic<bool>   s_residentOut{false};

PNG *make_decoder() {
    void *mem = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return mem ? new (mem) PNG() : nullptr;
}

// Where the line callback writes. Passed through PNGdec's pUser so a decode carries its own
// destination instead of borrowing file-scope state, which is what made the old copies safe
// only "because every caller is on the UI thread".
struct Target {
    uint8_t *buf;
    int      w;
    bool     alpha;
};

int line_cb(PNGDRAW *draw) {
    const Target *t = (const Target *)draw->pUser;
    const uint8_t *src = draw->pPixels;
    const bool rgba = (draw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA && draw->iBpp == 8);
    if (t->alpha) {
        uint8_t *dst = t->buf + (size_t)draw->y * t->w * 3;
        for (int x = 0; x < draw->iWidth; ++x, dst += 3) {
            if (rgba) { const uint16_t v = (uint16_t)(((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | (src[2] >> 3)); dst[0] = v & 0xFF; dst[1] = v >> 8; dst[2] = src[3]; src += 4; }
            else { dst[0] = dst[1] = dst[2] = 0; }
        }
    } else {
        uint16_t *dst = (uint16_t *)t->buf + (size_t)draw->y * t->w;
        for (int x = 0; x < draw->iWidth; ++x) {
            if (rgba) { dst[x] = (uint16_t)(((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | (src[2] >> 3)); src += 4; }
            else dst[x] = 0;
        }
    }
    return 1;
}

}  // namespace

namespace png_decode {

Lease::Lease() : m_png(nullptr), m_resident(false) {
    if (!s_residentOut.exchange(true)) {
        if (!s_resident) s_resident = make_decoder();
        if (s_resident) { m_png = s_resident; m_resident = true; return; }
        s_residentOut.store(false);
    }
    m_png = make_decoder();
}

Lease::~Lease() {
    if (!m_png) return;
    if (m_resident) { s_residentOut.store(false); return; }
    m_png->~PNG();
    heap_caps_free(m_png);
}

uint8_t *to_buffer(const uint8_t *png, uint32_t len, Format fmt, int &w, int &h,
                   const char *who, const char *tag) {
    const uint32_t t0 = millis();
    Lease dec;
    if (!dec) { Serial.printf("[%s] %s: decoder alloc failed\n", who, tag); return nullptr; }
    const bool alpha = (fmt == FMT_RGB565_ALPHA);
    Target t = { nullptr, 0, alpha };
    if (dec.get()->openRAM((uint8_t *)png, len, line_cb) != PNG_SUCCESS) {
        Serial.printf("[%s] %s: open failed\n", who, tag);
        return nullptr;
    }
    w = dec.get()->getWidth();
    h = dec.get()->getHeight();
    const size_t bytes = (size_t)w * h * (alpha ? 3 : 2);
    uint8_t *out = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out) {
        Serial.printf("[%s] %s: buffer alloc failed\n", who, tag);
        dec.get()->close();
        return nullptr;
    }
    t.buf = out; t.w = w;
    const int r = dec.get()->decode(&t, 0);
    dec.get()->close();
    if (r != PNG_SUCCESS) {
        Serial.printf("[%s] %s: decode failed\n", who, tag);
        heap_caps_free(out);
        return nullptr;
    }
    Serial.printf("[%s] %s: decoded %dx%d (%u KB) in %u ms\n", who, tag, w, h,
                  (unsigned)(bytes / 1024), (unsigned)(millis() - t0));
    return out;
}

}  // namespace png_decode

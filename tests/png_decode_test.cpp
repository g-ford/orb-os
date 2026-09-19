// Host test for src/theme/graphics/png_decode.cpp, against the real PNGdec.
//   tests/run_png_decode_test.sh
//
// The three things the shared decoder changed, each of which was a bug or a hazard in the
// copies it replaced:
//   1. a PNG that opens but fails to decode must free its buffer and return nullptr
//      (the old loaders leaked it, and one then drew the half-written buffer);
//   2. pixels come out exactly as the loaders always produced them;
//   3. two holders at once each get a working decoder, and neither disturbs the other.
#include "png_decode.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef __APPLE__
#include <malloc/malloc.h>
static size_t live_blocks() { malloc_statistics_t s; malloc_zone_statistics(nullptr, &s); return s.blocks_in_use; }
#else
#include <malloc.h>
static size_t live_blocks() { struct mallinfo2 m = mallinfo2(); return m.uordblks; }
#endif

static std::vector<uint8_t> slurp(const char *dir, const char *name) {
    char path[512]; snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    std::vector<uint8_t> v;
    uint8_t buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) v.insert(v.end(), buf, buf + n);
    fclose(f);
    return v;
}

static uint16_t rgb565(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture dir>\n", argv[0]); return 2; }
    const char *dir = argv[1];
    const std::vector<uint8_t> rgba  = slurp(dir, "rgba_4x3.png");
    const std::vector<uint8_t> big   = slurp(dir, "noise_96x96.png");
    const std::vector<uint8_t> trunc = slurp(dir, "noise_96x96_truncated.png");
    const std::vector<uint8_t> pal   = slurp(dir, "palette_4x3.png");

    // -- 2. pixel format. Pixel (x,y) of the fixture is (x*60, y*80, 200-x*10) alpha 255-x*40.
    {
        int w = 0, h = 0;
        uint8_t *px = png_decode::to_buffer(rgba.data(), (uint32_t)rgba.size(), png_decode::FMT_RGB565, w, h, "test", "rgb565");
        assert(px && w == 4 && h == 3);
        const uint16_t *p = (const uint16_t *)px;
        for (int y = 0; y < 3; ++y) for (int x = 0; x < 4; ++x)
            assert(p[y * 4 + x] == rgb565(x * 60, y * 80, 200 - x * 10));
        free(px);

        px = png_decode::to_buffer(rgba.data(), (uint32_t)rgba.size(), png_decode::FMT_RGB565_ALPHA, w, h, "test", "alpha");
        assert(px && w == 4 && h == 3);
        for (int y = 0; y < 3; ++y) for (int x = 0; x < 4; ++x) {
            const uint8_t *q = px + ((size_t)y * 4 + x) * 3;
            const uint16_t v = rgb565(x * 60, y * 80, 200 - x * 10);
            assert(q[0] == (v & 0xFF) && q[1] == (v >> 8) && q[2] == 255 - x * 40);
        }
        free(px);

        // Not 8-bit RGBA: decodes to zeros, as before.
        px = png_decode::to_buffer(pal.data(), (uint32_t)pal.size(), png_decode::FMT_RGB565, w, h, "test", "palette");
        assert(px);
        for (size_t i = 0; i < (size_t)w * h * 2; ++i) assert(px[i] == 0);
        free(px);
    }

    // The full-size fixture decodes at all, so the truncated one below fails for the right reason.
    {
        int w = 0, h = 0;
        uint8_t *px = png_decode::to_buffer(big.data(), (uint32_t)big.size(), png_decode::FMT_RGB565, w, h, "test", "big");
        assert(px && w == 96 && h == 96);
        free(px);
    }

    // -- 1. failure frees what it allocated. Warm the resident decoder first so its one-off
    // allocation is not counted, then run many failing decodes: any per-call leak shows as
    // growth in live blocks proportional to the loop.
    {
        int w = 0, h = 0;
        free(png_decode::to_buffer(rgba.data(), (uint32_t)rgba.size(), png_decode::FMT_RGB565, w, h, "test", "warm"));
        const size_t before = live_blocks();
        for (int i = 0; i < 200; ++i) {
            uint8_t *px = png_decode::to_buffer(trunc.data(), (uint32_t)trunc.size(), png_decode::FMT_RGB565, w, h, "test", "trunc");
            assert(px == nullptr);
        }
        const size_t after = live_blocks();
        printf("live blocks across 200 failed decodes: %zu -> %zu\n", before, after);
        assert(after <= before + 4);   // slack for stdio/locale bookkeeping; a leak would be +200

        uint8_t *px = png_decode::to_buffer((const uint8_t *)"not a png at all", 16, png_decode::FMT_RGB565, w, h, "test", "junk");
        assert(px == nullptr);
    }

    // -- 3. the lease. One resident, a temporary when it is already out, and the resident
    // comes back after release.
    {
        PNG *first = nullptr;
        {
            png_decode::Lease a;
            assert(a);
            first = a.get();
            {
                png_decode::Lease b;                // resident is out -> a temporary
                assert(b && b.get() != a.get());
                png_decode::Lease c;                // and another
                assert(c && c.get() != b.get() && c.get() != a.get());
            }
            // The resident is still ours and still works while temporaries came and went.
            assert(a.get() == first);
        }
        png_decode::Lease again;                    // released -> the same resident is handed out
        assert(again && again.get() == first);

        // A decode while the resident is out uses the temporary and still succeeds.
        int w = 0, h = 0;
        uint8_t *px = png_decode::to_buffer(rgba.data(), (uint32_t)rgba.size(), png_decode::FMT_RGB565, w, h, "test", "held");
        assert(px && w == 4);
        free(px);
    }

    printf("png_decode: all checks passed\n");
    return 0;
}

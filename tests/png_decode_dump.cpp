// Decodes one PNG with the firmware's own decoder (src/theme/graphics/png_decode.cpp, over the real
// PNGdec) and writes the RGB565 pixels, little-endian, to a file. Used by tests/test_fallout_theme.py
// to compare what the device would draw with what the PNG says: the decoder mis-reads some valid
// streams, and only a run through it shows which.
//   png_decode_dump <in.png> <out.raw>      prints "<w>x<h>"
#include "png_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <in.png> <out.raw>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::vector<uint8_t> png;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) png.insert(png.end(), buf, buf + n);
    fclose(f);
    int w = 0, h = 0;
    uint8_t *px = png_decode::to_buffer(png.data(), (uint32_t)png.size(), png_decode::FMT_RGB565, w, h, "dump", "rgb565");
    if (!px) { fprintf(stderr, "decode failed\n"); return 3; }
    FILE *o = fopen(argv[2], "wb");
    fwrite(px, 2, (size_t)w * h, o);
    fclose(o);
    free(px);
    printf("%dx%d\n", w, h);
    return 0;
}

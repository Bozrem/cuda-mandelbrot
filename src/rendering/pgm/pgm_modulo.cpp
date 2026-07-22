#include "pgm.h"

#include <cstdio>
#include <cstdlib>

void save_as_pgm_modulo(const char* path, const iter_frame_t frame, float band_density) {
    FILE* out = std::fopen(path, "wb");
    if (out == nullptr) {
        std::fprintf(stderr, "Failed to open %s for writing\n", path);
        std::exit(EXIT_FAILURE);
    }

    std::fprintf(out, "P5\n%u %u\n255\n", WIDTH, HEIGHT);

    for (uint16_t y = 0; y < HEIGHT; y++) {
        for (uint16_t x = 0; x < WIDTH; x++) {
            unsigned char pixel = (frame[y][x] == -1.0f) ?
                                  0 :
                                  (int)(frame[y][x] * band_density) % 256;
            std::fputc(pixel, out);
        }
    }

    std::fclose(out);
}

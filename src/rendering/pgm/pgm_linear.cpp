#include "pgm.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

void save_as_pgm_linear(const char* path, const iter_frame_t frame, float value_range) {
    FILE* out = std::fopen(path, "wb");
    if (out == nullptr) {
        std::fprintf(stderr, "Failed to open %s for writing\n", path);
        std::exit(EXIT_FAILURE);
    }

    std::fprintf(out, "P5\n%u %u\n255\n", WIDTH, HEIGHT);

    for (uint16_t y = 0; y < HEIGHT; y++) {
        for (uint16_t x = 0; x < WIDTH; x++) {
            unsigned char pixel = 0;
            if (frame[y][x] != -1.0f) {
                float t = frame[y][x] / value_range;
                t = std::clamp(t, 0.0f, 1.0f);
                pixel = (unsigned char)(t * 255.0f);
            }
            std::fputc(pixel, out);
        }
    }

    std::fclose(out);
}

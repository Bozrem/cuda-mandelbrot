#include "pgm.h"

#include <cstdio>
#include <cstdlib>

void save_as_pgm(const char* path, const frame_t frame) {
    FILE* out = std::fopen(path, "wb");
    if (out == nullptr) {
        std::fprintf(stderr, "Failed to open %s for writing\n", path);
        std::exit(EXIT_FAILURE);
    }

    std::fprintf(out, "P5\n%u %u\n255\n", WIDTH, HEIGHT);

    for (uint16_t y = 0; y < HEIGHT; y++) {
        for (uint16_t x = 0; x < WIDTH; x++) {
            float t = frame[y][x] / static_cast<float>(MAX_ITERATIONS);
            if (t < 0.0f) {
                t = 0.0f;
            }
            if (t > 1.0f) {
                t = 1.0f;
            }
            unsigned char pixel = static_cast<unsigned char>(t * 255.0f);
            std::fputc(pixel, out);
        }
    }

    std::fclose(out);
}

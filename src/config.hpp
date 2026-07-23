#pragma once

#include <cstdint>

// General
inline constexpr uint16_t WIDTH = 3840;
inline constexpr uint16_t HEIGHT = 2160;
inline constexpr int BLOCK_DIM = 16; // kernel block; warp 2x2 chroma shuffle assumes this == 16
inline constexpr double CENTER_X = -0.7746806106269039;
inline constexpr double CENTER_Y = 0.1374168856037867;

// Exact grid coverage (no per-thread OOB guard) + 4:2:0 + 16-wide warp pairing
static_assert(WIDTH % BLOCK_DIM == 0, "WIDTH must be divisible by BLOCK_DIM");
static_assert(HEIGHT % BLOCK_DIM == 0, "HEIGHT must be divisible by BLOCK_DIM");
static_assert(WIDTH % 2 == 0 && HEIGHT % 2 == 0, "4:2:0 requires even WIDTH and HEIGHT");
static_assert(BLOCK_DIM == 16, "chroma shuffle uses lane xor 16; requires 16-wide blocks");

// Animation
inline constexpr uint16_t FPS = 5;
inline constexpr float DURATION_SEC = 44.0f;
inline constexpr float MAGNIFICATION_PER_SEC = 2.0f; // zoom multiplier each second

// Escape alg (bailout radius 128, stored squared for the |z|^2 compare)
inline constexpr double ESCAPE_THRES2 = 128.0 * 128.0;

inline constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;

// Continuous iteration / escape values (host layout).
typedef float iter_frame_t[HEIGHT][WIDTH];

// P010: 10-bit YUV 4:2:0 semi-planar, samples stored in uint16 (high 10 bits).
// y  — full-res luma
// uv — interleaved CbCr, half height, same line width in samples as luma
inline constexpr uint16_t CHROMA_HEIGHT = HEIGHT / 2;
inline constexpr uint32_t LUMA_COUNT = PIXEL_COUNT;
inline constexpr uint32_t CHROMA_SAMPLE_COUNT = (uint32_t)WIDTH * CHROMA_HEIGHT;

typedef struct {
    uint16_t y[HEIGHT][WIDTH];
    uint16_t uv[CHROMA_HEIGHT][WIDTH];
} p010_frame_t;



// Color Palettes
struct Palette {
    float ax, ay, az;
    float bx, by, bz;
    float cx, cy, cz;
    float dx, dy, dz;
};

constexpr Palette PALETTE_FIRE_ICE = {
    0.5f, 0.5f, 0.5f,       // A
    0.5f, 0.5f, 0.5f,       // B
    1.0f, 1.0f, 1.0f,       // C
    0.0f, 0.33f, 0.67f      // D
};

constexpr Palette PALETTE_SUNSET = {
    0.5f, 0.5f, 0.5f,       // A: Offset
    0.5f, 0.5f, 0.5f,       // B: Amplitude
    1.0f, 1.0f, 1.0f,       // C: Frequency
    0.0f, 0.10f, 0.20f      // D: Phase shift
};

constexpr Palette ACTIVE_PALETTE = PALETTE_SUNSET;
constexpr float PALETTE_CYCLE_LENGTH = 50.0f;

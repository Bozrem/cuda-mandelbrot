#pragma once

#include <cstdint>

// General
inline constexpr uint16_t WIDTH = 3840;
inline constexpr uint16_t HEIGHT = 2160;
inline constexpr float CENTER_X = -0.77468f;
inline constexpr float CENTER_Y = -0.13741f;

// Escape alg
inline constexpr uint16_t ESCAPE_THRES2 = 128 * 128; // Threshold is 128, but we need it squared in the alg

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
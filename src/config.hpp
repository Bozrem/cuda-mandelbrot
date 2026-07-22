#pragma once

#include <cstdint>

// General
inline constexpr uint16_t WIDTH = 3840;
inline constexpr uint16_t HEIGHT = 2160;
inline constexpr float CENTER_X = -0.77468f;
inline constexpr float CENTER_Y = -0.13741f;

// Escape alg
inline constexpr uint16_t ESCAPE_THRES2 = 128 * 128; // Threshold is 128, but we need it squared in the alg

// Coloring
inline constexpr float PGM_BAND_DENSITY = 10.0f;


inline constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;

typedef float frame_t[HEIGHT][WIDTH];

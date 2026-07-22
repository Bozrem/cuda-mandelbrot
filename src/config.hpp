#pragma once

#include <cstdint>

inline constexpr uint16_t WIDTH = 3840;
inline constexpr uint16_t HEIGHT = 2160;
inline constexpr uint16_t MAX_ITERATIONS = 512;
inline constexpr float CENTER_X = -0.77468f;
inline constexpr float CENTER_Y = -0.13741f;

inline constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;

typedef float frame_t[HEIGHT][WIDTH];

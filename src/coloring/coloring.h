#pragma once

#include "config.hpp"

// Device-resident populator: maps a VRAM iteration frame into a blank P010
// frame (luma + interleaved chroma). Caller owns both buffers.
void populate_color(const float* d_iter, p010_frame_t* d_p010);

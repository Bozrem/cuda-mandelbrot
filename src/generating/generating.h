#pragma once

#include "config.hpp"

// Device-resident populator: writes continuous escape values into a
// pre-allocated device buffer. Caller owns allocation, residency, and any
// later copies or sinks — this never touches host memory.
void generate_escape(float* d_iter, float zoom, float max_iterations);

// Host-pipeline convenience: allocates device scratch, populates via
// generate_escape, copies D→H, then frees. Prefer generate_escape when the
// frame should stay in VRAM.
void render_escape_frame_host(iter_frame_t out, float zoom, float max_iterations);

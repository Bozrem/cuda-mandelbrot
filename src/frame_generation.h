#pragma once

#include "config.hpp"

// Device-resident fused pipeline: escape + smooth + palette AA + PQ/Rec.2020
// into a pre-allocated P010 frame. Caller owns allocation and residency.
void generate_fused(p010_frame_t* d_p010, float zoom, int max_iterations);

// Host-pipeline convenience: allocates device scratch, populates via
// generate_fused, copies D→H, then frees. Prefer generate_fused when the
// frame should stay in VRAM.
void render_fused_frame_host(p010_frame_t* h_p010, float zoom, int max_iterations);

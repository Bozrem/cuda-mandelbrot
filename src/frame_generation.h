#pragma once

#include "config.hpp"

// Device-resident fused pipeline: escape + smooth + palette AA + PQ/Rec.2020
// into a pre-allocated P010 frame. Caller owns allocation and residency.
// Escape / pixel mapping use FP64; palette and HDR encode stay FP32.
void generate_fused(p010_frame_t* d_p010, double zoom, int max_iterations);

#pragma once

#include "config.hpp"

// Device-resident smoother: modifies a VRAM P010 frame in place.
void smooth_p010(p010_frame_t* d_p010);

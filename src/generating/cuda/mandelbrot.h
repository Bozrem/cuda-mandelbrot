#pragma once

#include "config.hpp"

// Legacy wrapper: allocates, launches, DtoH, frees.
// Target API is generating::generate_escape in ../generating.h
// (device write only — main owns copies).
void render_escape_frame(frame_t out, float zoom);

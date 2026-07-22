#pragma once

#include "config.hpp"

// Modulo banding: multiplies continuous iteration by band_density, then wraps into 0–255.
void save_as_pgm_modulo(const char* path, const iter_frame_t frame, float band_density);

// Linear map: scales continuous iteration by value_range into 0–255.
// value_range should be the theoretical max continuous-iteration value
// (see main for the max_iterations / escape-threshold derivation).
void save_as_pgm_linear(const char* path, const iter_frame_t frame, float value_range);

#pragma once

#include <cuda_runtime.h>

#include <cstdio>
#include <cstdlib>

#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t _err = (call);                                             \
        if (_err != cudaSuccess) {                                             \
            std::fprintf(stderr, "CUDA error (%s): %s\n", #call,               \
                         cudaGetErrorString(_err));                            \
            std::exit(EXIT_FAILURE);                                           \
        }                                                                      \
    } while (0)

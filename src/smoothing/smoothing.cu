#include "smoothing/smoothing.h"
#include "cuda_util.h"

static __global__ void smooth_frame(p010_frame_t* p010) {
    uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    if (x >= WIDTH || y >= HEIGHT) {
        return;
    }

    // Identity for now — framework launch only.
    (void)p010;
}

void smooth_p010(p010_frame_t* d_p010) {
    dim3 block_dim(16, 16);
    dim3 grid_dim(
        (WIDTH + block_dim.x - 1) / block_dim.x,
        (HEIGHT + block_dim.y - 1) / block_dim.y
    );

    smooth_frame<<<grid_dim, block_dim>>>(d_p010);
    CUDA_CHECK(cudaGetLastError());
}

#include "generating/generating.h"
#include "cuda_util.h"

static __global__ void mandelbrot_frame(float* p, float zoom, float max_iterations) {
    uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    // Out of bounds protection. If a sync is needed, call here too
    if (x >= WIDTH || y >= HEIGHT) {
        return;
    }

    float cx = CENTER_X + (x - WIDTH / 2.0f) / zoom;
    float cy = CENTER_Y - (y - HEIGHT / 2.0f) / zoom;

    // Register optimization: use xy addressing early, can now remove x and y regs
    p += (y * WIDTH) + x;

    float zx = 0.0f;
    float zy = 0.0f;
    float zx2 = 0.0f;
    float zy2 = 0.0f;

    int i = 0;

    // Optimized Escape Time Algorithm
    for (; i < max_iterations && zx2 + zy2 <= ESCAPE_THRES2; i++) {
        zy = 2 * zx * zy + cy;
        zx = zx2 - zy2 + cx;
        zx2 = zx * zx;
        zy2 = zy * zy;
    }

    // Continuous Iteration (smoothing)
    float nu = (float)i + 1.0f - __log2f(0.5f * __log2f(zx2 + zy2));
    *p = (i == max_iterations) ? -1.0f : nu; // this should be a sel operation that doesn't diverge the warp
}

void generate_escape(float* d_iter, float zoom, float max_iterations) {
    dim3 block_dim(16, 16);
    dim3 grid_dim(
        (WIDTH + block_dim.x - 1) / block_dim.x,
        (HEIGHT + block_dim.y - 1) / block_dim.y
    );

    mandelbrot_frame<<<grid_dim, block_dim>>>(d_iter, zoom, max_iterations);
    CUDA_CHECK(cudaGetLastError());
}

void render_escape_frame_host(iter_frame_t h_iter, float zoom, float max_iterations) {
    float* d_iter = nullptr;

    CUDA_CHECK(cudaMalloc((void **)&d_iter, PIXEL_COUNT * sizeof(float)));

    generate_escape(d_iter, zoom, max_iterations);

    CUDA_CHECK(cudaMemcpy(
        h_iter,
        d_iter,
        PIXEL_COUNT * sizeof(float),
        cudaMemcpyDeviceToHost
    ));

    CUDA_CHECK(cudaFree(d_iter));
}

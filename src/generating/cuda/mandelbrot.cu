#include "mandelbrot.h"

#include <cstdio>
#include <cstdlib>

static void check_cuda(cudaError_t err, const char* what) {
    if (err == cudaSuccess) {
        return;
    }
    std::fprintf(stderr, "CUDA error (%s): %s\n", what, cudaGetErrorString(err));
    std::exit(EXIT_FAILURE);
}

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

void render_escape_frame(frame_t h_frame, float zoom, float max_iterations) {
    float* d_frame = nullptr;

    check_cuda(
        cudaMalloc(
            (void **)&d_frame,
            PIXEL_COUNT * sizeof(float)
        ),
        "cudaMalloc"
    );

    dim3 block_dim(16, 16);
    dim3 grid_dim(
        (WIDTH + block_dim.x - 1) / block_dim.x,
        (HEIGHT + block_dim.y - 1) / block_dim.y
    );

    mandelbrot_frame<<<grid_dim, block_dim>>>(d_frame, zoom, max_iterations);
    check_cuda(cudaGetLastError(), "mandelbrot_frame launch");
    check_cuda(cudaDeviceSynchronize(), "mandelbrot_frame sync");

    check_cuda(
        cudaMemcpy(
            h_frame,
            d_frame,
            PIXEL_COUNT * sizeof(float),
            cudaMemcpyDeviceToHost
        ),
        "cudaMemcpy DtoH"
    );

    check_cuda(cudaFree(d_frame), "cudaFree");
}

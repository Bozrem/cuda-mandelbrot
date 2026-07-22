#include "frame_generation.h"
#include "cuda_util.h"

__device__ __forceinline__ float get_smoothed_iter(float cx, float cy, int max_iterations) {
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
    return (i == max_iterations) ? -1.0f : nu; // this should be a sel operation that doesn't diverge the warp
}


// SMPTE ST 2084 (PQ Curve) transfer function
__device__ __forceinline__ float apply_pq(float color_channel) {
    // 1. Scale 0.0-1.0 to nits (Assuming 1.0 = 1000 nits. Max PQ is 10000 nits, so 1000/10000 = 0.1f)
    float L = color_channel * 0.1f;
    
    // 2. PQ Constants
    const float m1 = 0.15930175f;
    const float m2 = 78.84375f;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;

    // 3. Apply transfer math using fast hardware power intrinsic
    float L_m1 = __powf(L, m1);
    float num = c1 + c2 * L_m1;
    float den = 1.0f + c3 * L_m1;
    
    return __powf(num / den, m2);
}

// Palette + live accumulate. Optionally track R range for the chaos heuristic.
__device__ __forceinline__ void accumulate_sample(
    float nu,
    float& sum_r, float& sum_g, float& sum_b,
    float* min_r, float* max_r
) {
    float r, g, b;
    if (nu == -1.0f) {
        r = g = b = 0.0f;
    } else {
        float t = nu / PALETTE_CYCLE_LENGTH;
        r = ACTIVE_PALETTE.ax + ACTIVE_PALETTE.bx * __cosf(6.28318f * (ACTIVE_PALETTE.cx * t + ACTIVE_PALETTE.dx));
        g = ACTIVE_PALETTE.ay + ACTIVE_PALETTE.by * __cosf(6.28318f * (ACTIVE_PALETTE.cy * t + ACTIVE_PALETTE.dy));
        b = ACTIVE_PALETTE.az + ACTIVE_PALETTE.bz * __cosf(6.28318f * (ACTIVE_PALETTE.cz * t + ACTIVE_PALETTE.dz));
    }

    sum_r += r;
    sum_g += g;
    sum_b += b;

    if (min_r != nullptr) {
        *min_r = fminf(*min_r, r);
        *max_r = fmaxf(*max_r, r);
    }
}


static __global__ void mandelbrot_frame(p010_frame_t* p010, float zoom, int max_iterations) {
    uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    // Out of bounds protection. If a sync is needed, call here too
    if (x >= WIDTH || y >= HEIGHT) return;

    float pixel_w = 1.0f / zoom;
    float offset = pixel_w / 3.0f;
    
    float base_cx = CENTER_X + (x + 0.5f - WIDTH / 2.0f) / zoom;
    float base_cy = CENTER_Y - (y + 0.5f - HEIGHT / 2.0f) / zoom;

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    float min_r = 1e30f, max_r = -1e30f;

    // Center + 4 corners (track R range for chaos check)
    float nu = get_smoothed_iter(base_cx, base_cy, max_iterations); // Center
    accumulate_sample(nu, sum_r, sum_g, sum_b, &min_r, &max_r);

    nu = get_smoothed_iter(base_cx - offset, base_cy + offset, max_iterations); // Top left
    accumulate_sample(nu, sum_r, sum_g, sum_b, &min_r, &max_r);

    nu = get_smoothed_iter(base_cx + offset, base_cy + offset, max_iterations); // Top right
    accumulate_sample(nu, sum_r, sum_g, sum_b, &min_r, &max_r);

    nu = get_smoothed_iter(base_cx - offset, base_cy - offset, max_iterations); // Bottom left
    accumulate_sample(nu, sum_r, sum_g, sum_b, &min_r, &max_r);

    nu = get_smoothed_iter(base_cx + offset, base_cy - offset, max_iterations); // Bottom right
    accumulate_sample(nu, sum_r, sum_g, sum_b, &min_r, &max_r);

    float samples = 5.0f;

    // The threshold should be as high as we can make it without inducing visual artifacts
    if ((max_r - min_r) > 0.025f) {
        nu = get_smoothed_iter(base_cx, base_cy + offset, max_iterations); // Top midpoint
        accumulate_sample(nu, sum_r, sum_g, sum_b, nullptr, nullptr);

        nu = get_smoothed_iter(base_cx, base_cy - offset, max_iterations); // Bottom midpoint
        accumulate_sample(nu, sum_r, sum_g, sum_b, nullptr, nullptr);

        nu = get_smoothed_iter(base_cx - offset, base_cy, max_iterations); // Left midpoint
        accumulate_sample(nu, sum_r, sum_g, sum_b, nullptr, nullptr);

        nu = get_smoothed_iter(base_cx + offset, base_cy, max_iterations); // Right midpoint
        accumulate_sample(nu, sum_r, sum_g, sum_b, nullptr, nullptr);

        samples = 9.0f;
    }

    // Normalize the accumulators
    float r = fmaxf(0.0f, fminf(1.0f, sum_r / samples));
    float g = fmaxf(0.0f, fminf(1.0f, sum_g / samples));
    float b = fmaxf(0.0f, fminf(1.0f, sum_b / samples));    

    // Apply PQ curve independently to R, G, and B
    float r_pq = apply_pq(r);
    float g_pq = apply_pq(g);
    float b_pq = apply_pq(b);

    // Apply Rec. 2020 Matrix (Linear combination of the PQ values)
    float Y =  0.2627f * r_pq + 0.6780f * g_pq + 0.0593f * b_pq;
    float U = -0.1396f * r_pq - 0.3604f * g_pq + 0.5000f * b_pq;
    float V =  0.5000f * r_pq - 0.4598f * g_pq - 0.0402f * b_pq;

    // Scale to clamped 10 bit range
    float y_scaled = fmaxf(64.0f, fminf(940.0f, 64.0f + Y * 876.0f));
    float u_scaled = fmaxf(64.0f,  fminf(960.0f, 512.0f + U * 896.0f));
    float v_scaled = fmaxf(64.0f,  fminf(960.0f, 512.0f + V * 896.0f));

    // Round to uint16 and make it left aligned (per P010 spec)
    uint16_t y_out = __float2uint_rn(y_scaled) << 6;
    uint16_t u_out = __float2uint_rn(u_scaled) << 6;
    uint16_t v_out = __float2uint_rn(v_scaled) << 6;

    // Write values (y[HEIGHT][WIDTH], uv[CHROMA_HEIGHT][WIDTH] with UVUV interleave)
    p010->y[y][x] = y_out;

    // Chroma is 4:2:0 subsampled. Only top-left thread of a 2x2 block writes U and V
    if ((x % 2 == 0) && (y % 2 == 0)) {
        p010->uv[y / 2][x] = u_out; // Does the compiler combine these?
        p010->uv[y / 2][x + 1] = v_out;
    }
}

void generate_fused(p010_frame_t* d_p010, float zoom, int max_iterations) {
    dim3 block_dim(16, 16);
    dim3 grid_dim(
        (WIDTH + block_dim.x - 1) / block_dim.x,
        (HEIGHT + block_dim.y - 1) / block_dim.y
    );

    mandelbrot_frame<<<grid_dim, block_dim>>>(d_p010, zoom, max_iterations);
    CUDA_CHECK(cudaGetLastError());
}

void render_fused_frame_host(p010_frame_t* h_p010, float zoom, int max_iterations) {
    p010_frame_t* d_p010 = nullptr;

    CUDA_CHECK(cudaMalloc((void **)&d_p010, sizeof(p010_frame_t)));

    generate_fused(d_p010, zoom, max_iterations);

    CUDA_CHECK(cudaMemcpy(
        h_p010,
        d_p010,
        sizeof(p010_frame_t),
        cudaMemcpyDeviceToHost
    ));

    CUDA_CHECK(cudaFree(d_p010));
}
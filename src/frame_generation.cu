#include "frame_generation.h"
#include "cuda_util.h"

__device__ __forceinline__ float3& operator+=(float3& a, float3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

__device__ __forceinline__ float get_smoothed_iter(double cx, double cy, int max_iterations) {
    double zx = 0.0;
    double zy = 0.0;
    double zx2 = 0.0;
    double zy2 = 0.0;

    int i = 0;

    // Optimized Escape Time Algorithm (FP64 — depth past ~1e7–1e8 zoom)
    for (; i < max_iterations && zx2 + zy2 <= ESCAPE_THRES2; i++) {
        zy = 2.0 * zx * zy + cy;
        zx = zx2 - zy2 + cx;
        zx2 = zx * zx;
        zy2 = zy * zy;
    }

    // Continuous iteration (smoothing); log2 is the double-precision math API
    const float nu = static_cast<float>(i) + 1.0f
                   - static_cast<float>(log2(0.5 * log2(zx2 + zy2)));
    return (i == max_iterations) ? -1.0f : nu;
}


// SMPTE ST 2084 (PQ Curve) transfer function
__device__ __forceinline__ float apply_pq(float color_channel) {
    float L = color_channel * 0.07f;
    
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

__device__ __forceinline__ float3 apply_palette(float nu) {
    if (nu < 0.0f) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }

    float t = nu / PALETTE_CYCLE_LENGTH;
    return make_float3(
        ACTIVE_PALETTE.ax + ACTIVE_PALETTE.bx * __cosf(6.28318f * (ACTIVE_PALETTE.cx * t + ACTIVE_PALETTE.dx)),
        ACTIVE_PALETTE.ay + ACTIVE_PALETTE.by * __cosf(6.28318f * (ACTIVE_PALETTE.cy * t + ACTIVE_PALETTE.dy)),
        ACTIVE_PALETTE.az + ACTIVE_PALETTE.bz * __cosf(6.28318f * (ACTIVE_PALETTE.cz * t + ACTIVE_PALETTE.dz))
    );
}

static __global__ void mandelbrot_frame(p010_frame_t* p010, double zoom, int max_iterations) {
    const uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    const uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    // Center + 4 corners + 4 edge midpoints, in units of `offset`
    const double aa_offsets[9][2] = {
        { 0.0,  0.0},
        {-1.0,  1.0}, { 1.0,  1.0}, {-1.0, -1.0}, { 1.0, -1.0},
        { 0.0,  1.0}, { 0.0, -1.0}, {-1.0,  0.0}, { 1.0,  0.0},
    };

    const double offset = (1.0 / zoom) / 3.0;
    const double base_cx = CENTER_X + (static_cast<double>(x) + 0.5 - WIDTH / 2.0) / zoom;
    const double base_cy = CENTER_Y - (static_cast<double>(y) + 0.5 - HEIGHT / 2.0) / zoom;

    float3 sum_rgb = make_float3(0.0f, 0.0f, 0.0f);
    for (int s = 0; s < 9; s++) {
        const double cx = base_cx + aa_offsets[s][0] * offset;
        const double cy = base_cy + aa_offsets[s][1] * offset;
        sum_rgb += apply_palette(get_smoothed_iter(cx, cy, max_iterations));
    }

    const float r = fmaxf(0.0f, fminf(1.0f, sum_rgb.x / 9.0f));
    const float g = fmaxf(0.0f, fminf(1.0f, sum_rgb.y / 9.0f));
    const float b = fmaxf(0.0f, fminf(1.0f, sum_rgb.z / 9.0f));

    // Apply PQ curve independently to R, G, and B (HDR10 NCL)
    const float r_pq = apply_pq(r);
    const float g_pq = apply_pq(g);
    const float b_pq = apply_pq(b);

    // Apply Rec. 2020 Matrix (linear combination of the PQ values)
    const float Y =  0.2627f * r_pq + 0.6780f * g_pq + 0.0593f * b_pq;
    const float U = -0.1396f * r_pq - 0.3604f * g_pq + 0.5000f * b_pq;
    const float V =  0.5000f * r_pq - 0.4598f * g_pq - 0.0402f * b_pq;

    // Scale to clamped 10 bit range
    const float y_scaled = fmaxf(64.0f, fminf(940.0f, 64.0f + Y * 876.0f));
    float u_scaled = fmaxf(64.0f,  fminf(960.0f, 512.0f + U * 896.0f));
    float v_scaled = fmaxf(64.0f,  fminf(960.0f, 512.0f + V * 896.0f));

    // 2x2 box-average chroma within the warp (16-wide rows → vertical neighbor is lane±16)
    u_scaled += __shfl_xor_sync(0xffffffff, u_scaled, 1);
    u_scaled += __shfl_xor_sync(0xffffffff, u_scaled, BLOCK_DIM);
    u_scaled *= 0.25f;

    v_scaled += __shfl_xor_sync(0xffffffff, v_scaled, 1);
    v_scaled += __shfl_xor_sync(0xffffffff, v_scaled, BLOCK_DIM);
    v_scaled *= 0.25f;

    // Round to uint16 and make it left aligned (per P010 spec)
    const uint16_t y_out = __float2uint_rn(y_scaled) << 6;
    const uint16_t u_out = __float2uint_rn(u_scaled) << 6;
    const uint16_t v_out = __float2uint_rn(v_scaled) << 6;

    p010->y[y][x] = y_out;

    // Chroma is 4:2:0 subsampled. Top-left of each 2x2 writes the averaged U/V.
    if ((threadIdx.x & 1) == 0 && (threadIdx.y & 1) == 0) {
        p010->uv[y / 2][x] = u_out;
        p010->uv[y / 2][x + 1] = v_out;
    }
}

void generate_fused(p010_frame_t* d_p010, double zoom, int max_iterations) {
    dim3 block_dim(BLOCK_DIM, BLOCK_DIM);
    dim3 grid_dim(WIDTH / BLOCK_DIM, HEIGHT / BLOCK_DIM);

    mandelbrot_frame<<<grid_dim, block_dim>>>(d_p010, zoom, max_iterations);
    CUDA_CHECK(cudaGetLastError());
}

#include "coloring/coloring.h"
#include "cuda_util.h"

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

// Primary kernel: applies Cosine Palette, does PQ curve, converts to REC2020, then writes YUV data
static __global__ void color_frame(const float* iter, p010_frame_t* p010) {
    uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    if (x >= WIDTH || y >= HEIGHT) {
        return;
    }

    float smoothed_iters = iter[(y * WIDTH) + x];

    // decide rgb value
    float r, g, b;

    if (smoothed_iters == -1.0f) {
        r = g = b = 0.0f;
    } else {
        float palette_phase = smoothed_iters / PALETTE_CYCLE_LENGTH;

        r = ACTIVE_PALETTE.ax + ACTIVE_PALETTE.bx * __cosf(6.28318f * (ACTIVE_PALETTE.cx * palette_phase + ACTIVE_PALETTE.dx));
        g = ACTIVE_PALETTE.ay + ACTIVE_PALETTE.by * __cosf(6.28318f * (ACTIVE_PALETTE.cy * palette_phase + ACTIVE_PALETTE.dy));
        b = ACTIVE_PALETTE.az + ACTIVE_PALETTE.bz * __cosf(6.28318f * (ACTIVE_PALETTE.cz * palette_phase + ACTIVE_PALETTE.dz));
    }

    // DEBUG CODE: SET TO PURE RED AND TEST YUV MAP
    r = 1.0f; 
    g = 0.0f; 
    b = 0.0f;

    // Apply PQ curve independently to R, G, and B
    float r_pq = apply_pq(r);
    float g_pq = apply_pq(g);
    float b_pq = apply_pq(b);

    // Apply Rec. 2020 Matrix (Linear combination of the PQ values)
    float Y =  0.2627f * r_pq + 0.6780f * g_pq + 0.0593f * b_pq;
    float U = -0.1396f * r_pq - 0.3604f * g_pq + 0.5000f * b_pq;
    float V =  0.5000f * r_pq - 0.4598f * g_pq - 0.0402f * b_pq;

    // Scale to clamped 10 bit range
    float y_scaled = fmaxf(256.0f, fminf(940.0f, 256.0f + Y * 684.0f));
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

void populate_color(const float* d_iter, p010_frame_t* d_p010) {
    dim3 block_dim(16, 16);
    dim3 grid_dim(
        (WIDTH + block_dim.x - 1) / block_dim.x,
        (HEIGHT + block_dim.y - 1) / block_dim.y
    );

    color_frame<<<grid_dim, block_dim>>>(d_iter, d_p010);
    CUDA_CHECK(cudaGetLastError());
}

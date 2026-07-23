#include "frame_generation.h"
#include "cuda_util.h"
#include "NvencSink.hpp"

#include <cuda.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CU_CHECK(call)                                                        \
    do {                                                                      \
        CUresult _res = (call);                                               \
        if (_res != CUDA_SUCCESS) {                                           \
            const char* _err_str = nullptr;                                   \
            cuGetErrorString(_res, &_err_str);                                \
            std::fprintf(stderr, "CUDA driver error (%s): %s\n", #call,       \
                         _err_str ? _err_str : "unknown");                    \
            std::exit(EXIT_FAILURE);                                          \
        }                                                                     \
    } while (0)


int main() {
    constexpr double ZOOM_START = 500.0;
    const uint32_t TOTAL_FRAMES = static_cast<uint32_t>(FPS * DURATION_SEC);
    // Per-frame zoom multiplier so MAGNIFICATION_PER_SEC holds over one second.
    const double zoom_per_frame = std::pow(static_cast<double>(MAGNIFICATION_PER_SEC), 1.0 / FPS);

    // NVENC needs a CUDA *driver* API context. Creating it explicitly (rather than
    // letting the CUDA runtime lazily create its own primary context on first use)
    // means the runtime calls below (cudaMalloc/cudaMemcpy2D) and NvEncoderCuda both
    // operate on the same context.
    CU_CHECK(cuInit(0));
    CUdevice cu_device = 0;
    CU_CHECK(cuDeviceGet(&cu_device, 0));
    CUcontext cu_context = nullptr;
    CU_CHECK(cuCtxCreate(&cu_context, 0, cu_device));

    p010_frame_t* d_p010 = nullptr;
    CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&d_p010), sizeof(p010_frame_t)));

    try {
        // Scoped so ~NvencSink() (flush + close) runs before we tear down the CUDA context below.
        {
            NvencSink sink(cu_context, "output_hdr.mkv");

            double zoom = ZOOM_START;
            for (uint32_t frame = 0; frame < TOTAL_FRAMES; frame++) {
                int max_iterations = static_cast<int>(100.0 + 12.0 * std::log2(zoom));

                // Linear pipeline for now: generate, then encode, one frame at a time.
                // No overlap between GPU render and NVENC submission yet — slow but simple.
                generate_fused(d_p010, zoom, max_iterations);
                sink.queue_frame(d_p010); // frame stays device-resident the whole way through

                zoom *= zoom_per_frame;

                const uint32_t done = frame + 1;
                std::fprintf(stderr, "\r[%3u%%] frame %u/%u  zoom=%.3g  iters=%d",
                             (done * 100) / TOTAL_FRAMES, done, TOTAL_FRAMES,
                             zoom / zoom_per_frame, max_iterations);
                std::fflush(stderr);
            }
            std::fprintf(stderr, "\n");
        } // ~NvencSink() flushes remaining frames and finishes the Matroska mux here

        std::printf("Wrote output_hdr.mkv (%u frames @ %u fps)\n", TOTAL_FRAMES, FPS);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Encode failed: %s\n", e.what());
        CUDA_CHECK(cudaFree(d_p010));
        CU_CHECK(cuCtxDestroy(cu_context));
        return EXIT_FAILURE;
    }

    CUDA_CHECK(cudaFree(d_p010));
    CU_CHECK(cuCtxDestroy(cu_context));

    return 0;
}

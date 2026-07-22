#include "frame_generation.h"
#include "cuda_util.h"

#include <cstdio>

// Device pipeline dump: fused generate → write raw P010 for ffmpeg preview.

int main() {
    float zoom = 10000.0f;
    float max_iterations = 100.0f;

    p010_frame_t* d_p010 = nullptr;
    CUDA_CHECK(cudaMalloc((void **)&d_p010, sizeof(p010_frame_t)));

    generate_fused(d_p010, zoom, max_iterations);

    static p010_frame_t h_p010;
    CUDA_CHECK(cudaMemcpy(&h_p010, d_p010, sizeof(p010_frame_t),
                          cudaMemcpyDeviceToHost));

    FILE* f = std::fopen("frame.p010", "wb");
    if (!f) {
        std::perror("fopen frame.p010");
        std::exit(EXIT_FAILURE);
    }
    if (std::fwrite(&h_p010, 1, sizeof(h_p010), f) != sizeof(h_p010)) {
        std::fprintf(stderr, "fwrite failed for frame.p010\n");
        std::exit(EXIT_FAILURE);
    }
    std::fclose(f);
    std::printf("Wrote frame.p010 (%zu bytes)\n", sizeof(h_p010));

    CUDA_CHECK(cudaFree(d_p010));
    return 0;
}

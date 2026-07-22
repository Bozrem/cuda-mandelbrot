#include "frame_generation.h"
#include "cuda_util.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

int main() {
    constexpr float ZOOM_START = 500.0f;
    constexpr uint32_t TOTAL_FRAMES = static_cast<uint32_t>(FPS * DURATION_SEC);
    // Per-frame zoom multiplier so MAGNIFICATION_PER_SEC holds over one second.
    const float zoom_per_frame = std::pow(MAGNIFICATION_PER_SEC, 1.0f / FPS);

    // THIS DAMN THING WON'T WORK
    // I've tried different codecs and a bunch of options. I can't get it to render in HDR
    char ffmpeg_cmd[512];
    std::snprintf(
        ffmpeg_cmd, sizeof(ffmpeg_cmd),
        "ffmpeg -y -f rawvideo -pix_fmt p010le -s %ux%u -r %u -i - "
        "-c:v hevc_nvenc -profile:v main10 -pix_fmt p010le "
        "-preset p5 -rc vbr -cq 19 -b:v 0 "
        "-color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc "
        "-color_range tv -tag:v hvc1 output_hdr.mp4",
        static_cast<unsigned>(WIDTH), static_cast<unsigned>(HEIGHT),
        static_cast<unsigned>(FPS));

    FILE* ffmpeg_pipe = popen(ffmpeg_cmd, "w");
    if (!ffmpeg_pipe) {
        std::perror("popen ffmpeg");
        return EXIT_FAILURE;
    }

    p010_frame_t* d_p010 = nullptr;
    CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&d_p010), sizeof(p010_frame_t)));

    static p010_frame_t h_p010;
    float zoom = ZOOM_START;

    for (uint32_t frame = 0; frame < TOTAL_FRAMES; frame++) {
        // MVP iteration schedule — tune A/B as you go deeper.
        int max_iterations = static_cast<int>(50.0f + 20.0f * std::log2(zoom));

        generate_fused(d_p010, zoom, max_iterations);
        CUDA_CHECK(cudaMemcpy(&h_p010, d_p010, sizeof(p010_frame_t),
                              cudaMemcpyDeviceToHost));

        if (std::fwrite(&h_p010, 1, sizeof(h_p010), ffmpeg_pipe) != sizeof(h_p010)) {
            std::fprintf(stderr, "fwrite to ffmpeg failed at frame %u\n", frame);
            pclose(ffmpeg_pipe);
            CUDA_CHECK(cudaFree(d_p010));
            return EXIT_FAILURE;
        }

        zoom *= zoom_per_frame;
    }

    int ffmpeg_status = pclose(ffmpeg_pipe);
    CUDA_CHECK(cudaFree(d_p010));

    if (ffmpeg_status != 0) {
        std::fprintf(stderr, "ffmpeg exited with status %d\n", ffmpeg_status);
        return EXIT_FAILURE;
    }

    std::printf("Wrote output_hdr.mp4 (%u frames @ %u fps)\n", TOTAL_FRAMES, FPS);
    return 0;
}

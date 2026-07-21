#include <iostream>
#include <cstdint>
#include <vector>
#include <fstream>
#include <string>


// Making configurability concession to remove registers
constexpr uint16_t FRAME_WIDTH = 3840;
constexpr uint16_t FRAME_HEIGHT = 2160;
constexpr uint16_t MAX_ITERATIONS = 512;


void save_as_pgm(const std::string& filename, const std::vector<uint16_t>& image, uint16_t width, uint16_t height, uint16_t max_iterations);


__global__ void mandelbrot_frame(uint16_t *p, float center_x, float center_y, float zoom) {
    uint16_t x = (blockIdx.x * blockDim.x) + threadIdx.x;
    uint16_t y = (blockIdx.y * blockDim.y) + threadIdx.y;

    // Out of bounds protection. If a sync is needed, call here too
    if (x >= FRAME_WIDTH || y >= FRAME_HEIGHT) {
        return;
    }

    float cx = center_x + (x - FRAME_WIDTH / 2.0f) / zoom;
    float cy = center_y - (y - FRAME_HEIGHT / 2.0f) / zoom;

    // Register optimization: use xy addressing early, can now remove x and y regs
    p += (y * FRAME_WIDTH) + x; 

    float zx = 0.0f;
    float zy = 0.0f;
    float zx2 = 0.0f;
    float zy2 = 0.0f;

    int i = 0;

    // Optimized Escape Time Algorithm
    for (; i < MAX_ITERATIONS && zx2 + zy2 <= 4; i++) {
        zy = 2 * zx * zy + cy;
        zx = zx2 - zy2 + cx;
        zx2 = zx * zx;
        zy2 = zy * zy;
    }

    *p = i;
}


int main() {
    float center_x = -0.77468f;
    float center_y = -0.13741f;
    float zoom = 10000.0f;

    uint16_t *p;
    cudaMalloc((void **)&p, FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint16_t)); // TODO: err check

    dim3 blockDim(16, 16);
    dim3 gridDim((FRAME_WIDTH + 15) / 16, (FRAME_HEIGHT + 15) / 16);

    mandelbrot_frame<<<gridDim, blockDim>>>(p, center_x, center_y, zoom);

    std::vector<uint16_t> image(FRAME_WIDTH * FRAME_HEIGHT); // vector so its heap allocated

    cudaMemcpy(image.data(), p, FRAME_WIDTH * FRAME_HEIGHT * sizeof(uint16_t), cudaMemcpyDeviceToHost);

    cudaFree(p);

    save_as_pgm("mandelbrot.pgm", image, FRAME_WIDTH, FRAME_HEIGHT, MAX_ITERATIONS);
}



void save_as_pgm(const std::string& filename, const std::vector<uint16_t>& image, uint16_t width, uint16_t height, uint16_t max_iterations) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return; // Silent return on failure, add error handling if desired

    out << "P5\n" << width << " " << height << "\n255\n";

    std::vector<uint8_t> pixels(width * height);
    for (size_t i = 0; i < image.size(); i++) {
        if (image[i] == max_iterations) {
            pixels[i] = 0;
        } else {
            pixels[i] = (image[i] * 255) / max_iterations;        
        }
    }

    out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
}

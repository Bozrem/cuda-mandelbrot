#include "generating/cuda/mandelbrot.h"
#include "rendering/pgm/pgm.h"

int main() {
    // static: ~33MB frame lives in data segment, not on the stack
    static frame_t frame;
    float zoom = 10000.0f;
    int max_iterations = 100;

    render_escape_frame(frame, zoom, max_iterations);
    save_as_pgm("mandelbrot.pgm", frame);
    return 0;
}

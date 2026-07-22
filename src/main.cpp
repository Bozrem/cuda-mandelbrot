#include "generating/generating.h"
#include "rendering/pgm/pgm.h"

#include <cmath>

int main() {
    // static: ~33MB frame lives in data segment, not on the stack
    static iter_frame_t frame;
    float zoom = 10000.0f;
    int max_iterations = 100;

    // Continuous iteration: nu = i + 1 - log2(0.5 * log2(|z|^2)).
    // Escaped points stop with i <= max_iterations - 1 and |z| just above the
    // escape radius, so nu is bounded above by:
    //   max_iterations - log2(0.5 * log2(ESCAPE_THRES2))
    float continuous_range =
        (float)max_iterations - std::log2(0.5f * std::log2((float)ESCAPE_THRES2));

    render_escape_frame_host(frame, zoom, max_iterations);
    save_as_pgm_modulo("mandelbrot_modulo.pgm", frame, 5.0f);
    save_as_pgm_linear("mandelbrot_linear.pgm", frame, continuous_range);
    return 0;
}

# cuda-mandelbrot

Generates high quality videos of the Mandelbrot set using CUDA.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/mandelbrot
```

Writes `mandelbrot.pgm` in the working directory.

## Layout (Major I / Option A)

```text
src/
  main.cpp           # constexprs + orchestration
  frame.hpp          # FrameParams, EscapeFrame
  cuda/
    mandelbrot.h     # host-callable wrapper
    mandelbrot.cu    # kernel + wrapper impl
  io/
    pgm.h / pgm.cpp  # still-image sink
```

See `stages.md` for the longer roadmap.

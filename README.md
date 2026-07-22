# cuda-mandelbrot

Generates high quality videos of the Mandelbrot set using CUDA.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/mandelbrot         # host pipeline (PGM stills)
./build/mandelbrot_device  # device-resident pipeline
```

## Layout

```text
src/
  main.cpp                 # host orchestration + PGM sinks
  main_device.cpp          # device-resident orchestration
  config.hpp               # iter_frame_t, p010_frame_t, constants
  generating/
    generating.h           # escape populator + host convenience
    mandelbrot.cu
  coloring/
    coloring.h             # iter → P010 populator
    coloring.cu
  smoothing/
    smoothing.h            # in-place P010 smoother
    smoothing.cu
  rendering/pgm/           # still-image sinks
```

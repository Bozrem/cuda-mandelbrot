cmake_minimum_required(VERSION 3.18)
project(cuda-mandelbrot LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

add_executable(mandelbrot
    src/main.cpp
    src/generating/mandelbrot.cu
    src/rendering/pgm/pgm_modulo.cpp
    src/rendering/pgm/pgm_linear.cpp
)

# Device-resident pipeline (no hot-path D→H)
add_executable(mandelbrot_device
    src/main_device.cpp
    src/generating/mandelbrot.cu
    src/coloring/coloring.cu
    src/smoothing/smoothing.cu
)

target_include_directories(mandelbrot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(mandelbrot_device PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Adjust if your test machine targets a different arch
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES native)
endif()

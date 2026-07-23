cmake_minimum_required(VERSION 3.18)
project(cuda-mandelbrot LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# NVIDIA Video Codec SDK — adjust NVENC_SDK_ROOT if yours lives elsewhere.
set(NVENC_SDK_ROOT "$ENV{HOME}/nv_codec_sdk/Video_Codec_SDK_13.0.37/" CACHE PATH "Root of the NVIDIA Video Codec SDK")
set(NVENC_INTERFACE_DIR "${NVENC_SDK_ROOT}/Interface")
set(NVENC_SAMPLES_DIR "${NVENC_SDK_ROOT}/Samples")
set(NVENC_ENCODER_DIR "${NVENC_SAMPLES_DIR}/NvCodec/NvEncoder")

add_executable(mandelbrot
    src/main.cpp
    src/frame_generation.cu
    ${NVENC_ENCODER_DIR}/NvEncoder.cpp
    ${NVENC_ENCODER_DIR}/NvEncoderCuda.cpp
)

target_include_directories(mandelbrot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${NVENC_INTERFACE_DIR}
    ${NVENC_ENCODER_DIR}
    ${NVENC_SAMPLES_DIR}/NvCodec # NvEncoder.cpp/NvEncoderCuda.cpp include "NvEncoder/NvEncoder.h" (qualified)
    ${NVENC_SAMPLES_DIR}/Utils  # NvEncoder.h -> NvCodecUtils.h -> Logger.h
)

find_package(CUDAToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
# libnvidia-encode.so ships with the NVIDIA driver, not the CUDA toolkit, so it
# isn't covered by CUDAToolkit's imported targets — find it explicitly. It's
# usually alongside libcuda.so (stubs live under the toolkit's lib64/stubs too).
find_library(NVIDIA_ENCODE_LIB
    NAMES nvidia-encode
    PATHS ${CUDAToolkit_LIBRARY_DIR} /usr/lib/x86_64-linux-gnu /usr/lib64
)
if(NOT NVIDIA_ENCODE_LIB)
    message(FATAL_ERROR "libnvidia-encode not found — is the NVIDIA driver installed on this machine?")
endif()

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET 
    libavformat 
    libavcodec 
    libavutil
)

find_package(Threads REQUIRED)

target_link_libraries(mandelbrot PRIVATE
    ${NVIDIA_ENCODE_LIB}
    CUDA::cuda_driver
    Threads::Threads
    PkgConfig::FFMPEG
)

# Adjust if your test machine targets a different arch
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES native)
endif()

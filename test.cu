#include <iostream>
#include <cstdlib>

#define CUDA_CHECK(err) { \
    cudaError_t error = err; \
    if (error != cudaSuccess) { \
        std::cerr << "CUDA Error: " << cudaGetErrorString(error) \
                  << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(EXIT_FAILURE); \
    } \
}


constexpr int LEN = 1000;

__global__ void vector_add(float *x, float *y, float *z) {
    int tid = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (tid < LEN)
        z[tid] = x[tid] + y[tid];
}


int main() {
    // cudaMalloc the three vectors
    float *x, *y, *z;
    CUDA_CHECK(cudaMalloc((void **)&x, LEN * sizeof(float)));
    CUDA_CHECK(cudaMalloc((void **)&y, LEN * sizeof(float)));
    CUDA_CHECK(cudaMalloc((void **)&z, LEN * sizeof(float)));

    // Set up a host vector
    float host_arr[LEN];

    for (int i = 0; i < LEN; ++i) {
        host_arr[i] = static_cast<float>(i);
    }

    // cudaMemcpy the values from host vector
    CUDA_CHECK(cudaMemcpy(x, host_arr, LEN * sizeof(float),
          cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(y, host_arr, LEN * sizeof(float),
          cudaMemcpyHostToDevice));

    // Set up block and grid dims
    dim3 blockDim(256);
    dim3 gridDim((LEN + blockDim.x - 1) / blockDim.x);

    // Run kernel
    vector_add<<<gridDim, blockDim>>>(x, y, z);

    // cudaMemcpy z back to host vector
    CUDA_CHECK(cudaMemcpy(host_arr, z, LEN * sizeof(float),
          cudaMemcpyDeviceToHost));

    // Print first few values
    std::cout << "First 10 values from resulting array, each should be 2i:\n";
    for (int i = 0; i < LEN || i < 10; ++i) {
        std::cout << host_arr[i] << ", ";
    }
    std::cout << std::endl;

    CUDA_CHECK(cudaFree(x));
    CUDA_CHECK(cudaFree(y));
    CUDA_CHECK(cudaFree(z));

    return 0;
}

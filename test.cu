#include <iostream>

constexpr int LEN = 1000;

__global__ void vector_add(float *x, float *y, float *z) {
    int tid = (blockIdx.x * 1024) + threadIdx.x;
    z[tid] = x[tid] + y[tid];
}


int main() {
    // cudaMalloc the three vectors
    float *x, *y, *z;
    cudaMalloc((void **)&x, LEN * sizeof(float));
    cudaMalloc((void **)&y, LEN * sizeof(float));
    cudaMalloc((void **)&z, LEN * sizeof(float));

    // Set up a host vector
    float host_arr[LEN];

    for (int i = 0; i < LEN; ++i) {
        host_arr[i] = i;
    }

    // cudaMemcpy the values from host vector
    cudaMemcpy(x, host_arr, LEN * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(y, host_arr, LEN * sizeof(float), cudaMemcpyHostToDevice);

    // Set up block and grid dims
    int blockSize = LEN < 1024 ? LEN : 1024;
    dim3 gridDim((LEN + 1023) / 1024);

    // Run kernel
    vector_add<<<blockSize, gridDim>>>(x, y, z);

    // cudaMemcpy p back to host vector
    cudaMemcpy(host_arr, p, LEN * sizeof(float), cudaMemcpyDeviceToHost);

    // Print first few values
    std::cout << "First 10 values from resulting array, each should be 2i:\n";
    for (int i = 0; i < LEN || i < 10; ++i) {
        std::cout << i << ", ";
    }
    std::cout << std::endl;

    return 0;
}

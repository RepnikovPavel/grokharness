// CUDA SAXPY: naive vs tiled-ish (same kernel, two grid sizes) — validates ghar cuda+bench path.
#include <cstdio>
#include <cuda_runtime.h>
#include <vector>

__global__ void saxpy(int n, float a, const float* x, float* y)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        y[i] = a * x[i] + y[i];
}

static void die(const char* msg)
{
    std::fprintf(stderr, "%s\n", msg);
    std::exit(1);
}

int main(int argc, char** argv)
{
    const int n = argc > 1 ? std::atoi(argv[1]) : (1 << 24);
    const int block = argc > 2 ? std::atoi(argv[2]) : 256;
    const int reps = argc > 3 ? std::atoi(argv[3]) : 50;
    const float a = 2.0f;

    std::vector<float> hx(n, 1.0f), hy(n, 1.0f);
    float *dx = nullptr, *dy = nullptr;
    if (cudaMalloc(&dx, n * sizeof(float)) != cudaSuccess)
        die("cudaMalloc x");
    if (cudaMalloc(&dy, n * sizeof(float)) != cudaSuccess)
        die("cudaMalloc y");
    cudaMemcpy(dx, hx.data(), n * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(dy, hy.data(), n * sizeof(float), cudaMemcpyHostToDevice);

    const int grid = (n + block - 1) / block;
    // warmup
    saxpy<<<grid, block>>>(n, a, dx, dy);
    cudaDeviceSynchronize();

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    for (int r = 0; r < reps; ++r)
        saxpy<<<grid, block>>>(n, a, dx, dy);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float ms = 0;
    cudaEventElapsedTime(&ms, start, stop);
    ms /= static_cast<float>(reps);

    cudaMemcpy(hy.data(), dy, n * sizeof(float), cudaMemcpyDeviceToHost);
    // checksum
    double sum = 0;
    for (int i = 0; i < 1024; ++i)
        sum += hy[i];

    std::printf("n=%d\n", n);
    std::printf("block=%d\n", block);
    std::printf("reps=%d\n", reps);
    std::printf("mean_ms=%.6f\n", ms);
    std::printf("gbps=%.6f\n", (3.0 * n * sizeof(float)) / (ms * 1e6));
    std::printf("checksum=%.1f\n", sum);

    cudaFree(dx);
    cudaFree(dy);
    return 0;
}

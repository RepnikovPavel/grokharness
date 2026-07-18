// Cache-friendly blocked matmul + ikj loop order — measurable uplift vs naive.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef BLOCK
#define BLOCK 32
#endif

static void matmul_blocked(int n, const float* A, const float* B, float* C)
{
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            C[i * n + j] = 0.f;

    for (int ii = 0; ii < n; ii += BLOCK) {
        for (int kk = 0; kk < n; kk += BLOCK) {
            for (int jj = 0; jj < n; jj += BLOCK) {
                const int i_max = ii + BLOCK < n ? ii + BLOCK : n;
                const int k_max = kk + BLOCK < n ? kk + BLOCK : n;
                const int j_max = jj + BLOCK < n ? jj + BLOCK : n;
                for (int i = ii; i < i_max; ++i) {
                    for (int k = kk; k < k_max; ++k) {
                        const float aik = A[i * n + k];
                        for (int j = jj; j < j_max; ++j)
                            C[i * n + j] += aik * B[k * n + j];
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    const int n = argc > 1 ? std::atoi(argv[1]) : 256;
    const int reps = argc > 2 ? std::atoi(argv[2]) : 1;
    std::vector<float> A(static_cast<size_t>(n) * n, 1.0f);
    std::vector<float> B(static_cast<size_t>(n) * n, 1.0f);
    std::vector<float> C(static_cast<size_t>(n) * n, 0.0f);

    matmul_blocked(n, A.data(), B.data(), C.data());

    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r)
        matmul_blocked(n, A.data(), B.data(), C.data());
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(reps);

    double sum = 0;
    for (float v : C)
        sum += v;

    std::printf("n=%d\n", n);
    std::printf("reps=%d\n", reps);
    std::printf("mean_ms=%.6f\n", ms);
    std::printf("gflops=%.6f\n", (2.0 * n * n * n) / (ms * 1e6));
    std::printf("checksum=%.1f\n", sum);
    std::printf("block=%d\n", BLOCK);
    return 0;
}

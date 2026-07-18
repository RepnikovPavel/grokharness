// Naive NxN matmul (ijk) — intentional baseline for uplift measurement.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void matmul(int n, const float* A, const float* B, float* C)
{
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            float s = 0.f;
            for (int k = 0; k < n; ++k)
                s += A[i * n + k] * B[k * n + j];
            C[i * n + j] = s;
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

    // touch
    matmul(n, A.data(), B.data(), C.data());

    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r)
        matmul(n, A.data(), B.data(), C.data());
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(reps);

    // checksum prevents DCE
    double sum = 0;
    for (float v : C)
        sum += v;

    // Agent-parseable metrics on stdout
    std::printf("n=%d\n", n);
    std::printf("reps=%d\n", reps);
    std::printf("mean_ms=%.6f\n", ms);
    std::printf("gflops=%.6f\n", (2.0 * n * n * n) / (ms * 1e6));
    std::printf("checksum=%.1f\n", sum);
    return 0;
}

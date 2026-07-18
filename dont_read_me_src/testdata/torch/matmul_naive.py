"""Naive matmul — pure Python triple loop (intentionally slow for uplift)."""
import time

try:
    import torch
except ImportError:
    torch = None  # type: ignore


def pure_python_matmul(n: int = 64) -> list:
    # small n so suite finishes fast, but O(n^3) Python is still slow vs BLAS
    a = [[float(i + j) for j in range(n)] for i in range(n)]
    b = [[float(i * j + 1) for j in range(n)] for i in range(n)]
    out = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            aik = a[i][k]
            for j in range(n):
                out[i][j] += aik * b[k][j]
    return out


def run_op(n: int = 64, reps: int = 1):
    last = None
    for _ in range(reps):
        last = pure_python_matmul(n)
    if torch is not None:
        return torch.tensor(last, dtype=torch.float32)
    return last


if __name__ == "__main__":
    t0 = time.perf_counter()
    y = pure_python_matmul(64)
    ms = (time.perf_counter() - t0) * 1000.0
    s = sum(sum(row) for row in y)
    print(f"naive_ms={ms:.4f} checksum={s:.4f}")

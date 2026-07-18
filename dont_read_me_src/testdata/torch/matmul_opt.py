"""Optimized matmul — single torch.matmul (BLAS)."""
import time
import torch


def run_op(n: int = 64, reps: int = 1) -> torch.Tensor:
    a = torch.arange(n * n, dtype=torch.float32).reshape(n, n)
    # match naive: a[i,j] = i+j, b[i,j] = i*j+1
    ii = torch.arange(n, dtype=torch.float32).unsqueeze(1)
    jj = torch.arange(n, dtype=torch.float32).unsqueeze(0)
    a = ii + jj
    b = ii * jj + 1.0
    out = None
    for _ in range(reps):
        out = torch.matmul(a, b)
    assert out is not None
    return out


if __name__ == "__main__":
    # warm import already paid; time only op path with a few reps
    t0 = time.perf_counter()
    y = run_op(64, reps=5)
    ms = (time.perf_counter() - t0) * 1000.0
    print(f"opt_ms={ms:.4f} checksum={float(y.sum()):.4f}")

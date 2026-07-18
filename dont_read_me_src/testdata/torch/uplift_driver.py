#!/usr/bin/env python3
"""
Single-process uplift driver: import torch once, time naive vs opt.
Used by ghar bench / assert so process overhead does not hide speedup.

  python3 uplift_driver.py naive
  python3 uplift_driver.py opt
  python3 uplift_driver.py both   # print both + speedup
"""
from __future__ import annotations

import sys
import time


def pure_python_matmul(n: int = 80) -> list:
    a = [[float(i + j) for j in range(n)] for i in range(n)]
    b = [[float(i * j + 1) for j in range(n)] for i in range(n)]
    out = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            aik = a[i][k]
            for j in range(n):
                out[i][j] += aik * b[k][j]
    return out


def torch_matmul(n: int = 80, reps: int = 8):
    import torch

    ii = torch.arange(n, dtype=torch.float32).unsqueeze(1)
    jj = torch.arange(n, dtype=torch.float32).unsqueeze(0)
    a = ii + jj
    b = ii * jj + 1.0
    out = None
    for _ in range(reps):
        out = torch.matmul(a, b)
    return out


def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else "both"
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 80

    if mode == "naive":
        t0 = time.perf_counter()
        pure_python_matmul(n)
        ms = (time.perf_counter() - t0) * 1000.0
        print(f"ms={ms:.4f}")
        return 0

    if mode == "opt":
        # pay import once here
        import torch  # noqa: F401

        t0 = time.perf_counter()
        torch_matmul(n)
        ms = (time.perf_counter() - t0) * 1000.0
        print(f"ms={ms:.4f}")
        return 0

    # both
    t0 = time.perf_counter()
    pure_python_matmul(n)
    naive_ms = (time.perf_counter() - t0) * 1000.0
    import torch  # noqa: F401

    t0 = time.perf_counter()
    torch_matmul(n)
    opt_ms = (time.perf_counter() - t0) * 1000.0
    speedup = naive_ms / opt_ms if opt_ms > 0 else float("inf")
    print(f"naive_ms={naive_ms:.4f} opt_ms={opt_ms:.4f} speedup={speedup:.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

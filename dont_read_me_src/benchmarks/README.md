# Benchmarks & tests

This directory exists so `ghar` is **not dead code**: every release can show
**where the uplift is** with numbers.

## What “uplift” means here

| Axis | Without ghar | With ghar | Headline metric |
|------|--------------|-----------|-----------------|
| Hallucination detection | Agent ships false claims | Programmatic reject (exit 4) | `recall_catch_rate` |
| Optimization claims | “2× faster” is vibes | Timed `speedup` + assert | `speedup` (matmul) |
| Delivery | Trust the model | `ghar gate` | exit 0 only if claims ok |

## Code scan (internal)

Optional bulk static scan driver: `oracles/repo_scan.py` + `benchmarks/run_openmmlab_scan.sh`.
Produces `results/*findings.tsv` for the local UI — not a public marketing pillar.

## Run

```sh
# from repo root, after build
bash scripts/run_uplift_report.sh   # tests + synthetic + perf + real-model

# or pieces:
bash tests/run_all.sh
bash benchmarks/run_hallucination_suite.sh
bash benchmarks/run_py_torch_suite.sh
bash benchmarks/run_openmmlab_scan.sh
bash benchmarks/run_perf_uplift.sh
ollama pull qwen2.5-coder:1.5b
bash benchmarks/run_real_model_eval.sh

ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -L uplift --output-on-failure
```

### Real-model pillar

| Item | Detail |
|------|--------|
| Model | `qwen2.5-coder:1.5b` via local Ollama (default) |
| Why | Small open **coding** LLM — invents packages/APIs often (cheap-LLM hallu class) |
| Without harness | bare trust (always accept) → catch_rate 0 |
| With harness | `ghar import/compile/symbols/run` → measured catch_rate |
| Offline | `REAL_MODEL_FIXTURE=1` replays `benchmarks/real_model/fixtures/` (recorded live transcripts) |

Outputs land in `results/` (gitignored). A snapshot of the last local run is
copied under `if_necessary_you_can_read_me/`.

## Workloads

| File | Role |
|------|------|
| `workloads/matmul_naive.cpp` | ijk baseline |
| `workloads/matmul_opt.cpp` | blocked ikj (real CPU uplift) |
| `workloads/saxpy_cuda.cu` | CUDA path + block-size probe |
| `hallucinations/cases.tsv` | catalog of synthetic agent lies |

## Expected (order of magnitude)

On a typical desktop CPU, blocked matmul N=256 should show **~2–4×** wall-time
speedup vs naive. Hallucination suite should stay at **catch_rate = 1.0** on the
built-in 10 cases (5 bad / 5 good).

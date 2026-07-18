# ghar (grokharness)

**Local-first CLI that programmatically catches LLM hallucinations** in coding work.

Agents (and humans) claim things: “this compiles”, “speedup is 2×”, “torch is installed”,
“this CUDA kernel works”, “symbol `foo` exists”. **`ghar` does not ask another model** —
it runs compilers, timers, `importlib`, `nm`, `nvidia-smi`, tests, and numeric asserts.

Coverage focus: **coding only** — GPU/CPU optimizations, backend, CV training glue,
TensorRT tooling presence, custom CUDA kernels, research code adaptations.

## Benchmark: OpenMMLab static scan

We automatically scan curated [OpenMMLab](https://github.com/open-mmlab) repositories with **ghar**
programmatic oracles (AST syntax + full `torch.*` attribute resolve — **no LLM**).

| Metric | Latest run |
|--------|----------:|
| Repos | **5** ([org](https://github.com/open-mmlab)) |
| Python files scanned | **1480** |
| Torch attr refs checked | **5353** |
| Syntax failures | **0** |
| Findings total | **67** |
| → **api_missing** (stock torch) | **6** |
| → accel backends (npu/mlu/musa) | **56** |
| → build flavor (ROCm/HIP) | **5** |
| Host torch | `2.6.0+cu124` |
| Report date (UTC) | 2026-07-18T11:26:38Z |

| Repo | Files | Syntax fail | Torch files | Attr refs | api_missing | accel | Findings |
|------|------:|------------:|------------:|----------:|------------:|------:|---------:|
| `mmengine` | 150 | 0 | 82 | 448 | 2 | 45 | 50 |
| `mmcv` | 226 | 0 | 149 | 966 | 3 | 11 | 16 |
| `mmpose` | 303 | 0 | 140 | 933 | 0 | 0 | 0 |
| `mmdetection` | 603 | 0 | 377 | 2217 | 1 | 0 | 1 |
| `mmsegmentation` | 198 | 0 | 124 | 789 | 0 | 0 | 0 |

**Headline:** on torch `2.6.0+cu124`, ghar flags **6** unresolved stock-API references
(e.g. `torch.nn.SyncBatchNorm2d`, `torch.utils.data.PoolDataLoader`, `torch.distributed.group.WORLD`)
plus **56** vendor-device symbols expected missing on CUDA wheels.

| Interesting `api_missing` | Where |
|---------------------------|--------|
| `torch.nn.SyncBatchNorm2d` | mmengine / mmcv `parrots_wrapper.py` |
| `torch.utils.data.PoolDataLoader` | mmengine / mmcv `parrots_wrapper.py` |
| `torch.distributed.group.WORLD` | mmcv `ops/sync_bn.py`, mmdet `dist_utils.py` |

Full report: [`benchmarks/openmmlab/OPENMMLAB_SCAN.md`](benchmarks/openmmlab/OPENMMLAB_SCAN.md) ·
raw TSV: [`benchmarks/openmmlab/openmmlab_findings.tsv`](benchmarks/openmmlab/openmmlab_findings.tsv)

```sh
bash benchmarks/run_openmmlab_scan.sh
```

*This is the first public “scan a famous org” pillar — more ecosystems next.*

## Proof it is not dead code (tests + uplift)

```sh
cmake -S . -B build && cmake --build build -j"$(nproc)"

# Fast integration tests
ctest --test-dir build -L integration --output-on-failure

# Full report: catch-rate + matmul speedup + markdown
bash scripts/run_uplift_report.sh
# → results/UPLIFT_REPORT.md
# → results/hallucination_summary.tsv
# → results/perf_uplift.tsv
```

| Pillar | What is measured |
|--------|------------------|
| Integration | CLI exit codes, compile/import/**python/torch**/assert/gate |
| Hallucination suite | precision / recall / FPR on synthetic agent lies |
| **Python / PyTorch suite** | AST/import/exec + torch attr/forward + matmul uplift |
| Perf uplift | blocked matmul vs naive: `mean_ms`, `gflops`, `speedup` |
| Real-model eval | live Ollama coding model claims vs ghar gate (`results/REAL_MODEL_EVAL.md`) |

```sh
# Real LLM pillar (requires Ollama + pulled model, default qwen2.5-coder:1.5b)
ollama pull qwen2.5-coder:1.5b
bash benchmarks/run_real_model_eval.sh
bash scripts/run_uplift_report.sh   # fails if any pillar fails
```

## Install

Requires C++17 (`g++` ≥ 9), `cmake` ≥ 3.10.

```sh
cmake -S . -B build && cmake --build build -j"$(nproc)"
sudo cmake --install build   # installs `ghar` to /usr/local/bin
# or: cp build/ghar ~/.local/bin/
```

One-liner (after push to GitHub):

```sh
curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/grokharness/main/read_me_if_it_is_not_installed/install.sh | sh
```

## Quick start (industry workflow)

Practices from **Aider** (lint/test loop), **Claude Code Stop hooks**, and **long work sessions**
(anti-5-minute agent quit):

```sh
ghar scaffold                 # .ghar/config + integrations/ + AGENTS.md
ghar config --format

# Short task:
ghar verify                   # lint → build → test → gate

# Deep / multi-hour task (REQUIRED when user wants long autonomous work):
ghar work start --minutes 120 --goal "ship feature X with tests"
# … keep working: heartbeat often, verify after changes …
ghar work heartbeat --note "what I just did"
ghar verify
ghar work status              # remain_min / quotas
ghar work done                # exit 0 only when time+verify+heartbeat quotas met
```

Env: `GHAR_MIN_WORK_MINUTES=120`. Config keys: `min_work_minutes`, `min_verify_ok`, `min_heartbeats`.

**How to force autonomous long runs:** see
[`if_necessary_you_can_read_me/AUTONOMOUS.md`](if_necessary_you_can_read_me/AUTONOMOUS.md).
Bug audit log: [`if_necessary_you_can_read_me/BUGS_FIXED.md`](if_necessary_you_can_read_me/BUGS_FIXED.md).

### Wire into agents

```sh
# Aider
aider --test-cmd 'ghar verify' --auto-test

# Claude Code: merge integrations/claude/settings.fragment.json
# (Stop hook runs integrations/claude/ghar-stop.sh → ghar verify)
```

### Optional domain checks (CUDA / Python / PyTorch)

```sh
ghar cuda --device --name gpu
ghar compile kernels/foo.cu --name kfoo
ghar bench --name perf --repeat 20 -- ./bin
ghar assert --from perf --metric mean_ms --op lt --value 5

# Python source truth (AST + imports + optional exec)
ghar python --file model/train.py --exec --name train_py

# PyTorch: resolve torch.* attrs, run Module.forward / run_op / main
ghar torch-attr torch.nn.Linear torch.matmul
ghar torch --file models/net.py --forward --device cpu --name net
ghar verify                   # still required before user delivery
```

Oracle (no LLM): `oracles/py_torch_validate.py` — invoked by `ghar python|torch|torch-attr`.  
Suite: `bash benchmarks/run_py_torch_suite.sh` → `results/py_torch_summary.tsv`.

Config: `ghar.conf` or `.ghar/config` (`lint_cmd`, `build_cmd`, `test_cmd`, `step.*`).  
Details: [`if_necessary_you_can_read_me/BEST_PRACTICES.md`](if_necessary_you_can_read_me/BEST_PRACTICES.md).

## Agent workflow (required)

```
1. ghar scaffold   # once
2. …edit code…
3. ghar verify     # MUST exit 0 before answering the user
4. FEEDBACK → fix → verify again (never hand-wave)
```

## Metrics (programmer / math friendly)

| Check    | Example metrics |
|----------|-----------------|
| compile  | `wall_ms`, `error_lines`, `warning_lines`, `binary_bytes`, `compiler_exit` |
| cuda     | `gpu_count`, `compute_cap`, `mem_free_mib`, `nvcc`, compile.* |
| **python** | `syntax_ok`, `imports_ok`, `exec_ok`, `failure_class`, `ast_nodes` |
| **torch**  | `attrs_ok`, `forward_ok`, `out_shape`, `out_finite`, `torch_version`, `missing_list` |
| bench    | `mean_ms`, `std_ms`, `min_ms`, `max_ms`, `median_ms`, `cv`, `speedup` |
| assert   | `actual`, `expected`, `abs_err`, `rel_err`, `op` |
| import   | `ok`, `fail`, `path.<mod>`, `ver.<mod>` |
| symbols  | `found`, `missing`, `missing_list` |
| run      | `wall_ms`, `proc_exit`, `stdout_fp`, `out.<KEY>` from `KEY=value` stdout |
| test     | `passed`, `failed`, `skipped`, `runner` |
| gate     | `ok`, `fail`, per-claim status |

Stdout is **TSV** (`kind=result` rows with `key=value` fields). Use `--format` for tables.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok |
| 1 | usage |
| 2 | required tool missing |
| 3 | I/O error |
| 4 | **verification failed** (hallucination / broken claim) |

## State

`<project>/.ghar/`:

- `results.tsv` — append-only log of every check  
- `claims.tsv` — latest named claim snapshots (what `gate` reads)  
- `session.tsv` — last gate summary  

## Not in scope (v0.1)

- LLM-as-judge / “ask GPT if this looks right”
- Full TensorRT engine builder (probes `trtexec` via `doctor`; run your own via `ghar run`)
- Non-code domains (chat, web Q&A)

## Docs

- Agent brief: [`prompt.txt`](prompt.txt)
- Install: [`read_me_if_it_is_not_installed/`](read_me_if_it_is_not_installed/)
- Architecture: [`if_necessary_you_can_read_me/ARCHITECTURE.md`](if_necessary_you_can_read_me/ARCHITECTURE.md)

## License

0BSD (see `LICENSE`).

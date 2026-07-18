# Architecture

## Goal

Cheap (and expensive) LLMs invent APIs, libraries, CUDA kernels, and performance
numbers. **ghar** is a **deterministic grounding layer**: every claim an agent
makes about code can be turned into a **programmatic check** with **numeric or
boolean metrics**. Another neural net is never the verifier.

## Components

```
CLI (main.cpp)
  ├─ scaffold    write .ghar/config + Claude/Aider integrations (industry hooks)
  ├─ config      show resolved lint/build/test oracles
  ├─ verify      Aider-style pipeline → gate (PRIMARY delivery command)
  ├─ doctor      probe host toolchain / GPU
  ├─ compile     g++/clang++/nvcc subprocess
  ├─ cuda        nvidia-smi metrics + nvcc
  ├─ symbols     nm + header identifier scan
  ├─ import      python3 -c importlib
  ├─ python      AST + importlib + optional exec (oracles/py_torch_validate.py)
  ├─ torch       strict torch.* attr resolve + Module.forward / run_op
  ├─ torch-attr  resolve attribute paths against installed torch
  ├─ run         arbitrary process + KEY=value scrape
  ├─ bench       repeated wall-clock stats + optional baseline speedup
  ├─ assert      numeric/string compare vs stored claim metrics
  ├─ test        pytest / ctest adapters
  └─ gate        AND of substantive claims in .ghar/claims.tsv
                 (python|torch count as substantive kinds)
```

Python/PyTorch oracles live in `oracles/py_torch_validate.py` (pure program checks).
C++ wrappers: `dont_read_me_src/python_check.{hpp,cpp}`. Suite:
`benchmarks/run_py_torch_suite.sh`.

`verify` loads `.ghar/config` / `ghar.conf`, runs each step via shell (execution as
oracle), prints FEEDBACK on failure (Aider loop), then `gate` (Claude Stop pattern).

## Data flow

1. Check runs external tools via `fork/exec` (`util.cpp`).
2. Metrics collected into `FieldMap` (string key/value).
3. `finish_check` appends `results.tsv`, upserts named row in `claims.tsv`, prints TSV.
4. `assert` loads claim metrics by `--from` name.
5. `gate` fails (exit 4) if any claim `status != ok|skip` or no claims exist.

## Why TSV

Same contract as `memc`/`tokenc`: agents parse columns without JSON schema fights.
`--format` is for humans only.

## Extending

Add a new check:

1. `foo_check.hpp/cpp` producing `CheckResult`
2. Wire command in `main.cpp`
3. Document metrics in `prompt.txt` / README

Prefer **shelling out** to real tools (nvcc, trtexec, ncu) over reimplementing them.

## Non-goals

- Semantic code review by LLM
- Cloud telemetry
- Replacing unit tests (wraps them)

## Work sessions (v0.3)

`.ghar/work.tsv` tracks active long sessions. Quotas block delivery (`ghar work done`,
Stop hook, optional `ghar verify --deliver`). This is the harness fix for agents that
stop after a few minutes of wall-clock chat.

# ghar uplift report

Generated: 2026-07-18T11:18:10Z → 2026-07-18T11:19:07Z (57s)  
Binary: `/home/user/grokharness/build/ghar`

## Why this exists

Without programmatic checks, an agent can claim:

- “package X is installed”
- “this compiles”
- “API `foo` exists”
- “optimization is **1000×** faster”

and ship **dead / wrong code**. `ghar` turns those claims into **measured metrics** and a **gate**.

## 1. Integration tests

| Suite | Exit |
|-------|------|
| tests/run_all.sh | **0** (0 = all pass) |

Log: `results/integration.log`

## 2. Synthetic hallucination suite

Labeled claims checked without another neural net.

| Metric | Value |
|--------|------:|
| accuracy | **1.0000** |
| recall (catch rate) | **1.0000** |
| precision | **1.0000** |
| false_positive_rate | **0.0000** |
| avg_latency_ms | **14.422** |
| tp / fn | **5** / **0** |

Exit: **0** — table: `results/hallucination_cases.tsv`

**Uplift:** without ghar, false claims pass at 100%. With ghar, catch_rate=**1.0000**.

## 3. Python / PyTorch validators

Programmatic checks: AST/syntax, importlib, exec, full `torch.*` attr resolve
(including import aliases like `F`; **no parent-module fallback** for hallucinated leaves),
Module.forward / run_op.

| Metric | Value |
|--------|------:|
| accuracy | **1.0000** |
| without validator catch_rate | **0.0000** |
| with validator catch_rate | **1.0000** |
| Δ catch_rate (with − without) | **1.0000** |
| precision | **1.0000** |
| false_positive_rate | **0.0000** |
| tp / fp / fn | **8** / **0** / **0** |
| in-process matmul speedup (pure-Python → torch) | **22.1758**× |
| naive_ms / opt_ms | 34.6068 / 1.5606 |

Exit: **0** — `results/py_torch_cases.tsv`, `results/py_torch_summary.tsv`

Commands: `ghar python`, `ghar torch`, `ghar torch-attr` (oracle: `oracles/py_torch_validate.py`).

## 4. Performance uplift (matmul naive → blocked)

| Metric | Naive | Opt | Uplift |
|--------|------:|----:|-------:|
| mean_ms | 35.109945 | 12.219766 | **2.844961x** |
| gflops | 2.075780 | 7.833876 | — |

Exit: **0** — `results/perf_uplift.tsv` (assert speedup ≥ 1.3)

## 5. Real-model eval (live LLM, not only synthetic)

Model elicits coding claims; bare trust vs `ghar` gate.

| Metric | Value |
|--------|------:|
| model | **qwen2.5-coder:1.5b** |
| n_trials | **8** |
| without harness catch_rate | **0.0000** |
| with harness catch_rate | **1.0000** |
| accuracy | **0.8333** |
| false claims caught (tp) | **2** |

Exit: **0** — `results/REAL_MODEL_EVAL.md`, `results/real_model_cases.tsv`

**Uplift:** Δ catch_rate = with − without (without is always 0 by protocol).

## 6. Agent delivery contract

```
ghar verify          # lint→build→test→gate
ghar python --file x.py --exec
ghar torch --file model.py --forward
```

## Summary scoreboard

| Pillar | Status | Headline |
|--------|--------|----------|
| Integration | PASS | CLI suites |
| Synthetic hallu | PASS | catch_rate=1.0000 |
| Python/Torch | PASS | catch=1.0000 speedup=22.1758 |
| Perf uplift | PASS | speedup=2.844961x |
| Real-model eval | PASS | model=qwen2.5-coder:1.5b catch=1.0000 |

Overall exit: 0

**Acceptance rule:** exit 0 only if synthetic detection + python/torch + perf speedup + real-model pillar all pass (no dead-code / synthetic-only acceptance).

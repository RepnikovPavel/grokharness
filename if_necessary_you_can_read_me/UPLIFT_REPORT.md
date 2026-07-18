# ghar uplift report

Generated: 2026-07-18T09:44:54Z → 2026-07-18T09:45:02Z (8s)  
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

## 2. Hallucination catch suite (agent claims vs reality)

Synthetic claims that cheap LLMs invent, checked **without** another neural net.

| Metric | Value | Meaning |
|--------|------:|---------|
| accuracy | **1.0000** | correct verdicts / all cases |
| recall (catch rate) | **1.0000** | fraction of *bad* claims caught (tp/(tp+fn)) |
| precision | **1.0000** | when ghar says fail, it was really bad |
| false_positive_rate | **0.0000** | good claims wrongly failed |
| avg_latency_ms | **14.866** | mean check latency |
| tp / fn | **5** / **0** | caught / missed hallucinations |

Exit: **0**

Per-case table: `results/hallucination_cases.tsv`

**Uplift here:** without ghar, false claims pass at 100% (agent self-confidence).  
With ghar, catch_rate = **1.0000** on the bad-claim subset — that is the detection uplift.

## 3. Performance uplift (matmul naive → blocked)

Measured by `ghar bench` + `ghar assert` (not by the model).

| Metric | Naive | Opt (blocked) | Uplift |
|--------|------:|--------------:|-------:|
| mean_ms | 36.063407 | 11.964943 | **2.925606x** |
| gflops | 2.050990 | 8.211870 | — |

Exit: **0**

Raw: `results/perf_uplift.tsv`  
CUDA probe (if any): `results/cuda_uplift.tsv`

**Uplift here:** opt vs naive wall time — real, reproducible, gateable (`speedup >= 1.3`).

## 4. Agent delivery contract

```
ghar reset
# … edits …
ghar compile / cuda / import / bench / assert
ghar gate   # must exit 0
```

If gate ≠ 0, the agent is **not allowed** to claim success to the user.

## Summary scoreboard

| Pillar | Status | Headline metric |
|--------|--------|-----------------|
| Integration | PASS | all CLI suites |
| Hallucination catch | PASS | catch_rate=1.0000 |
| Perf uplift | PASS | speedup=2.925606x |

Overall exit: 0

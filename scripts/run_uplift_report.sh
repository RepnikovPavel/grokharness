#!/usr/bin/env bash
# Full validation: integration tests + hallucination suite + perf uplift.
# Produces results/UPLIFT_REPORT.md with numbers — proof ghar is not dead code.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export OUT_DIR="${OUT_DIR:-$ROOT/results}"
export GHAR="${GHAR:-$ROOT/build/ghar}"
mkdir -p "$OUT_DIR"

echo "############################################################"
echo "# ghar uplift report"
echo "# GHAR=$GHAR"
echo "# OUT_DIR=$OUT_DIR"
echo "############################################################"

if [[ ! -x "$GHAR" ]]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j"$(nproc)"
  export GHAR="$ROOT/build/ghar"
fi

TS_START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
t0=$(date +%s)

export GHAR_ROOT="${GHAR_ROOT:-$ROOT}"

echo
echo ">>> [1/5] Integration tests"
set +e
bash "$ROOT/tests/run_all.sh" | tee "$OUT_DIR/integration.log"
test_rc=${PIPESTATUS[0]}
set -e

echo
echo ">>> [2/5] Hallucination catch suite (synthetic labeled claims)"
set +e
bash "$ROOT/benchmarks/run_hallucination_suite.sh" | tee "$OUT_DIR/hallucination.log"
hall_rc=${PIPESTATUS[0]}
set -e

echo
echo ">>> [3/5] Python / PyTorch validators suite"
set +e
bash "$ROOT/benchmarks/run_py_torch_suite.sh" | tee "$OUT_DIR/py_torch.log"
pyt_rc=${PIPESTATUS[0]}
set -e

echo
echo ">>> [4/5] Perf uplift (matmul + optional CUDA)"
set +e
bash "$ROOT/benchmarks/run_perf_uplift.sh" | tee "$OUT_DIR/perf.log"
perf_rc=${PIPESTATUS[0]}
set -e

echo
echo ">>> [5/5] Real-model eval (live Ollama coding model)"
set +e
# Prefer SCRATCH if set (goal harness); else results/real_model_scratch
export SCRATCH="${SCRATCH:-$OUT_DIR/real_model_scratch}"
bash "$ROOT/benchmarks/run_real_model_eval.sh" | tee "$OUT_DIR/real_model.log"
real_rc=${PIPESTATUS[0]}
set -e

t1=$(date +%s)
elapsed=$((t1 - t0))
TS_END=$(date -u +%Y-%m-%dT%H:%M:%SZ)

# Pull summary numbers
hall_acc=$(awk -F'\t' '$1=="accuracy"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "n/a")
hall_rec=$(awk -F'\t' '$1=="recall_catch_rate"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "n/a")
hall_prec=$(awk -F'\t' '$1=="precision"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "n/a")
hall_fpr=$(awk -F'\t' '$1=="false_positive_rate"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "n/a")
hall_lat=$(awk -F'\t' '$1=="avg_latency_ms"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "n/a")
hall_tp=$(awk -F'\t' '$1=="tp"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "0")
hall_fn=$(awk -F'\t' '$1=="fn"{print $2}' "$OUT_DIR/hallucination_summary.tsv" 2>/dev/null || echo "0")

speedup=$(awk -F'\t' '$1=="mean_ms"{print $4}' "$OUT_DIR/perf_uplift.tsv" 2>/dev/null || echo "n/a")
naive_ms=$(awk -F'\t' '$1=="mean_ms"{print $2}' "$OUT_DIR/perf_uplift.tsv" 2>/dev/null || echo "n/a")
opt_ms=$(awk -F'\t' '$1=="mean_ms"{print $3}' "$OUT_DIR/perf_uplift.tsv" 2>/dev/null || echo "n/a")
naive_gf=$(awk -F'\t' '$1=="gflops"{print $2}' "$OUT_DIR/perf_uplift.tsv" 2>/dev/null || echo "n/a")
opt_gf=$(awk -F'\t' '$1=="gflops"{print $3}' "$OUT_DIR/perf_uplift.tsv" 2>/dev/null || echo "n/a")

pyt_acc=$(awk -F'\t' '$1=="accuracy"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")
pyt_rec=$(awk -F'\t' '$1=="recall_catch_rate"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")
pyt_prec=$(awk -F'\t' '$1=="precision"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")
pyt_tp=$(awk -F'\t' '$1=="tp"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "0")
pyt_fn=$(awk -F'\t' '$1=="fn"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "0")
pyt_spd=$(awk -F'\t' '$1=="torch_matmul_speedup"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")
pyt_naive=$(awk -F'\t' '$1=="torch_matmul_naive_ms"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")
pyt_opt=$(awk -F'\t' '$1=="torch_matmul_opt_ms"{print $2}' "$OUT_DIR/py_torch_summary.tsv" 2>/dev/null || echo "n/a")

rm_model=$(awk -F'\t' '$1=="model"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "n/a")
rm_catch=$(awk -F'\t' '$1=="with_harness_catch_rate"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "n/a")
rm_without=$(awk -F'\t' '$1=="without_harness_catch_rate"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "n/a")
rm_n=$(awk -F'\t' '$1=="n_trials"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "0")
rm_tp=$(awk -F'\t' '$1=="tp"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "0")
rm_acc=$(awk -F'\t' '$1=="accuracy"{print $2}' "$OUT_DIR/real_model_summary.tsv" 2>/dev/null || echo "n/a")

REPORT="$OUT_DIR/UPLIFT_REPORT.md"
{
  cat <<EOF
# ghar uplift report

Generated: $TS_START → $TS_END (${elapsed}s)  
Binary: \`$GHAR\`

## Why this exists

Without programmatic checks, an agent can claim:

- “package X is installed”
- “this compiles”
- “API \`foo\` exists”
- “optimization is **1000×** faster”

and ship **dead / wrong code**. \`ghar\` turns those claims into **measured metrics** and a **gate**.

## 1. Integration tests

| Suite | Exit |
|-------|------|
| tests/run_all.sh | **$test_rc** (0 = all pass) |

Log: \`results/integration.log\`

## 2. Synthetic hallucination suite

Labeled claims checked without another neural net.

| Metric | Value |
|--------|------:|
| accuracy | **$hall_acc** |
| recall (catch rate) | **$hall_rec** |
| precision | **$hall_prec** |
| false_positive_rate | **$hall_fpr** |
| avg_latency_ms | **$hall_lat** |
| tp / fn | **$hall_tp** / **$hall_fn** |

Exit: **$hall_rc** — table: \`results/hallucination_cases.tsv\`

**Uplift:** without ghar, false claims pass at 100%. With ghar, catch_rate=**$hall_rec**.

## 3. Python / PyTorch validators

Programmatic checks: AST/syntax, importlib, exec, \`torch.*\` attr resolve, Module.forward.

| Metric | Value |
|--------|------:|
| accuracy | **$pyt_acc** |
| recall (catch rate) | **$pyt_rec** |
| precision | **$pyt_prec** |
| tp / fn | **$pyt_tp** / **$pyt_fn** |
| in-process matmul speedup (pure-Python → torch) | **$pyt_spd**× |
| naive_ms / opt_ms | $pyt_naive / $pyt_opt |

Exit: **$pyt_rc** — \`results/py_torch_cases.tsv\`, \`results/py_torch_summary.tsv\`

Commands: \`ghar python\`, \`ghar torch\`, \`ghar torch-attr\` (oracle: \`oracles/py_torch_validate.py\`).

## 4. Performance uplift (matmul naive → blocked)

| Metric | Naive | Opt | Uplift |
|--------|------:|----:|-------:|
| mean_ms | $naive_ms | $opt_ms | **$speedup** |
| gflops | $naive_gf | $opt_gf | — |

Exit: **$perf_rc** — \`results/perf_uplift.tsv\` (assert speedup ≥ 1.3)

## 5. Real-model eval (live LLM, not only synthetic)

Model elicits coding claims; bare trust vs \`ghar\` gate.

| Metric | Value |
|--------|------:|
| model | **$rm_model** |
| n_trials | **$rm_n** |
| without harness catch_rate | **$rm_without** |
| with harness catch_rate | **$rm_catch** |
| accuracy | **$rm_acc** |
| false claims caught (tp) | **$rm_tp** |

Exit: **$real_rc** — \`results/REAL_MODEL_EVAL.md\`, \`results/real_model_cases.tsv\`

**Uplift:** Δ catch_rate = with − without (without is always 0 by protocol).

## 6. Agent delivery contract

\`\`\`
ghar verify          # lint→build→test→gate
ghar python --file x.py --exec
ghar torch --file model.py --forward
\`\`\`

## Summary scoreboard

| Pillar | Status | Headline |
|--------|--------|----------|
| Integration | $([[ $test_rc -eq 0 ]] && echo PASS || echo FAIL) | CLI suites |
| Synthetic hallu | $([[ $hall_rc -eq 0 ]] && echo PASS || echo FAIL) | catch_rate=$hall_rec |
| Python/Torch | $([[ $pyt_rc -eq 0 ]] && echo PASS || echo FAIL) | catch=$pyt_rec speedup=$pyt_spd |
| Perf uplift | $([[ $perf_rc -eq 0 ]] && echo PASS || echo FAIL) | speedup=$speedup |
| Real-model eval | $([[ $real_rc -eq 0 ]] && echo PASS || echo FAIL) | model=$rm_model catch=$rm_catch |

Overall exit: $([[ $test_rc -eq 0 && $hall_rc -eq 0 && $pyt_rc -eq 0 && $perf_rc -eq 0 && $real_rc -eq 0 ]] && echo 0 || echo 4)

**Acceptance rule:** exit 0 only if synthetic detection + python/torch + perf speedup + real-model pillar all pass (no dead-code / synthetic-only acceptance).
EOF
} > "$REPORT"

echo
echo "############################################################"
echo "# REPORT → $REPORT"
echo "############################################################"
cat "$REPORT"

if [[ "$test_rc" -eq 0 && "$hall_rc" -eq 0 && "$pyt_rc" -eq 0 && "$perf_rc" -eq 0 && "$real_rc" -eq 0 ]]; then
  exit 0
fi
exit 4

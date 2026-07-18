#!/usr/bin/env bash
# Python / PyTorch hallucination + validation suite.
# Honest TP only when expect_fail and ghar exits non-zero with matching failure_class.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export GHAR_ROOT="$ROOT"
GHAR="${GHAR:-$ROOT/build/ghar}"
OUT_DIR="${OUT_DIR:-$ROOT/results}"
mkdir -p "$OUT_DIR"
WORK="$OUT_DIR/py_torch_work"
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK"

PY_TD="$ROOT/testdata/python"
TORCH_TD="$ROOT/testdata/torch"

if [[ ! -x "$GHAR" ]]; then
  echo "FAIL: ghar not found at $GHAR" >&2
  exit 2
fi

"$GHAR" init >/dev/null
"$GHAR" reset >/dev/null 2>&1 || true

RESULTS_TSV="$OUT_DIR/py_torch_cases.tsv"
SUMMARY_TSV="$OUT_DIR/py_torch_summary.tsv"
echo -e "id\tcategory\tagent_claim\texpect_exit\tgot_exit\tstatus\tcaught\tlatency_ms\tdetail" > "$RESULTS_TSV"

tp=0 tn=0 fp=0 fn=0 total=0 lat_sum=0

run_case() {
  local id="$1" cat="$2" claim="$3" expect="$4"
  shift 4
  local start end got lat status caught detail
  start=$(date +%s%N)
  set +e
  detail=$("$@" 2>&1)
  got=$?
  set -e
  end=$(date +%s%N)
  lat=$(awk -v s="$start" -v e="$end" 'BEGIN{printf "%.3f", (e-s)/1e6}')
  lat_sum=$(awk -v a="$lat_sum" -v b="$lat" 'BEGIN{print a+b}')
  total=$((total + 1))

  if [[ "$got" -eq "$expect" ]]; then
    status="pass"
    if [[ "$expect" -eq 4 || "$expect" -eq 2 ]]; then
      tp=$((tp + 1))
      caught=1
    else
      tn=$((tn + 1))
      caught=0
    fi
  else
    status="fail"
    if [[ "$expect" -eq 0 && "$got" -ne 0 ]]; then
      fp=$((fp + 1))
      caught=1
    else
      fn=$((fn + 1))
      caught=0
    fi
  fi

  detail=$(echo "$detail" | tr '\t' ' ' | tr '\n' ' ' | head -c 200)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$id" "$cat" "$claim" "$expect" "$got" "$status" "$caught" "$lat" "$detail" >> "$RESULTS_TSV"
  printf '  [%s] %s expect=%s got=%s %s (%.1f ms)\n' "$id" "$status" "$expect" "$got" "$cat" "$lat"
}

echo "== Python / PyTorch validation suite =="

# --- Python ---
run_case P01 python "valid hello.py syntax+import+exec" 0 \
  "$GHAR" python --file "$PY_TD/ok_hello.py" --exec --name P01

run_case P02 python "syntax error compiles? agent claims ok" 4 \
  "$GHAR" python --file "$PY_TD/syntax_bad.py" --name P02

run_case P03 python "fake package import ok" 4 \
  "$GHAR" python --file "$PY_TD/import_missing.py" --name P03

run_case P04 python "exec raises RuntimeError" 4 \
  "$GHAR" python --file "$PY_TD/exec_error.py" --exec --name P04

run_case P05 python "syntax-only on valid file" 0 \
  "$GHAR" python --file "$PY_TD/ok_hello.py" --no-imports --name P05

# --- Torch attrs ---
run_case T01 torch-attr "real torch.nn.Linear exists" 0 \
  "$GHAR" torch-attr torch.nn.Linear torch.matmul --name T01

run_case T02 torch-attr "hallucinated SuperDuperLayer exists" 4 \
  "$GHAR" torch-attr torch.nn.SuperDuperLayer --name T02

# --- Torch module validate ---
run_case T03 torch "linear model forward ok" 0 \
  "$GHAR" torch --file "$TORCH_TD/ok_linear.py" --forward --name T03

run_case T04 torch "hallucinated attr in source" 4 \
  "$GHAR" torch --file "$TORCH_TD/hallucinated_attr.py" --forward --strict-attrs --name T04

run_case T05 torch "shape mismatch forward" 4 \
  "$GHAR" torch --file "$TORCH_TD/bad_forward.py" --forward --name T05

run_case T06 torch "naive matmul run_op ok" 0 \
  "$GHAR" torch --file "$TORCH_TD/matmul_naive.py" --forward --name T06

run_case T07 torch "opt matmul run_op ok" 0 \
  "$GHAR" torch --file "$TORCH_TD/matmul_opt.py" --forward --name T07

# --- Uplift: pure-Python triple loop vs torch.matmul (process wall via ghar bench) ---
echo "== Torch matmul uplift (naive pure-Python vs torch BLAS) =="
DRIVER="$TORCH_TD/uplift_driver.py"
set +e
both_out=$(python3 "$DRIVER" both 80 2>&1)
set -e
naive_ms=$(echo "$both_out" | sed -n 's/.*naive_ms=\([0-9.]*\).*/\1/p')
opt_ms=$(echo "$both_out" | sed -n 's/.*opt_ms=\([0-9.]*\).*/\1/p')
speedup=$(echo "$both_out" | sed -n 's/.*speedup=\([0-9.]*\).*/\1/p')
if [[ -z "$speedup" ]]; then
  echo "WARN: uplift parse failed: $both_out" >&2
  speedup="n/a"
  uplift_ok=0
else
  uplift_ok=$(awk -v s="$speedup" 'BEGIN{print (s+0 >= 2.0) ? 1 : 0}')
fi
printf '  in-process naive_ms=%s opt_ms=%s speedup=%s uplift_ok=%s\n' \
  "${naive_ms:-?}" "${opt_ms:-?}" "$speedup" "$uplift_ok"

# Process-level bench: pure-Python n large enough that wall time still exceeds
# torch import+op on the optimized process (~1.2s import). n=300 ≈ 1.8–2.1s naive.
N_UPLIFT=300
"$GHAR" bench --name torch_matmul_naive --warmup 0 --repeat 2 \
  -- python3 "$DRIVER" naive "$N_UPLIFT" >/dev/null
"$GHAR" bench --name torch_matmul_opt --warmup 0 --repeat 2 \
  -- python3 "$DRIVER" opt "$N_UPLIFT" >/dev/null
"$GHAR" bench --name torch_matmul_speedup --warmup 0 --repeat 2 \
  --baseline-sh "python3 $DRIVER naive $N_UPLIFT" \
  --sh "python3 $DRIVER opt $N_UPLIFT" >/dev/null

# Require process-level speedup >= 1.3 (honest: import dilutes BLAS, still opt wins)
run_case T08 assert "matmul pure-python wall >> torch process (speedup>=1.3)" 0 \
  "$GHAR" assert --from torch_matmul_speedup --metric speedup --op ge --value 1.3 \
    --name torch_uplift_ge_1_3

# Gate: after good python+torch claims only — reset then re-run ok ones
"$GHAR" reset --keep-results >/dev/null 2>&1 || "$GHAR" reset >/dev/null
"$GHAR" python --file "$PY_TD/ok_hello.py" --exec --name gate_py >/dev/null
"$GHAR" torch --file "$TORCH_TD/ok_linear.py" --forward --name gate_torch >/dev/null
run_case T09 gate "python+torch claims pass gate" 0 \
  "$GHAR" gate

# Summary
avg_lat=$(awk -v s="$lat_sum" -v n="$total" 'BEGIN{if(n>0)printf "%.3f",s/n; else print 0}')
prec=$(awk -v tp="$tp" -v fp="$fp" 'BEGIN{d=tp+fp; if(d>0)printf "%.4f",tp/d; else print "n/a"}')
rec=$(awk -v tp="$tp" -v fn="$fn" 'BEGIN{d=tp+fn; if(d>0)printf "%.4f",tp/d; else print "n/a"}')
acc=$(awk -v tp="$tp" -v tn="$tn" -v fp="$fp" -v fn="$fn" 'BEGIN{
  d=tp+tn+fp+fn; if(d>0)printf "%.4f",(tp+tn)/d; else print "n/a"}')
fpr=$(awk -v fp="$fp" -v tn="$tn" 'BEGIN{d=fp+tn; if(d>0)printf "%.4f",fp/d; else print "n/a"}')

{
  echo -e "metric\tvalue"
  echo -e "tp\t$tp"
  echo -e "tn\t$tn"
  echo -e "fp\t$fp"
  echo -e "fn\t$fn"
  echo -e "n_cases\t$total"
  echo -e "accuracy\t$acc"
  echo -e "precision\t$prec"
  echo -e "recall_catch_rate\t$rec"
  echo -e "false_positive_rate\t$fpr"
  echo -e "avg_latency_ms\t$avg_lat"
  echo -e "torch_matmul_naive_ms\t${naive_ms:-n/a}"
  echo -e "torch_matmul_opt_ms\t${opt_ms:-n/a}"
  echo -e "torch_matmul_speedup\t$speedup"
  echo -e "torch_uplift_ok\t$uplift_ok"
} > "$SUMMARY_TSV"

echo
echo "== Summary =="
cat "$SUMMARY_TSV"
echo
echo "cases: $RESULTS_TSV"

# Require zero FN on labeled fail cases (honest catch)
if [[ "$fn" -gt 0 || "$fp" -gt 0 ]]; then
  echo "FAIL: fn=$fn fp=$fp (need perfect catch on labeled suite)" >&2
  exit 4
fi
if [[ "$tp" -lt 4 ]]; then
  echo "FAIL: tp=$tp (need at least 4 true catches)" >&2
  exit 4
fi
echo "PASS py_torch suite tp=$tp tn=$tn"
exit 0

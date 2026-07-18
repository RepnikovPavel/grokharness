#!/usr/bin/env bash
# Synthetic agent claims vs programmatic ghar checks.
# Metrics: catch_rate, false_positive_rate, precision, recall, latency_ms.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHAR="${GHAR:-$ROOT/build/ghar}"
OUT_DIR="${OUT_DIR:-$ROOT/results}"
mkdir -p "$OUT_DIR"
WORK="$OUT_DIR/hallucination_work"
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK"

"$GHAR" init >/dev/null
"$GHAR" reset >/dev/null 2>&1 || true

# Prepare artifacts for symbol/compile cases
cat > hello.cpp <<'EOF'
#include <cstdio>
int main(){ std::puts("ok"); return 0; }
EOF
cat > bad.cpp <<'EOF'
int main(){ return no_such_identifier_abc; }
EOF

"$GHAR" compile hello.cpp --name hello_bin >/dev/null
BIN="./.ghar/bin_hello_bin"

# Precompute real matmul speedup for H08 (truth claim)
N="${MATMUL_N:-192}"
g++ -O2 -std=c++17 -o mat_naive "$ROOT/dont_read_me_src/benchmarks/workloads/matmul_naive.cpp"
g++ -O2 -std=c++17 -o mat_opt "$ROOT/dont_read_me_src/benchmarks/workloads/matmul_opt.cpp"
"$GHAR" bench --name mat_naive --warmup 1 --repeat 5 -- ./mat_naive "$N" 1 >/dev/null
"$GHAR" bench --name mat_opt --warmup 1 --repeat 5 -- ./mat_opt "$N" 1 >/dev/null
# speedup via baseline
"$GHAR" bench --name mat_vs --warmup 1 --repeat 5 \
  --baseline-sh "./mat_naive $N 1" --sh "./mat_opt $N 1" >/dev/null

RESULTS_TSV="$OUT_DIR/hallucination_cases.tsv"
echo -e "id\tcategory\tagent_claim\texpect_exit\tgot_exit\tstatus\tcaught\tlatency_ms\tdetail" > "$RESULTS_TSV"

tp=0  # true positive: expect fail (4) and got fail
tn=0  # true negative: expect ok (0) and got ok
fp=0  # false positive: expect ok but got fail
fn=0  # false negative: expect fail but got ok
total=0
lat_sum=0

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
    if [[ "$expect" -eq 4 ]]; then
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

  # one-line detail
  detail=$(echo "$detail" | tr '\t' ' ' | tr '\n' ' ' | head -c 160)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$id" "$cat" "$claim" "$expect" "$got" "$status" "$caught" "$lat" "$detail" >> "$RESULTS_TSV"
  printf '  [%s] %s expect=%s got=%s %s (%.1f ms)\n' "$id" "$status" "$expect" "$got" "$cat" "$lat"
}

echo "== Hallucination suite =="

run_case H01 import "fake package exists" 4 \
  "$GHAR" import totally_fake_llm_package_xyz --name H01

run_case H02 import "sys imports" 0 \
  "$GHAR" import sys --name H02

run_case H03 compile "bad C++ compiles" 4 \
  "$GHAR" compile bad.cpp --name H03

run_case H04 compile "hello compiles" 0 \
  "$GHAR" compile hello.cpp --name H04

run_case H05 symbols "fake API in binary" 4 \
  "$GHAR" symbols cudaMagicFrobEx --bin "$BIN" --name H05

run_case H06 symbols "main exists" 0 \
  "$GHAR" symbols main --bin "$BIN" --name H06

# Agent claims 1000x without evidence → assert against measured speedup metric
run_case H07 assert_perf "1000x speedup claim" 4 \
  "$GHAR" assert --from mat_vs --metric speedup --op ge --value 1000 --name H07

run_case H08 assert_perf ">=1.2x speedup measured" 0 \
  "$GHAR" assert --from mat_vs --metric speedup --op ge --value 1.2 --name H08

run_case H09 run "false exits 0" 4 \
  "$GHAR" run --name H09 -- false

run_case H10 run "true exits 0" 0 \
  "$GHAR" run --name H10 -- true

# Derived metrics
# precision = tp/(tp+fp), recall = tp/(tp+fn)
prec=$(awk -v tp="$tp" -v fp="$fp" 'BEGIN{if(tp+fp==0)print 0; else printf "%.4f", tp/(tp+fp)}')
rec=$(awk -v tp="$tp" -v fn="$fn" 'BEGIN{if(tp+fn==0)print 0; else printf "%.4f", tp/(tp+fn)}')
# catch_rate among claims that SHOULD fail
catch=$(awk -v tp="$tp" -v fn="$fn" 'BEGIN{if(tp+fn==0)print 0; else printf "%.4f", tp/(tp+fn)}')
# false positive rate among claims that SHOULD pass
fpr=$(awk -v fp="$fp" -v tn="$tn" 'BEGIN{if(fp+tn==0)print 0; else printf "%.4f", fp/(fp+tn)}')
acc=$(awk -v tp="$tp" -v tn="$tn" -v n="$total" 'BEGIN{printf "%.4f", (tp+tn)/n}')
avg_lat=$(awk -v s="$lat_sum" -v n="$total" 'BEGIN{printf "%.3f", s/n}')

SUMMARY="$OUT_DIR/hallucination_summary.tsv"
{
  echo -e "metric\tvalue"
  echo -e "total_cases\t$total"
  echo -e "tp\t$tp"
  echo -e "tn\t$tn"
  echo -e "fp\t$fp"
  echo -e "fn\t$fn"
  echo -e "accuracy\t$acc"
  echo -e "precision\t$prec"
  echo -e "recall_catch_rate\t$rec"
  echo -e "false_positive_rate\t$fpr"
  echo -e "avg_latency_ms\t$avg_lat"
} > "$SUMMARY"

echo
echo "== Hallucination metrics =="
column -t -s $'\t' "$SUMMARY" 2>/dev/null || cat "$SUMMARY"
echo
echo "Per-case: $RESULTS_TSV"

# Fail suite if any FN (missed hallucination) or FP (broke good claim)
if [[ "$fn" -ne 0 || "$fp" -ne 0 ]]; then
  echo "HALLUCINATION SUITE REGRESSION: fn=$fn fp=$fp" >&2
  exit 4
fi
echo "HALLUCINATION SUITE OK (catch_rate=$catch accuracy=$acc)"
exit 0

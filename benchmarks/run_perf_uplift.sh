#!/usr/bin/env bash
# Measure REAL optimization uplift with ghar bench/assert.
# Outputs: results/perf_uplift.tsv + human table.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GHAR="${GHAR:-$ROOT/build/ghar}"
OUT_DIR="${OUT_DIR:-$ROOT/results}"
mkdir -p "$OUT_DIR"
WORK="$OUT_DIR/perf_work"
rm -rf "$WORK"
mkdir -p "$WORK"
cd "$WORK"

N="${MATMUL_N:-256}"
REPEAT="${BENCH_REPEAT:-8}"
WARMUP="${BENCH_WARMUP:-2}"

echo "== Build workloads (ghar compile) =="
"$GHAR" init >/dev/null
"$GHAR" compile "$ROOT/benchmarks/workloads/matmul_naive.cpp" \
  --name matmul_naive_build --flag -O2 >/dev/null
"$GHAR" compile "$ROOT/benchmarks/workloads/matmul_opt.cpp" \
  --name matmul_opt_build --flag -O2 >/dev/null

NAIVE_BIN="./.ghar/bin_matmul_naive_build"
OPT_BIN="./.ghar/bin_matmul_opt_build"

echo "== Bench naive vs blocked matmul N=$N =="
"$GHAR" bench --name matmul_naive --warmup "$WARMUP" --repeat "$REPEAT" \
  -- "$NAIVE_BIN" "$N" 1
"$GHAR" bench --name matmul_opt --warmup "$WARMUP" --repeat "$REPEAT" \
  -- "$OPT_BIN" "$N" 1
"$GHAR" bench --name matmul_speedup --warmup "$WARMUP" --repeat "$REPEAT" \
  --baseline-sh "$NAIVE_BIN $N 1" --sh "$OPT_BIN $N 1"

# Extract metrics from claims via report TSV
parse_metric() {
  local name="$1" key="$2"
  # claims lines: kind=claim ... m.KEY=val name=NAME
  "$GHAR" claims 2>/dev/null | tr '\t' '\n' | awk -v n="$name" -v k="$key" '
    BEGIN{name="";}
    /^name=/ {name=substr($0,6)}
    name==n && $0 ~ ("^m\\." k "=") {
      split($0,a,"="); print a[2]; exit
    }
  '
}

# Fallback: parse from claims.tsv in .ghar
from_claims_file() {
  local name="$1" key="$2"
  if [[ ! -f .ghar/claims.tsv ]]; then
    echo ""
    return
  fi
  awk -F'\t' -v n="$name" -v k="$key" '
    $1==n {
      n2=split($4, parts, ";");
      for(i=1;i<=n2;i++){
        split(parts[i], kv, "=");
        if(kv[1]==k){ print kv[2]; exit }
      }
    }
  ' .ghar/claims.tsv
}

naive_ms=$(from_claims_file matmul_naive mean_ms)
opt_ms=$(from_claims_file matmul_opt mean_ms)
speedup=$(from_claims_file matmul_speedup speedup)
naive_std=$(from_claims_file matmul_naive std_ms)
opt_std=$(from_claims_file matmul_opt std_ms)
naive_cv=$(from_claims_file matmul_naive cv)
opt_cv=$(from_claims_file matmul_opt cv)

# Assert contracts (this is the "agent must prove uplift" gate)
set +e
"$GHAR" assert --from matmul_speedup --metric speedup --op ge --value 1.3 --name uplift_ge_1_3 >/dev/null
a1=$?
"$GHAR" assert --from matmul_opt --metric mean_ms --op lt --value "$naive_ms" --name opt_faster_than_naive >/dev/null
a2=$?
set -e

# GFLOPS from one run stdout
naive_out=$("$NAIVE_BIN" "$N" 1)
opt_out=$("$OPT_BIN" "$N" 1)
naive_gflops=$(echo "$naive_out" | awk -F= '/^gflops=/{print $2}')
opt_gflops=$(echo "$opt_out" | awk -F= '/^gflops=/{print $2}')

TSV="$OUT_DIR/perf_uplift.tsv"
{
  echo -e "metric\tnaive\topt\tuplift"
  echo -e "mean_ms\t$naive_ms\t$opt_ms\t${speedup}x"
  echo -e "std_ms\t$naive_std\t$opt_std\t"
  echo -e "cv\t$naive_cv\t$opt_cv\t"
  echo -e "gflops\t$naive_gflops\t$opt_gflops\t"
  echo -e "n\t$N\t$N\t"
  echo -e "repeat\t$REPEAT\t$REPEAT\t"
  echo -e "assert_speedup_ge_1.3\t-\t-\texit=$a1"
  echo -e "assert_opt_lt_naive\t-\t-\texit=$a2"
} > "$TSV"

echo
echo "== PERF UPLIFT (matmul blocked vs naive) =="
printf '%-22s %14s %14s %12s\n' "metric" "naive" "opt" "uplift"
printf '%-22s %14s %14s %12s\n' "----------------------" "--------------" "--------------" "------------"
printf '%-22s %14s %14s %12s\n' "mean_ms" "$naive_ms" "$opt_ms" "${speedup}x"
printf '%-22s %14s %14s %12s\n' "std_ms" "$naive_std" "$opt_std" ""
printf '%-22s %14s %14s %12s\n' "gflops" "$naive_gflops" "$opt_gflops" ""
printf '%-22s %14s %14s %12s\n' "assert speedup>=1.3" "-" "-" "exit $a1"
echo
echo "Wrote $TSV"

# CUDA path is optional — must never fail the required matmul uplift pillar.
if command -v nvcc >/dev/null 2>&1 || [[ -x /usr/local/cuda/bin/nvcc ]]; then
  echo "== CUDA SAXPY block-size probe (optional) =="
  set +e
  NVCC="${NVCC:-$(command -v nvcc 2>/dev/null || echo /usr/local/cuda/bin/nvcc)}"
  "$NVCC" -O2 -o saxpy "$ROOT/benchmarks/workloads/saxpy_cuda.cu"
  nvcc_rc=$?
  if [[ "$nvcc_rc" -eq 0 ]]; then
    NCUDA="${CUDA_N:-16777216}"
    "$GHAR" bench --name saxpy_b128 --warmup 1 --repeat 5 -- ./saxpy "$NCUDA" 128 30
    "$GHAR" bench --name saxpy_b256 --warmup 1 --repeat 5 -- ./saxpy "$NCUDA" 256 30
    b128=$(from_claims_file saxpy_b128 mean_ms)
    b256=$(from_claims_file saxpy_b256 mean_ms)
    cuda_ratio=$(awk -v a="$b128" -v b="$b256" 'BEGIN{if(b==0)print "nan"; else printf "%.4f", a/b}')
    CUDA_TSV="$OUT_DIR/cuda_uplift.tsv"
    {
      echo -e "metric\tblock128\tblock256\tratio_128_over_256"
      echo -e "mean_ms\t$b128\t$b256\t$cuda_ratio"
      echo -e "n\t$NCUDA\t$NCUDA\t"
    } > "$CUDA_TSV"
    printf '%-22s %14s %14s %12s\n' "cuda mean_ms" "$b128" "$b256" "ratio $cuda_ratio"
    echo "Wrote $CUDA_TSV"
  else
    echo "SKIP CUDA SAXPY (nvcc compile failed)"
  fi
  set -e
else
  echo "SKIP CUDA SAXPY (no nvcc)"
fi

if [[ "$a1" -ne 0 || "$a2" -ne 0 ]]; then
  echo "PERF UPLIFT ASSERT FAILED (speedup too small or measurement broken)" >&2
  exit 4
fi
echo "PERF UPLIFT OK speedup=${speedup}x"
exit 0

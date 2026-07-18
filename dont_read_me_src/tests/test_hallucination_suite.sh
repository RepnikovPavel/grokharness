#!/usr/bin/env bash
# Drive the real synthetic hallucination suite (H01–H10) on the shipped binary.
# Proves ghar catches labeled model-style lies with fn=0 fp=0.
source "$(dirname "$0")/common.sh"

echo "== test_hallucination_suite =="

SUITE="$ROOT/dont_read_me_src/benchmarks/run_hallucination_suite.sh"
if [[ -f "$SUITE" ]]; then suite_exists=yes; else suite_exists=no; fi
assert_eq "$suite_exists" "yes" "suite script exists"

# Isolate suite artifacts under TMPBASE so we do not race the repo results/
OUT="$TMPBASE/hallu_out"
mkdir -p "$OUT"
set +e
GHAR="$GHAR" OUT_DIR="$OUT" bash "$SUITE" >"$TMPBASE/hallu_suite.out" 2>&1
ec=$?
set -e

assert_eq "$ec" "0" "hallucination suite exit 0"
out=$(cat "$TMPBASE/hallu_suite.out")
assert_contains "$out" "HALLUCINATION SUITE OK" "suite OK banner"
assert_contains "$out" "[H01] pass" "H01 fake import caught"
assert_contains "$out" "[H07] pass" "H07 1000x speedup claim caught"
assert_contains "$out" "[H10] pass" "H10 true run green"

sum="$OUT/hallucination_summary.tsv"
assert_eq "$(test -f "$sum" && echo yes)" "yes" "summary tsv written"
# Parse metrics from the real suite summary (not hard-coded in this shell)
fn=$(awk -F'\t' '$1=="fn"{print $2; exit}' "$sum")
fp=$(awk -F'\t' '$1=="fp"{print $2; exit}' "$sum")
rec=$(awk -F'\t' '$1=="recall_catch_rate"{print $2; exit}' "$sum")
assert_eq "$fn" "0" "fn=0 (no missed hallucinations)"
assert_eq "$fp" "0" "fp=0 (no broken good claims)"
assert_eq "$rec" "1.0000" "recall_catch_rate=1.0000"

# Per-case: every expect-fail row must have caught=1
cases="$OUT/hallucination_cases.tsv"
assert_eq "$(test -f "$cases" && echo yes)" "yes" "cases tsv written"
missed=$(awk -F'\t' 'NR>1 && $4=="4" && $7!="1"{c++} END{print c+0}' "$cases")
assert_eq "$missed" "0" "all fail-expect cases caught"

summary test_hallucination_suite

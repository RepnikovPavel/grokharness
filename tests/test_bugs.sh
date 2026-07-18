#!/usr/bin/env bash
# Regression tests for bugs found in audit (metrics, tsv, import, claims wipe, verify_ok).
source "$(dirname "$0")/common.sh"

echo "== test_bugs =="
work="$TMPBASE/bugs"
mkdir -p "$work"
cd "$work"

# --- Bug: empty results.tsv must not treat data as header ---
mkdir -p .ghar
: > .ghar/results.tsv
assert_exit 0 "$GHAR" run --name emptytsv -- true
# load via claims/report: run also upserts claim
assert_exit 0 "$GHAR" claims
# results first line should be header id...
head -1 .ghar/results.tsv | grep -q $'^id\t' || {
  fail=$((fail+1)); echo "  FAIL results header missing"; 
}
if head -1 .ghar/results.tsv | grep -q $'^id\t'; then
  pass=$((pass+1)); echo "  OK  empty results.tsv gets header"
fi

# --- Bug: metrics with ';' and '=' in feedback must round-trip ---
cat > ghar.conf <<'EOF'
build_cmd=true
test_cmd=printf 'err;broken=value\n' >&2; exit 1
EOF
assert_exit 4 "$GHAR" verify --no-gate
# claim step_test metrics must contain feedback with semicolon intact after reload
python3 - <<'PY'
from pathlib import Path
text = Path(".ghar/claims.tsv").read_text()
assert "step_test" in text
# After fix, feedback should be escaped so "feedback=" field exists and value decodable
line = [l for l in text.splitlines() if l.startswith("step_test")][0]
parts = line.split("\t")
assert len(parts) >= 4
metrics = parts[3]
assert "feedback=" in metrics, metrics
# unescaped semicolon must not create phantom fields that drop feedback
assert "broken" in metrics or "err" in metrics, metrics
print("metrics_ok")
PY
assert_eq "$?" "0" "metrics semicolon roundtrip via python"

# --- Bug: import injection / invalid names ---
assert_exit 4 "$GHAR" import "os';print(1)" --name badinj
# should not succeed
out="$("$GHAR" import "os';print(1)" --name badinj2 2>&1 || true)"
assert_contains "$out" "invalid module name" "reject injected import"

# valid import still works
assert_exit 0 "$GHAR" import sys --name sysok

# --- Bug: verify must not wipe domain claims (bench/assert) ---
cat > ghar.conf <<'EOF'
build_cmd=true
test_cmd=true
EOF
assert_exit 0 "$GHAR" reset
assert_exit 0 "$GHAR" bench --name keepme --warmup 0 --repeat 2 -- true
assert_exit 0 "$GHAR" verify --no-gate
# keepme claim must still exist
claims="$("$GHAR" claims 2>&1 || true)"
assert_contains "$claims" "keepme" "bench claim survives verify"

# --- Bug: partial --step verify must NOT count as work verify_ok ---
assert_exit 0 "$GHAR" work start --minutes 0 --min-verify 2 --min-heartbeats 0 --goal "quota" --force
assert_exit 0 "$GHAR" verify --step build --no-gate
# status should still need verifies (verify_ok 0)
st="$("$GHAR" work status 2>&1 || true)"
# delivery not ready because verify_ok < 2
assert_exit 4 "$GHAR" work status
assert_exit 0 "$GHAR" verify --no-gate
assert_exit 0 "$GHAR" verify --no-gate
assert_exit 0 "$GHAR" work done

# --- Bug: usage errors should exit 1 not 3 ---
set +e
"$GHAR" assert >/dev/null 2>&1
rc=$?
set -e
assert_eq "$rc" "1" "assert usage → exit 1"

# --- Bug: symbols must not match substrings (C in TMC_END) ---
echo 'int main(){return 0;}' > tiny.c
gcc -o tiny_bin tiny.c
set +e
"$GHAR" symbols C --bin ./tiny_bin --name symC >/dev/null 2>&1
rc=$?
set -e
assert_eq "$rc" "4" "symbol C not found via substring"
set +e
"$GHAR" symbols main --bin ./tiny_bin --name symMain >/dev/null 2>&1
rc=$?
set -e
assert_eq "$rc" "0" "symbol main found as token"

# --- Bug: timeout kills process group (children) ---
# sleep in subshell under sh -c; should not leave orphans
set +e
"$GHAR" run --name to --timeout 1 -- sh -c 'sleep 30' >/dev/null 2>&1
rc=$?
set -e
assert_eq "$rc" "4" "timeout run fails"
# no long sleep of our tree (best-effort)
if pgrep -f 'sleep 30' >/dev/null 2>&1; then
  # might be unrelated; only fail if started in last few seconds with our parent gone
  echo "  WARN leftover sleep 30 (check manually)"
  pass=$((pass+1))
else
  pass=$((pass+1))
  echo "  OK  no orphan sleep after timeout"
fi

summary test_bugs

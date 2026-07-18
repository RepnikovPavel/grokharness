#!/usr/bin/env bash
# Real HTTP integration test against ui/app.py (shipped entrypoint).
# Uses sample_results so it never depends on host scan state.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRATCH="${SCRATCH:-/tmp/grok-goal-c543634211f1/implementer}"
mkdir -p "$SCRATCH"
LOG="$SCRATCH/test_ui_http.log"
: >"$LOG"

pick_port() {
  python3 - <<'PY'
import socket
s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()
PY
}

PORT="$(pick_port)"
export GHAR_RESULTS="$ROOT/ui/sample_results"
export GHAR_UI_HOST=127.0.0.1
export GHAR_UI_PORT="$PORT"

echo "== test_ui_http port=$PORT results=$GHAR_RESULTS ==" | tee -a "$LOG"

python3 "$ROOT/ui/app.py" >>"$LOG" 2>&1 &
PID=$!
cleanup() { kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; }
trap cleanup EXIT

# wait ready
ok=0
for i in $(seq 1 40); do
  if curl -sf --max-time 1 "http://127.0.0.1:$PORT/api/health" >/dev/null; then
    ok=1
    break
  fi
  sleep 0.1
done
if [[ "$ok" != "1" ]]; then
  echo "FAIL server did not become ready" | tee -a "$LOG"
  cat "$LOG" | tail -30
  exit 4
fi

BASE="http://127.0.0.1:$PORT"
fail=0
check() {
  local path="$1" needle="$2"
  local body code
  body=$(curl -sS --max-time 3 "$BASE$path")
  code=$(curl -sS -o /dev/null -w '%{http_code}' --max-time 3 "$BASE$path")
  if [[ "$code" != "200" ]]; then
    echo "FAIL $path http=$code" | tee -a "$LOG"
    fail=$((fail+1))
    return
  fi
  if [[ "$body" != *"$needle"* ]]; then
    echo "FAIL $path missing '$needle'" | tee -a "$LOG"
    echo "  body[:200]=${body:0:200}" | tee -a "$LOG"
    fail=$((fail+1))
    return
  fi
  echo "OK  $path contains $needle" | tee -a "$LOG"
}

check / "Actionable findings"
check /findings "SuperDuperLayer"
check "/findings?mode=actionable" "api_missing"
check /real-model "CAUGHT"
check /real-model "RM01"
check /hallucinations "H01"
check /benchmarks "hallucination_suite"
check /benchmarks "py_torch_suite"
check /benchmarks "real_model_eval"
check /api/health '"status": "ok"'

export BASE="$BASE"
python3 - <<'PY' | tee -a "$LOG"
import json, urllib.request, os
base = os.environ["BASE"]
raw = urllib.request.urlopen(base + "/api/findings?mode=actionable", timeout=3).read()
data = json.loads(raw)
rows = data["findings"]
assert rows, "actionable findings empty"
assert all(r.get("path") and r.get("kind") for r in rows)
attrs = {r.get("attr") for r in rows}
assert "torch.nn.SuperDuperLayer" in attrs, attrs
raw2 = urllib.request.urlopen(base + "/api/findings?mode=all", timeout=3).read()
all_rows = json.loads(raw2)["findings"]
assert len(all_rows) >= len(rows)
rm = json.loads(urllib.request.urlopen(base + "/api/real-model", timeout=3).read())
assert any(str(c.get("caught_by_ghar")) == "1" for c in rm["cases"])
print("OK  api json contracts")
PY

# Also exercise loaders unit-level against sample
python3 - <<PY | tee -a "$LOG"
import sys
from pathlib import Path
sys.path.insert(0, "$ROOT/ui")
import data
root = Path("$ROOT/ui/sample_results")
f = data.load_code_findings(root)
assert any(x.actionable and x.attr == "torch.nn.SuperDuperLayer" for x in f)
assert any(not x.actionable and x.category == "accel_backend" for x in f)
assert data.load_real_model_cases(root)[0]["id"] == "RM01"
b = {x.id for x in data.list_benchmarks(root)}
assert {"hallucination_suite","py_torch_suite","real_model_eval"} <= b
print("OK  data loaders sample_results")
PY

if [[ "$fail" -ne 0 ]]; then
  echo "FAIL test_ui_http ($fail checks)" | tee -a "$LOG"
  exit 4
fi
echo "PASS test_ui_http" | tee -a "$LOG"

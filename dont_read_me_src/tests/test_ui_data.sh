#!/usr/bin/env bash
# Unit-level checks on ui/data.py against sample_results (always present).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRATCH="${SCRATCH:-/tmp/grok-goal-c543634211f1/implementer}"
mkdir -p "$SCRATCH"
export ROOT
SAMPLE="$ROOT/dont_read_me_src/ui/sample_results"

echo "== test_ui_data sample=$SAMPLE =="

python3 - <<PY | tee "$SCRATCH/test_ui_data.log"
import sys
from pathlib import Path
repo = Path("$ROOT")
sys.path.insert(0, str(repo / "dont_read_me_src" / "ui"))
import data

root = Path("$SAMPLE")
findings = data.load_code_findings(root)
assert findings, "sample findings empty"
actionable = [f for f in findings if f.actionable]
assert actionable, "need actionable findings in sample"
assert any(f.attr == "torch.nn.SuperDuperLayer" for f in actionable)
assert any(f.category == "accel_backend" and not f.actionable for f in findings)
for f in actionable:
    assert f.path and f.kind

real = data.load_real_model_cases(root)
assert real and real[0]["id"].startswith("RM")
assert any(str(r.get("caught_by_ghar")) == "1" for r in real)

hallu = data.load_hallucination_cases(root)
assert hallu and hallu[0]["id"] == "H01"

benches = data.list_benchmarks(root)
by_id = {b.id: b for b in benches}
for need in ("hallucination_suite", "py_torch_suite", "real_model_eval", "code_scan"):
    assert need in by_id, need
    assert by_id[need].available, f"{need} should have sample data"

st = data.dashboard_stats(root)
assert st["findings_actionable"] >= 1
assert st["real_model_caught"] >= 1
print("PASS test_ui_data")
PY

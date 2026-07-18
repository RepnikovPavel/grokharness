#!/usr/bin/env bash
# Drive real ui/data.py loaders against on-disk results (not hard-coded).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRATCH="${SCRATCH:-/tmp/grok-goal-166377fddc20/implementer}"
mkdir -p "$SCRATCH"
export GHAR_RESULTS="${GHAR_RESULTS:-$ROOT/results}"
export ROOT

echo "== test_ui_data (GHAR_RESULTS=$GHAR_RESULTS) =="

python3 - <<'PY' | tee "$SCRATCH/test_ui_data.log"
import os
import sys
from pathlib import Path

repo = Path(os.environ["ROOT"])
root = Path(os.environ["GHAR_RESULTS"])
sys.path.insert(0, str(repo / "ui"))
import data

findings = data.load_code_findings(root)
print("findings", len(findings))
ft = root / "openmmlab_findings.tsv"
if ft.is_file() and ft.stat().st_size > 40:
    assert len(findings) >= 1, "expected findings from TSV"
    assert findings[0].kind and findings[0].path, "kind/path required"
    print("sample", findings[0].kind, findings[0].path, findings[0].attr)

real = data.load_real_model_cases(root)
assert (root / "real_model_cases.tsv").is_file(), "need real_model_cases.tsv"
assert len(real) >= 1 and real[0].get("id"), "real model rows need id"
print("real_model", len(real), "sample", real[0].get("id"), real[0].get("ghar_detail", "")[:60])

hallu = data.load_hallucination_cases(root)
assert len(hallu) >= 1 and hallu[0].get("id")
print("hallu", len(hallu), "sample", hallu[0].get("id"))

benches = data.list_benchmarks(root)
ids = {b.id for b in benches}
need = {"hallucination_suite", "py_torch_suite", "real_model_eval"}
assert need <= ids, f"missing {need - ids}"
print("benchmarks", sorted(ids))
print("PASS test_ui_data")
PY

# lightweight app import / route unit via handler (no bind)
python3 - <<'PY' | tee -a "$SCRATCH/test_ui_data.log"
import os, sys
from pathlib import Path
from io import BytesIO

repo = Path(os.environ["ROOT"])
sys.path.insert(0, str(repo / "ui"))
os.environ["GHAR_RESULTS"] = os.environ["GHAR_RESULTS"]
import app as ui_app

html = ui_app.page_findings()
assert "Code findings" in html
assert "Kind" in html
# real data marker: either a known path fragment from findings or empty state
assert ("missing_attr" in html) or ("No findings" in html) or ("path" in html.lower())

rm = ui_app.page_real_model()
assert "Coding-model" in rm or "coding-model" in rm.lower() or "Local coding-model" in rm
assert "RM0" in rm or "Case id" in rm

bm = ui_app.page_benchmarks()
assert "hallucination_suite" in bm
assert "py_torch_suite" in bm
assert "real_model_eval" in bm
print("PASS ui page render")
PY

echo "---- test_ui_data PASS ----"

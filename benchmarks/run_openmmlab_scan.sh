#!/usr/bin/env bash
# Clone curated OpenMMLab repos and run ghar static scanners (syntax + torch attrs).
# Produces:
#   results/openmmlab_findings.tsv
#   results/openmmlab_summary.tsv
#   results/OPENMMLAB_SCAN.md
#   results/README_OPENMMLAB_SNIPPET.md
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export GHAR_ROOT="$ROOT"
GHAR="${GHAR:-$ROOT/build/ghar}"
OUT_DIR="${OUT_DIR:-$ROOT/results}"
CACHE="${OPENMMLAB_CACHE:-$OUT_DIR/openmmlab_cache}"
REPOS_TSV="${REPOS_TSV:-$ROOT/benchmarks/openmmlab/repos.tsv}"
MAX_FILES="${MAX_FILES:-0}"
DEPTH="${CLONE_DEPTH:-1}"

mkdir -p "$OUT_DIR" "$CACHE"

if [[ ! -x "$GHAR" ]]; then
  echo "Building ghar..."
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j"$(nproc)"
  GHAR="$ROOT/build/ghar"
fi

FINDINGS="$OUT_DIR/openmmlab_findings.tsv"
SUMMARY="$OUT_DIR/openmmlab_summary.tsv"
REPORT="$OUT_DIR/OPENMMLAB_SCAN.md"
SNIPPET="$OUT_DIR/README_OPENMMLAB_SNIPPET.md"
LOG="$OUT_DIR/openmmlab_scan.log"

rm -f "$FINDINGS" "$SUMMARY"
: > "$LOG"

TORCH_VER=$(python3 -c 'import torch; print(torch.__version__)' 2>/dev/null || echo n/a)
echo "############################################################" | tee -a "$LOG"
echo "# ghar × OpenMMLab static scan" | tee -a "$LOG"
echo "# cache=$CACHE torch=$TORCH_VER" | tee -a "$LOG"
echo "############################################################" | tee -a "$LOG"

TS_START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
t0=$(date +%s)

first=1
while IFS=$'\t' read -r name url ref globs || [[ -n "${name:-}" ]]; do
  [[ -z "${name:-}" || "$name" =~ ^# ]] && continue

  dest="$CACHE/$name"
  if [[ -d "$dest/.git" ]]; then
    echo "  [cache] $name" | tee -a "$LOG"
    git -C "$dest" fetch --depth "$DEPTH" origin "$ref" >>"$LOG" 2>&1 || true
    git -C "$dest" checkout -q FETCH_HEAD 2>/dev/null \
      || git -C "$dest" checkout -q "$ref" 2>/dev/null \
      || true
  else
    echo "  [clone] $name ($url @$ref)" | tee -a "$LOG"
    rm -rf "$dest"
    if ! git clone --depth "$DEPTH" --branch "$ref" --single-branch "$url" "$dest" >>"$LOG" 2>&1; then
      git clone --depth "$DEPTH" "$url" "$dest" >>"$LOG" 2>&1 || {
        echo "  FAIL clone $name" | tee -a "$LOG"
        continue
      }
    fi
  fi

  sha=$(git -C "$dest" rev-parse --short HEAD 2>/dev/null || echo unknown)
  echo "  [scan] $name @$sha globs=${globs:-.}" | tee -a "$LOG"

  append=()
  if [[ "$first" -eq 0 ]]; then
    append=(--append-findings)
  fi
  python3 "$ROOT/oracles/repo_scan.py" \
    --root "$dest" \
    --repo "$name" \
    --globs "${globs:-}" \
    --max-files "$MAX_FILES" \
    --findings "$FINDINGS" \
    --summary "$SUMMARY" \
    "${append[@]}" 2>&1 | tee -a "$LOG"
  first=0

  # Spot-check first package .py through shipped ghar CLI
  sample=$(find "$dest/${globs%%,*}" -name '*.py' 2>/dev/null | head -1 || true)
  if [[ -n "$sample" && -f "$sample" ]]; then
    set +e
    "$GHAR" python --file "$sample" --no-imports --name "omml_${name}_spot" >>"$LOG" 2>&1
    set -e
  fi
done < "$REPOS_TSV"

t1=$(date +%s)
elapsed=$((t1 - t0))
TS_END=$(date -u +%Y-%m-%dT%H:%M:%SZ)

if [[ ! -f "$SUMMARY" ]]; then
  echo "FAIL: no summary written" >&2
  exit 4
fi

python3 - <<PY
import sys
from pathlib import Path
from collections import Counter

summary_path = Path("$SUMMARY")
findings_path = Path("$FINDINGS")
report = Path("$REPORT")
snippet = Path("$SNIPPET")
ts0, ts1, elapsed = "$TS_START", "$TS_END", int("$elapsed")
torch_ver = "$TORCH_VER"
ghar = "$GHAR"

def read_tsv(p: Path):
    if not p.is_file():
        return [], []
    lines = p.read_text(encoding="utf-8").splitlines()
    if not lines:
        return [], []
    cols = lines[0].split("\t")
    rows = []
    for ln in lines[1:]:
        if not ln.strip():
            continue
        parts = ln.split("\t")
        row = {c: (parts[i] if i < len(parts) else "") for i, c in enumerate(cols)}
        rows.append(row)
    return cols, rows

_, srows = read_tsv(summary_path)
_, frows = read_tsv(findings_path)

total_files = sum(int(r.get("files_scanned") or 0) for r in srows)
total_syn_fail = sum(int(r.get("syntax_fail") or 0) for r in srows)
total_missing = sum(int(r.get("missing_attr_findings") or 0) for r in srows)
total_findings = sum(int(r.get("findings_total") or 0) for r in srows)
total_torch_files = sum(int(r.get("torch_files") or 0) for r in srows)
total_attr_refs = sum(int(r.get("torch_attr_refs") or 0) for r in srows)
api_n = sum(int(r.get("api_missing_n") or 0) for r in srows)
accel_n = sum(int(r.get("accel_backend_n") or 0) for r in srows)
build_n = sum(int(r.get("build_flavor_n") or 0) for r in srows)
# prefer recount from findings if category column present
if frows and "category" in frows[0]:
    cat_c = Counter(r.get("category") for r in frows)
    api_n = cat_c.get("api_missing", 0)
    accel_n = cat_c.get("accel_backend", 0)
    build_n = cat_c.get("build_flavor", 0)

by_kind = Counter(r.get("kind", "") for r in frows)
by_cat = Counter(r.get("category", "") for r in frows)
attr_c = Counter(r.get("attr") for r in frows if r.get("kind") == "missing_attr" and r.get("attr"))
top_attrs = attr_c.most_common(15)
api_attrs = Counter(
    r.get("attr") for r in frows if r.get("category") == "api_missing" and r.get("attr")
).most_common(20)

def md_table_repo():
    lines = [
        "| Repo | Files | Syntax fail | Torch files | Attr refs | api_missing | accel | Findings |",
        "|------|------:|------------:|------------:|----------:|------------:|------:|---------:|",
    ]
    for r in srows:
        lines.append(
            f"| \`{r['repo']}\` | {r.get('files_scanned','')} | {r.get('syntax_fail','')} | "
            f"{r.get('torch_files','')} | {r.get('torch_attr_refs','')} | "
            f"{r.get('api_missing_n','')} | {r.get('accel_backend_n','')} | {r.get('findings_total','')} |"
        )
    return "\n".join(lines)

# Prefer showing api_missing examples first (more interesting than npu stubs)
examples = []
for prefer in ("api_missing", "syntax_error", "build_flavor", "accel_backend"):
    for r in frows:
        if (r.get("category") or r.get("kind")) != prefer and r.get("kind") != prefer:
            if r.get("category") != prefer:
                continue
        examples.append(
            f"| \`{r.get('repo')}\` | \`{r.get('path')}\` | \`{r.get('category') or r.get('kind')}\` | "
            f"\`{(r.get('attr') or r.get('line') or '')}\` | {(r.get('detail') or '')[:90]} |"
        )
        if len(examples) >= 12:
            break
    if len(examples) >= 12:
        break
if not examples:
    examples = ["| — | — | — | — | no findings |"]

kind_rows = "\n".join(f"| \`{k}\` | {v} |" for k, v in by_kind.most_common()) or "| — | 0 |"
cat_rows = "\n".join(f"| \`{k}\` | {v} |" for k, v in by_cat.most_common()) or "| — | 0 |"
attr_rows = "\n".join(f"| \`{a}\` | {n} |" for a, n in top_attrs) or "| — | 0 |"
api_rows = "\n".join(f"| \`{a}\` | {n} |" for a, n in api_attrs) or "| — | 0 |"

report_md = f"""# ghar × OpenMMLab scan report

Generated: {ts0} → {ts1} ({elapsed}s)  
Scanner: \`oracles/repo_scan.py\` + spot checks via \`{ghar}\`  
Installed torch: **{torch_ver}**  
Org: [open-mmlab](https://github.com/open-mmlab)

## What this measures (honest scope)

Programmatic static checks only — **no LLM judge**, no full training run:

| Check / category | Meaning |
|------------------|---------|
| **syntax_error** | \`ast.parse\` fails — real syntax bug or corrupt tree |
| **api_missing** | \`torch.*\` name does not exist on stock installed torch (API drift / typo / dead wrapper) |
| **accel_backend** | vendor device namespaces (\`torch.npu\` / \`mlu\` / \`musa\`) absent on CUDA wheels — **expected** |
| **build_flavor** | ROCm/HIP-only symbols on a CUDA build |

Not counted: missing \`mmcv\`/\`mmdet\` package imports, training correctness, CUDA kernel numerics.

## Scoreboard

| Metric | Value |
|--------|------:|
| repos scanned | **{len(srows)}** |
| Python files | **{total_files}** |
| syntax failures | **{total_syn_fail}** |
| files with torch usage | **{total_torch_files}** |
| torch attr references checked | **{total_attr_refs}** |
| findings total | **{total_findings}** |
| of which **api_missing** | **{api_n}** |
| of which **accel_backend** | **{accel_n}** |
| of which **build_flavor** | **{build_n}** |

### Per repository

{md_table_repo()}

### Findings by category

| Category | Count |
|----------|------:|
{cat_rows}

### api_missing attributes (interesting)

| Attr | Count |
|------|------:|
{api_rows}

### Top unresolved attributes overall (host torch={torch_ver})

| Attr | Count |
|------|------:|
{attr_rows}

### Example findings (api_missing first)

| Repo | Path | Category | Attr/Line | Detail |
|------|------|----------|-----------|--------|
{chr(10).join(examples)}

## Reproduce

\`\`\`sh
cmake -S . -B build && cmake --build build -j\$(nproc)
bash benchmarks/run_openmmlab_scan.sh
# → results/OPENMMLAB_SCAN.md
# → results/openmmlab_findings.tsv
\`\`\`

Cache: \`results/openmmlab_cache/\` (shallow clones).  
Config: \`benchmarks/openmmlab/repos.tsv\`.
"""
report.write_text(report_md, encoding="utf-8")

snippet_md = f"""## Benchmark: OpenMMLab static scan

We automatically scan curated [OpenMMLab](https://github.com/open-mmlab) repositories with **ghar**
programmatic oracles (AST syntax + full \`torch.*\` attribute resolve — **no LLM**).

| Metric | Latest run |
|--------|----------:|
| Repos | **{len(srows)}** ([org](https://github.com/open-mmlab)) |
| Python files scanned | **{total_files}** |
| Torch attr refs checked | **{total_attr_refs}** |
| Syntax failures | **{total_syn_fail}** |
| Findings total | **{total_findings}** |
| → **api_missing** (stock torch) | **{api_n}** |
| → accel backends (npu/mlu/musa) | **{accel_n}** |
| → build flavor (ROCm/HIP) | **{build_n}** |
| Host torch | \`{torch_ver}\` |
| Report date (UTC) | {ts1} |

{md_table_repo()}

**Headline:** on torch \`{torch_ver}\`, ghar flags **{api_n}** unresolved stock-API references (e.g. \`SyncBatchNorm2d\`, \`PoolDataLoader\`) plus **{accel_n}** vendor-device symbols expected missing on CUDA wheels.

Full report: [\`benchmarks/openmmlab/OPENMMLAB_SCAN.md\`](benchmarks/openmmlab/OPENMMLAB_SCAN.md) · raw TSV: [\`benchmarks/openmmlab/openmmlab_findings.tsv\`](benchmarks/openmmlab/openmmlab_findings.tsv)

\`\`\`sh
bash benchmarks/run_openmmlab_scan.sh
\`\`\`

*More ecosystems next — this is the first public “scan a famous org” pillar.*
"""
snippet.write_text(snippet_md, encoding="utf-8")
print(f"Wrote {report}")
print(f"Wrote {snippet}")
print(f"TOTAL files={total_files} syntax_fail={total_syn_fail} missing_attr={total_missing} api={api_n} accel={accel_n}")
PY

# Publish committed snapshot under benchmarks/openmmlab/ (results/ is gitignored)
PUB="$ROOT/benchmarks/openmmlab"
mkdir -p "$PUB"
cp -f "$REPORT" "$PUB/OPENMMLAB_SCAN.md"
cp -f "$FINDINGS" "$PUB/openmmlab_findings.tsv"
cp -f "$SUMMARY" "$PUB/openmmlab_summary.tsv"
cp -f "$SNIPPET" "$PUB/README_OPENMMLAB_SNIPPET.md"

echo
echo "DONE → $REPORT"
echo "Published → $PUB/"
cat "$SNIPPET"
exit 0

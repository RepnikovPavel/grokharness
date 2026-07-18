#!/usr/bin/env python3
"""Load ghar result artifacts (TSV/MD) for the local results UI.

Reads only real on-disk outputs produced by ghar suites / scanners.
Never invents findings — empty files yield empty lists.
"""
from __future__ import annotations

import csv
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


def results_dir() -> Path:
    env = os.environ.get("GHAR_RESULTS") or os.environ.get("RESULTS_DIR")
    if env:
        return Path(env)
    # default: repo results/ relative to this file
    here = Path(__file__).resolve().parent
    cand = here.parent / "results"
    if cand.is_dir():
        return cand
    sample = here / "sample_results"
    return sample


def read_tsv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    text = path.read_text(encoding="utf-8", errors="replace")
    # skip comment lines
    lines = [ln for ln in text.splitlines() if ln.strip() and not ln.startswith("#")]
    if not lines:
        return []
    reader = csv.DictReader(lines, delimiter="\t")
    rows: list[dict[str, str]] = []
    for row in reader:
        if row is None:
            continue
        # normalize None → ""
        rows.append({(k or ""): (v if v is not None else "") for k, v in row.items()})
    return rows


def read_kv_tsv(path: Path) -> dict[str, str]:
    """metric\\tvalue style summaries."""
    out: dict[str, str] = {}
    for row in read_tsv(path):
        # try metric/value columns
        if "metric" in row and "value" in row:
            out[row["metric"]] = row["value"]
        elif len(row) >= 2:
            keys = list(row.keys())
            out[row[keys[0]]] = row[keys[1]]
    return out


@dataclass
class Finding:
    source: str
    kind: str
    path: str
    line: str = ""
    attr: str = ""
    category: str = ""
    severity: str = ""
    detail: str = ""
    extra: dict[str, str] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        d = {
            "source": self.source,
            "kind": self.kind,
            "path": self.path,
            "line": self.line,
            "attr": self.attr,
            "category": self.category,
            "severity": self.severity,
            "detail": self.detail,
        }
        d.update(self.extra)
        return d


def load_code_findings(root: Path | None = None) -> list[Finding]:
    """Static code findings (repo scan + py/torch fail cases with path-like detail)."""
    root = root or results_dir()
    findings: list[Finding] = []

    # Primary: openmmlab / repo scan style findings (internal name ok; UI labels generically)
    for name in ("openmmlab_findings.tsv", "code_findings.tsv", "repo_findings.tsv"):
        p = root / name
        for row in read_tsv(p):
            findings.append(
                Finding(
                    source=name,
                    kind=row.get("kind") or "finding",
                    path=row.get("path") or row.get("file") or "",
                    line=row.get("line") or "",
                    attr=row.get("attr") or "",
                    category=row.get("category") or "",
                    severity=row.get("severity") or "",
                    detail=row.get("detail") or "",
                    extra={"repo": row.get("repo", "")},
                )
            )

    # py/torch suite cases that failed (caught) — location in detail + id
    for name in ("py_torch_cases.tsv",):
        p = root / name
        for row in read_tsv(p):
            caught = row.get("caught", "")
            got = row.get("got_exit", "")
            if caught != "1" and got not in ("4", "2"):
                continue
            findings.append(
                Finding(
                    source=name,
                    kind=row.get("category") or "python/torch",
                    path=row.get("id") or "",
                    line="",
                    attr="",
                    category="suite_case",
                    severity="error" if got == "4" else "info",
                    detail=(row.get("detail") or row.get("agent_claim") or "")[:300],
                    extra={
                        "claim": row.get("agent_claim", ""),
                        "expect_exit": row.get("expect_exit", ""),
                        "got_exit": got,
                    },
                )
            )

    return findings


def load_hallucination_cases(root: Path | None = None) -> list[dict[str, str]]:
    root = root or results_dir()
    return read_tsv(root / "hallucination_cases.tsv")


def load_real_model_cases(root: Path | None = None) -> list[dict[str, str]]:
    root = root or results_dir()
    rows = read_tsv(root / "real_model_cases.tsv")
    if rows:
        return rows
    # fixtures from benchmarks if results empty
    fixtures_dir = Path(__file__).resolve().parent.parent / "benchmarks" / "real_model" / "fixtures"
    # fixtures are JSON — UI can still list summary from tasks.json
    return rows


def load_py_torch_cases(root: Path | None = None) -> list[dict[str, str]]:
    root = root or results_dir()
    return read_tsv(root / "py_torch_cases.tsv")


@dataclass
class BenchmarkInfo:
    id: str
    name: str
    description: str
    script: str
    result_files: list[str]
    available: bool
    summary: dict[str, str] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "script": self.script,
            "result_files": self.result_files,
            "available": self.available,
            "summary": self.summary,
        }


BENCHMARKS: list[dict[str, Any]] = [
    {
        "id": "hallucination_suite",
        "name": "Synthetic hallucination suite",
        "description": "Labeled agent claims (import/compile/symbols/assert/run) vs ghar exit codes.",
        "script": "benchmarks/run_hallucination_suite.sh",
        "result_files": ["hallucination_cases.tsv", "hallucination_summary.tsv"],
        "summary_file": "hallucination_summary.tsv",
    },
    {
        "id": "py_torch_suite",
        "name": "Python / PyTorch validators suite",
        "description": "AST/import/exec + torch attr/forward + matmul uplift; catches invented APIs.",
        "script": "benchmarks/run_py_torch_suite.sh",
        "result_files": ["py_torch_cases.tsv", "py_torch_summary.tsv"],
        "summary_file": "py_torch_summary.tsv",
    },
    {
        "id": "real_model_eval",
        "name": "Local coding-model eval",
        "description": "Live or fixture Ollama coding model claims → ghar gate; per-trial catch locations.",
        "script": "benchmarks/run_real_model_eval.sh",
        "result_files": ["real_model_cases.tsv", "real_model_summary.tsv", "REAL_MODEL_EVAL.md"],
        "summary_file": "real_model_summary.tsv",
    },
    {
        "id": "perf_uplift",
        "name": "Perf uplift (matmul)",
        "description": "Naive vs optimized matmul: measured speedup + assert floor.",
        "script": "benchmarks/run_perf_uplift.sh",
        "result_files": ["perf_uplift.tsv"],
        "summary_file": "perf_uplift.tsv",
    },
    {
        "id": "code_scan",
        "name": "Code scan findings",
        "description": "Static syntax + torch.* attribute resolve on scanned trees (local results).",
        "script": "oracles/repo_scan.py",
        "result_files": ["openmmlab_findings.tsv", "code_findings.tsv", "openmmlab_summary.tsv"],
        "summary_file": "openmmlab_summary.tsv",
    },
]


def list_benchmarks(root: Path | None = None) -> list[BenchmarkInfo]:
    root = root or results_dir()
    out: list[BenchmarkInfo] = []
    for b in BENCHMARKS:
        files = b["result_files"]
        available = any((root / f).is_file() for f in files)
        summary: dict[str, str] = {}
        sf = b.get("summary_file")
        if sf and (root / sf).is_file():
            summary = read_kv_tsv(root / sf)
            # for multi-column summaries keep first rows raw
            if not summary:
                rows = read_tsv(root / sf)
                if rows:
                    summary = {f"row0.{k}": v for k, v in list(rows[0].items())[:8]}
        out.append(
            BenchmarkInfo(
                id=b["id"],
                name=b["name"],
                description=b["description"],
                script=b["script"],
                result_files=files,
                available=available,
                summary=summary,
            )
        )
    return out


def dashboard_stats(root: Path | None = None) -> dict[str, Any]:
    root = root or results_dir()
    findings = load_code_findings(root)
    hallu = load_hallucination_cases(root)
    real = load_real_model_cases(root)
    py = load_py_torch_cases(root)
    benches = list_benchmarks(root)
    return {
        "results_dir": str(root),
        "findings_count": len(findings),
        "hallucination_cases": len(hallu),
        "real_model_cases": len(real),
        "py_torch_cases": len(py),
        "benchmarks_available": sum(1 for b in benches if b.available),
        "benchmarks_total": len(benches),
        "hallu_summary": read_kv_tsv(root / "hallucination_summary.tsv"),
        "py_torch_summary": read_kv_tsv(root / "py_torch_summary.tsv"),
        "real_model_summary": read_kv_tsv(root / "real_model_summary.tsv"),
    }

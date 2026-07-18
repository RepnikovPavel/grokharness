#!/usr/bin/env python3
"""Load ghar result artifacts (TSV) for the local results UI.

Only reads on-disk files produced by ghar suites / scanners.
Never invents findings. Empty / missing files → empty lists.
"""
from __future__ import annotations

import csv
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def results_dir() -> Path:
    """Prefer GHAR_RESULTS; else ./results if it has data; else ui/sample_results."""
    env = os.environ.get("GHAR_RESULTS") or os.environ.get("RESULTS_DIR")
    if env:
        return Path(env).resolve()
    cand = _repo_root() / "results"
    # use results/ only if it has at least one expected artifact
    markers = (
        "code_findings.tsv",
        "openmmlab_findings.tsv",
        "hallucination_cases.tsv",
        "real_model_cases.tsv",
        "py_torch_cases.tsv",
    )
    if cand.is_dir() and any((cand / m).is_file() for m in markers):
        return cand.resolve()
    return (Path(__file__).resolve().parent / "sample_results").resolve()


def read_tsv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = [ln for ln in text.splitlines() if ln.strip() and not ln.lstrip().startswith("#")]
    if not lines:
        return []
    reader = csv.DictReader(lines, delimiter="\t")
    rows: list[dict[str, str]] = []
    for row in reader:
        if not row:
            continue
        rows.append({(k or "").strip(): (v if v is not None else "") for k, v in row.items()})
    return rows


def read_kv_tsv(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for row in read_tsv(path):
        if "metric" in row and "value" in row:
            out[row["metric"]] = row["value"]
        else:
            keys = [k for k in row.keys() if k]
            if len(keys) >= 2:
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
    repo: str = ""

    @property
    def actionable(self) -> bool:
        """True for findings a developer should look at first (not vendor stubs)."""
        cat = (self.category or "").lower()
        sev = (self.severity or "").lower()
        if cat in {"api_missing", "syntax_error", "suite_case"}:
            return True
        if sev == "error" and cat not in {"accel_backend", "build_flavor"}:
            return True
        if self.kind == "syntax_error":
            return True
        return False

    def location(self) -> str:
        if self.line and self.line not in ("0", ""):
            return f"{self.path}:{self.line}"
        return self.path or "—"

    def to_dict(self) -> dict[str, Any]:
        return {
            "source": self.source,
            "kind": self.kind,
            "path": self.path,
            "line": self.line,
            "attr": self.attr,
            "category": self.category,
            "severity": self.severity,
            "detail": self.detail,
            "repo": self.repo,
            "actionable": self.actionable,
            "location": self.location(),
        }


def _first_existing(root: Path, names: tuple[str, ...]) -> Path | None:
    for n in names:
        p = root / n
        if p.is_file():
            return p
    return None


def load_code_findings(root: Path | None = None) -> list[Finding]:
    root = root or results_dir()
    findings: list[Finding] = []

    scan = _first_existing(
        root, ("code_findings.tsv", "openmmlab_findings.tsv", "repo_findings.tsv")
    )
    if scan:
        for row in read_tsv(scan):
            findings.append(
                Finding(
                    source="code_scan",
                    kind=row.get("kind") or "finding",
                    path=row.get("path") or row.get("file") or "",
                    line=str(row.get("line") or ""),
                    attr=row.get("attr") or "",
                    category=row.get("category") or "",
                    severity=row.get("severity") or "",
                    detail=row.get("detail") or "",
                    repo=row.get("repo") or "",
                )
            )

    # Failed suite cases (caught) from py/torch
    for name in ("py_torch_cases.tsv",):
        for row in read_tsv(root / name):
            caught = row.get("caught", "")
            got = row.get("got_exit", "")
            if caught != "1" and got not in ("4", "2"):
                continue
            findings.append(
                Finding(
                    source="py_torch_suite",
                    kind=row.get("category") or "suite",
                    path=row.get("id") or "",
                    category="suite_case",
                    severity="error",
                    detail=(row.get("detail") or row.get("agent_claim") or "")[:400],
                    repo=row.get("agent_claim") or "",
                )
            )
    return findings


def load_hallucination_cases(root: Path | None = None) -> list[dict[str, str]]:
    return read_tsv((root or results_dir()) / "hallucination_cases.tsv")


def load_real_model_cases(root: Path | None = None) -> list[dict[str, str]]:
    return read_tsv((root or results_dir()) / "real_model_cases.tsv")


def load_py_torch_cases(root: Path | None = None) -> list[dict[str, str]]:
    return read_tsv((root or results_dir()) / "py_torch_cases.tsv")


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


_BENCHMARK_DEFS: list[dict[str, Any]] = [
    {
        "id": "hallucination_suite",
        "name": "Synthetic claims suite",
        "description": "Labeled false/true agent claims (import, compile, symbols, assert, run).",
        "script": "benchmarks/run_hallucination_suite.sh",
        "result_files": ["hallucination_cases.tsv", "hallucination_summary.tsv"],
        "summary_file": "hallucination_summary.tsv",
    },
    {
        "id": "py_torch_suite",
        "name": "Python / PyTorch validators",
        "description": "AST, imports, exec, torch attribute resolve, forward/op run.",
        "script": "benchmarks/run_py_torch_suite.sh",
        "result_files": ["py_torch_cases.tsv", "py_torch_summary.tsv"],
        "summary_file": "py_torch_summary.tsv",
    },
    {
        "id": "real_model_eval",
        "name": "Local coding-model eval",
        "description": "Claims from a local coding model (Ollama) checked by ghar gate.",
        "script": "benchmarks/run_real_model_eval.sh",
        "result_files": ["real_model_cases.tsv", "real_model_summary.tsv"],
        "summary_file": "real_model_summary.tsv",
    },
    {
        "id": "perf_uplift",
        "name": "Perf uplift (matmul)",
        "description": "Measured speedup naive vs optimized; assert floor.",
        "script": "benchmarks/run_perf_uplift.sh",
        "result_files": ["perf_uplift.tsv"],
        "summary_file": "perf_uplift.tsv",
    },
    {
        "id": "code_scan",
        "name": "Static code scan",
        "description": "Syntax + torch.* attribute resolve on scanned trees → findings TSV.",
        "script": "oracles/repo_scan.py",
        "result_files": ["code_findings.tsv", "openmmlab_findings.tsv"],
        "summary_file": "openmmlab_summary.tsv",
    },
]


def list_benchmarks(root: Path | None = None) -> list[BenchmarkInfo]:
    root = root or results_dir()
    out: list[BenchmarkInfo] = []
    for b in _BENCHMARK_DEFS:
        files = list(b["result_files"])
        available = any((root / f).is_file() for f in files)
        summary: dict[str, str] = {}
        sf = b.get("summary_file")
        if sf and (root / sf).is_file():
            summary = read_kv_tsv(root / sf)
            if not summary:
                rows = read_tsv(root / sf)
                if rows:
                    summary = {f"col.{k}": v for k, v in list(rows[0].items())[:6]}
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
    actionable = [f for f in findings if f.actionable]
    real = load_real_model_cases(root)
    caught = [r for r in real if str(r.get("caught_by_ghar") or r.get("caught") or "") == "1"]
    hallu = load_hallucination_cases(root)
    py = load_py_torch_cases(root)
    benches = list_benchmarks(root)
    return {
        "results_dir": str(root),
        "findings_total": len(findings),
        "findings_actionable": len(actionable),
        "real_model_cases": len(real),
        "real_model_caught": len(caught),
        "hallucination_cases": len(hallu),
        "py_torch_cases": len(py),
        "benchmarks_available": sum(1 for b in benches if b.available),
        "benchmarks_total": len(benches),
        "hallu_summary": read_kv_tsv(root / "hallucination_summary.tsv"),
        "py_torch_summary": read_kv_tsv(root / "py_torch_summary.tsv"),
        "real_model_summary": read_kv_tsv(root / "real_model_summary.tsv"),
        "top_actionable": [f.to_dict() for f in actionable[:8]],
        "top_caught": caught[:8],
    }

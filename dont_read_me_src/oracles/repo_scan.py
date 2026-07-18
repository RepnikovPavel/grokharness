#!/usr/bin/env python3
"""
Bulk static scan of a Python/PyTorch codebase (no LLM judge).

Checks per .py file:
  1) AST syntax parse
  2) torch.* / alias attribute existence against the *installed* torch
     (same resolver as py_torch_validate — full path, no parent fallback)

Does NOT exec project modules (open-mmlab packages need their own deps).
Does NOT treat missing third-party imports as bugs.

Usage:
  python3 oracles/repo_scan.py --root PATH --repo NAME [--globs pkg1,pkg2] \\
      --findings OUT.tsv --summary OUT_summary.tsv

Exit: 0 always after writing report (scan tool); non-zero only on usage/IO.
Findings with severity=error are real static failures under installed torch.
"""
from __future__ import annotations

import argparse
import ast
import importlib
import os
import sys
import time
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable

# Reuse attribute logic from sibling oracle
_ORACLE_DIR = Path(__file__).resolve().parent
if str(_ORACLE_DIR) not in sys.path:
    sys.path.insert(0, str(_ORACLE_DIR))

from py_torch_validate import (  # noqa: E402
    _collect_torch_attr_paths,
    _resolve_attr,
)


SKIP_DIR_NAMES = {
    ".git",
    ".github",
    "__pycache__",
    ".pytest_cache",
    ".mypy_cache",
    "build",
    "dist",
    "node_modules",
    ".tox",
    "venv",
    ".venv",
    "third_party",
    "3rdparty",
    "docs",  # often has snippet junk
    "docker",
}

# Vendor / optional accelerator namespaces not present on stock CUDA torch wheels.
# Counted as findings but labeled so README is honest about "not a logic bug".
ACCEL_PREFIXES = (
    "torch.npu",
    "torch.mlu",
    "torch.musa",
    "torch.xpu",
    "torch.meta",
)


def classify_missing_attr(attr: str) -> str:
    """Return subcategory for a missing torch attr."""
    a = attr or ""
    # torch.npu / torch.npu_foo / torch.mlu.is_available / torch.is_mlu_available
    for p in ACCEL_PREFIXES:
        if a == p or a.startswith(p + ".") or a.startswith(p + "_"):
            return "accel_backend"
    low = a.lower()
    if any(x in low for x in ("_npu", "npu_", "_mlu", "mlu_", "_musa", "musa_", "is_mlu", "is_npu", "is_musa")):
        return "accel_backend"
    if a.startswith("torch.version.") or a in {"torch.version.hip", "torch.version.cuda"}:
        return "build_flavor"
    if "ROCM" in a or "HIP" in a or a.startswith("torch.ops."):
        return "build_flavor"
    return "api_missing"


@dataclass
class Finding:
    repo: str
    path: str
    kind: str
    severity: str
    detail: str
    line: int = 0
    attr: str = ""
    category: str = ""  # accel_backend | api_missing | build_flavor | syntax | ...


@dataclass
class FileStats:
    files: int = 0
    syntax_ok: int = 0
    syntax_fail: int = 0
    torch_files: int = 0
    attr_refs: int = 0
    missing_attrs: int = 0
    findings: list[Finding] = field(default_factory=list)


def iter_py_files(root: Path, globs: list[str] | None, max_files: int) -> Iterable[Path]:
    roots: list[Path]
    if globs:
        roots = []
        for g in globs:
            p = root / g
            if p.is_dir():
                roots.append(p)
            elif p.is_file() and p.suffix == ".py":
                yield p
        if not roots and not globs:
            roots = [root]
        if not roots:
            # fall back to whole tree if package dir missing
            roots = [root]
    else:
        roots = [root]

    n = 0
    for base in roots:
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES and not d.startswith(".")]
            for fn in filenames:
                if not fn.endswith(".py"):
                    continue
                # skip tests optionally? keep them — bugs there still matter
                yield Path(dirpath) / fn
                n += 1
                if max_files > 0 and n >= max_files:
                    return


def scan_file(repo: str, path: Path, root: Path, check_torch: bool) -> tuple[list[Finding], dict[str, int]]:
    rel = str(path.relative_to(root)) if path.is_relative_to(root) else str(path)
    findings: list[Finding] = []
    stats = {"syntax_ok": 0, "syntax_fail": 0, "torch_file": 0, "attr_refs": 0, "missing": 0}

    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        findings.append(
            Finding(repo, rel, "io_error", "error", f"read failed: {e}")
        )
        stats["syntax_fail"] = 1
        return findings, stats

    try:
        tree = ast.parse(text, filename=str(path))
    except SyntaxError as e:
        findings.append(
            Finding(
                repo,
                rel,
                "syntax_error",
                "error",
                f"{e.msg}",
                line=int(e.lineno or 0),
            )
        )
        stats["syntax_fail"] = 1
        return findings, stats

    stats["syntax_ok"] = 1

    if not check_torch:
        return findings, stats

    # only torch-related files
    src_lower = text[:4000]
    if "torch" not in text and "import torch" not in src_lower:
        # cheap filter
        has_torch = False
        for node in ast.walk(tree):
            if isinstance(node, (ast.Import, ast.ImportFrom)):
                if isinstance(node, ast.Import):
                    if any(a.name == "torch" or a.name.startswith("torch.") for a in node.names):
                        has_torch = True
                        break
                else:
                    if node.module and (node.module == "torch" or node.module.startswith("torch.")):
                        has_torch = True
                        break
        if not has_torch:
            return findings, stats

    stats["torch_file"] = 1
    try:
        paths = _collect_torch_attr_paths(tree)
    except Exception as e:
        findings.append(
            Finding(repo, rel, "scan_error", "warn", f"attr collect failed: {e}")
        )
        return findings, stats

    stats["attr_refs"] = len(paths)
    missing: list[str] = []
    for a in sorted(set(paths)):
        parts = a.split(".")
        if len(parts) < 2:
            continue
        # skip private/dunder dynamism noise somewhat
        if any(p.startswith("_") and p != "__version__" for p in parts[1:]):
            continue
        try:
            _resolve_attr(a)
        except Exception:
            missing.append(a)

    stats["missing"] = len(missing)
    for a in missing[:50]:  # cap per file
        cat = classify_missing_attr(a)
        sev = "info" if cat == "accel_backend" else "error"
        findings.append(
            Finding(
                repo,
                rel,
                "missing_attr",
                sev,
                f"torch attr not found in installed torch: {a}",
                attr=a,
                category=cat,
            )
        )
    return findings, stats


def write_tsv(path: Path, rows: list[dict[str, Any]], cols: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write("\t".join(cols) + "\n")
        for r in rows:
            f.write("\t".join(str(r.get(c, "")).replace("\t", " ").replace("\n", " ") for c in cols) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(prog="repo_scan")
    ap.add_argument("--root", required=True, help="repo checkout path")
    ap.add_argument("--repo", required=True, help="logical repo name for reports")
    ap.add_argument("--globs", default="", help="comma-separated package dirs under root")
    ap.add_argument("--max-files", type=int, default=0, help="0 = no limit")
    ap.add_argument("--no-torch", action="store_true")
    ap.add_argument("--findings", required=True, help="append/write findings TSV")
    ap.add_argument("--summary", required=True, help="write per-repo summary TSV row file")
    ap.add_argument("--append-findings", action="store_true")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if not root.is_dir():
        print(f"root not a directory: {root}", file=sys.stderr)
        return 1

    globs = [g.strip() for g in args.globs.split(",") if g.strip()]
    check_torch = not args.no_torch
    torch_ver = ""
    if check_torch:
        try:
            import torch

            torch_ver = torch.__version__
        except ImportError:
            print("torch not installed; use --no-torch", file=sys.stderr)
            return 2

    t0 = time.perf_counter()
    all_findings: list[Finding] = []
    n_files = 0
    n_syn_ok = 0
    n_syn_fail = 0
    n_torch = 0
    n_attr_refs = 0
    n_missing = 0

    for py in iter_py_files(root, globs or None, args.max_files):
        n_files += 1
        findings, st = scan_file(args.repo, py, root, check_torch)
        n_syn_ok += st["syntax_ok"]
        n_syn_fail += st["syntax_fail"]
        n_torch += st["torch_file"]
        n_attr_refs += st["attr_refs"]
        n_missing += st["missing"]
        all_findings.extend(findings)

    wall = time.perf_counter() - t0

    # findings TSV
    fcols = ["repo", "path", "kind", "category", "severity", "line", "attr", "detail"]
    frows = [
        {
            "repo": f.repo,
            "path": f.path,
            "kind": f.kind,
            "category": f.category or f.kind,
            "severity": f.severity,
            "line": f.line,
            "attr": f.attr,
            "detail": f.detail,
        }
        for f in all_findings
    ]
    findings_path = Path(args.findings)
    if args.append_findings and findings_path.is_file():
        with findings_path.open("a", encoding="utf-8") as f:
            for r in frows:
                f.write("\t".join(str(r.get(c, "")).replace("\t", " ").replace("\n", " ") for c in fcols) + "\n")
    else:
        write_tsv(findings_path, frows, fcols)

    kind_counts = Counter(f.kind for f in all_findings)
    cat_counts = Counter(f.category or f.kind for f in all_findings)
    summary = {
        "repo": args.repo,
        "root": str(root),
        "torch_version": torch_ver,
        "files_scanned": n_files,
        "syntax_ok": n_syn_ok,
        "syntax_fail": n_syn_fail,
        "torch_files": n_torch,
        "torch_attr_refs": n_attr_refs,
        "missing_attr_findings": n_missing,
        "findings_total": len(all_findings),
        "syntax_error_n": kind_counts.get("syntax_error", 0),
        "missing_attr_n": kind_counts.get("missing_attr", 0),
        "api_missing_n": cat_counts.get("api_missing", 0),
        "accel_backend_n": cat_counts.get("accel_backend", 0),
        "build_flavor_n": cat_counts.get("build_flavor", 0),
        "wall_s": f"{wall:.3f}",
    }
    scols = list(summary.keys())
    spath = Path(args.summary)
    if spath.is_file():
        # append row
        with spath.open("a", encoding="utf-8") as f:
            f.write("\t".join(str(summary[c]) for c in scols) + "\n")
    else:
        write_tsv(spath, [summary], scols)

    # human line
    print(
        f"repo={args.repo} files={n_files} syntax_fail={n_syn_fail} "
        f"torch_files={n_torch} missing_attr={n_missing} findings={len(all_findings)} "
        f"wall_s={wall:.2f} torch={torch_ver}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

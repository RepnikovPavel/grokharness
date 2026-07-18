#!/usr/bin/env python3
"""
ghar Python / PyTorch oracles — pure program checks (no LLM judge).

Invoked by the ghar CLI:
  python3 oracles/py_torch_validate.py python --file PATH [--exec] [--timeout N]
  python3 oracles/py_torch_validate.py torch  --file PATH [--forward] [--device cpu|cuda] [--timeout N]
  python3 oracles/py_torch_validate.py torch-attr ATTR [ATTR...]

Stdout: one TSV line: status\\tkey=val\\tkey=val...
Exit: 0 ok | 1 usage | 2 missing tool | 4 fail
"""
from __future__ import annotations

import argparse
import ast
import importlib
import io
import os
import re
import sys
import traceback
import types
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
from typing import Any


EXIT_OK = 0
EXIT_USAGE = 1
EXIT_TOOL = 2
EXIT_FAIL = 4


def emit(status: str, metrics: dict[str, Any], detail: str = "") -> None:
    parts = [f"status={status}"]
    if detail:
        parts.append("detail=" + detail.replace("\t", " ").replace("\n", " | "))
    for k, v in metrics.items():
        parts.append(f"{k}={v}")
    # machine line
    print("\t".join(parts))


def die(status: str, metrics: dict[str, Any], detail: str, code: int) -> None:
    emit(status, metrics, detail)
    sys.exit(code)


def parse_file(path: Path) -> tuple[str, ast.AST | None]:
    text = path.read_text(encoding="utf-8", errors="replace")
    try:
        tree = ast.parse(text, filename=str(path))
        return text, tree
    except SyntaxError as e:
        return text, None  # caller handles


def cmd_python(args: argparse.Namespace) -> int:
    path = Path(args.file)
    m: dict[str, Any] = {"path": str(path), "bytes": path.stat().st_size if path.is_file() else 0}
    if not path.is_file():
        die("fail", m, f"file not found: {path}", EXIT_FAIL)

    text, tree = parse_file(path)
    if tree is None:
        # re-raise parse for message
        try:
            ast.parse(text, filename=str(path))
        except SyntaxError as e:
            m["failure_class"] = "syntax_error"
            m["lineno"] = e.lineno or 0
            die("fail", m, f"syntax_error line {e.lineno}: {e.msg}", EXIT_FAIL)

    m["syntax_ok"] = 1
    m["ast_nodes"] = sum(1 for _ in ast.walk(tree))

    # collect top-level imports for optional check
    imports: list[str] = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for a in node.names:
                imports.append(a.name.split(".")[0])
        elif isinstance(node, ast.ImportFrom) and node.module:
            imports.append(node.module.split(".")[0])
    m["import_count"] = len(imports)

    if args.check_imports:
        missing = []
        for mod in sorted(set(imports)):
            if mod in {"__future__"}:
                continue
            try:
                importlib.import_module(mod)
            except Exception:
                missing.append(mod)
        m["imports_missing"] = ",".join(missing) if missing else ""
        if missing:
            m["failure_class"] = "import_missing"
            die("fail", m, f"import_missing: {','.join(missing)}", EXIT_FAIL)
        m["imports_ok"] = 1

    if args.exec_code:
        # timed exec in isolated globals
        import signal

        timeout = max(1, int(args.timeout))
        glb: dict[str, Any] = {"__name__": "__ghar_exec__", "__file__": str(path)}
        buf_out, buf_err = io.StringIO(), io.StringIO()

        def _handler(signum, frame):
            raise TimeoutError(f"exec timeout {timeout}s")

        old = signal.signal(signal.SIGALRM, _handler)
        signal.alarm(timeout)
        try:
            with redirect_stdout(buf_out), redirect_stderr(buf_err):
                exec(compile(tree, str(path), "exec"), glb, glb)
            signal.alarm(0)
        except TimeoutError as e:
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old)
            m["failure_class"] = "exec_timeout"
            die("fail", m, str(e), EXIT_FAIL)
        except Exception as e:
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old)
            m["failure_class"] = "exec_error"
            m["exc_type"] = type(e).__name__
            die("fail", m, f"exec_error: {type(e).__name__}: {e}", EXIT_FAIL)
        finally:
            signal.signal(signal.SIGALRM, old)

        m["exec_ok"] = 1
        m["stdout_bytes"] = len(buf_out.getvalue())
        m["stderr_bytes"] = len(buf_err.getvalue())

    emit("ok", m, "python validate ok")
    return EXIT_OK


def _resolve_attr(path: str) -> tuple[Any, str]:
    """Return (object, full_path) or raise AttributeError/ImportError.

    Resolves every segment of the path — no parent-module fallback.
    """
    parts = path.split(".")
    if not parts or not parts[0]:
        raise ImportError(f"empty attr path: {path}")
    # Prefer import_module for package paths (torch.nn.functional), then getattr
    # for the remainder so torch.nn.Linear works (Linear is not a submodule).
    obj: Any = None
    built = ""
    for i, p in enumerate(parts):
        cand = ".".join(parts[: i + 1])
        try:
            obj = importlib.import_module(cand)
            built = cand
            continue
        except ImportError:
            if obj is None:
                # first segment must be importable
                if i == 0:
                    raise ImportError(f"cannot import: {p}") from None
                raise
            if not hasattr(obj, p):
                raise AttributeError(f"missing_attr: {built}.{p}") from None
            obj = getattr(obj, p)
            built = f"{built}.{p}"
    assert obj is not None
    return obj, built


def _collect_import_aliases(tree: ast.AST) -> dict[str, str]:
    """Map local name → fully-qualified module/attr path for torch-related imports.

    Examples:
      import torch                         → {"torch": "torch"}
      import torch.nn as nn                → {"nn": "torch.nn"}
      import torch.nn.functional as F      → {"F": "torch.nn.functional"}
      from torch import nn                 → {"nn": "torch.nn"}
      from torch.nn import functional as F → {"F": "torch.nn.functional"}
      from torch.nn.functional import relu → {"relu": "torch.nn.functional.relu"}
    """
    aliases: dict[str, str] = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for a in node.names:
                full = a.name
                local = a.asname or full.split(".")[0]
                # only track torch* trees (and their local roots)
                if full == "torch" or full.startswith("torch."):
                    if a.asname:
                        aliases[a.asname] = full
                    else:
                        # import torch.nn → name "torch" still binds top package
                        # but attribute chains start from torch; record root
                        aliases[full.split(".")[0]] = full.split(".")[0]
                        # also record multi-segment if used as torch.nn without alias?
                        # Standard: import torch.nn binds name "torch" only.
                        aliases["torch"] = "torch"
        elif isinstance(node, ast.ImportFrom) and node.module:
            mod = node.module
            if not (mod == "torch" or mod.startswith("torch.")):
                continue
            for a in node.names:
                if a.name == "*":
                    continue
                full = f"{mod}.{a.name}"
                local = a.asname or a.name
                aliases[local] = full
    # always know bare torch if present in any form
    if any(v == "torch" or v.startswith("torch.") for v in aliases.values()):
        aliases.setdefault("torch", "torch")
    return aliases


def _attr_chain_from_node(node: ast.Attribute) -> tuple[str | None, list[str]]:
    """Return (root_name, [attr, attr, ...]) for root.attr.attr chains."""
    chain: list[str] = []
    cur: ast.AST = node
    while isinstance(cur, ast.Attribute):
        chain.append(cur.attr)
        cur = cur.value
    if isinstance(cur, ast.Name):
        chain.reverse()
        return cur.id, chain
    return None, []


def _collect_torch_attr_paths(tree: ast.AST) -> list[str]:
    """Collect fully-qualified torch.* paths referenced in the AST.

    Resolves import aliases (nn, F, functional, …) so
    `import torch.nn.functional as F; F.fake` → torch.nn.functional.fake.
    """
    aliases = _collect_import_aliases(tree)
    # also: if file does `import torch` only, aliases has torch→torch
    # Detect bare `import torch` even without asname
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for a in node.names:
                if a.name == "torch" or a.name.startswith("torch."):
                    aliases.setdefault("torch", "torch")
        if isinstance(node, ast.ImportFrom) and node.module and (
            node.module == "torch" or node.module.startswith("torch.")
        ):
            aliases.setdefault("torch", "torch")

    paths: list[str] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Attribute):
            continue
        root, chain = _attr_chain_from_node(node)
        if root is None or not chain:
            continue
        if root in aliases:
            base = aliases[root]
            paths.append(".".join([base] + chain))
        elif root == "torch":
            paths.append(".".join(["torch"] + chain))
    # from-import leaves used as bare names: `from torch.nn.functional import relu; relu(x)`
    used_names: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load):
            used_names.add(node.id)
    for local, full in aliases.items():
        if local in ("torch",):
            continue
        if local in used_names and full.startswith("torch."):
            paths.append(full)

    # de-dupe preserve order
    seen: set[str] = set()
    out: list[str] = []
    for p in paths:
        if p not in seen:
            seen.add(p)
            out.append(p)
    return out


def cmd_torch_attr(args: argparse.Namespace) -> int:
    m: dict[str, Any] = {"n": len(args.attrs)}
    try:
        import torch  # noqa: F401
    except ImportError:
        die("error", m, "torch not installed", EXIT_TOOL)

    missing = []
    found = []
    for a in args.attrs:
        try:
            _, full = _resolve_attr(a)
            found.append(full)
        except Exception as e:
            missing.append(f"{a}:{e}")
    m["found"] = len(found)
    m["missing"] = len(missing)
    m["found_list"] = ",".join(found)
    m["missing_list"] = ",".join(missing)
    if missing:
        m["failure_class"] = "missing_attr"
        die("fail", m, f"missing_attr: {missing[0]}", EXIT_FAIL)
    emit("ok", m, "torch attrs ok")
    return EXIT_OK


def cmd_torch(args: argparse.Namespace) -> int:
    path = Path(args.file)
    m: dict[str, Any] = {"path": str(path)}
    if not path.is_file():
        die("fail", m, f"file not found: {path}", EXIT_FAIL)

    try:
        import torch
    except ImportError:
        die("error", m, "torch not installed", EXIT_TOOL)

    m["torch_version"] = torch.__version__
    m["cuda_available"] = int(torch.cuda.is_available())
    device = args.device
    if device == "cuda" and not torch.cuda.is_available():
        device = "cpu"
        m["device_fallback"] = "cpu"
    m["device"] = device

    text, tree = parse_file(path)
    if tree is None:
        try:
            ast.parse(text, filename=str(path))
        except SyntaxError as e:
            m["failure_class"] = "syntax_error"
            die("fail", m, f"syntax_error: {e.msg}", EXIT_FAIL)

    m["syntax_ok"] = 1

    # Scan torch.* paths including import aliases (nn, F, functional, …).
    # --strict-attrs resolves the FULL path; no parent-module fallback
    # (that previously accepted torch.nn.functional.<hallucinated>).
    torch_attrs = _collect_torch_attr_paths(tree)
    m["torch_attr_refs"] = len(torch_attrs)
    m["torch_attr_sample"] = ",".join(sorted(set(torch_attrs))[:12])

    if args.strict_attrs and torch_attrs:
        missing: list[str] = []
        for a in sorted(set(torch_attrs)):
            parts = a.split(".")
            if len(parts) < 2:
                continue
            try:
                _resolve_attr(a)
            except Exception:
                missing.append(str(a))
        if missing:
            m["failure_class"] = "missing_attr"
            m["missing_list"] = ",".join(missing[:10])
            die("fail", m, f"missing_attr: {missing[0]}", EXIT_FAIL)
        m["attrs_ok"] = 1
    elif args.strict_attrs and not torch_attrs:
        # No torch attr refs found — still ok statically; forward may fail later
        m["attrs_ok"] = 1
        m["torch_attr_refs"] = 0

    # Optional named attrs from CLI
    if args.require_attr:
        for a in args.require_attr:
            try:
                _resolve_attr(a)
            except Exception:
                m["failure_class"] = "missing_attr"
                die("fail", m, f"missing_attr: {a}", EXIT_FAIL)
        m["require_attr_ok"] = 1

    if not args.forward:
        emit("ok", m, "torch static ok")
        return EXIT_OK

    # Execute module and run forward/op
    import signal
    import time

    timeout = max(1, int(args.timeout))
    glb: dict[str, Any] = {
        "__name__": "__ghar_torch__",
        "__file__": str(path),
        "torch": torch,
    }
    # common aliases models expect
    try:
        import torch.nn as nn

        glb["nn"] = nn
    except Exception:
        pass

    def _handler(signum, frame):
        raise TimeoutError(f"torch forward timeout {timeout}s")

    old = signal.signal(signal.SIGALRM, _handler)
    signal.alarm(timeout)
    t0 = time.perf_counter()
    try:
        exec(compile(tree, str(path), "exec"), glb, glb)
        # Find a Module instance or build_fn / forward / model
        model = None
        if "model" in glb and isinstance(glb["model"], torch.nn.Module):
            model = glb["model"]
        elif "Model" in glb and isinstance(glb["Model"], type) and issubclass(glb["Model"], torch.nn.Module):
            model = glb["Model"]()
        elif "build_model" in glb and callable(glb["build_model"]):
            model = glb["build_model"]()

        if model is None:
            # try run_op() or main tensor op
            if "run_op" in glb and callable(glb["run_op"]):
                out = glb["run_op"]()
            elif "main" in glb and callable(glb["main"]):
                out = glb["main"]()
            else:
                signal.alarm(0)
                m["failure_class"] = "no_entry"
                die(
                    "fail",
                    m,
                    "no_entry: define model/Model/build_model/run_op/main",
                    EXIT_FAIL,
                )
        else:
            model = model.to(device)
            model.eval()
            # infer input
            if "example_input" in glb:
                x = glb["example_input"]
                if isinstance(x, torch.Tensor):
                    x = x.to(device)
            else:
                x = torch.randn(2, 3, 8, 8, device=device)
            with torch.no_grad():
                out = model(x)

        signal.alarm(0)
        ms = (time.perf_counter() - t0) * 1000.0
        m["forward_ok"] = 1
        m["wall_ms"] = f"{ms:.4f}"
        if isinstance(out, torch.Tensor):
            m["out_shape"] = "x".join(str(d) for d in out.shape)
            m["out_dtype"] = str(out.dtype).replace("torch.", "")
            m["out_device"] = str(out.device)
            m["out_finite"] = int(bool(torch.isfinite(out).all().item()))
            if not m["out_finite"]:
                m["failure_class"] = "non_finite"
                die("fail", m, "non_finite output", EXIT_FAIL)
        m["run_ok"] = 1
    except TimeoutError as e:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old)
        m["failure_class"] = "exec_timeout"
        die("fail", m, str(e), EXIT_FAIL)
    except Exception as e:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, old)
        m["failure_class"] = "forward_error"
        m["exc_type"] = type(e).__name__
        die("fail", m, f"forward_error: {type(e).__name__}: {e}", EXIT_FAIL)
    finally:
        signal.signal(signal.SIGALRM, old)

    emit("ok", m, "torch validate ok")
    return EXIT_OK


def main() -> int:
    ap = argparse.ArgumentParser(prog="py_torch_validate")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_py = sub.add_parser("python")
    p_py.add_argument("--file", required=True)
    p_py.add_argument("--exec", dest="exec_code", action="store_true")
    p_py.add_argument("--check-imports", action="store_true")
    p_py.add_argument("--timeout", type=int, default=30)

    p_t = sub.add_parser("torch")
    p_t.add_argument("--file", required=True)
    p_t.add_argument("--forward", action="store_true")
    p_t.add_argument("--strict-attrs", action="store_true")
    p_t.add_argument("--require-attr", action="append", default=[])
    p_t.add_argument("--device", default="cpu", choices=["cpu", "cuda"])
    p_t.add_argument("--timeout", type=int, default=60)

    p_a = sub.add_parser("torch-attr")
    p_a.add_argument("attrs", nargs="+")

    args = ap.parse_args()
    if args.cmd == "python":
        return cmd_python(args)
    if args.cmd == "torch":
        return cmd_torch(args)
    if args.cmd == "torch-attr":
        return cmd_torch_attr(args)
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main())

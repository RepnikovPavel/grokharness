#!/usr/bin/env python3
"""
ghar results UI — local dashboard for oracle findings and hallu benchmarks.

  export GHAR_RESULTS=./results   # optional; falls back to ui/sample_results
  python3 ui/app.py               # http://127.0.0.1:8765/

  docker compose up --build       # same port 8765
"""
from __future__ import annotations

import html
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent))

from data import (  # noqa: E402
    dashboard_stats,
    list_benchmarks,
    load_code_findings,
    load_hallucination_cases,
    load_py_torch_cases,
    load_real_model_cases,
    results_dir,
)

DEFAULT_PORT = 8765

CSS = """
:root {
  --bg:#0c1017; --panel:#151c28; --border:#2a3548; --text:#e8eef7;
  --muted:#8fa0b8; --accent:#4aa3ff; --ok:#3ecf8e; --bad:#ff6b7a; --warn:#f0c14a;
  --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  --sans:system-ui,-apple-system,"Segoe UI",sans-serif;
}
*{box-sizing:border-box} body{margin:0;font-family:var(--sans);background:var(--bg);color:var(--text);line-height:1.5}
a{color:var(--accent);text-decoration:none} a:hover{text-decoration:underline}
header{border-bottom:1px solid var(--border);padding:.9rem 1.25rem;display:flex;flex-wrap:wrap;gap:.75rem 1.25rem;align-items:center;background:#101722;position:sticky;top:0;z-index:20}
.brand{font-weight:700;font-size:1.05rem} .brand span{color:var(--muted);font-weight:500;font-size:.85rem;margin-left:.5rem}
nav{display:flex;gap:.35rem;flex-wrap:wrap}
nav a{color:var(--muted);padding:.4rem .7rem;border-radius:8px;border:1px solid transparent;font-size:.92rem}
nav a:hover,nav a.active{color:var(--text);background:var(--panel);border-color:var(--border);text-decoration:none}
main{padding:1.25rem 1.25rem 3rem;max-width:1200px;margin:0 auto}
h1{font-size:1.35rem;margin:0 0 .35rem} h2{font-size:1rem;margin:1.4rem 0 .5rem;color:var(--muted)}
.sub{color:var(--muted);margin:0 0 1rem;font-size:.95rem;max-width:70ch}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:.65rem;margin:1rem 0 1.25rem}
.card{background:var(--panel);border:1px solid var(--border);border-radius:12px;padding:.85rem 1rem}
.card .n{font-size:1.55rem;font-weight:700;font-variant-numeric:tabular-nums}
.card .l{color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.04em;margin-top:.15rem}
.card.alert .n{color:var(--bad)} .card.good .n{color:var(--ok)}
table{width:100%;border-collapse:collapse;font-size:.86rem;background:var(--panel);border:1px solid var(--border);border-radius:12px;overflow:hidden}
th,td{text-align:left;padding:.55rem .65rem;border-bottom:1px solid var(--border);vertical-align:top}
th{background:#121a28;color:var(--muted);font-weight:600;font-size:.78rem;text-transform:uppercase;letter-spacing:.03em}
tr:last-child td{border-bottom:0} tr:hover td{background:#1a2434}
.mono{font-family:var(--mono);font-size:.8rem}
.detail{color:var(--muted);max-width:36rem;word-break:break-word}
.badge{display:inline-block;padding:.1rem .45rem;border-radius:999px;font-size:.72rem;font-family:var(--mono);border:1px solid var(--border)}
.badge.bad{color:var(--bad);background:#2a1418;border-color:#5a2a32}
.badge.ok{color:var(--ok);background:#122a1e;border-color:#1e4d38}
.badge.warn{color:var(--warn);background:#2a2410;border-color:#5c4a20}
.badge.info{color:var(--accent);background:#122030}
.empty{color:var(--muted);padding:1.5rem;text-align:center;border:1px dashed var(--border);border-radius:12px;background:var(--panel)}
.filters{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;margin:0 0 1rem}
input[type=search],select,button{background:var(--panel);border:1px solid var(--border);color:var(--text);padding:.45rem .65rem;border-radius:8px;font:inherit}
button.primary{background:var(--accent);border-color:transparent;color:#041018;font-weight:600;cursor:pointer}
.callout{background:#121c2c;border:1px solid var(--border);border-left:3px solid var(--accent);border-radius:8px;padding:.75rem 1rem;margin:0 0 1rem;font-size:.92rem}
.callout code{font-family:var(--mono);font-size:.85rem;color:var(--warn)}
footer{color:var(--muted);font-size:.8rem;margin-top:2rem;padding-top:1rem;border-top:1px solid var(--border)}
.how ol{margin:.4rem 0 0 1.1rem;color:var(--muted)} .how li{margin:.25rem 0}
"""


def esc(s: Any) -> str:
    return html.escape("" if s is None else str(s))


def layout(title: str, active: str, body: str) -> str:
    def nav(href: str, label: str, key: str) -> str:
        cls = "active" if active == key else ""
        return f'<a class="{cls}" href="{href}">{esc(label)}</a>'

    return f"""<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>{esc(title)} · ghar</title><style>{CSS}</style>
</head><body>
<header>
  <div class="brand">ghar <span>local results</span></div>
  <nav>
    {nav("/", "Home", "home")}
    {nav("/findings", "Findings", "findings")}
    {nav("/real-model", "Coding model", "real")}
    {nav("/hallucinations", "Synthetic suite", "hallu")}
    {nav("/benchmarks", "Benchmarks", "bench")}
  </nav>
</header>
<main>{body}
<footer>
  Data dir: <span class="mono">{esc(results_dir())}</span>
  · oracles only (no LLM judge)
  · <a href="/api/health">/api/health</a>
</footer>
</main></body></html>"""


def page_home() -> str:
    st = dashboard_stats()
    top_f = st.get("top_actionable") or []
    top_c = st.get("top_caught") or []
    rs = st.get("real_model_summary") or {}
    catch = rs.get("with_harness_catch_rate", "—")

    frows = []
    for f in top_f:
        frows.append(
            f"<tr><td><span class='badge bad'>{esc(f.get('kind'))}</span></td>"
            f"<td class='mono'>{esc(f.get('location'))}</td>"
            f"<td class='mono'>{esc(f.get('attr'))}</td>"
            f"<td class='detail'>{esc((f.get('detail') or '')[:160])}</td></tr>"
        )
    ftable = (
        f"<table><thead><tr><th>Kind</th><th>Where</th><th>Attr</th><th>Detail</th></tr></thead>"
        f"<tbody>{''.join(frows)}</tbody></table>"
        if frows
        else '<div class="empty">No actionable findings yet. Run a suite or use sample data.</div>'
    )

    crows = []
    for r in top_c:
        crows.append(
            f"<tr><td class='mono'>{esc(r.get('id'))}</td>"
            f"<td>{esc(r.get('category'))}</td>"
            f"<td class='mono'>{esc(r.get('claim') or '—')}</td>"
            f"<td class='detail mono'>{esc((r.get('ghar_detail') or '')[:140])}</td></tr>"
        )
    ctable = (
        f"<table><thead><tr><th>Case</th><th>Check</th><th>Claim</th><th>ghar detail</th></tr></thead>"
        f"<tbody>{''.join(crows)}</tbody></table>"
        if crows
        else '<div class="empty">No coding-model catches in this results dir.</div>'
    )

    body = f"""
    <h1>What ghar found</h1>
    <p class="sub">Concrete oracle results for <strong>existing code</strong> and
    <strong>neural-net-generated claims</strong>. This page only shows measured artifacts — no invented scores.</p>

    <div class="callout how">
      <strong>How to use</strong>
      <ol>
        <li>Build CLI: <code>cmake -S . -B build && cmake --build build -j</code></li>
        <li>Run a suite: <code>bash benchmarks/run_py_torch_suite.sh</code></li>
        <li>Refresh this UI (already on <code>http://127.0.0.1:{DEFAULT_PORT}/</code>)</li>
      </ol>
    </div>

    <div class="cards">
      <div class="card alert"><div class="n">{st['findings_actionable']}</div><div class="l">Actionable findings</div></div>
      <div class="card"><div class="n">{st['findings_total']}</div><div class="l">All scan findings</div></div>
      <div class="card alert"><div class="n">{st['real_model_caught']}</div><div class="l">Model claims caught</div></div>
      <div class="card"><div class="n">{st['real_model_cases']}</div><div class="l">Model trials</div></div>
      <div class="card good"><div class="n">{esc(catch)}</div><div class="l">Model catch rate</div></div>
      <div class="card"><div class="n">{st['benchmarks_available']}/{st['benchmarks_total']}</div><div class="l">Benchmarks w/ data</div></div>
    </div>

    <h2>Top actionable code issues</h2>
    <p class="sub">api_missing / syntax / suite failures — vendor accelerator stubs hidden by default on the Findings page.</p>
    {ftable}
    <p><a href="/findings">All findings →</a></p>

    <h2>Coding-model claims ghar rejected</h2>
    {ctable}
    <p><a href="/real-model">All model trials →</a> · <a href="/benchmarks">Benchmark catalog →</a></p>
    """
    return layout("Home", "home", body)


def page_findings(q: str = "", mode: str = "actionable") -> str:
    rows = load_code_findings()
    mode = mode if mode in ("actionable", "all") else "actionable"
    if mode == "actionable":
        rows = [f for f in rows if f.actionable]

    qn = q.lower().strip()
    if qn:
        rows = [
            f
            for f in rows
            if qn in f.path.lower()
            or qn in f.kind.lower()
            or qn in f.attr.lower()
            or qn in f.detail.lower()
            or qn in f.category.lower()
            or qn in f.repo.lower()
        ]

    if not rows:
        table = (
            '<div class="empty">No rows for this filter. '
            'Try <a href="/findings?mode=all">show all</a> or run a scan/suite into <span class="mono">results/</span>.</div>'
        )
    else:
        trs = []
        for f in rows[:400]:
            badge = "bad" if f.actionable else "info"
            trs.append(
                f"<tr>"
                f"<td><span class='badge {badge}'>{esc(f.kind)}</span></td>"
                f"<td class='mono'>{esc(f.category or '—')}</td>"
                f"<td class='mono'>{esc(f.repo or f.source)}</td>"
                f"<td class='mono'>{esc(f.location())}</td>"
                f"<td class='mono'>{esc(f.attr or '—')}</td>"
                f"<td class='detail'>{esc(f.detail[:240])}</td>"
                f"</tr>"
            )
        table = f"""
        <table>
          <thead><tr>
            <th>Kind</th><th>Category</th><th>Source</th><th>Where</th><th>Attribute</th><th>What failed</th>
          </tr></thead>
          <tbody>{''.join(trs)}</tbody>
        </table>
        <p class="sub">Showing {len(trs)} row(s){(' (capped at 400)' if len(rows)>400 else '')}.</p>
        """

    sel_a = "selected" if mode == "actionable" else ""
    sel_all = "selected" if mode == "all" else ""
    body = f"""
    <h1>Code findings</h1>
    <p class="sub">Each row is a real oracle failure: <strong>where</strong> (path) and <strong>what</strong> (kind / attr / detail).</p>
    <form class="filters" method="get" action="/findings">
      <select name="mode">
        <option value="actionable" {sel_a}>Actionable only (recommended)</option>
        <option value="all" {sel_all}>All (include npu/mlu/musa stubs)</option>
      </select>
      <input type="search" name="q" value="{esc(q)}" placeholder="filter path, attr, kind…" style="min-width:14rem"/>
      <button class="primary" type="submit">Apply</button>
    </form>
    {table}
    """
    return layout("Findings", "findings", body)


def page_real_model() -> str:
    rows = load_real_model_cases()
    if not rows:
        table = (
            '<div class="empty">No <span class="mono">real_model_cases.tsv</span>. '
            "Run <span class=\"mono\">bash benchmarks/run_real_model_eval.sh</span> "
            "(needs Ollama) or rely on sample_results.</div>"
        )
    else:
        trs = []
        for r in rows:
            caught = str(r.get("caught_by_ghar") or r.get("caught") or "0")
            badge = "bad" if caught == "1" else "ok"
            label = "CAUGHT" if caught == "1" else "passed / skipped"
            claim = r.get("claim") or "—"
            exit_v = r.get("with_harness_exit") or r.get("got_exit") or "—"
            expect = r.get("expect_false") or ""
            expect_lbl = "false claim" if expect == "1" else ("true claim" if expect == "0" else expect or "—")
            trs.append(
                f"<tr>"
                f"<td class='mono'>{esc(r.get('id',''))}</td>"
                f"<td><span class='badge info'>{esc(r.get('category',''))}</span></td>"
                f"<td class='mono'>{esc(expect_lbl)}</td>"
                f"<td class='mono'>{esc(claim)}</td>"
                f"<td class='mono'>{esc(exit_v)}</td>"
                f"<td><span class='badge {badge}'>{label}</span></td>"
                f"<td class='detail mono'>{esc((r.get('ghar_detail') or r.get('detail') or '')[:200])}</td>"
                f"</tr>"
            )
        table = f"""
        <table>
          <thead><tr>
            <th>Case id</th><th>Check type</th><th>Expected</th><th>Claim / snippet</th>
            <th>ghar exit</th><th>Result</th><th>Where / detail</th>
          </tr></thead>
          <tbody>{''.join(trs)}</tbody>
        </table>
        """
    body = f"""
    <h1>Local coding-model results</h1>
    <p class="sub">Per-trial identity: <strong>case id</strong>, check type, claim text, ghar exit, and whether the harness caught a false claim.
    Without harness every claim would be accepted.</p>
    {table}
    """
    return layout("Coding model", "real", body)


def page_hallucinations() -> str:
    def render(title: str, data: list[dict[str, str]]) -> str:
        if not data:
            return f'<h2>{esc(title)}</h2><div class="empty">No data file for this suite.</div>'
        trs = []
        for r in data:
            caught = str(r.get("caught", "0"))
            badge = "bad" if caught == "1" else "ok"
            trs.append(
                f"<tr>"
                f"<td class='mono'>{esc(r.get('id',''))}</td>"
                f"<td>{esc(r.get('category',''))}</td>"
                f"<td>{esc(r.get('agent_claim',''))}</td>"
                f"<td class='mono'>{esc(r.get('expect_exit',''))} → {esc(r.get('got_exit',''))}</td>"
                f"<td><span class='badge {badge}'>{'caught' if caught=='1' else 'ok'}</span></td>"
                f"<td class='detail'>{esc((r.get('detail') or '')[:180])}</td>"
                f"</tr>"
            )
        return f"""
        <h2>{esc(title)}</h2>
        <table>
          <thead><tr><th>Id</th><th>Category</th><th>Claim</th><th>exit expect→got</th><th>Status</th><th>Detail</th></tr></thead>
          <tbody>{''.join(trs)}</tbody>
        </table>"""

    body = f"""
    <h1>Synthetic hallucination suites</h1>
    <p class="sub">Fixed labeled cases — no live model required. Used to prove ghar catch rate.</p>
    {render("General suite (import / compile / symbols / …)", load_hallucination_cases())}
    {render("Python / PyTorch suite", load_py_torch_cases())}
    """
    return layout("Synthetic suite", "hallu", body)


def page_benchmarks() -> str:
    benches = list_benchmarks()
    trs = []
    for b in benches:
        av = (
            '<span class="badge ok">has data</span>'
            if b.available
            else '<span class="badge warn">no results yet</span>'
        )
        keys = ("accuracy", "recall_catch_rate", "with_harness_catch_rate", "tp", "fn", "n_cases", "total_cases")
        bits = [f"{k}={b.summary[k]}" for k in keys if k in b.summary]
        if not bits:
            bits = [f"{k}={v}" for k, v in list(b.summary.items())[:5]]
        summ = ", ".join(bits) if bits else "—"
        trs.append(
            f"<tr>"
            f"<td class='mono'>{esc(b.id)}</td>"
            f"<td><strong>{esc(b.name)}</strong><div class='detail'>{esc(b.description)}</div></td>"
            f"<td class='mono'>{esc(b.script)}</td>"
            f"<td>{av}</td>"
            f"<td class='mono detail'>{esc(summ)}</td>"
            f"</tr>"
        )
    body = f"""
    <h1>Benchmarks</h1>
    <p class="sub">Hallucination-search and validation suites shipped with ghar. Run the script, then open the matching UI page.</p>
    <table>
      <thead><tr><th>Id</th><th>Name</th><th>How to run</th><th>Data</th><th>Latest metrics</th></tr></thead>
      <tbody>{''.join(trs)}</tbody>
    </table>
    """
    return layout("Benchmarks", "bench", body)


class Handler(BaseHTTPRequestHandler):
    server_version = "ghar-ui/1.1"
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, content_type: str) -> None:
        try:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
            self.wfile.flush()
        except BrokenPipeError:
            pass

    def _html(self, code: int, s: str) -> None:
        self._send(code, s.encode("utf-8"), "text/html; charset=utf-8")

    def _json(self, code: int, obj: Any) -> None:
        self._send(code, json.dumps(obj, indent=2, ensure_ascii=False).encode("utf-8"), "application/json; charset=utf-8")

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        qs = parse_qs(parsed.query)
        try:
            if path == "/":
                return self._html(200, page_home())
            if path == "/findings":
                return self._html(
                    200,
                    page_findings((qs.get("q") or [""])[0], (qs.get("mode") or ["actionable"])[0]),
                )
            if path == "/real-model":
                return self._html(200, page_real_model())
            if path == "/hallucinations":
                return self._html(200, page_hallucinations())
            if path == "/benchmarks":
                return self._html(200, page_benchmarks())
            if path == "/api/health":
                return self._json(
                    200,
                    {"status": "ok", "service": "ghar-ui", "results_dir": str(results_dir()), "stats": dashboard_stats()},
                )
            if path == "/api/findings":
                mode = (qs.get("mode") or ["all"])[0]
                rows = load_code_findings()
                if mode == "actionable":
                    rows = [f for f in rows if f.actionable]
                return self._json(200, {"findings": [f.to_dict() for f in rows], "mode": mode})
            if path == "/api/real-model":
                return self._json(200, {"cases": load_real_model_cases()})
            if path == "/api/hallucinations":
                return self._json(
                    200,
                    {"synthetic": load_hallucination_cases(), "py_torch": load_py_torch_cases()},
                )
            if path == "/api/benchmarks":
                return self._json(200, {"benchmarks": [b.to_dict() for b in list_benchmarks()]})
            return self._html(404, layout("Not found", "", f"<h1>404</h1><p class='mono'>{esc(path)}</p>"))
        except Exception as e:
            return self._json(500, {"error": str(e), "type": type(e).__name__})


def main() -> int:
    host = os.environ.get("GHAR_UI_HOST", "0.0.0.0")
    port = int(os.environ.get("GHAR_UI_PORT", str(DEFAULT_PORT)))
    print(f"ghar UI  http://127.0.0.1:{port}/  (bind {host})  results={results_dir()}", flush=True)
    httpd = ThreadingHTTPServer((host, port), Handler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nshutdown", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

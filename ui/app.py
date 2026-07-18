#!/usr/bin/env python3
"""
ghar local results UI — serves findings, coding-model hallu, and benchmark list.

  GHAR_RESULTS=./results python3 ui/app.py
  # or via docker compose

Env:
  GHAR_RESULTS / RESULTS_DIR  — path to TSV artifacts
  GHAR_UI_HOST                — default 0.0.0.0
  GHAR_UI_PORT                — default 8080
"""
from __future__ import annotations

import html
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

# allow `python3 ui/app.py` from repo root
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


CSS = """
:root {
  --bg: #0f1419;
  --panel: #1a2332;
  --border: #2d3a4d;
  --text: #e7ecf3;
  --muted: #8b9bb4;
  --accent: #3d9cf0;
  --ok: #3dd68c;
  --bad: #f07178;
  --warn: #e6b450;
  --mono: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  --sans: "Segoe UI", system-ui, sans-serif;
}
* { box-sizing: border-box; }
body {
  margin: 0; font-family: var(--sans); background: var(--bg); color: var(--text);
  line-height: 1.45;
}
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
header {
  border-bottom: 1px solid var(--border); padding: 1rem 1.5rem;
  display: flex; flex-wrap: wrap; gap: 1rem; align-items: center;
  background: #121a24;
  position: sticky; top: 0; z-index: 10;
}
header .brand { font-weight: 700; letter-spacing: 0.02em; font-size: 1.15rem; }
header nav { display: flex; gap: 0.75rem; flex-wrap: wrap; }
header nav a {
  color: var(--muted); padding: 0.35rem 0.7rem; border-radius: 6px;
  border: 1px solid transparent;
}
header nav a:hover, header nav a.active {
  color: var(--text); border-color: var(--border); background: var(--panel);
}
main { padding: 1.25rem 1.5rem 3rem; max-width: 1400px; margin: 0 auto; }
h1 { font-size: 1.4rem; margin: 0 0 0.5rem; }
h2 { font-size: 1.1rem; margin: 1.5rem 0 0.6rem; color: var(--muted); font-weight: 600; }
.sub { color: var(--muted); margin-bottom: 1rem; font-size: 0.95rem; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 0.75rem; }
.card {
  background: var(--panel); border: 1px solid var(--border); border-radius: 10px;
  padding: 0.9rem 1rem;
}
.card .n { font-size: 1.6rem; font-weight: 700; font-variant-numeric: tabular-nums; }
.card .l { color: var(--muted); font-size: 0.8rem; text-transform: uppercase; letter-spacing: 0.04em; }
table {
  width: 100%; border-collapse: collapse; font-size: 0.88rem;
  background: var(--panel); border: 1px solid var(--border); border-radius: 10px;
  overflow: hidden;
}
th, td {
  text-align: left; padding: 0.55rem 0.7rem; border-bottom: 1px solid var(--border);
  vertical-align: top;
}
th { background: #152033; color: var(--muted); font-weight: 600; position: sticky; top: 56px; }
tr:hover td { background: #1e2a3c; }
.mono { font-family: var(--mono); font-size: 0.82rem; }
.badge {
  display: inline-block; padding: 0.12rem 0.45rem; border-radius: 999px;
  font-size: 0.75rem; font-family: var(--mono); border: 1px solid var(--border);
}
.badge.bad { color: var(--bad); border-color: #5c3038; background: #2a1518; }
.badge.ok { color: var(--ok); border-color: #1e4d38; background: #132a20; }
.badge.warn { color: var(--warn); border-color: #5c4a20; background: #2a2210; }
.badge.info { color: var(--accent); }
.detail { color: var(--muted); max-width: 420px; word-break: break-word; }
.empty { color: var(--muted); padding: 2rem; text-align: center; border: 1px dashed var(--border); border-radius: 10px; }
footer { color: var(--muted); font-size: 0.8rem; margin-top: 2rem; }
.filter-bar { margin: 0.75rem 0 1rem; display: flex; gap: 0.5rem; flex-wrap: wrap; align-items: center; }
input[type=search], select {
  background: var(--panel); border: 1px solid var(--border); color: var(--text);
  padding: 0.4rem 0.6rem; border-radius: 6px; font-family: var(--sans);
}
"""


def esc(s: Any) -> str:
    return html.escape("" if s is None else str(s))


def layout(title: str, active: str, body: str) -> str:
    def nav(href: str, label: str, key: str) -> str:
        cls = "active" if active == key else ""
        return f'<a class="{cls}" href="{href}">{esc(label)}</a>'

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>{esc(title)} · ghar</title>
  <style>{CSS}</style>
</head>
<body>
  <header>
    <div class="brand">ghar results</div>
    <nav>
      {nav("/", "Dashboard", "home")}
      {nav("/findings", "Code findings", "findings")}
      {nav("/real-model", "Coding model", "real")}
      {nav("/hallucinations", "Synthetic hallu", "hallu")}
      {nav("/benchmarks", "Benchmarks", "bench")}
      {nav("/api/health", "API", "api")}
    </nav>
  </header>
  <main>
    {body}
    <footer>Local-only UI · data from <span class="mono">{esc(results_dir())}</span> · no LLM judge</footer>
  </main>
</body>
</html>
"""


def page_home() -> str:
    st = dashboard_stats()
    cards = f"""
    <div class="cards">
      <div class="card"><div class="n">{st['findings_count']}</div><div class="l">Code findings</div></div>
      <div class="card"><div class="n">{st['real_model_cases']}</div><div class="l">Model trials</div></div>
      <div class="card"><div class="n">{st['hallucination_cases']}</div><div class="l">Synthetic cases</div></div>
      <div class="card"><div class="n">{st['py_torch_cases']}</div><div class="l">Py/Torch cases</div></div>
      <div class="card"><div class="n">{st['benchmarks_available']}/{st['benchmarks_total']}</div><div class="l">Benchmarks w/ data</div></div>
    </div>
    """
    hs = st.get("real_model_summary") or {}
    catch = hs.get("with_harness_catch_rate") or hs.get("recall") or "—"
    body = f"""
    <h1>Results dashboard</h1>
    <p class="sub">Concrete ghar oracle results for problematic code and neural-net-generated claims.
    Run suites locally, then refresh — this UI only reads artifacts under <span class="mono">{esc(st['results_dir'])}</span>.</p>
    {cards}
    <h2>Quick links</h2>
    <ul>
      <li><a href="/findings">Code findings</a> — kind, path, line/attr, detail</li>
      <li><a href="/real-model">Coding-model hallucinations</a> — per-trial id, claim, ghar check, exit</li>
      <li><a href="/hallucinations">Synthetic hallucination suite</a> — labeled claims</li>
      <li><a href="/benchmarks">Benchmark catalog</a> — what exists and latest summaries</li>
    </ul>
    <h2>Coding-model catch rate (if available)</h2>
    <p class="mono">with_harness_catch_rate = {esc(catch)}</p>
    """
    return layout("Dashboard", "home", body)


def page_findings(q: str = "", cat: str = "") -> str:
    rows = load_code_findings()
    qn = q.lower().strip()
    if qn:
        rows = [
            f
            for f in rows
            if qn in (f.path or "").lower()
            or qn in (f.kind or "").lower()
            or qn in (f.attr or "").lower()
            or qn in (f.detail or "").lower()
            or qn in (f.extra.get("repo") or "").lower()
        ]
    if cat:
        rows = [f for f in rows if (f.category or "") == cat or (f.kind or "") == cat]

    cats = sorted({f.category or f.kind for f in load_code_findings() if (f.category or f.kind)})
    opts = "".join(
        f'<option value="{esc(c)}"{" selected" if c == cat else ""}>{esc(c)}</option>' for c in cats
    )

    if not rows:
        table = '<div class="empty">No findings in results yet. Run scanners / suites, or mount results/.</div>'
    else:
        trs = []
        for f in rows[:500]:
            sev = f.severity or ("error" if f.kind else "info")
            badge = "bad" if sev in ("error", "fail") else ("warn" if sev == "warn" else "info")
            loc = f.path
            if f.line and f.line not in ("0", ""):
                loc = f"{f.path}:{f.line}"
            repo = f.extra.get("repo") or ""
            trs.append(
                f"<tr>"
                f'<td><span class="badge {badge}">{esc(f.kind)}</span></td>'
                f'<td class="mono">{esc(f.category)}</td>'
                f'<td class="mono">{esc(repo)}</td>'
                f'<td class="mono">{esc(loc)}</td>'
                f'<td class="mono">{esc(f.attr)}</td>'
                f'<td class="detail">{esc(f.detail[:220])}</td>'
                f"</tr>"
            )
        table = f"""
        <table>
          <thead><tr>
            <th>Kind</th><th>Category</th><th>Repo/source</th><th>Where (path:line)</th><th>Attr</th><th>Detail</th>
          </tr></thead>
          <tbody>{"".join(trs)}</tbody>
        </table>
        <p class="sub">Showing {len(trs)} of {len(rows)} filtered / total matching rows (cap 500).</p>
        """

    body = f"""
    <h1>Code findings</h1>
    <p class="sub">What ghar found in scanned / validated code: failure kind and exact location.</p>
    <form class="filter-bar" method="get" action="/findings">
      <input type="search" name="q" placeholder="filter path, attr, kind…" value="{esc(q)}" style="min-width:220px"/>
      <select name="cat">
        <option value="">all categories</option>
        {opts}
      </select>
      <button type="submit" style="background:var(--accent);border:0;color:#fff;padding:0.4rem 0.8rem;border-radius:6px;cursor:pointer">Filter</button>
    </form>
    {table}
    """
    return layout("Findings", "findings", body)


def page_real_model() -> str:
    rows = load_real_model_cases()
    if not rows:
        table = '<div class="empty">No real_model_cases.tsv yet. Run <span class="mono">benchmarks/run_real_model_eval.sh</span> or use fixtures.</div>'
    else:
        trs = []
        for r in rows:
            caught = r.get("caught_by_ghar") or r.get("caught") or "0"
            badge = "bad" if caught == "1" else "ok"
            trs.append(
                f"<tr>"
                f'<td class="mono">{esc(r.get("id",""))}</td>'
                f'<td><span class="badge info">{esc(r.get("category",""))}</span></td>'
                f'<td class="mono">{esc(r.get("claim",""))}</td>'
                f'<td class="mono">{esc(r.get("with_harness_exit") or r.get("got_exit",""))}</td>'
                f'<td><span class="badge {badge}">{"caught" if caught=="1" else "not caught"}</span></td>'
                f'<td class="detail mono">{esc((r.get("ghar_detail") or r.get("detail") or "")[:200])}</td>'
                f'<td class="mono">{esc(r.get("expect_false",""))}</td>'
                f"</tr>"
            )
        table = f"""
        <table>
          <thead><tr>
            <th>Case id</th><th>Category</th><th>Claim / location</th><th>ghar exit</th><th>Caught</th><th>ghar detail</th><th>expect_false</th>
          </tr></thead>
          <tbody>{"".join(trs)}</tbody>
        </table>
        """
    body = f"""
    <h1>Local coding-model hallucinations</h1>
    <p class="sub">Per-trial results: where the model’s claim was checked and whether ghar rejected it.
    Identities are case id + check category + claim text + exit.</p>
    {table}
    """
    return layout("Coding model", "real", body)


def page_hallucinations() -> str:
    rows = load_hallucination_cases()
    py = load_py_torch_cases()
    def render(title: str, data: list[dict[str, str]]) -> str:
        if not data:
            return f'<h2>{esc(title)}</h2><div class="empty">No data</div>'
        trs = []
        for r in data:
            caught = r.get("caught", "0")
            badge = "bad" if caught == "1" else "ok"
            trs.append(
                f"<tr>"
                f'<td class="mono">{esc(r.get("id",""))}</td>'
                f'<td>{esc(r.get("category",""))}</td>'
                f'<td>{esc(r.get("agent_claim",""))}</td>'
                f'<td class="mono">{esc(r.get("expect_exit",""))}→{esc(r.get("got_exit",""))}</td>'
                f'<td><span class="badge {badge}">{esc("TP" if caught=="1" else "—")}</span></td>'
                f'<td class="detail">{esc((r.get("detail") or "")[:180])}</td>'
                f"</tr>"
            )
        return f"""
        <h2>{esc(title)}</h2>
        <table>
          <thead><tr><th>Id</th><th>Category</th><th>Claim</th><th>exit</th><th>Caught</th><th>Detail / location</th></tr></thead>
          <tbody>{"".join(trs)}</tbody>
        </table>
        """

    body = f"""
    <h1>Synthetic hallucination benchmarks</h1>
    <p class="sub">Labeled false/true claims checked by ghar (no live model required).</p>
    {render("General hallu suite", rows)}
    {render("Python / PyTorch suite", py)}
    """
    return layout("Synthetic hallu", "hallu", body)


def page_benchmarks() -> str:
    benches = list_benchmarks()
    trs = []
    for b in benches:
        av = '<span class="badge ok">data present</span>' if b.available else '<span class="badge warn">no results yet</span>'
        summ = ", ".join(f"{k}={v}" for k, v in list(b.summary.items())[:6]) or "—"
        files = ", ".join(b.result_files)
        trs.append(
            f"<tr>"
            f'<td class="mono">{esc(b.id)}</td>'
            f"<td><strong>{esc(b.name)}</strong><br/><span class=\"detail\">{esc(b.description)}</span></td>"
            f'<td class="mono">{esc(b.script)}</td>'
            f'<td class="mono detail">{esc(files)}</td>'
            f"<td>{av}</td>"
            f'<td class="mono detail">{esc(summ)}</td>'
            f"</tr>"
        )
    body = f"""
    <h1>Hallucination-search benchmarks</h1>
    <p class="sub">Catalog of in-repo suites that search for hallucinations / false code claims.</p>
    <table>
      <thead><tr>
        <th>Id</th><th>Name</th><th>Script</th><th>Result artifacts</th><th>Status</th><th>Latest summary</th>
      </tr></thead>
      <tbody>{"".join(trs)}</tbody>
    </table>
    """
    return layout("Benchmarks", "bench", body)


class Handler(BaseHTTPRequestHandler):
    server_version = "ghar-ui/1.0"
    # HTTP/1.0 avoids keep-alive issues with some Docker userland proxies
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt: str, *args) -> None:
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
        body = json.dumps(obj, indent=2, ensure_ascii=False).encode("utf-8")
        self._send(code, body, "application/json; charset=utf-8")

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        qs = parse_qs(parsed.query)

        try:
            if path == "/":
                return self._html(200, page_home())
            if path == "/findings":
                q = (qs.get("q") or [""])[0]
                cat = (qs.get("cat") or [""])[0]
                return self._html(200, page_findings(q, cat))
            if path == "/real-model":
                return self._html(200, page_real_model())
            if path == "/hallucinations":
                return self._html(200, page_hallucinations())
            if path == "/benchmarks":
                return self._html(200, page_benchmarks())

            # JSON API
            if path == "/api/health":
                return self._json(
                    200,
                    {
                        "status": "ok",
                        "service": "ghar-ui",
                        "results_dir": str(results_dir()),
                        "stats": dashboard_stats(),
                    },
                )
            if path == "/api/findings":
                return self._json(200, {"findings": [f.to_dict() for f in load_code_findings()]})
            if path == "/api/real-model":
                return self._json(200, {"cases": load_real_model_cases()})
            if path == "/api/hallucinations":
                return self._json(
                    200,
                    {
                        "synthetic": load_hallucination_cases(),
                        "py_torch": load_py_torch_cases(),
                    },
                )
            if path == "/api/benchmarks":
                return self._json(200, {"benchmarks": [b.to_dict() for b in list_benchmarks()]})

            return self._html(404, layout("Not found", "", f"<h1>404</h1><p>{esc(path)}</p>"))
        except Exception as e:
            return self._json(500, {"error": str(e), "type": type(e).__name__})


def main() -> int:
    host = os.environ.get("GHAR_UI_HOST", "0.0.0.0")
    port = int(os.environ.get("GHAR_UI_PORT", "8080"))
    rd = results_dir()
    print(f"ghar UI listening on http://{host}:{port}/  results={rd}", flush=True)
    httpd = ThreadingHTTPServer((host, port), Handler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nshutdown", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

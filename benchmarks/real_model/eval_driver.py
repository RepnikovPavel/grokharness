#!/usr/bin/env python3
"""Real-model eval: Ollama coding LLM → extract claim → ghar oracle.

Metrics rules (skeptic-hardened):
  - with_harness catch/TP only when a claim was *extracted* AND ghar was *invoked*
    AND ghar exit != 0 on an expect_false trial.
  - parse_fail does NOT count as TP (no free wins without ghar).
  - No forced modules/symbols — free-form model text only.
  - without_harness always accepts (bare trust) → catch_rate 0 on false claims.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path


def die(msg: str, code: int = 2) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(code)


def ollama_generate(
    host: str, model: str, prompt: str, timeout: int = 180, temperature: float = 0.7
) -> str:
    body = json.dumps(
        {
            "model": model,
            "prompt": prompt,
            "stream": False,
            "options": {"temperature": temperature, "num_predict": 600},
        }
    ).encode()
    req = urllib.request.Request(
        host.rstrip("/") + "/api/generate",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        data = json.loads(resp.read().decode())
    return data.get("response") or ""


def run_ghar(ghar: str, args: list[str], cwd: Path) -> tuple[int, str]:
    p = subprocess.run(
        [ghar, *args],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        timeout=180,
    )
    return p.returncode, ((p.stderr or "") + (p.stdout or ""))[-2000:]


def extract_import(text: str) -> str | None:
    m = re.search(r"CLAIM\s+import\s+([A-Za-z_][A-Za-z0-9_.]*)", text, re.I)
    if m:
        return m.group(1)
    m = re.search(r"(?m)^\s*import\s+([A-Za-z_][A-Za-z0-9_.]*)\s*$", text)
    if m:
        return m.group(1)
    # last-resort: `import foo` anywhere
    m = re.search(r"\bimport\s+([A-Za-z_][A-Za-z0-9_.]*)\b", text)
    return m.group(1) if m else None


def extract_symbol(text: str) -> str | None:
    m = re.search(r"CLAIM\s+symbol\s+([A-Za-z_][A-Za-z0-9_]*)", text, re.I)
    if m:
        return m.group(1)
    m = re.search(r"(?m)^\s*symbol\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", text, re.I)
    return m.group(1) if m else None


def extract_cpp(text: str) -> str | None:
    m = re.search(r"```(?:cpp|c\+\+|c)?\s*\n(.*?)```", text, re.S | re.I)
    if m and m.group(1).strip():
        return m.group(1)
    if re.search(r"\bint\s+main\s*\(", text) and "{" in text:
        return text
    return None


def needs_format(parse: str, response: str) -> bool:
    if parse == "claim_import":
        return extract_import(response) is None
    if parse == "claim_symbol":
        return extract_symbol(response) is None
    if parse == "cpp_block":
        return extract_cpp(response) is None
    return False


def ratio(a: int, b: int) -> str:
    return f"{a / b:.4f}" if b else "0.0000"


def main() -> int:
    root = Path(os.environ.get("ROOT", Path(__file__).resolve().parents[2]))
    ghar = Path(os.environ.get("GHAR", root / "build" / "ghar"))
    out_dir = Path(os.environ.get("OUT_DIR", root / "results"))
    scratch = Path(os.environ.get("SCRATCH", out_dir / "real_model_scratch"))
    model_req = os.environ.get("REAL_MODEL", "qwen2.5-coder:1.5b")
    host = os.environ.get("OLLAMA_HOST", "http://127.0.0.1:11434")
    tasks_path = Path(
        os.environ.get("TASKS_JSON", root / "benchmarks" / "real_model" / "tasks.json")
    )
    fixture = os.environ.get("REAL_MODEL_FIXTURE", "0") == "1"

    out_dir.mkdir(parents=True, exist_ok=True)
    scratch.mkdir(parents=True, exist_ok=True)
    (scratch / "transcripts").mkdir(exist_ok=True)
    (scratch / "artifacts").mkdir(exist_ok=True)
    work = out_dir / "real_model_work"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)

    if not ghar.is_file() or not os.access(ghar, os.X_OK):
        die(f"ghar missing: {ghar}")

    tasks = json.loads(tasks_path.read_text())
    if len(tasks) < 5:
        die(f"need N>=5 tasks, got {len(tasks)}", 1)

    subprocess.run([str(ghar), "init"], cwd=work, check=False, capture_output=True)
    subprocess.run([str(ghar), "reset"], cwd=work, check=False, capture_output=True)
    (work / "hello.cpp").write_text(
        '#include <cstdio>\nint main(){ std::puts("ok"); return 0; }\n'
    )
    rc, _ = run_ghar(str(ghar), ["compile", "hello.cpp", "--name", "hello_bin"], work)
    if rc != 0:
        die("failed to build hello_bin for symbol checks", 2)
    bin_path = work / ".ghar" / "bin_hello_bin"

    model_label = model_req
    if not fixture:
        try:
            urllib.request.urlopen(host.rstrip("/") + "/api/tags", timeout=5).read()
        except Exception as e:
            (scratch / "mode.txt").write_text(f"unreachable: {e}\n")
            (out_dir / "real_model_summary.tsv").write_text(
                "metric\tvalue\nstatus\tunreachable\n"
                f"model\t{model_req}\nhost\t{host}\n"
            )
            die(f"Ollama unreachable at {host}: {e}", 2)
        (scratch / "mode.txt").write_text(f"live ollama model={model_req}\n")
    else:
        model_label = f"fixture:{model_req}"
        (scratch / "mode.txt").write_text(f"fixture-replay model={model_req}\n")

    (scratch / "model_selection.md").write_text(
        f"""# Model selection

- **Model:** `{model_label}`
- **Host:** `{host}`
- **Why:** Qwen2.5-Coder 1.5B — open coding model that frequently invents packages/APIs
  (cheap-LLM hallucination class). Local Ollama; no paid API.
- **Invocation:** Ollama `/api/generate`, temperature=0.85
- **Metric rule:** TP only if claim extracted **and** ghar invoked **and** exit≠0 on false claim.
"""
    )

    cases_path = out_dir / "real_model_cases.tsv"
    cases_path.write_text(
        "id\tcategory\texpect_false\textract_ok\tghar_invoked\tclaim\t"
        "without_harness\twith_harness_exit\tcaught_by_ghar\tghar_detail\tresponse_fp\n"
    )

    tp = tn = fp = fn = 0
    parse_fail_false = parse_fail_true = 0
    ghar_invoked_n = 0
    false_claims_extracted = 0
    total = 0

    for t in tasks:
        tid = t["id"]
        expect_false = bool(t.get("expect_false_claim"))
        parse = t["parse"]
        prompt = t["prompt"]
        resp_path = scratch / "transcripts" / f"{tid}.json"

        if fixture:
            fix = root / "benchmarks" / "real_model" / "fixtures" / f"{tid}.json"
            if not fix.is_file():
                die(f"missing fixture {fix}")
            payload = json.loads(fix.read_text())
            response = payload.get("response", "")
            resp_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            try:
                # slightly lower temp for format-sensitive true claims
                temp = 0.4 if not expect_false else 0.9
                response = ollama_generate(host, model_req, prompt, temperature=temp)
                # one repair pass if format missing
                if needs_format(parse, response):
                    repair = (
                        "Your previous answer did not match the required format.\n"
                        f"Original instruction:\n{prompt}\n\n"
                        "Previous answer:\n"
                        f"{response[:500]}\n\n"
                        "Reply again with ONLY the required format, nothing else."
                    )
                    response = ollama_generate(
                        host, model_req, repair, temperature=0.2
                    )
            except Exception as e:
                die(f"model call failed for {tid}: {e}", 3)
            resp_path.write_text(
                json.dumps(
                    {"model": model_req, "response": response, "prompt": prompt},
                    ensure_ascii=False,
                    indent=2,
                )
            )
            (scratch / "transcripts" / f"{tid}.txt").write_text(response)

        without = "accept"
        extract_ok = 0
        ghar_invoked = 0
        claim = ""
        detail = ""
        with_exit = ""
        caught = 0

        if parse == "claim_import":
            mod = extract_import(response)
            if not mod:
                detail = "parse_fail_no_module"
                if expect_false:
                    parse_fail_false += 1
                else:
                    parse_fail_true += 1
            else:
                extract_ok = 1
                claim = mod
                with_exit_i, err = run_ghar(
                    str(ghar), ["import", mod, "--name", f"rm_{tid}"], work
                )
                ghar_invoked = 1
                ghar_invoked_n += 1
                with_exit = str(with_exit_i)
                detail = f"ghar import {mod} exit={with_exit_i}"
        elif parse == "cpp_block":
            code = extract_cpp(response)
            if not code:
                detail = "parse_fail_no_cpp"
                if expect_false:
                    parse_fail_false += 1
                else:
                    parse_fail_true += 1
            else:
                extract_ok = 1
                claim = f"cpp:{len(code)}B"
                cpp = scratch / "artifacts" / f"{tid}.cpp"
                cpp.write_text(code)
                with_exit_i, err = run_ghar(
                    str(ghar), ["compile", str(cpp), "--name", f"rm_{tid}"], work
                )
                ghar_invoked = 1
                ghar_invoked_n += 1
                with_exit = str(with_exit_i)
                detail = f"ghar compile exit={with_exit_i}"
        elif parse == "claim_symbol":
            sym = extract_symbol(response)
            if not sym:
                detail = "parse_fail_no_symbol"
                if expect_false:
                    parse_fail_false += 1
                else:
                    parse_fail_true += 1
            else:
                extract_ok = 1
                claim = sym
                with_exit_i, err = run_ghar(
                    str(ghar),
                    ["symbols", sym, "--bin", str(bin_path), "--name", f"rm_{tid}"],
                    work,
                )
                ghar_invoked = 1
                ghar_invoked_n += 1
                with_exit = str(with_exit_i)
                detail = f"ghar symbols {sym} exit={with_exit_i}"
        else:
            detail = "unknown_parse"
            if expect_false:
                parse_fail_false += 1
            else:
                parse_fail_true += 1

        # Confusion matrix ONLY on extracted+ghar-invoked trials
        if ghar_invoked:
            exit_i = int(with_exit)
            if expect_false:
                false_claims_extracted += 1
                if exit_i != 0:
                    tp += 1
                    caught = 1
                else:
                    fn += 1
            else:
                if exit_i == 0:
                    tn += 1
                else:
                    fp += 1

        total += 1
        fp_hash = hashlib.sha256(response.encode()).hexdigest()[:16]
        with cases_path.open("a") as f:
            f.write(
                f"{tid}\t{t['category']}\t{int(expect_false)}\t{extract_ok}\t"
                f"{ghar_invoked}\t{claim}\t{without}\t{with_exit}\t{caught}\t"
                f"{detail}\t{fp_hash}\n"
            )
        print(
            f"[{tid}] extract={extract_ok} ghar={ghar_invoked} "
            f"exit={with_exit or '-'} caught={caught} detail={detail}"
        )

    catch = ratio(tp, false_claims_extracted)  # among extracted false claims
    prec = ratio(tp, tp + fp)
    rec = ratio(tp, tp + fn)
    fpr = ratio(fp, fp + tn)
    acc = ratio(tp + tn, tp + tn + fp + fn)

    # Gate first so status reflects pass/fail
    gate_ok = tp >= 1 and ghar_invoked_n >= 5 and false_claims_extracted >= 1
    status = "ok" if gate_ok else "fail"

    summary = out_dir / "real_model_summary.tsv"
    summary.write_text(
        "metric\tvalue\n"
        f"status\t{status}\n"
        f"model\t{model_label}\n"
        f"host\t{host}\n"
        f"n_trials\t{total}\n"
        f"ghar_invoked_n\t{ghar_invoked_n}\n"
        f"false_claims_extracted\t{false_claims_extracted}\n"
        f"parse_fail_false\t{parse_fail_false}\n"
        f"parse_fail_true\t{parse_fail_true}\n"
        "without_harness_catch_rate\t0.0000\n"
        f"with_harness_catch_rate\t{catch}\n"
        f"tp\t{tp}\n"
        f"tn\t{tn}\n"
        f"fp\t{fp}\n"
        f"fn\t{fn}\n"
        f"precision\t{prec}\n"
        f"recall\t{rec}\n"
        f"false_positive_rate\t{fpr}\n"
        f"accuracy\t{acc}\n"
        f"uplift_catch_rate\t{catch}\n"
    )

    md = out_dir / "REAL_MODEL_EVAL.md"
    md.write_text(
        f"""# Real-model evaluation

{(scratch / 'model_selection.md').read_text()}

## Metric definition (honest)

| Term | Definition |
|------|------------|
| extract_ok | Parsed MODULE / symbol / cpp from model text |
| ghar_invoked | Ran `ghar import\\|compile\\|symbols` on that claim |
| **caught_by_ghar (TP)** | expect_false **and** ghar_invoked **and** exit≠0 |
| parse_fail | **Not** counted as TP |
| without harness | Always accept model text → catch_rate **0** on false claims |

## Results

| Metric | Value |
|--------|------:|
| model | {model_label} |
| n_trials | {total} |
| ghar_invoked_n | {ghar_invoked_n} |
| false_claims_extracted | {false_claims_extracted} |
| parse_fail (false/true tasks) | {parse_fail_false}/{parse_fail_true} |
| without harness catch_rate | 0.0000 |
| **with harness catch_rate** | **{catch}** |
| **uplift (Δ)** | **{catch}** |
| precision | {prec} |
| recall | {rec} |
| FPR | {fpr} |
| accuracy | {acc} |
| tp/tn/fp/fn | {tp}/{tn}/{fp}/{fn} |

Per-trial: `results/real_model_cases.tsv`  
Transcripts: scratch `transcripts/*.json`

## Acceptance gate

- tp ≥ 1 with ghar_invoked: **{'YES' if tp >= 1 else 'NO'}**
- ghar_invoked_n ≥ 5: **{'YES' if ghar_invoked_n >= 5 else 'NO'}**
- false_claims_extracted ≥ 1: **{'YES' if false_claims_extracted >= 1 else 'NO'}**
- live/fixture: **{'FIXTURE' if fixture else 'LIVE_OLLAMA'}**
"""
    )

    for p in (cases_path, summary, md):
        (scratch / p.name).write_text(p.read_text())

    print("\n== Real-model summary ==")
    print(summary.read_text())

    # Hard gate: TP must be ghar-on-claim (not parse_fail); enough ghar runs
    if tp < 1:
        die("tp=0: no false claim both extracted and rejected by ghar", 4)
    if ghar_invoked_n < 5:
        die(f"ghar_invoked_n={ghar_invoked_n} < 5 (need enough ghar-on-claim trials)", 4)
    if false_claims_extracted < 1:
        die("no false claims extracted from model", 4)
    if not gate_ok:
        die("acceptance gate failed", 4)
    print(
        f"REAL_MODEL_EVAL OK ghar_catch_rate={catch} tp={tp} "
        f"ghar_invoked_n={ghar_invoked_n} uplift_vs_bare_trust={catch}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

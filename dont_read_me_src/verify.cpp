// SPDX-License-Identifier: 0BSD
#include "verify.hpp"

#include "config.hpp"
#include "gate.hpp"
#include "output.hpp"
#include "store.hpp"
#include "tsv.hpp"
#include "util.hpp"
#include "work.hpp"

#include <algorithm>
#include <cstdio>

namespace ghar {
namespace {

std::string tail_lines(const std::string& s, int max_lines)
{
    auto lines = split(s, '\n');
    std::string out;
    const int start = std::max(0, static_cast<int>(lines.size()) - max_lines);
    for (int i = start; i < static_cast<int>(lines.size()); ++i) {
        if (!out.empty())
            out += "\n";
        out += lines[static_cast<size_t>(i)];
    }
    if (out.size() > 2000)
        out = out.substr(out.size() - 2000);
    return out;
}

// Map step name to claim kind (gate substantive set).
std::string step_kind(const std::string& name)
{
    if (name == "lint" || name == "build" || name == "test" || name == "cuda")
        return name;
    return "verify";
}

}  // namespace

int cmd_config_show(const std::string& root, bool pretty)
{
    auto cfg = load_project_config(root);
    if (pretty) {
        std::printf("config_file: %s\n", cfg.path.empty() ? "(defaults from layout)" : cfg.path.c_str());
        std::printf("timeout_sec: %d\n", cfg.timeout_sec);
        std::printf("on_fail: %s\n", cfg.on_fail.c_str());
        std::printf("steps:\n");
        for (const auto& s : cfg.steps)
            std::printf("  - %s: %s\n", s.name.c_str(), s.cmd.c_str());
        return EXIT_OK;
    }
    FieldMap f;
    f["config_file"] = cfg.path.empty() ? "defaults" : cfg.path;
    f["timeout_sec"] = std::to_string(cfg.timeout_sec);
    f["on_fail"] = cfg.on_fail;
    f["n_steps"] = std::to_string(cfg.steps.size());
    emit_row("config", f);
    for (const auto& s : cfg.steps) {
        FieldMap sf;
        sf["name"] = s.name;
        sf["cmd"] = s.cmd;
        sf["kind"] = step_kind(s.name);
        emit_row("config_step", sf);
    }
    return EXIT_OK;
}

int cmd_verify(const VerifyOpts& opts)
{
    Store store(opts.root);
    store.ensure();
    auto cfg = load_project_config(opts.root);

    // Clear only previous pipeline step claims; keep domain claims (bench/assert/…).
    {
        auto claims = store.load_claims();
        std::vector<Claim> keep;
        for (const auto& c : claims) {
            if (c.name == "pipeline" || starts_with(c.name, "step_"))
                continue;
            keep.push_back(c);
        }
        write_file(join_path(store.dir(), "claims.tsv"),
                   "name\tkind\tstatus\tmetrics\tupdated_at\tsource_id\n");
        for (const auto& c : keep)
            store.upsert_claim(c);
    }

    const bool fail_fast = opts.fail_fast && (cfg.on_fail != "continue");

    std::vector<VerifyStep> steps;
    for (const auto& s : cfg.steps) {
        if (!opts.only_step.empty() && s.name != opts.only_step)
            continue;
        steps.push_back(s);
    }

    if (steps.empty()) {
        CheckResult r;
        r.kind = "verify";
        r.name = "pipeline";
        r.status = "error";
        r.exit_code = EXIT_USAGE;
        r.detail = opts.only_step.empty() ? "no verify steps configured — run: ghar scaffold"
                                          : ("unknown step: " + opts.only_step);
        return finish_check(store, r, opts.pretty);
    }

    int step_ok = 0, step_fail = 0;
    int first_fail_code = EXIT_OK;
    std::string first_fail_name;

    if (opts.pretty) {
        std::printf("ghar verify: %zu step(s) from %s\n", steps.size(),
                    cfg.path.empty() ? "layout defaults" : cfg.path.c_str());
    }

    for (const auto& step : steps) {
        CheckResult r;
        r.kind = step_kind(step.name);
        r.name = "step_" + step.name;
        r.metrics["cmd"] = step.cmd;
        r.metrics["step"] = step.name;

        // Industry pattern (Aider): run shell oracle, non-zero = fail, feed stderr to agent
        auto cr = run_shell(step.cmd, cfg.timeout_sec, opts.root);
        r.metrics["wall_ms"] = std::to_string(cr.wall_ms);
        r.metrics["proc_exit"] = std::to_string(cr.exit_code);
        r.metrics["timed_out"] = cr.timed_out ? "1" : "0";
        r.metrics["stdout_bytes"] = std::to_string(cr.stdout_s.size());
        r.metrics["stderr_bytes"] = std::to_string(cr.stderr_s.size());
        r.metrics["stdout_fp"] = sha256_hex(cr.stdout_s);
        r.metrics["stderr_fp"] = sha256_hex(cr.stderr_s);

        // Parse KEY=value floats from combined output (same as run)
        const std::string combined = cr.stdout_s + "\n" + cr.stderr_s;
        for (const auto& line : split(combined, '\n')) {
            auto L = trim(line);
            auto eq = L.find('=');
            if (eq == std::string::npos || eq == 0)
                continue;
            std::string key = L.substr(0, eq);
            std::string val = L.substr(eq + 1);
            bool key_ok = true;
            for (char ch : key) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.')) {
                    key_ok = false;
                    break;
                }
            }
            if (!key_ok || key.size() > 64)
                continue;
            double d;
            if (parse_double(trim(val), d))
                r.metrics["out." + key] = trim(val);
        }

        if (cr.timed_out) {
            r.status = "fail";
            r.exit_code = EXIT_FAIL;
            r.detail = "step " + step.name + " timed out";
        } else if (cr.exit_code != 0) {
            r.status = "fail";
            r.exit_code = EXIT_FAIL;
            const std::string feedback = tail_lines(
                cr.stderr_s.empty() ? cr.stdout_s : cr.stderr_s, 40);
            r.detail = "step " + step.name + " failed (exit " + std::to_string(cr.exit_code) + ")";
            // Store raw feedback; finish_check prints FEEDBACK on stderr (Aider loop).
            r.metrics["feedback"] = feedback;
        } else {
            r.status = "ok";
            r.exit_code = EXIT_OK;
            r.detail = "step " + step.name + " ok";
            ++step_ok;
        }

        if (r.status != "ok") {
            ++step_fail;
            if (first_fail_code == EXIT_OK) {
                first_fail_code = EXIT_FAIL;
                first_fail_name = step.name;
            }
        }

        finish_check(store, r, opts.pretty);

        if (r.status != "ok" && fail_fast)
            break;
    }

    // Pipeline summary claim
    CheckResult summary;
    summary.kind = "verify";
    summary.name = "pipeline";
    summary.metrics["steps_ok"] = std::to_string(step_ok);
    summary.metrics["steps_fail"] = std::to_string(step_fail);
    summary.metrics["steps_total"] = std::to_string(steps.size());
    summary.metrics["config"] = cfg.path.empty() ? "defaults" : cfg.path;
    if (step_fail == 0) {
        summary.status = "ok";
        summary.exit_code = EXIT_OK;
        summary.detail = "all verify steps passed";
    } else {
        summary.status = "fail";
        summary.exit_code = EXIT_FAIL;
        summary.detail = "failed at step: " + first_fail_name;
    }
    finish_check(store, summary, opts.pretty);

    if (step_fail != 0)
        return EXIT_FAIL;

    // Count only *full* pipeline successes toward work quotas (not --step partials).
    if (opts.only_step.empty())
        work_note_verify_ok(opts.root);

    if (opts.skip_gate)
        return EXIT_OK;

    // Claude Stop-hook pattern: final delivery gate
    const int gate_rc = cmd_gate(opts.root, opts.pretty);
    if (gate_rc != EXIT_OK)
        return gate_rc;

    // Anti-5-minute-quit: if work session active and deliver/enforce, block early stop
    const WorkConfig wc = load_work_config(opts.root);
    if (opts.deliver || wc.enforce_on_verify) {
        std::string reason;
        const int wrc = work_check_delivery(opts.root, opts.pretty, reason);
        if (wrc != EXIT_OK)
            return wrc;
    }
    return EXIT_OK;
}

int cmd_scaffold(const std::string& root, bool force)
{
    Store store(root);
    if (!store.ensure())
        return EXIT_IO;
    if (!write_default_config(root, force))
        return EXIT_IO;

    // Integration snippets (Claude Stop hook, Aider, AGENTS)
    const std::string integ = join_path(root, "if_necessary_you_can_read_me/integrations");
    mkdir_p(join_path(integ, "claude"));
    mkdir_p(join_path(integ, "aider"));

    const std::string stop_hook = R"HOOK(#!/usr/bin/env bash
# Claude Code Stop hook — block stop until verify OK and work quotas met.
# Anti-5-minute-quit: if ghar work session active, require work done readiness.
set -euo pipefail
INPUT=$(cat || true)
if [[ "$(echo "$INPUT" | jq -r '.stop_hook_active // empty' 2>/dev/null)" == "true" ]]; then
  exit 0
fi

ROOT="${CLAUDE_PROJECT_DIR:-$(pwd)}"
cd "$ROOT" || exit 0

GHAR="$(command -v ghar || true)"
if [[ -z "$GHAR" && -x "$ROOT/build/ghar" ]]; then GHAR="$ROOT/build/ghar"; fi
if [[ -z "$GHAR" ]]; then
  echo '{"decision":"block","reason":"ghar not found — build/install grokharness"}'
  exit 0
fi

json_reason() {
  printf '%s' "$1" | tail -c 3500 | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))' 2>/dev/null \
    || echo "\"ghar blocked stop\""
}

set +e
OUT="$("$GHAR" verify -C "$ROOT" 2>&1)"
RC=$?
set -e
if [[ "$RC" -ne 0 ]]; then
  printf '{"decision":"block","reason":%s}\n' "$(json_reason "$OUT")"
  exit 0
fi

# If long work session active, block stop until quotas ready (work status exit 0)
if [[ -f "$ROOT/.ghar/work.tsv" ]] && grep -q $'status\tactive' "$ROOT/.ghar/work.tsv" 2>/dev/null; then
  set +e
  WOUT="$("$GHAR" work status -C "$ROOT" 2>&1)"
  WRC=$?
  set -e
  if [[ "$WRC" -ne 0 ]]; then
    MSG="WORK SESSION NOT DONE — keep working (not 5-minute quit). $WOUT"
    printf '{"decision":"block","reason":%s}\n' "$(json_reason "$MSG")"
    exit 0
  fi
fi
exit 0
)HOOK";

    const std::string settings_frag = R"JSON({
  "hooks": {
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "if_necessary_you_can_read_me/integrations/claude/ghar-stop.sh",
            "timeout": 600
          }
        ]
      }
    ]
  }
}
)JSON";

    const std::string aider_md = R"MD(# Aider + ghar

Industry pattern from https://aider.chat/docs/usage/lint-test.html :

```bash
# After each edit: lint/test via ghar pipeline (config: .ghar/config)
aider --lint-cmd 'ghar verify --no-gate --step lint' \
      --test-cmd  'ghar verify' \
      --auto-test
```

Or single oracle:

```bash
aider --test-cmd 'ghar verify' --auto-test
```

`ghar verify` runs lint→build→test from `.ghar/config`, prints FEEDBACK on failure,
then `ghar gate` (delivery contract).
)MD";

    const std::string agents = R"MD(# Agent delivery contract (ghar)

Borrowed from Aider auto-test + Claude Code Stop hooks:

1. **Never** use another LLM to judge correctness.
2. After edits run **oracles**: `ghar verify` (lint → build → test from `.ghar/config`).
3. On failure, read **FEEDBACK on stderr** (import/compile/verify/gate), fix, re-run — do not hand-wave.
4. Before answering the user: `ghar verify` must exit **0**. Intentional fail demos → `ghar reset`.
5. Optional domain claims: `ghar cuda`, `ghar bench` + `ghar assert`, then verify/gate.
6. Catch model lies: `bash dont_read_me_src/benchmarks/run_hallucination_suite.sh` (need fn=0 fp=0).

```sh
ghar scaffold          # write .ghar/config + hooks
ghar config            # show resolved oracles
# … edit code …
ghar verify            # MUST exit 0 before user delivery
```
)MD";

    write_file(join_path(integ, "claude/ghar-stop.sh"), stop_hook);
    // make executable best-effort
    run_shell("chmod +x " + join_path(integ, "claude/ghar-stop.sh"), 5, root);
    write_file(join_path(integ, "claude/settings.fragment.json"), settings_frag);
    write_file(join_path(integ, "aider/README.md"), aider_md);
    write_file(join_path(integ, "AGENTS.md"), agents);
    // also root-level convenience for agents
    if (force || !path_exists(join_path(root, "AGENTS.md")))
        write_file(join_path(root, "AGENTS.md"), agents);

    // root ghar.conf convenience copy (do not clobber a hand-tuned ghar.conf)
    if (force || !path_exists(join_path(root, "ghar.conf")))
        write_file(join_path(root, "ghar.conf"), default_config_text());

    CheckResult r;
    r.kind = "scaffold";
    r.name = "integrations";
    r.status = "ok";
    r.detail = "wrote .ghar/config + if_necessary_you_can_read_me/integrations/{claude,aider} + AGENTS.md";
    r.metrics["config"] = join_path(root, ".ghar/config");
    return finish_check(store, r, false);
}

}  // namespace ghar

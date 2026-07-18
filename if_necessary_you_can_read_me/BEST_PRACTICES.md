# Best practices ported into ghar

Sources (industry, not invented here):

| Source | Practice | ghar mapping |
|--------|----------|--------------|
| [Aider lint/test](https://aider.chat/docs/usage/lint-test.html) | After each edit run lint/test; non-zero exit → fix loop | `ghar verify`, `.ghar/config` `lint_cmd`/`build_cmd`/`test_cmd` |
| Claude Code **Stop hooks** | Block agent stop until `tsc`/tests pass; feed stderr back | `integrations/claude/ghar-stop.sh` → `ghar verify` |
| SWE-bench / OpenHands / Copilot harness | Unit tests + execution as oracle | project `test_cmd`, not LLM-as-judge |
| EvalPlus / LiveCodeBench | Execute generated code | `ghar run` / `ghar test` / pipeline steps |
| Agent harness surveys | Verification tools > retrieval for code quality | `ghar gate` after substantive claims |

## What we deliberately did **not** copy

- NeMo Guardrails / Cleanlab / Guardrails “hallucination” scores → **another model judging**
- Meta agentic code reasoning without execution → opposite of local execution-first

## Agent contract (required)

### Short task

```
edit → ghar verify → (FEEDBACK → fix)* → exit 0 → answer user
```

### Deep / multi-hour task (anti-5-minute-quit)

Agents often stop after ~5 minutes. **ghar work** makes early delivery a hard fail:

```
ghar work start --minutes 120 --goal "..."
loop:
  real work
  ghar work heartbeat --note "..."
  ghar verify
ghar work done    # exit 0 only if min_minutes + min_verify_ok + min_heartbeats
→ answer user
```

| Quota | Meaning |
|-------|---------|
| `min_work_minutes` | wall-clock floor (default 60) |
| `min_verify_ok` | successful verify pipelines in session |
| `min_heartbeats` | agent must ping while working |
| `heartbeat_max_gap_sec` | stale if silent too long |

Env: `GHAR_MIN_WORK_MINUTES`. Claude Stop hook blocks stop while session active and quotas unmet.

Never: “looks correct to me” without oracle exit 0.  
Never: answer the user while `ghar work done` fails with EARLY-STOP BLOCK.

## Config sketch

```
# .ghar/config or ghar.conf
lint_cmd=...
build_cmd=cmake --build build -j$(nproc)
test_cmd=bash tests/run_all.sh
# step.cuda=...
on_fail=stop
```

## Wiring

```bash
ghar scaffold   # writes config + hooks + AGENTS.md

# Aider
aider --test-cmd 'ghar verify' --auto-test

# Claude Code: merge integrations/claude/settings.fragment.json into settings
```

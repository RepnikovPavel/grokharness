# Grok Build: how to make agents actually autonomous

Research basis (this machine, **grok 0.2.103**):

- CLI: `grok --help`
- Local docs: `~/.grok/docs/user-guide/{04,05,14,15,16,19,22}-*.md`
- Binary internals (strings): `todo_gate_*`, `goal_*`, `laziness_*`, plan-mode block
- Product: https://x.ai/news/introducing-goal
- Your session metadata: `agent_name = "grok-build-plan"`

---

## Why “answers in 1–3 minutes” is NOT fixed by always-approve

| Layer | Role | Stops short chat turns? |
|-------|------|-------------------------|
| Permission / yolo | Auto-allow tools **inside one turn** | **No** |
| Plan mode | **Blocks file edits** even with always-approve | Makes work shorter/shallower |
| Normal chat turn | Model calls “done” and returns text | **Yes — this is the 1–3 min stop** |
| **`/goal`** | Multi-turn harness until checklist + classifier | **Yes** |
| Headless `--max-turns N` | Cap on agentic tool loops in one process | Extends one run |
| **TodoGate** (built-in) | If todos still pending/in_progress, harness **nudges** instead of ending turn | Helps when model uses todos |
| **Laziness classifier** | Detects stall, fires nudge | Internal |
| **Goal classifier / verifiers** | Rejects fake `update_goal(completed:true)` | Prevents false Complete |

Your config already has:

```toml
[ui]
permission_mode = "always-approve"
```

So the agent is **not waiting for permission clicks**. It is **ending the turn** (and this session is labeled **`grok-build-plan`** — plan-oriented agent name in summary.json).

Binary UI string (exact):

> *Always-approve ON: plan mode still blocks file edits until you exit plan mode*

---

## What to do (ordered by impact)

### A. Interactive long work — use **`/goal`** (official)

In TUI (not a plain user message alone):

```text
/goal <objective> [--budget <tokens>]
/goal status
/goal pause | resume | clear
```

Example for this repo:

```text
/goal In /home/user/grokharness: keep implementing and fixing until
ctest -L integration is green and ghar verify exits 0. Budget for deep work.
--budget 2000000
```

Internals (from binary, not fully documented in user-guide):

- Goal planner → workers → **goal classifier** + optional **skeptic verifiers**
- Premature stop detection (`goal_premature_stop_detected`)
- Budget-limited state (`budget_limited` / `budget_exceeded`)
- Fake completion blocked until classifier verdict
- Feature gates exist in binary: `goal_enabled`, `goal_classifier_enabled`,
  `goal_planner_enabled`, `goal_summary_enabled`, `goal_verifier_count`,
  `goal_classifier_max_runs`, `goal_strategist_every`, …

Docs say `/goal` appears only when **goal feature is enabled** and `update_goal`
is in the toolset. If `/goal` is missing from the palette, goal is disabled for
that build/account — use headless recipe B instead.

### B. Unattended / CI-style autonomy — headless multi-turn

```bash
cd /home/user/grokharness

grok -p "$(cat <<'EOF'
Do not stop after a short summary. Keep using tools until:
1) ctest --test-dir build -L integration passes
2) ./build/ghar verify exits 0
3) List remaining risks only after both are green
EOF
)" \
  --yolo \
  --effort high \
  --max-turns 200 \
  --check \
  --cwd /home/user/grokharness
```

| Flag | Meaning (0.2.103) |
|------|-------------------|
| `-p` / `--single` | Headless run |
| `--yolo` / `--always-approve` / `--permission-mode bypassPermissions` | No permission prompts |
| **`--max-turns N`** | Max agentic turns (stop when hit → `max_turns_reached`) |
| **`--check` / `--self-verify`** | Append verification loop to the prompt (headless) |
| `--effort high` / `xhigh` | Deeper reasoning |
| `--best-of-n N` | N parallel attempts, pick best (headless) |
| `-c` / `--continue` or `--resume ID` | Continue after interrupt |

Resume after Ctrl+C:

```bash
grok -p "continue the same goal until tests green" --continue --yolo --max-turns 200
```

### C. Exit plan mode (critical if edits blocked)

```text
# If you entered /plan or agent is plan-profile:
# leave plan mode before expecting long implementation
```

Start implement sessions with full agent, not plan:

```bash
# default general agent
grok --always-approve --effort high

# NOT the read-only plan subagent for implementation
# (bundled agents/plan.md is READ-ONLY, permission_mode: plan)
```

If TUI shows plan mode or you used `/plan`, exit it before long implement work.

### D. Force multi-step via **todos** (TodoGate)

Harness behavior (binary strings):

- Turn-end **TodoGate**: if todos still `pending`/`in_progress`, agent is **nudged**
  instead of being allowed to stop cleanly.
- After max fires: *“exhausted retries, falling through to user”*
  (`todo_gate_max_fires_per_prompt`, `todo_gate_enabled`)

Practical implication: prompts that create a real todo list and say
“do not complete until all todos done” engage TodoGate. Empty todos → no gate.

### E. Project contracts (orthogonal, still useful)

```text
AGENTS.md
ghar work start / verify / work done
hooks: integrations/claude/ghar-stop.sh
```

These **punish false delivery**; they do **not** replace `/goal` or `--max-turns`.

### F. Subagents

Keep enabled (`GROK_SUBAGENTS=1`). Parallel explore/implement/review **inside** a
goal or long headless run — not a substitute for multi-turn goal mode.

---

## Config checklist (`~/.grok/config.toml`)

```toml
[ui]
permission_mode = "always-approve"
# yolo is legacy; permission_mode wins

[models]
default = "grok-build"   # or your coding model slug

# optional depth
# set per session: /effort high
```

Project file (`.grok/config.toml`) is for MCP/plugins/permission **rules** only —
**not** for global `permission_mode` (docs).

Env:

| Env | Use |
|-----|-----|
| `GROK_SUBAGENTS=1` | Subagents on |
| `GROK_AGENT=...` | Custom agent definition |
| `GROK_SANDBOX=off` | Fewer sandbox stops (you already use sandbox off in session) |

---

## What this chat session is doing wrong (evidence)

Session summary fields:

```json
"agent_name": "grok-build-plan",
"reasoning_effort": "high",
"sandbox_profile": "off"
```

- **Plan-named agent** + normal multi-turn chat without `/goal` → short “answers”
- High effort only deepens **one** turn; does not keep the loop alive for hours
- Wall-clock of my reply ≠ autonomy of the **harness**

---

## Minimal recipe for *you* (copy-paste)

### Interactive deep task

```text
1) New session: grok --always-approve --effort high
2) Confirm NOT in plan mode (Shift+Tab / exit plan)
3) /goal <конкретная цель с критерием done>
4) /goal status  until Complete
```

### Headless overnight-style

```bash
grok -p "…" --yolo --effort high --max-turns 300 --check \
  --cwd /home/user/grokharness
```

### If still short

1. Confirm `/goal` exists in palette (feature on)
2. Raise `--max-turns`
3. Use `--check`
4. Ensure todos / goal checklist non-empty
5. Do not use plan agent for implement
6. After `max_turns_reached`, `--continue` with same instructions

---

## References

| Source | What |
|--------|------|
| `grok --help` (0.2.103) | `--max-turns`, `--yolo`, `--check`, `--best-of-n` |
| `~/.grok/docs/user-guide/04-slash-commands.md` | `/goal`, `/always-approve` |
| `~/.grok/docs/user-guide/14-headless-mode.md` | headless automation |
| `~/.grok/docs/user-guide/22-permissions-and-safety.md` | permission modes |
| `~/.grok/bundled/agents/plan.md` | plan = **read-only** |
| Binary strings | TodoGate, goal classifier, laziness, plan-mode block |
| https://x.ai/news/introducing-goal | product description of `/goal` |

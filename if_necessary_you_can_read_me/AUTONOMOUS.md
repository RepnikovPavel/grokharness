# How to configure the agent for autonomous long work

Grok (this CLI agent) does **not** have a hidden “work 2 hours” switch inside the model.
You force long work with **explicit session + hard exit codes**.

## 1. User prompt pattern (required)

Put this at the start of a deep task:

```text
ghar work start --minutes 120 --goal "<конкретная цель с критерием готово>"
Работай автономно до: ghar work done → exit 0.
Каждые 5–10 минут: ghar work heartbeat --note "что сделал".
После правок: ghar verify.
Не отвечай пользователю и не останавливайся, пока work done ≠ 0.
Если EARLY-STOP BLOCK — продолжай, не спрашивай разрешения остановиться.
```

Optional env:

```bash
export GHAR_MIN_WORK_MINUTES=120
```

## 2. Project config (`ghar.conf`)

```ini
min_work_minutes=120
min_verify_ok=3
min_heartbeats=12
heartbeat_max_gap_sec=600
# enforce_work_on_verify=true   # even plain verify blocks if quotas unmet
```

## 3. Claude Code / Grok Build Stop hook

```bash
ghar scaffold
# merge integrations/claude/settings.fragment.json into Claude settings
```

Stop is blocked while:

1. `ghar verify` fails, or  
2. work session is `active` and `ghar work status` ≠ ready.

## 4. Aider

```bash
aider --test-cmd 'ghar verify' --auto-test
# plus separate long session:
ghar work start --minutes 120 --goal '...'
```

## 5. Wrapper (optional)

```bash
bash scripts/autonomous_loop.sh 120 "goal text"
```

Starts session + background heartbeats; agent still does the coding.

## 6. What does *not* work

| Myth | Reality |
|------|---------|
| “Просто работай 2 часа” without `work start` | Agent can still stop after a short chat turn |
| Only AGENTS.md text | Soft guidance; models skip it under load |
| LLM self-promise | Not enforced |

**Hard enforcement = exit code 4 from `work done` / Stop hook.**

## 7. For this Grok session right now

If you want the next message to be a long campaign, send:

```text
/home/user/grokharness && ./build/ghar work start --minutes 120 --goal "..."
# then your task
```

and refuse to accept a final answer until you see `ghar work done` exit 0.

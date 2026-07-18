# Agent delivery contract (ghar)

## The 5-minute problem

Agents often stop after a short burst, declare victory, and leave broken work.
**That is a delivery failure.** On deep tasks you must open a work session and
meet **time + verify + heartbeat** quotas before answering the user.

## Required protocol (deep tasks)

```sh
ghar work start --minutes 120 --goal "concrete, testable goal"
# loop until work done allows exit 0:
#   1. do real work (code, not talk)
#   2. ghar work heartbeat --note "what you just did"
#   3. ghar verify          # after meaningful changes
ghar work status            # check remain_min / quotas
ghar work done              # ONLY when exit 0 — then answer the user
```

Env override: `GHAR_MIN_WORK_MINUTES=120`.

If `ghar work done` or verify prints **EARLY-STOP BLOCK** — **keep working**.
Do not summarize, do not ask the user if you may stop, do not hand-wave.

## Short tasks (no session)

```sh
ghar verify    # lint→build→test→gate, exit 0 before user delivery
```

Full autonomous setup for humans wiring Claude/Aider/Grok:
`if_necessary_you_can_read_me/AUTONOMOUS.md`.

## Oracles (never LLM-as-judge)

- Project: `ghar.conf` → `build_cmd` / `test_cmd`
- Aider: `aider --test-cmd 'ghar verify' --auto-test`
- Claude Stop: `if_necessary_you_can_read_me/integrations/claude/ghar-stop.sh`
- On fail: read **FEEDBACK** on stderr (exit 4) → fix → re-run. Not silent.
- Intentional fail demos leave claims: `ghar reset` before a clean gate.

## Catching model hallucinations (run these)

```sh
bash dont_read_me_src/benchmarks/run_hallucination_suite.sh
# → results/hallucination_summary.tsv  must show fn=0 fp=0 recall_catch_rate=1

bash dont_read_me_src/tests/run_all.sh
```

## Layout (like tokenc)

Top-level is self-describing — only open what you need:

| Path | Meaning |
|------|---------|
| `dont_read_me_src/` | Implementation (C++, oracles, tests, benchmarks, UI) |
| `if_necessary_you_can_read_me/` | Extra docs + integrations |
| `read_me_if_it_is_not_installed/` | Install helpers |
| `prompt.txt` / `AGENTS.md` / `README.md` | Agent + human entry |

Do not invent more first-class folders at the repo root.

## Git commit style (match `tokenc` / RepnikovPavel)

1. **Author** (always):
   ```
   RepnikovPavel <RepnikovPavel@users.noreply.github.com>
   ```
   Do **not** set Author to a bot name.

2. **Subject**: short imperative English sentence (`Add …`, `Fix …`, `Document …`).

3. **Body**: plain prose explaining *why*.

4. **Agent credit** (required by `/grok` — **not** as Author):
   ```
   Co-authored-by: Grok 4.5 <grok@x.ai>
   ```
   Use the literal name **Grok 4.5** (not "Grok Build implementer").

5. Never commit secrets (`/grok/secrets.md`, tokens, passwords, private keys).

Example:

```
Document commit authorship for agents

Align ghar agent commits with the tokenc style: human Author, imperative
subject, explanatory body, agent via Co-authored-by.

Co-authored-by: Grok 4.5 <grok@x.ai>
```

## This repository

```sh
cmake -S . -B build && cmake --build build -j"$(nproc)"
./build/ghar verify
./build/ghar work start --minutes 60 --goal "improve ghar"
```

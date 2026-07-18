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
- Claude Stop: `integrations/claude/ghar-stop.sh`
- On fail: read **FEEDBACK** on stderr (exit 4) → fix → re-run. Not silent.
- Intentional fail demos leave claims: `ghar reset` before a clean gate.

## Catching model hallucinations (run these)

```sh
# Synthetic agent lies vs programmatic checks (import/compile/symbols/assert/run)
bash benchmarks/run_hallucination_suite.sh
# → results/hallucination_summary.tsv  must show fn=0 fp=0 recall_catch_rate=1

bash tests/run_all.sh   # integration + hallucination suite on real binary
```

## Git commit style (match `tokenc` / RepnikovPavel)

Reference: the owner's **tokenc** repo voice — not Conventional Commits as Author.

1. **Author** (always):
   ```
   RepnikovPavel <RepnikovPavel@users.noreply.github.com>
   ```
   Do **not** set Author to `Grok Build`, `OpenCode Agent`, or other bot identities.

2. **Subject**: short **imperative English sentence** (title case OK), like tokenc:
   - Good: `Add FEEDBACK on stderr for failed claims`
   - Good: `Expand OpenMMLab scan to full org repos`
   - Good: `Document commit authorship for agents`
   - Avoid as the only style: `feat(scope): …` / `fix(ui): …` unless the rest of the
     message still reads like a plain English change note.

3. **Body**: plain prose explaining *why* and what changed; complete sentences.

4. **Agent credit** (required by `/grok`, without stealing Author):
   ```
   Co-authored-by: Grok Build implementer <grok-build@x.ai>
   ```
   Also put model version in the body or a trailer, e.g. `Model: Grok 4.5 (xAI)`.

5. Never commit secrets (`/grok/secrets.md`, tokens, passwords, private keys).

Example:

```
Document commit authorship for agents

Align ghar agent commits with the tokenc style: human Author, imperative
subject, explanatory body, agent via Co-authored-by.

Co-authored-by: Grok Build implementer <grok-build@x.ai>
```

## This repository

```sh
cmake -S . -B build && cmake --build build -j"$(nproc)"
./build/ghar verify
./build/ghar work start --minutes 60 --goal "improve ghar"
```

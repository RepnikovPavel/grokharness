# Agent delivery contract (ghar)

Borrowed from Aider auto-test + Claude Code Stop hooks:

1. **Never** use another LLM to judge correctness.
2. After edits run **oracles**: `ghar verify` (lint → build → test from `.ghar/config`).
3. On failure, read **FEEDBACK on stderr** (import/compile/verify/gate), fix, re-run — do not hand-wave.
4. Before answering the user: `ghar verify` must exit **0**. Intentional fail demos → `ghar reset`.
5. Optional domain claims: `ghar cuda`, `ghar bench` + `ghar assert`, then verify/gate.
6. Catch model lies: `bash benchmarks/run_hallucination_suite.sh` (need fn=0 fp=0).

```sh
ghar scaffold          # write .ghar/config + hooks
ghar config            # show resolved oracles
# … edit code …
ghar verify            # MUST exit 0 before user delivery
```

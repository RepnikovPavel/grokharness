# Agent delivery contract (ghar)

Borrowed from Aider auto-test + Claude Code Stop hooks:

1. **Never** use another LLM to judge correctness.
2. After edits run **oracles**: `ghar verify` (lint → build → test from `.ghar/config`).
3. On failure, read `ghar verify FEEDBACK`, fix, re-run — do not hand-wave.
4. Before answering the user: `ghar verify` must exit **0**.
5. Optional domain claims: `ghar cuda`, `ghar bench` + `ghar assert`, then verify/gate.

```sh
ghar scaffold          # write .ghar/config + hooks
ghar config            # show resolved oracles
# … edit code …
ghar verify            # MUST exit 0 before user delivery
```

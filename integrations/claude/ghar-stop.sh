#!/usr/bin/env bash
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

#!/usr/bin/env bash
# Template for long autonomous agent sessions (anti-5-minute-quit).
# Usage:
#   bash scripts/autonomous_loop.sh 120 "implement X end-to-end"
# The AGENT still does the real work; this script enforces heartbeats + verify cadence.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GHAR="${GHAR:-$ROOT/build/ghar}"
MINUTES="${1:-120}"
GOAL="${2:-autonomous work session}"

if [[ ! -x "$GHAR" ]]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j"$(nproc)"
  GHAR="$ROOT/build/ghar"
fi

cd "$ROOT"
"$GHAR" work start --minutes "$MINUTES" --goal "$GOAL" --force --format

echo "Session started. Agent must keep working until: ghar work done → 0"
echo "Suggested cadence: every 5–10 min → ghar work heartbeat --note '...'"
echo "After each meaningful change → ghar verify"
echo

# Heartbeat daemon (background) so wall-clock sessions don't go stale
HB_GAP=300
(
  n=0
  while true; do
    sleep "$HB_GAP"
    n=$((n + 1))
    if ! "$GHAR" work status >/dev/null 2>&1; then
      # status exit 4 means not ready yet but session may still be active
      :
    fi
    st="$("$GHAR" work status 2>/dev/null | head -1 || true)"
    if echo "$st" | grep -q 'status=done\|status.*none'; then
      exit 0
    fi
    "$GHAR" work heartbeat --note "auto-hb #$n $(date -u +%H:%M:%SZ)" || true
  done
) &
HB_PID=$!
trap 'kill $HB_PID 2>/dev/null || true' EXIT

echo "Auto-heartbeat PID=$HB_PID every ${HB_GAP}s"
echo "Run your agent against this repo; finish with: $GHAR work done"
# Block until session ends (done/abandon) — optional wait loop for CI wrappers
while true; do
  set +e
  "$GHAR" work status >/tmp/ghar_work_st_$$ 2>&1
  rc=$?
  set -e
  if grep -q $'status\tdone\|status=done\|work status: done' /tmp/ghar_work_st_$$ 2>/dev/null; then
    echo "Session done."
    exit 0
  fi
  if grep -q 'abandoned\|status: none' /tmp/ghar_work_st_$$ 2>/dev/null; then
    echo "Session ended without success."
    exit 4
  fi
  # print remain once in a while
  "$GHAR" work status --format 2>/dev/null | head -12 || true
  sleep 60
done

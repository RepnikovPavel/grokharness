#!/usr/bin/env bash
# Shared helpers for ghar integration tests.
set -euo pipefail

# Repo root (this file lives in dont_read_me_src/tests/)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/dont_read_me_src"
# Always prefer an absolute path so tests still work after `cd` into TMPBASE.
if [[ -n "${GHAR:-}" && -x "$GHAR" ]]; then
  if [[ "$GHAR" != /* ]]; then
    GHAR="$(cd "$(dirname "$GHAR")" && pwd)/$(basename "$GHAR")"
  fi
elif [[ -x "$ROOT/build/ghar" ]]; then
  GHAR="$ROOT/build/ghar"
elif command -v ghar >/dev/null 2>&1; then
  GHAR="$(command -v ghar)"
else
  echo "FAIL: ghar binary not found (build first)" >&2
  exit 2
fi
export GHAR
export GHAR_ROOT="${GHAR_ROOT:-$ROOT}"

TMPBASE="${TMPDIR:-/tmp}/ghar_tests_$$"
mkdir -p "$TMPBASE"
cleanup() { rm -rf "$TMPBASE"; }
trap cleanup EXIT

pass=0
fail=0

assert_eq() {
  local got="$1" want="$2" msg="${3:-}"
  if [[ "$got" == "$want" ]]; then
    pass=$((pass + 1))
    echo "  OK  ${msg} (got=$got)"
  else
    fail=$((fail + 1))
    echo "  FAIL ${msg}: got='$got' want='$want'" >&2
  fi
}

assert_exit() {
  local want="$1"
  shift
  set +e
  "$@" >/dev/null 2>&1
  local got=$?
  set -e
  assert_eq "$got" "$want" "exit $* → $want"
}

assert_contains() {
  local hay="$1" needle="$2" msg="${3:-contains}"
  if [[ "$hay" == *"$needle"* ]]; then
    pass=$((pass + 1))
    echo "  OK  $msg"
  else
    fail=$((fail + 1))
    echo "  FAIL $msg: missing '$needle' in: ${hay:0:200}" >&2
  fi
}

summary() {
  echo "---- $1: pass=$pass fail=$fail ----"
  if [[ "$fail" -ne 0 ]]; then
    exit 4
  fi
  exit 0
}

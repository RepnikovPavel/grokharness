#!/usr/bin/env bash
# CLI smoke: version, help, doctor, init, gate empty fails
source "$(dirname "$0")/common.sh"

echo "== test_cli_smoke =="

out="$("$GHAR" --version)"
assert_contains "$out" "ghar" "version string"

assert_exit 0 "$GHAR" --help

work="$TMPBASE/smoke"
mkdir -p "$work"
cd "$work"

assert_exit 0 "$GHAR" init
assert_eq "$(test -d .ghar && echo yes)" "yes" ".ghar created"

assert_exit 0 "$GHAR" doctor

# empty claims → gate must fail (no free pass)
assert_exit 4 "$GHAR" gate

# unknown command
assert_exit 1 "$GHAR" notacommand

summary test_cli_smoke

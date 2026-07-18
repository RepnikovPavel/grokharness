#!/usr/bin/env bash
# import truth, assert chain, gate pass/fail
source "$(dirname "$0")/common.sh"

echo "== test_import_assert_gate =="
work="$TMPBASE/assert"
mkdir -p "$work"
cd "$work"
"$GHAR" init >/dev/null

assert_exit 0 "$GHAR" import sys os --name stdlib
assert_exit 4 "$GHAR" import definitely_not_installed_pkg_42 --name fake_pkg

# bench true is fast; assert lt 1000 ok, gt 1e9 fails
assert_exit 0 "$GHAR" bench --name tbench --warmup 0 --repeat 3 -- true
assert_exit 0 "$GHAR" assert --from tbench --metric mean_ms --op lt --value 1000 --name a_ok
assert_exit 4 "$GHAR" assert --from tbench --metric mean_ms --op gt --value 1000000 --name a_bad

# gate must fail due to fake_pkg + a_bad
assert_exit 4 "$GHAR" gate

# reset and only good claims → gate ok
assert_exit 0 "$GHAR" reset
assert_exit 0 "$GHAR" import sys --name stdlib2
assert_exit 0 "$GHAR" bench --name tbench2 --warmup 0 --repeat 2 -- true
assert_exit 0 "$GHAR" assert --from tbench2 --metric mean_ms --op lt --value 5000 --name a_ok2
assert_exit 0 "$GHAR" gate

summary test_import_assert_gate

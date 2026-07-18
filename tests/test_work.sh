#!/usr/bin/env bash
# Long-work session: early-stop block + quotas (anti-5-minute-quit)
source "$(dirname "$0")/common.sh"

echo "== test_work =="
work="$TMPBASE/work"
mkdir -p "$work"
cd "$work"

cat > ghar.conf <<'EOF'
timeout_sec=60
on_fail=stop
build_cmd=true
test_cmd=true
min_work_minutes=60
min_verify_ok=2
min_heartbeats=3
heartbeat_max_gap_sec=86400
EOF

assert_exit 0 "$GHAR" init

# Session requiring wall time → done must fail early
assert_exit 0 "$GHAR" work start --minutes 60 --min-verify 1 --min-heartbeats 1 \
  --goal "deep work test" --force --format
assert_exit 4 "$GHAR" work status
assert_exit 4 "$GHAR" work done

set +e
out="$("$GHAR" work done 2>&1)"
rc=$?
set -e
assert_eq "$rc" "4" "done blocked"
assert_contains "$out" "early_stop" "mentions early_stop"

# Fast session: zero wall wait, quotas via verify+heartbeat
assert_exit 0 "$GHAR" work start --minutes 0 --min-verify 2 --min-heartbeats 2 \
  --goal "fast quotas" --force
assert_exit 0 "$GHAR" work heartbeat --note "h1"
assert_exit 0 "$GHAR" work heartbeat --note "h2"
assert_exit 0 "$GHAR" verify --no-gate
assert_exit 0 "$GHAR" verify --no-gate
assert_exit 0 "$GHAR" work status --format
assert_exit 0 "$GHAR" work done --format

# abandon fails delivery
assert_exit 0 "$GHAR" work start --minutes 30 --goal "will abandon" --force
assert_exit 4 "$GHAR" work abandon --note "user cancelled"

summary test_work

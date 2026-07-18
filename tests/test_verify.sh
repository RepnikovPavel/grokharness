#!/usr/bin/env bash
# verify pipeline + scaffold (Aider / Stop-hook practices)
source "$(dirname "$0")/common.sh"

echo "== test_verify =="
work="$TMPBASE/verify"
mkdir -p "$work"
cd "$work"

# minimal project with oracle that succeeds
cat > ghar.conf <<'EOF'
timeout_sec=60
on_fail=stop
build_cmd=true
test_cmd=true
EOF

assert_exit 0 "$GHAR" init
assert_exit 0 "$GHAR" config
assert_exit 0 "$GHAR" verify --format

# failing oracle → exit 4 + FEEDBACK
cat > ghar.conf <<'EOF'
timeout_sec=60
on_fail=stop
build_cmd=true
test_cmd=false
EOF
assert_exit 0 "$GHAR" reset
assert_exit 4 "$GHAR" verify

# single step
cat > ghar.conf <<'EOF'
build_cmd=true
test_cmd=true
lint_cmd=true
EOF
assert_exit 0 "$GHAR" verify --step build --no-gate

# scaffold writes integrations
assert_exit 0 "$GHAR" scaffold --force
assert_eq "$(test -f integrations/claude/ghar-stop.sh && echo yes)" "yes" "claude stop hook"
assert_eq "$(test -f integrations/aider/README.md && echo yes)" "yes" "aider readme"
assert_eq "$(test -f AGENTS.md && echo yes)" "yes" "AGENTS.md"

summary test_verify
